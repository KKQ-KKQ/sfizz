// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "AudioSpan.h"
#include "absl/types/span.h"
#include "sfizz/Synth.h"
#include "sfizz/FilePool.h"
#include "TestHelpers.h"
#include "catch2/catch.hpp"
#include <algorithm>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <functional>
#include <atomic>
using namespace Catch::literals;

static void WAIT(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

TEST_CASE("[FilePool] Shared samples")
{
    std::unique_ptr<sfz::Synth> synth1 { new sfz::Synth() };
    std::unique_ptr<sfz::Synth> synth2 { new sfz::Synth() };
    std::unique_ptr<sfz::Synth> synth3 { new sfz::Synth() };

    synth1->setSamplesPerBlock(256);
    synth2->setSamplesPerBlock(256);
    synth3->setSamplesPerBlock(256);

    synth1->loadSfzFile(fs::current_path() / "tests/TestFiles/looped_regions.sfz");

    CHECK(synth1->getNumPreloadedSamples() == 1);
    CHECK(synth2->getNumPreloadedSamples() == 0);
    CHECK(synth3->getNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth3->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);

    synth2->loadSfzFile(fs::current_path() / "tests/TestFiles/looped_regions.sfz");

    CHECK(synth1->getNumPreloadedSamples() == 1);
    CHECK(synth2->getNumPreloadedSamples() == 1);
    CHECK(synth3->getNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);

    sfz::AudioBuffer<float> buffer { 2, 256 };

    synth2->loadSfzFile("");

    CHECK(synth1->getNumPreloadedSamples() == 1);
    CHECK(synth2->getNumPreloadedSamples() == 0);
    CHECK(synth3->getNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth3->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);

    synth2->loadSfzFile(fs::current_path() / "tests/TestFiles/looped_regions.sfz");

    CHECK(synth1->getNumPreloadedSamples() == 1);
    CHECK(synth2->getNumPreloadedSamples() == 1);
    CHECK(synth3->getNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);

// Crash test
    synth1->noteOn(0, 60, 100);
    for (unsigned i = 0; i < 100; ++i) {
        synth1->renderBlock(buffer);
        synth2->renderBlock(buffer);
        WAIT(10);
    }

    synth2->noteOn(0, 60, 100);
    for (unsigned i = 0; i < 100; ++i) {
        synth1->renderBlock(buffer);
        synth2->renderBlock(buffer);
        WAIT(10);
    }

    synth1->noteOff(0, 60, 100);
    for (unsigned i = 0; i < 100; ++i) {
        synth1->renderBlock(buffer);
        synth2->renderBlock(buffer);
        WAIT(10);
    }

    synth1->loadSfzFile("");
    synth1->allSoundOff();
    for (unsigned i = 0; i < 100; ++i) {
        synth1->renderBlock(buffer);
        synth2->renderBlock(buffer);
        WAIT(10);
    }

    CHECK(synth1->getNumPreloadedSamples() == 0);
    CHECK(synth2->getNumPreloadedSamples() == 1);
    CHECK(synth3->getNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth2->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);

    synth2.reset();

    CHECK(synth1->getNumPreloadedSamples() == 0);
    CHECK(synth3->getNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth3->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getGlobalNumPreloadedSamples() == 0);
    CHECK(synth3->getResources().getFilePool().getGlobalNumPreloadedSamples() == 0);

    synth2.reset(new sfz::Synth());

    synth2->setSamplesPerBlock(256);

    synth1->loadSfzFile(fs::current_path() / "tests/TestFiles/looped_regions.sfz");
    synth2->loadSfzFile(fs::current_path() / "tests/TestFiles/kick_embedded.sfz");

    CHECK(synth1->getNumPreloadedSamples() == 1);
    CHECK(synth2->getNumPreloadedSamples() == 1);
    CHECK(synth3->getNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth2->getResources().getFilePool().getActualNumPreloadedSamples() == 1);
    CHECK(synth3->getResources().getFilePool().getActualNumPreloadedSamples() == 0);
    CHECK(synth1->getResources().getFilePool().getGlobalNumPreloadedSamples() == 2);
    CHECK(synth2->getResources().getFilePool().getGlobalNumPreloadedSamples() == 2);
    CHECK(synth3->getResources().getFilePool().getGlobalNumPreloadedSamples() == 2);

    // Release
    synth1.reset();
    synth2.reset();
    synth3.reset();

    const unsigned synthCount = 100;
    
    struct TestSynthThread {
        TestSynthThread()
        {
            synth.setSamplesPerBlock(256);
            synth.loadSfzFile(fs::current_path() / "tests/TestFiles/looped_regions.sfz");
        }

        ~TestSynthThread()
        {
            running = false;
            std::error_code ec;
            semBarrier.post(ec);
            ASSERT(!ec);
            thread.join();
        }

        void job() noexcept
        {
            while (semBarrier.wait(), running) {
                synth.renderBlock(buffer);
                execution();
            }
        }

        void noteOn()
        {
            synth.noteOn(0, 60, 100);
        }

        void noteOff()
        {
            synth.noteOff(0, 60, 100);
        }

        void trigger()
        {
            std::error_code ec;
            semBarrier.post(ec);
            ASSERT(!ec);
        }

        sfz::AudioBuffer<float> buffer { 2, 256 };
        sfz::Synth synth;
        std::thread thread { &TestSynthThread::job, this };
        RTSemaphore semBarrier { 0 };
        bool running { true };
        std::function<void()> execution;
    };

    std::unique_ptr<TestSynthThread[]> synthThreads { new TestSynthThread[synthCount]() };

    std::atomic<int> finishCount { 0 };
    auto countFunc = [&finishCount]() {
        ++finishCount;
    };
    for (unsigned i = 0; i < synthCount; ++i) {
        synthThreads[i].execution = countFunc;
    }

    for (unsigned i = 0; i < synthCount; ++i) {
        CHECK(synthThreads[i].synth.getNumPreloadedSamples() == 1);
        CHECK(synthThreads[i].synth.getResources().getFilePool().getActualNumPreloadedSamples() == 1);
        CHECK(synthThreads[i].synth.getResources().getFilePool().getGlobalNumPreloadedSamples() == 1);
    }
    
    for (unsigned i = 0; i < synthCount; ++i) {
        synthThreads[i].noteOn();
    }
    for (unsigned i = 0; i < 100; ++i) {
        finishCount = 0;
        for (unsigned j = 0; j < synthCount; ++j) {
            synthThreads[j].trigger();
        }
        while (finishCount != synthCount) {
            WAIT(10);
        }
    }

    for (unsigned i = 0; i < synthCount; ++i) {
        synthThreads[i].noteOff();
    }
    for (unsigned i = 0; i < 100; ++i) {
        finishCount = 0;
        for (unsigned j = 0; j < synthCount; ++j) {
            synthThreads[j].trigger();
        }
        while (finishCount != synthCount) {
            WAIT(10);
        }
    }

    synthThreads.reset();
}