// SPDX-License-Identifier: BSD-2-Clause

// Copyright (c) 2019-2020, Paul Ferrand, Andrea Zanellato
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "FilePool.h"
#include "AudioReader.h"
#include "Buffer.h"
#include "AudioBuffer.h"
#include "AudioSpan.h"
#include "Config.h"
#include "SynthConfig.h"
#include "utility/SwapAndPop.h"
#include "utility/Debug.h"
#include <ThreadPool.h>
#include <absl/types/span.h>
#include <absl/strings/match.h>
#include <absl/memory/memory.h>
#include <algorithm>
#include <memory>
#include <thread>
#include <system_error>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#if defined(SFIZZ_FILEOPENPREEXEC)
#include "FileOpenPreexec.h"
#endif
using namespace std::placeholders;

sfz::FilePool::GlobalObject::GlobalObject(size_t num_threads)
: lastGarbageCollection_ { highResNow() },
  threadPool { new ThreadPool(num_threads) }
{
    garbageThread.reset(new std::thread(&GlobalObject::garbageJob, this));
}

sfz::FilePool::GlobalObject::~GlobalObject()
{
    garbageFlag = false;
    std::error_code ec;
    semGarbageBarrier.post(ec);
    ASSERT(!ec);
    garbageThread->join();
    // clear semaphore
    while (semGarbageBarrier.try_wait());
}

RTSemaphore sfz::FilePool::GlobalObject::semGarbageBarrier { 0 };
std::weak_ptr<sfz::FilePool::GlobalObject> sfz::FilePool::globalObjectWeakPtr;
std::mutex sfz::FilePool::globalObjectMutex;

std::shared_ptr<sfz::FilePool::GlobalObject> sfz::FilePool::getGlobalObject()
{
    std::lock_guard<std::mutex> lock { globalObjectMutex };

    std::shared_ptr<GlobalObject> globalObject = globalObjectWeakPtr.lock();
    if (globalObject)
        return globalObject;

#if 1
    constexpr size_t numThreads = 1;
#else
    size_t numThreads = std::thread::hardware_concurrency();
    numThreads = (numThreads > 2) ? (numThreads - 2) : 1;
#endif
    globalObject.reset(new GlobalObject(numThreads));
    globalObjectWeakPtr = globalObject;
    return globalObject;
}

void sfz::FileData::addOwner(FilePool* owner)
{
    std::lock_guard<std::mutex> guard { ownerMutex };
    auto it = ownerMap.find(owner);
    if (it != ownerMap.end()) {
        if (!it->second) {
            it->second = true;
            preloadCallCount++;
        }
    }
    else {
        ownerMap[owner] = true;
        preloadCallCount += 2;
    }
}

void sfz::FileData::prepareForRemovingOwner(FilePool* owner)
{
    std::lock_guard<std::mutex> guard { ownerMutex };
    auto it = ownerMap.find(owner);
    if (it != ownerMap.end()) {
        if (it->second) {
            it->second = false;
            preloadCallCount--;
        }
    }
}

bool sfz::FileData::checkAndRemoveOwner(FilePool* owner, const FileId& fileId)
{
    std::lock_guard<std::mutex> lock { ownerMutex };
    auto it = ownerMap.find(owner);
    if (it != ownerMap.end()) {
        if (!it->second) {
            ownerMap.erase(it);
            --preloadCallCount;
            return true;
        }
    }
    return false;
}

bool sfz::FileData::addSecondaryOwner(FilePool* owner)
{
    {
        std::unique_lock<std::mutex> guard { readyMutex };
        if (!readyCond.wait_for(guard, std::chrono::seconds(10), [this]{ return ready; })) {
            return false;
        }
    }

    std::lock_guard<std::mutex> guard2 { ownerMutex };
    if (preloadCallCount != 0) {
        auto it = ownerMap.find(owner);
        if (it != ownerMap.end()) {
            if (!it->second) {
                it->second = true;
                preloadCallCount++;
            }
        }
        else {
            ownerMap[owner] = true;
            preloadCallCount += 2;
        }
        return true;
    }
    return false;
}

void readBaseFile(sfz::AudioReader& reader, sfz::FileAudioBuffer& output, uint32_t numFrames)
{
    output.reset();
    output.resize(numFrames);

    const unsigned channels = reader.channels();

    if (channels == 1) {
        output.addChannel();
        output.clear();
        reader.readNextBlock(output.channelWriter(0), numFrames);
    } else if (channels == 2) {
        output.addChannel();
        output.addChannel();
        output.clear();
        sfz::Buffer<float> tempReadBuffer { 2 * numFrames };
        reader.readNextBlock(tempReadBuffer.data(), numFrames);
        sfz::readInterleaved(tempReadBuffer, output.getSpan(0), output.getSpan(1));
    }
}

sfz::FileAudioBuffer readFromFile(sfz::AudioReader& reader, uint32_t numFrames)
{
    sfz::FileAudioBuffer baseBuffer;
    readBaseFile(reader, baseBuffer, numFrames);
    return baseBuffer;
}

/*
 * this function returns false when the loading is not completed.
 */
bool streamFromFile(sfz::AudioReader& reader, sfz::FileAudioBuffer& output, std::atomic<size_t>& filledFrames, bool freeWheeling)
{
    const auto numFrames = static_cast<size_t>(reader.frames());
    const auto numChannels = reader.channels();
    const auto chunkSize = static_cast<size_t>(sfz::config::fileChunkSize);

    if (filledFrames == 0) {
        output.reset();
        output.addChannels(reader.channels());
        output.resize(numFrames);
        output.clear();
    }

    sfz::Buffer<float> fileBlock { chunkSize * numChannels };
    size_t inputFrameCounter { filledFrames };
    size_t outputFrameCounter { inputFrameCounter };
    bool inputEof = false;
    bool seekable = reader.seekable();
    if (seekable)
        reader.seek(inputFrameCounter);

    int chunkCounter = (freeWheeling || !seekable) ? INT_MAX : static_cast<int>(sfz::config::numChunkForLoadingAtOnce);

    while (!inputEof && inputFrameCounter < numFrames)
    {
        if (chunkCounter-- == 0)
            return false;

        auto thisChunkSize = std::min(chunkSize, numFrames - inputFrameCounter);
        const auto numFramesRead = static_cast<size_t>(
            reader.readNextBlock(fileBlock.data(), thisChunkSize));
        if (numFramesRead == 0)
            break;

        if (numFramesRead < thisChunkSize) {
            inputEof = true;
            thisChunkSize = numFramesRead;
        }
        const auto outputChunkSize = thisChunkSize;

        for (size_t chanIdx = 0; chanIdx < numChannels; chanIdx++) {
            const auto outputChunk = output.getSpan(chanIdx).subspan(outputFrameCounter, outputChunkSize);
            for (size_t i = 0; i < thisChunkSize; ++i)
                outputChunk[i] = fileBlock[i * numChannels + chanIdx];
        }
        inputFrameCounter += thisChunkSize;
        outputFrameCounter += outputChunkSize;

        filledFrames.fetch_add(outputChunkSize);
    }
    return true;
}

#if defined(SFIZZ_FILEOPENPREEXEC)
sfz::FilePool::FilePool(const SynthConfig& synthConfig, FileOpenPreexec& preexec_in)
#else
sfz::FilePool::FilePool(const SynthConfig& synthConfig)
#endif
    : filesToLoad(alignedNew<FileQueue>()),
      globalObject(getGlobalObject()),
      synthConfig(synthConfig)
#if defined(SFIZZ_FILEOPENPREEXEC)
    , preexec(preexec_in)
#endif
{
    loadingJobs.reserve(config::maxVoices * 16);
}

sfz::FilePool::~FilePool()
{
    clear();

    std::error_code ec;

    dispatchFlag = false;
    dispatchBarrier.post(ec);
    dispatchThread.join();

    for (auto& job : loadingJobs)
        job.wait();
}

bool sfz::FilePool::checkSample(std::string& filename) const noexcept
{
    fs::path path { rootDirectory / filename };
    std::error_code ec;
#if defined(SFIZZ_FILEOPENPREEXEC)
    bool ret = false;
    preexec.executeFileOpen(path, [&ec, &ret, &path](const fs::path &path2) {
        if (fs::exists(path2, ec)) {
            ret = true;
            path = path2;
        }
    });
    if (ret) {
        filename = path.string();
        return true;
    }
    return false;
#else
    if (fs::exists(path, ec))
        return true;

#if defined(_WIN32)
    return false;
#else
    fs::path oldPath = std::move(path);
    path = oldPath.root_path();

    static const fs::path dot { "." };
    static const fs::path dotdot { ".." };

    for (const fs::path& part : oldPath.relative_path()) {
        if (part == dot || part == dotdot) {
            path /= part;
            continue;
        }

        if (fs::exists(path / part, ec)) {
            path /= part;
            continue;
        }

        auto it = path.empty() ? fs::directory_iterator { dot, ec } : fs::directory_iterator { path, ec };
        if (ec) {
            DBG("Error creating a directory iterator for " << filename << " (Error code: " << ec.message() << ")");
            return false;
        }

        auto searchPredicate = [&part](const fs::directory_entry &ent) -> bool {
#if !defined(GHC_USE_WCHAR_T)
            return absl::EqualsIgnoreCase(
                ent.path().filename().native(), part.native());
#else
            return absl::EqualsIgnoreCase(
                ent.path().filename().u8string(), part.u8string());
#endif
        };

        while (it != fs::directory_iterator {} && !searchPredicate(*it))
            it.increment(ec);

        if (it == fs::directory_iterator {}) {
            DBG("File not found, could not resolve " << filename);
            return false;
        }

        path /= it->path().filename();
    }

    const auto newPath = fs::relative(path, rootDirectory, ec);
    if (ec) {
        DBG("Error extracting the new relative path for " << filename << " (Error code: " << ec.message() << ")");
        return false;
    }
    DBG("Updating " << filename << " to " << newPath);
    filename = newPath.string();
    return true;
#endif
#endif
}

bool sfz::FilePool::checkSampleId(FileId& fileId) const noexcept
{
    if (loadedFiles.contains(fileId))
        return true;

    std::string filename = fileId.filename();
    bool result = checkSample(filename);
    if (result)
        fileId = FileId(std::move(filename), fileId.isReverse());
    return result;
}

absl::optional<sfz::FileInformation> getReaderInformation(sfz::AudioReader* reader) noexcept
{
    const unsigned channels = reader->channels();
    if (channels != 1 && channels != 2)
        return {};

    sfz::FileInformation returnedValue;
    returnedValue.end = static_cast<uint32_t>(reader->frames()) - 1;
    returnedValue.sampleRate = static_cast<double>(reader->sampleRate());
    returnedValue.numChannels = static_cast<int>(channels);

    // Check for instrument info
    sfz::InstrumentInfo instrumentInfo {};
    if (reader->getInstrumentInfo(instrumentInfo)) {
        returnedValue.rootKey = clamp<uint8_t>(instrumentInfo.basenote, 0, 127);
        if (reader->type() == sfz::AudioReaderType::Forward) {
            if (instrumentInfo.loop_count > 0) {
                returnedValue.hasLoop = true;
                returnedValue.loopStart = instrumentInfo.loops[0].start;
                returnedValue.loopEnd =
                    min(returnedValue.end, static_cast<int64_t>(instrumentInfo.loops[0].end - 1));
            }
        } else {
            // TODO loops ignored when reversed
            //   prehaps it can make use of SF_LOOP_BACKWARD?
        }
    }

    // Check for wavetable info
    sfz::WavetableInfo wt {};
    if (reader->getWavetableInfo(wt))
        returnedValue.wavetable = wt;

    return returnedValue;
}

absl::optional<sfz::FileInformation> sfz::FilePool::checkExistingFileInformation(const FileId& fileId) noexcept
{
    const auto loadedFile = loadedFiles.find(fileId);
    if (loadedFile != loadedFiles.end())
        return loadedFile->second->information;

    const auto preloadedFile = preloadedFiles.find(fileId);
    if (preloadedFile != preloadedFiles.end())
        return preloadedFile->second->information;

    return {};
}

absl::optional<sfz::FileInformation> sfz::FilePool::getFileInformation(const FileId& fileId) noexcept
{
    auto existingInformation = checkExistingFileInformation(fileId);
    if (existingInformation)
        return existingInformation;

    const fs::path file { rootDirectory / fileId.filename() };

#if defined(SFIZZ_FILEOPENPREEXEC)
    AudioReaderPtr reader = createAudioReader(file, fileId.isReverse(), preexec);
#else
    if (!fs::exists(file))
        return {};

    AudioReaderPtr reader = createAudioReader(file, fileId.isReverse());
#endif
    return getReaderInformation(reader.get());
}

bool sfz::FilePool::preloadFile(const FileId& fileId, uint32_t maxOffset) noexcept
{
    const auto loadedFile = loadedFiles.find(fileId);
    if (loadedFile != loadedFiles.end()) {
        auto& fileData = loadedFile->second;
        if (fileData->addSecondaryOwner(this))
            return true;
    }

    {
        std::lock_guard<std::mutex> guard { globalObject->loadedFilesMutex };
        auto& files = globalObject->loadedFiles;
        const auto loadedFile = files.find(fileId);
        if (loadedFile != files.end()) {
            auto& fileData = loadedFile->second;
            if (fileData->addSecondaryOwner(this)) {
                loadedFiles[fileId] = fileData;
                return true;
            }
        }
    }

    auto fileInformation = getFileInformation(fileId);
    if (!fileInformation)
        return false;

    fileInformation->maxOffset = maxOffset;
    const fs::path file { rootDirectory / fileId.filename() };
#if defined(SFIZZ_FILEOPENPREEXEC)
    AudioReaderPtr reader = createAudioReader(file, fileId.isReverse(), preexec);
#else
    AudioReaderPtr reader = createAudioReader(file, fileId.isReverse());
#endif

    const auto frames = static_cast<uint32_t>(reader->frames());
    const auto framesToLoad = [&]() {
        if (loadInRam)
            return frames;
        else
            return min(frames, maxOffset + preloadSize);
    }();

    const auto existingFile = preloadedFiles.find(fileId);
    if (existingFile != preloadedFiles.end()) {
        auto& fileData = existingFile->second;
        if (fileData->addSecondaryOwner(this)) {
            if (framesToLoad > fileData->preloadedData.getNumFrames()) {
                fileData->information.maxOffset = maxOffset;
                fileData->preloadedData = readFromFile(*reader, framesToLoad);
                if (frames == framesToLoad && fileData->status != FileData::Status::FullLoaded)
                    // Forced overwrite status
                    fileData->status = FileData::Status::FullLoaded;
            }
            return true;
        }
    }
    {
        auto& files = globalObject->preloadedFiles;
        std::unique_lock<std::mutex> guard { globalObject->preloadedFilesMutex };
        const auto existingFile = files.find(fileId);
        if (existingFile != files.end()) {
            auto fileData = existingFile->second;
            if (fileData->addSecondaryOwner(this)) {
                preloadedFiles[fileId] = fileData;
                guard.unlock();
                if (framesToLoad > fileData->preloadedData.getNumFrames()) {
                    fileData->information.maxOffset = maxOffset;
                    fileData->preloadedData = readFromFile(*reader, framesToLoad);
                }
                if (fileData->status == FileData::Status::FullLoaded || fileData->status == FileData::Status::Preloaded) {
                    fileData->status = frames == framesToLoad ? FileData::Status::FullLoaded : FileData::Status::Preloaded;
                }
                return true;
            }
        }
        fileInformation->sampleRate = static_cast<double>(reader->sampleRate());

        auto insertedPair = preloadedFiles.insert_or_assign(fileId, std::shared_ptr<FileData>(new FileData()));
        auto& fileData = insertedPair.first->second;
        files[fileId] = fileData;
        fileData->addOwner(this);
        guard.unlock();
        fileData->initWith(frames == framesToLoad ? FileData::Status::FullLoaded : FileData::Status::Preloaded, {
            readFromFile(*reader, framesToLoad),
            *fileInformation
        });
    }

    return true;
}

void sfz::FilePool::resetPreloadCallCounts() noexcept
{
    for (auto& preloadedFile: preloadedFiles)
        preloadedFile.second->prepareForRemovingOwner(this);

    for (auto& loadedFile: loadedFiles)
        loadedFile.second->prepareForRemovingOwner(this);
}

void sfz::FilePool::removeUnusedPreloadedData() noexcept
{
    for (auto it = preloadedFiles.begin(), end = preloadedFiles.end(); it != end; ) {
        auto copyIt = it++;
        if (copyIt->second->checkAndRemoveOwner(this, copyIt->first)) {
            DBG("[sfizz] Removing unused preloaded data: " << copyIt->first.filename());
            preloadedFiles.erase(copyIt);
        }
    }

    for (auto it = loadedFiles.begin(), end = loadedFiles.end(); it != end; ) {
        auto copyIt = it++;
        if (copyIt->second->checkAndRemoveOwner(this, copyIt->first)) {
            DBG("[sfizz] Removing unused loaded data: " << copyIt->first.filename());
            loadedFiles.erase(copyIt);
        }
    }
}

sfz::FileDataHolder sfz::FilePool::loadFile(const FileId& fileId) noexcept
{
    auto fileInformation = getFileInformation(fileId);
    if (!fileInformation)
        return {};

    const auto existingFile = loadedFiles.find(fileId);
    if (existingFile != loadedFiles.end()) {
        auto& fileData = existingFile->second;
        if (fileData->addSecondaryOwner(this))
            return { fileData };
    }

    auto& files = globalObject->loadedFiles;
    std::unique_lock<std::mutex> guard { globalObject->loadedFilesMutex };
    {
        const auto existingFile = files.find(fileId);
        if (existingFile != files.end()) {
            auto& fileData = existingFile->second;
            if (fileData->addSecondaryOwner(this)) {
                loadedFiles[fileId]= fileData;
                return { fileData };
            }
        }
    }

    const fs::path file { rootDirectory / fileId.filename() };
#if defined(SFIZZ_FILEOPENPREEXEC)
    AudioReaderPtr reader = createAudioReader(file, fileId.isReverse(), preexec);
#else
    AudioReaderPtr reader = createAudioReader(file, fileId.isReverse());
#endif

    const auto frames = static_cast<uint32_t>(reader->frames());
    auto insertedPair = loadedFiles.insert_or_assign(fileId, std::shared_ptr<FileData>(new FileData()));
    auto& fileData = insertedPair.first->second;
    files[fileId] = fileData;
    fileData->addOwner(this);
    guard.unlock();
    fileData->initWith(FileData::Status::FullLoaded, {
        readFromFile(*reader, frames),
        *fileInformation
    });
    return { fileData };
}

sfz::FileDataHolder sfz::FilePool::loadFromRam(const FileId& fileId, const std::vector<char>& data) noexcept
{
    const auto loaded = loadedFiles.find(fileId);
    if (loaded != loadedFiles.end()) {
        auto& fileData = loaded->second;
        if (fileData->addSecondaryOwner(this))
            return { fileData };
    }

    auto& files = globalObject->loadedFiles;
    std::unique_lock<std::mutex> guard { globalObject->loadedFilesMutex };
    {
        const auto loaded = files.find(fileId);
        if (loaded != files.end()) {
            auto& fileData = loaded->second;
            if (fileData->addSecondaryOwner(this)) {
                loadedFiles[fileId] = fileData;
                return { fileData };
            }
        }
    }

    auto reader = createAudioReaderFromMemory(data.data(), data.size(), fileId.isReverse());
    auto fileInformation = getReaderInformation(reader.get());
    const auto frames = static_cast<uint32_t>(reader->frames());
    auto insertedPair = loadedFiles.insert_or_assign(fileId, std::shared_ptr<FileData>(new FileData()));
    auto& fileData = insertedPair.first->second;
    files[fileId] = fileData;
    fileData->addOwner(this);
    guard.unlock();
    fileData->initWith(FileData::Status::FullLoaded, {
        readFromFile(*reader, frames),
        *fileInformation
    });
    DBG("Added a file " << fileId.filename());
    return { fileData };
}

sfz::FileDataHolder sfz::FilePool::getFilePromise(const std::shared_ptr<FileId>& fileId) noexcept
{
    const auto loaded = loadedFiles.find(*fileId);
    if (loaded != loadedFiles.end())
        return { loaded->second };

    const auto preloaded = preloadedFiles.find(*fileId);
    if (preloaded == preloadedFiles.end()) {
        DBG("[sfizz] File not found in the preloaded files: " << fileId->filename());
        return {};
    }

    auto& fileData = preloaded->second;
    auto status = fileData->status.load();
    if (status == FileData::Status::Preloaded) {
        QueuedFileData queuedData { fileId, fileData };
        if (!filesToLoad->try_push(queuedData)) {
            DBG("[sfizz] Could not enqueue the file to load for " << fileId << " (queue capacity " << filesToLoad->capacity() << ")");
            return {};
        }
        fileData->status.compare_exchange_strong(status, FileData::Status::PendingStreaming);
        std::error_code ec;
        dispatchBarrier.post(ec);
        ASSERT(!ec);
    }

    return { fileData };
}

void sfz::FilePool::setPreloadSize(uint32_t preloadSize) noexcept
{
    this->preloadSize = preloadSize;
    if (loadInRam)
        return;

    // Update all the preloaded sizes
    for (auto& preloadedFile : preloadedFiles) {
        auto& fileId = preloadedFile.first;
        auto& fileData = preloadedFile.second;
        const uint32_t maxOffset = fileData->information.maxOffset;
        fs::path file { rootDirectory / fileId.filename() };
#if defined(SFIZZ_FILEOPENPREEXEC)
        AudioReaderPtr reader = createAudioReader(file, fileId.isReverse(), preexec);
#else
        AudioReaderPtr reader = createAudioReader(file, fileId.isReverse());
#endif
        const uint32_t frames = static_cast<uint32_t>(reader->frames());
        const uint32_t framesToLoad = min(frames, maxOffset + preloadSize);
        fileData->preloadedData = readFromFile(*reader, framesToLoad);

        FileData::Status status = fileData->status;
        bool fullLoaded = frames == framesToLoad;
        if (fullLoaded && status != FileData::Status::FullLoaded) {
            // Forced overwrite status
            fileData->status = FileData::Status::FullLoaded;
        }
        else if (!fullLoaded && status == FileData::Status::FullLoaded) {
            // Exchange status
            fileData->status.compare_exchange_strong(status, FileData::Status::Preloaded);
        }
    }
}

void sfz::FilePool::loadingJob(const QueuedFileData& data) noexcept
{
    raiseCurrentThreadPriority();

    std::shared_ptr<FileId> id = data.id.lock();
    if (!id) {
        // file ID was nulled, it means the region was deleted, ignore
        return;
    }

    const fs::path file { rootDirectory / id->filename() };
    std::error_code readError;
#if defined(SFIZZ_FILEOPENPREEXEC)
    AudioReaderPtr reader = createAudioReader(file, id->isReverse(), preexec, &readError);
#else
    AudioReaderPtr reader = createAudioReader(file, id->isReverse(), &readError);
#endif

    if (readError) {
        DBG("[sfizz] reading the file errored for " << *id << " with code " << readError << ": " << readError.message());
        return;
    }

    FileDataHolder holder( data.data );

    FileData::Status currentStatus = data.data->status.load();

    unsigned spinCounter { 0 };
    while (currentStatus == FileData::Status::Invalid) {
        // Spin until the state changes
        if (spinCounter > 1024) {
            DBG("[sfizz] " << *id << " is stuck on Invalid? Leaving the load");
            return;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));
        currentStatus = data.data->status.load();
        spinCounter += 1;
    }

    // Already loading, loaded, or released
    if (currentStatus != FileData::Status::PendingStreaming)
        return;

    // Someone else got the token
    if (!data.data->status.compare_exchange_strong(currentStatus, FileData::Status::Streaming))
        return;

    bool completed = streamFromFile(*reader, data.data->fileData, data.data->availableFrames, synthConfig.freeWheeling);

    currentStatus = FileData::Status::Streaming;
    // Status might be changed to FullLoaded
    if (completed) {
        data.data->status.compare_exchange_strong(currentStatus, FileData::Status::Done);
    }
    else if (data.data->status.compare_exchange_strong(currentStatus, FileData::Status::PendingStreaming)) {
        if (filesToLoad->try_push(data)) {
            std::error_code ec;
            dispatchBarrier.post(ec);
            ASSERT(!ec);
        }
    }
}

void sfz::FilePool::clear()
{
    resetPreloadCallCounts();
    removeUnusedPreloadedData();
    ASSERT(preloadedFiles.size() == 0);
    ASSERT(loadedFiles.size() == 0);
    emptyFileLoadingQueues();
    std::error_code ec;
    ASSERT(!ec);
}

uint32_t sfz::FilePool::getPreloadSize() const noexcept
{
    return preloadSize;
}

template <typename R>
bool is_ready(std::future<R> const& f)
{
    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void sfz::FilePool::dispatchingJob() noexcept
{
    while (dispatchBarrier.wait(), dispatchFlag) {
        std::lock_guard<std::mutex> guard { loadingJobsMutex };

        QueuedFileData queuedData;
        if (filesToLoad->try_pop(queuedData)) {
            if (queuedData.id.expired() || queuedData.data->status != FileData::Status::PendingStreaming) {
                // file ID was nulled, it means the region was deleted, ignore
            }
            else
                loadingJobs.push_back(
                    globalObject->threadPool->enqueue([this](const QueuedFileData& data) { loadingJob(data); }, std::move(queuedData)));
        }

        // Clear finished jobs
        swapAndPopAll(loadingJobs, [](std::future<void>& future) {
            return is_ready(future);
        });
    }
}

void sfz::FilePool::waitForBackgroundLoading() noexcept
{
    std::lock_guard<std::mutex> guard { loadingJobsMutex };

    for (auto& job : loadingJobs)
        job.wait();

    loadingJobs.clear();
}

void sfz::FilePool::raiseCurrentThreadPriority() noexcept
{
#if defined(_WIN32)
    HANDLE thread = GetCurrentThread();
    const int priority = THREAD_PRIORITY_ABOVE_NORMAL; /*THREAD_PRIORITY_HIGHEST*/
    if (!SetThreadPriority(thread, priority)) {
        std::system_error error(GetLastError(), std::system_category());
        DBG("[sfizz] Cannot set current thread priority: " << error.what());
    }
#else
    pthread_t thread = pthread_self();
    int policy;
    sched_param param;

    if (pthread_getschedparam(thread, &policy, &param) != 0) {
        DBG("[sfizz] Cannot get current thread scheduling parameters");
        return;
    }

    policy = SCHED_RR;
    const int minprio = sched_get_priority_min(policy);
    const int maxprio = sched_get_priority_max(policy);
    param.sched_priority = minprio + config::backgroundLoaderPthreadPriority * (maxprio - minprio) / 100;

    if (pthread_setschedparam(thread, policy, &param) != 0) {
        DBG("[sfizz] Cannot set current thread scheduling parameters");
        return;
    }
#endif
}

void sfz::FilePool::setRamLoading(bool loadInRam) noexcept
{
    if (loadInRam == this->loadInRam)
        return;

    this->loadInRam = loadInRam;

    if (loadInRam) {
        for (auto& preloadedFile : preloadedFiles) {
            auto& fileId = preloadedFile.first;
            auto& fileData = preloadedFile.second;
            fs::path file { rootDirectory / fileId.filename() };
#if defined(SFIZZ_FILEOPENPREEXEC)
            AudioReaderPtr reader = createAudioReader(file, fileId.isReverse(), preexec);
#else
            AudioReaderPtr reader = createAudioReader(file, fileId.isReverse());
#endif
            fileData->preloadedData = readFromFile(
                *reader,
                fileData->information.end
            );
            // Initially set status
            fileData->status = FileData::Status::FullLoaded;
        }
    } else {
        setPreloadSize(preloadSize);
    }
}

void sfz::FilePool::startRender()
{
    globalObject->runningRender++;
}

void sfz::FilePool::stopRender()
{
    globalObject->runningRender--;
    std::error_code ec;
    globalObject->semGarbageBarrier.post(ec);
    ASSERT(!ec);
}

void sfz::FilePool::GlobalObject::garbageJob()
{
    while (semGarbageBarrier.timed_wait(sfz::config::fileClearingPeriod * 1000), garbageFlag) {
        if (runningRender != 0) {
            continue;
        }
        const auto now = highResNow();
        const auto timeSinceLastCollection =
        std::chrono::duration_cast<std::chrono::seconds>(now - lastGarbageCollection_);

        if (timeSinceLastCollection.count() < config::fileClearingPeriod) {
            continue;
        }
        lastGarbageCollection_ = now;

        {
            std::unique_lock<std::mutex> guard { preloadedFilesMutex, std::try_to_lock };
            if (guard.owns_lock()) {
                for (auto it = preloadedFiles.begin(); it != preloadedFiles.end();) {
                    auto copyIt = it++;
                    auto& data = copyIt->second;

                    if (data->canRemove()) {
                        preloadedFiles.erase(copyIt);
                        continue;
                    }

                    if (data->availableFrames == 0 || data->readerCount != 0)
                        continue;

                    FileData::Status status = data->status.load();
                    switch (status) {
                        case FileData::Status::Invalid:
                        case FileData::Status::Streaming:
                            continue;
                        default:
                            break;
                    }

                    const auto secondsIdle = std::chrono::duration_cast<std::chrono::seconds>(now - data->lastViewerLeftAt).count();
                    if (secondsIdle < config::fileClearingPeriod)
                        continue;

                    // construct/destruct outside the lock
                    FileAudioBuffer garbage;
                    // short time spin lock
                    std::unique_lock<SpinMutex> guard2 { data->garbageMutex, std::try_to_lock };
                    if (guard2.owns_lock() && data->readerCount == 0) {
                        data->availableFrames = 0;
                        if (status != FileData::Status::FullLoaded) {
                            data->status = FileData::Status::Preloaded;
                        }
                        garbage = std::move(data->fileData);
                    }
                }
            }
        }
        {
            std::unique_lock<std::mutex> guard { loadedFilesMutex, std::try_to_lock };
            if (guard.owns_lock()) {
                for (auto it = loadedFiles.begin(); it != loadedFiles.end();) {
                    auto copyIt = it++;
                    auto& data = copyIt->second;

                    if (data->canRemove()) {
                        loadedFiles.erase(copyIt);
                        continue;
                    }
                }
            }
        }
    }
}
