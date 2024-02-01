// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "sfizz/OnePoleFilter.h"
#include "sfizz/Smoothers.h"
#include "catch2/catch.hpp"
#include "TestHelpers.h"
// #include "cnpy.h"
// #include "sfizz/utility/ghc.hpp"
#include <absl/types/span.h>
#include <algorithm>
#include <array>
#include <string>
#include <iostream>
using namespace Catch::literals;

template <class Type, size_t N>
void testFilter(const std::array<Type, N>& input, const std::array<Type, N>& expectedLow, const std::array<Type, N>& expectedHigh, Type gain)
{
    std::array<Type, N> output { static_cast<Type>(0.0) };
    auto outputSpan = absl::MakeSpan(output);

    std::array<Type, N> gains;
    std::fill(gains.begin(), gains.end(), gain);

    sfz::OnePoleFilter<Type> filter;
    filter.setGain(gain);
    filter.processLowpass(input, outputSpan);
    REQUIRE(approxEqual(absl::MakeConstSpan(output), absl::MakeConstSpan(expectedLow)));

    filter.reset();
    filter.processLowpass(input, outputSpan, gains);
    REQUIRE(approxEqual(absl::MakeConstSpan(output), absl::MakeConstSpan(expectedLow)));

    filter.reset();
    filter.processHighpass(input, outputSpan);
    REQUIRE(approxEqual(absl::MakeConstSpan(output), absl::MakeConstSpan(expectedHigh)));

    filter.reset();
    filter.processHighpass(input, outputSpan, gains);
    REQUIRE(approxEqual(absl::MakeConstSpan(output), absl::MakeConstSpan(expectedHigh)));
}

constexpr std::array<float, 64> floatInput01 = {
    0.7224561488760388f, 0.7385973866948313f, -0.7270493193023231f, -0.10016187172526334f,
    -0.21705352538152722f, 0.2043840469350767f, -0.9596683661285715f, 0.8180755983644133f,
    -0.5353916316790325f, -0.3824795777486836f, -0.5199451873852872f, 1.224527476430308f,
    -1.5955302866080707f, -0.98620318862471f, 0.21447545419407035f, 1.9078154714253879f,
    -1.18546464770056f, 0.1160266352187059f, -0.14909079569914221f, 0.8926491360964995f,
    -0.25664421027272116f, -2.88400550880041f, 0.8120130589050852f, -1.2802705105092436f,
    1.1786547670902738f, 0.564152384756787f, 0.6572670188585557f, -1.2583862043651877f,
    0.06968219078056098f, -1.8460954875508593f, 0.31619623348534576f, -1.118168076837949f,
    -0.23268233682843759f, -0.05426088793091379f, -2.369490498577162f, 0.8741655425846802f,
    0.9695153245133031f, -0.9676818537948265f, 0.3584177148506203f, 0.4488503636037592f,
    -0.3389304100181121f, 1.2027174865060049f, 0.03546769154829243f, 0.09928053501681143f,
    -0.7585793622743948f, -0.5387748222254498f, -0.2199304726734363f, 0.63404938853515f,
    1.1666758495956464f, -0.04382336233428379f, 0.43819763320865857f, 0.1740610608056625f,
    0.30473671729007396f, -0.4065881153810866f, -0.9784770671900811f, -0.17674381857142665f,
    0.3493213284003123f, -1.2540491577273034f, 1.2597719140599792f, 0.4198847510851298f,
    0.9612865621570132f, -1.5614809857797225f, -0.31416474166626646f, -1.4741449502960542f
};

constexpr std::array<float, 64> floatOutputLow01 = {
    0.06567783171600353f, 0.18655945645590016f, 0.15368937959051f, 0.05054483866245487f,
    0.012517104623209388f, 0.00908949665113036f, -0.06122534993939287f, -0.06296553792897218f,
    -0.025818715879578978f, -0.10456724112217516f, -0.16759363047577702f, -0.07306912593063386f,
    -0.0935113585048607f, -0.3112123365251388f, -0.32478534210517174f, -0.07279792302973517f,
    0.0061063196779283235f, -0.09222555776186356f, -0.07846310730338257f, 0.0033991249697194748f,
    0.060599731868295786f, -0.23593201202349717f, -0.38139823255516353f, -0.3546219586000573f,
    -0.2993830337108623f, -0.08651274105006361f, 0.040254975833160934f, -0.02171130936438033f,
    -0.1258277998058227f, -0.2644421359111548f, -0.3554434979332642f, -0.3637239386138164f,
    -0.42039689647188494f, -0.370046844818756f, -0.5231066354433527f, -0.5639349704529687f,
    -0.2937939879071577f, -0.24021022004054027f, -0.2519232835735517f, -0.13273104306432598f,
    -0.09860540309029879f, -0.0021510501204360377f, 0.11080233881548844f, 0.10290629780949997f,
    0.024259804820719655f, -0.09809235828303345f, -0.14923059267692612f, -0.08445058347551104f,
    0.09460636244101792f, 0.17948270447550216f, 0.18270169192308128f, 0.20514308375655024f,
    0.21137141199133533f, 0.16368102816645502f, 0.008005824629720673f, -0.0984698603721838f,
    -0.06487738486552441f, -0.13532948119242824f, -0.11020387039992532f, 0.06252925741325283f,
    0.1767213299964926f, 0.0900270496677931f, -0.09685475276689556f, -0.24181840607858007f
};

constexpr std::array<float, 64> floatOutputHigh01 = {
    0.6567783171600353f, 0.5520379302389311f, -0.8807386988928331f, -0.1507067103877182f,
    -0.2295706300047366f, 0.19529455028394632f, -0.8984430161891787f, 0.8810411362933855f,
    -0.5095729157994535f, -0.2779123366265084f, -0.35235155690951014f, 1.297596602360942f,
    -1.50201892810321f, -0.6749908520995712f, 0.5392607962992421f, 1.980613394455123f,
    -1.1915709673784882f, 0.20825219298056946f, -0.07062768839575964f, 0.88925001112678f,
    -0.3172439421410169f, -2.6480734967769126f, 1.1934112914602488f, -0.9256485519091863f,
    1.4780378008011361f, 0.6506651258068505f, 0.6170120430253947f, -1.2366748950008073f,
    0.1955099905863837f, -1.5816533516397044f, 0.67163973141861f, -0.7544441382241325f,
    0.18771455964344735f, 0.3157859568878422f, -1.8463838631338094f, 1.4381005130376487f,
    1.2633093124204609f, -0.7274716337542863f, 0.610340998424172f, 0.5815814066680851f,
    -0.24032500692781328f, 1.204868536626441f, -0.07533464726719602f, -0.0036257627926885444f,
    -0.7828391670951145f, -0.4406824639424164f, -0.07069987999651017f, 0.7184999720106611f,
    1.0720694871546286f, -0.22330606680978596f, 0.2554959412855773f, -0.031082022950887744f,
    0.09336530529873863f, -0.5702691435475415f, -0.9864828918198018f, -0.07827395819924285f,
    0.41419871326583674f, -1.1187196765348753f, 1.3699757844599045f, 0.35735549367187697f,
    0.7845652321605207f, -1.6515080354475156f, -0.2173099888993709f, -1.2323265442174742f
};

constexpr std::array<float, 64> floatInput05 = {
    -0.8247415510202276f, -1.0299159073255513f, 0.7689727513393745f, -0.023063681797826918f,
    -0.1893245087721241f, -1.615552722124904f, -1.251848891438835f, 0.5338780197836666f,
    -0.3244188100039259f, 2.598277589396897f, -0.12170602745517456f, -2.7269013649087737f,
    -1.1332228949082876f, 0.5657123485919064f, 1.7914098463628945f, 0.7841799713943826f,
    0.22029184793596254f, 0.19814576077109303f, -0.0507307457285169f, 1.190488685111505f,
    -0.6761916505498549f, -1.083729826174603f, 0.405468008682514f, -1.2478635255587003f,
    -0.25157954751030825f, -0.9671361521687468f, -0.6434412998426552f, -0.9664977307097671f,
    -0.9150555987123582f, 1.697917162123366f, -1.3216510109214192f, -1.3943141278602609f,
    -0.7314910022591513f, -0.8889827595848262f, 1.3514782911515115f, 2.297097472618343f,
    -0.8897506799878153f, -0.706235549786705f, -0.25391776134956306f, -1.739982172732943f,
    -0.23465780260154823f, -0.0475767318206883f, -0.441164577073652f, -0.5072245472018251f,
    -1.1057148994224053f, 0.40324702616815694f, 0.815435779107782f, 0.25403283232711865f,
    -0.6137810912250902f, -0.7039958189789415f, -1.33840097232278f, 0.4786946763969468f,
    -0.46464793721558995f, -1.7121509287301122f, 0.7887828546774234f, -0.902172963851904f,
    -0.2591368523894675f, -0.9510361177022718f, 0.5739217219088085f, -0.25730420306720403f,
    0.41740839680521646f, -2.0979181310103074f, 1.1494564006889283f, 0.5059893282486726f
};

constexpr std::array<float, 64> floatOutputLow05 = {
    -0.27491385034007587f, -0.7098571028952849f, -0.32360008629382064f, 0.14076966108257558f,
    -0.023872843162458468f, -0.6095833580198289f, -1.1589949905278558f, -0.6256552873943414f,
    -0.1387320258715336f, 0.7117089178404791f, 1.0627601599274006f, -0.5952824108121825f,
    -1.4851355568764144f, -0.6842153677309318f, 0.557635609074623f, 1.0444084756106333f,
    0.6829600983136596f, 0.3671325690069051f, 0.1715158613498271f, 0.43709126691093836f,
    0.3171294338241961f, -0.48093068096675384f, -0.3863974994862809f, -0.4095976721208223f,
    -0.6363469150632769f, -0.6183542049141106f, -0.7429772189751709f, -0.7843054165091977f,
    -0.8886195819771077f, -0.03525267285536671f, 0.11367115944885997f, -0.8674313264442733f,
    -0.9977454855212284f, -0.8727397491217354f, -0.13674807251835008f, 1.170609230417168f,
    0.8593186743492318f, -0.24555585180842954f, -0.4019030543148992f, -0.7986009961324684f,
    -0.9244136571556533f, -0.40221606385929665f, -0.29698579091787897f, -0.4151249717311187f,
    -0.6760214727851164f, -0.4594964486797883f, 0.2530621188653835f, 0.44084357676676134f,
    0.02703177262292994f, -0.4302483791937005f, -0.8242150568318073f, -0.5613071175858801f,
    -0.18242012613484115f, -0.7864063306935144f, -0.5699248015820677f, -0.22777163691884944f,
    -0.46302715105340697f, -0.5577333737150487f, -0.311615923169504f, 0.001667198557366828f,
    0.05392379743179307f, -0.5421953122577658f, -0.49688568085971485f, 0.3861866826926287f
};

constexpr std::array<float, 64> floatOutputHigh05 = {
    -0.5498277006801517f, -0.32005880443026635f, 1.0925728376331951f, -0.1638333428804025f,
    -0.16545166560966562f, -1.005969364105075f, -0.09285390091097923f, 1.159533307178008f,
    -0.1856867841323923f, 1.886568671556418f, -1.1844661873825753f, -2.131618954096591f,
    0.35191266196812676f, 1.2499277163228384f, 1.2337742372882716f, -0.26022850421625077f,
    -0.462668250377697f, -0.16898680823581208f, -0.222246607078344f, 0.7533974182005667f,
    -0.993321084374051f, -0.602799145207849f, 0.7918655081687949f, -0.838265853437878f,
    0.38476736755296864f, -0.3487819472546362f, 0.0995359191325157f, -0.18219231420056936f,
    -0.026436016735250534f, 1.7331698349787326f, -1.4353221703702792f, -0.5268828014159875f,
    0.2662544832620771f, -0.016243010463090846f, 1.4882263636698616f, 1.126488242201175f,
    -1.7490693543370472f, -0.46067969797827546f, 0.14798529296533613f, -0.9413811766004746f,
    0.689755854554105f, 0.35463933203860837f, -0.144178786155773f, -0.09209957547070641f,
    -0.42969342663728893f, 0.8627434748479452f, 0.5623736602423985f, -0.18681074443964268f,
    -0.6408128638480202f, -0.27374743978524096f, -0.5141859154909728f, 1.040001793982827f,
    -0.2822278110807488f, -0.9257445980365978f, 1.358707656259491f, -0.6744013269330547f,
    0.20389029866393948f, -0.3933027439872231f, 0.8855376450783126f, -0.2589714016245709f,
    0.36348459937342337f, -1.5557228187525416f, 1.6463420815486431f, 0.11980264555604392f
};

constexpr std::array<float, 64> floatInput09 = {
    -0.9629663717342508f, 1.054078826032172f, -1.0644939081323097f, -0.05328934531304567f,
    -0.04857086206002074f, 1.612607856597715f, 1.0513263960877668f, -1.4323863476593215f,
    2.2461810968138463f, -0.6561891523704232f, 0.022772627664592485f, 0.07616465991959669f,
    0.8305193318990887f, -0.4888237081549593f, 0.8564039983858606f, 1.4871994957279644f,
    0.22673465240234947f, 1.658079098180724f, -1.7453062858413877f, -0.11612580324446467f,
    -0.20232260689840872f, -1.1476998404072543f, -0.6202811352543974f, 1.545975326252028f,
    1.0442436933320733f, -1.0968040236666232f, 0.7595527972844077f, 1.2073698123442007f,
    -0.8873573213734941f, -0.17122644896880435f, -1.7574830431070918f, 0.19907680046299245f,
    1.27872961557419f, -0.7422656051046687f, -0.6846620117838057f, -0.13384854423135875f,
    0.9007202159691193f, 1.1254806648626967f, -0.04344693397840567f, 0.7948730146831712f,
    1.1781603468141004f, 0.1875496039927383f, 1.692965002836772f, -0.04201566153548597f,
    1.1210100661199038f, -0.7501096833348359f, 0.020210228191464837f, 1.9979804313376157f,
    0.7517403248613556f, 1.1194691465807607f, -1.1160170942539855f, -1.0010374555669668f,
    2.1609909686692763f, -0.07213993925443297f, -0.5083174992310037f, -0.7489925703250175f,
    0.5119124853257149f, -0.33950253799120345f, -0.26764774112191847f, 0.10271208568438035f,
    -0.09893862035889031f, -0.4154625911342657f, 0.11272544601558693f, -0.6895075000870634f
};

constexpr std::array<float, 64> floatOutputLow09 = {
    -0.45614196555832937f, 0.01915105911173487f, -0.003925509462605503f, -0.5296828837089897f,
    -0.0761276184245572f, 0.7368529122323524f, 1.3006453256000892f, -0.1120470651865213f,
    0.37958450932653687f, 0.7731322110167023f, -0.25934823743872504f, 0.033215123727314666f,
    0.4312300552681833f, 0.1845521404718603f, 0.18383025013420895f, 1.1198032472188755f,
    0.8708005568627211f, 0.9386381216900201f, 0.008083864881265557f, -0.8813055229942847f,
    -0.19722848496211295f, -0.6498647637217411f, -0.8716680812987687f, 0.39260945461473207f,
    1.2476095068879813f, 0.040766659677738515f, -0.1576049672506421f, 0.9234051852319387f,
    0.20018513705096297f, -0.49089835768577506f, -0.9394359887562548f, -0.7876364301343762f,
    0.6585590165368562f, 0.2887755321453973f, -0.6607143694658354f, -0.4224899670317008f,
    0.34101868834779714f, 0.9777277166228496f, 0.5640016470832352f, 0.38562296702242765f,
    0.9548906958156775f, 0.6971726448988013f, 0.9274633740191786f, 0.8308424971437238f,
    0.5548311651791308f, 0.204891295276039f, -0.3349580947902264f, 0.9383556758406053f,
    1.3518864464016498f, 0.9575142994410892f, 0.0520306721253716f, -1.0000768566454319f,
    0.496816040067124f, 1.015603963410564f, -0.22150068331359823f, -0.6072258583851468f,
    -0.14426034859888792f, 0.07407521986377441f, -0.2836988048502276f, -0.09305893177831953f,
    -0.003110407570995219f, -0.24382743742154736f, -0.15623482860471877f, -0.28143543764463197f
};

constexpr std::array<float, 64> floatOutputHigh09 = {
    -0.5068244061759215f, 1.034927766920437f, -1.0605683986697043f, 0.47639353839594406f,
    0.027556756364536465f, 0.8757549443653627f, -0.24931892951232237f, -1.3203392824728002f,
    1.8665965874873094f, -1.4293213633871256f, 0.2821208651033175f, 0.04294953619228202f,
    0.39928927663090535f, -0.6733758486268195f, 0.6725737482516516f, 0.3673962485090889f,
    -0.6440659044603716f, 0.7194409764907039f, -1.7533901507226533f, 0.76517971974982f,
    -0.00509412193629577f, -0.4978350766855132f, 0.2513869460443713f, 1.1533658716372959f,
    -0.203365813555908f, -1.1375706833443617f, 0.9171577645350498f, 0.283964627112262f,
    -1.087542458424457f, 0.3196719087169707f, -0.818047054350837f, 0.9867132305973687f,
    0.6201705990373338f, -1.031041137250066f, -0.023947642317970308f, 0.288641422800342f,
    0.5597015276213222f, 0.14775294823984708f, -0.6074485810616409f, 0.4092500476607435f,
    0.22326965099842289f, -0.5096230409060629f, 0.7655016288175934f, -0.8728581586792098f,
    0.5661789009407731f, -0.9550009786108749f, 0.35516832298169126f, 1.0596247554970104f,
    -0.6001461215402942f, 0.16195484713967145f, -1.1680477663793571f, -0.0009605989215348831f,
    1.6641749286021523f, -1.087743902664997f, -0.28681681591740543f, -0.14176671193987078f,
    0.6561728339246028f, -0.41357775785497786f, 0.016051063728309112f, 0.19577101746269987f,
    -0.09582821278789509f, -0.17163515371271834f, 0.2689602746203057f, -0.40807206244243144f
};

constexpr std::array<double, 64> doubleInput01 = {
    0.7224561488760388, 0.7385973866948313, -0.7270493193023231, -0.10016187172526334,
    -0.21705352538152722, 0.2043840469350767, -0.9596683661285715, 0.8180755983644133,
    -0.5353916316790325, -0.3824795777486836, -0.5199451873852872, 1.224527476430308,
    -1.5955302866080707, -0.98620318862471, 0.21447545419407035, 1.9078154714253879,
    -1.18546464770056, 0.1160266352187059, -0.14909079569914221, 0.8926491360964995,
    -0.25664421027272116, -2.88400550880041, 0.8120130589050852, -1.2802705105092436,
    1.1786547670902738, 0.564152384756787, 0.6572670188585557, -1.2583862043651877,
    0.06968219078056098, -1.8460954875508593, 0.31619623348534576, -1.118168076837949,
    -0.23268233682843759, -0.05426088793091379, -2.369490498577162, 0.8741655425846802,
    0.9695153245133031, -0.9676818537948265, 0.3584177148506203, 0.4488503636037592,
    -0.3389304100181121, 1.2027174865060049, 0.03546769154829243, 0.09928053501681143,
    -0.7585793622743948, -0.5387748222254498, -0.2199304726734363, 0.63404938853515,
    1.1666758495956464, -0.04382336233428379, 0.43819763320865857, 0.1740610608056625,
    0.30473671729007396, -0.4065881153810866, -0.9784770671900811, -0.17674381857142665,
    0.3493213284003123, -1.2540491577273034, 1.2597719140599792, 0.4198847510851298,
    0.9612865621570132, -1.5614809857797225, -0.31416474166626646, -1.4741449502960542
};

constexpr std::array<double, 64> doubleOutputLow01 = {
    0.06567783171600353, 0.18655945645590016, 0.15368937959051, 0.05054483866245487,
    0.012517104623209388, 0.00908949665113036, -0.06122534993939287, -0.06296553792897218,
    -0.025818715879578978, -0.10456724112217516, -0.16759363047577702, -0.07306912593063386,
    -0.0935113585048607, -0.3112123365251388, -0.32478534210517174, -0.07279792302973517,
    0.0061063196779283235, -0.09222555776186356, -0.07846310730338257, 0.0033991249697194748,
    0.060599731868295786, -0.23593201202349717, -0.38139823255516353, -0.3546219586000573,
    -0.2993830337108623, -0.08651274105006361, 0.040254975833160934, -0.02171130936438033,
    -0.1258277998058227, -0.2644421359111548, -0.3554434979332642, -0.3637239386138164,
    -0.42039689647188494, -0.370046844818756, -0.5231066354433527, -0.5639349704529687,
    -0.2937939879071577, -0.24021022004054027, -0.2519232835735517, -0.13273104306432598,
    -0.09860540309029879, -0.0021510501204360377, 0.11080233881548844, 0.10290629780949997,
    0.024259804820719655, -0.09809235828303345, -0.14923059267692612, -0.08445058347551104,
    0.09460636244101792, 0.17948270447550216, 0.18270169192308128, 0.20514308375655024,
    0.21137141199133533, 0.16368102816645502, 0.008005824629720673, -0.0984698603721838,
    -0.06487738486552441, -0.13532948119242824, -0.11020387039992532, 0.06252925741325283,
    0.1767213299964926, 0.0900270496677931, -0.09685475276689556, -0.24181840607858007
};

constexpr std::array<double, 64> doubleOutputHigh01 = {
    0.6567783171600353, 0.5520379302389311, -0.8807386988928331, -0.1507067103877182,
    -0.2295706300047366, 0.19529455028394632, -0.8984430161891787, 0.8810411362933855,
    -0.5095729157994535, -0.2779123366265084, -0.35235155690951014, 1.297596602360942,
    -1.50201892810321, -0.6749908520995712, 0.5392607962992421, 1.980613394455123,
    -1.1915709673784882, 0.20825219298056946, -0.07062768839575964, 0.88925001112678,
    -0.3172439421410169, -2.6480734967769126, 1.1934112914602488, -0.9256485519091863,
    1.4780378008011361, 0.6506651258068505, 0.6170120430253947, -1.2366748950008073,
    0.1955099905863837, -1.5816533516397044, 0.67163973141861, -0.7544441382241325,
    0.18771455964344735, 0.3157859568878422, -1.8463838631338094, 1.4381005130376487,
    1.2633093124204609, -0.7274716337542863, 0.610340998424172, 0.5815814066680851,
    -0.24032500692781328, 1.204868536626441, -0.07533464726719602, -0.0036257627926885444,
    -0.7828391670951145, -0.4406824639424164, -0.07069987999651017, 0.7184999720106611,
    1.0720694871546286, -0.22330606680978596, 0.2554959412855773, -0.031082022950887744,
    0.09336530529873863, -0.5702691435475415, -0.9864828918198018, -0.07827395819924285,
    0.41419871326583674, -1.1187196765348753, 1.3699757844599045, 0.35735549367187697,
    0.7845652321605207, -1.6515080354475156, -0.2173099888993709, -1.2323265442174742
};

constexpr std::array<double, 64> doubleInput05 = {
    -0.8247415510202276, -1.0299159073255513, 0.7689727513393745, -0.023063681797826918,
    -0.1893245087721241, -1.615552722124904, -1.251848891438835, 0.5338780197836666,
    -0.3244188100039259, 2.598277589396897, -0.12170602745517456, -2.7269013649087737,
    -1.1332228949082876, 0.5657123485919064, 1.7914098463628945, 0.7841799713943826,
    0.22029184793596254, 0.19814576077109303, -0.0507307457285169, 1.190488685111505,
    -0.6761916505498549, -1.083729826174603, 0.405468008682514, -1.2478635255587003,
    -0.25157954751030825, -0.9671361521687468, -0.6434412998426552, -0.9664977307097671,
    -0.9150555987123582, 1.697917162123366, -1.3216510109214192, -1.3943141278602609,
    -0.7314910022591513, -0.8889827595848262, 1.3514782911515115, 2.297097472618343,
    -0.8897506799878153, -0.706235549786705, -0.25391776134956306, -1.739982172732943,
    -0.23465780260154823, -0.0475767318206883, -0.441164577073652, -0.5072245472018251,
    -1.1057148994224053, 0.40324702616815694, 0.815435779107782, 0.25403283232711865,
    -0.6137810912250902, -0.7039958189789415, -1.33840097232278, 0.4786946763969468,
    -0.46464793721558995, -1.7121509287301122, 0.7887828546774234, -0.902172963851904,
    -0.2591368523894675, -0.9510361177022718, 0.5739217219088085, -0.25730420306720403,
    0.41740839680521646, -2.0979181310103074, 1.1494564006889283, 0.5059893282486726
};

constexpr std::array<double, 64> doubleOutputLow05 = {
    -0.27491385034007587, -0.7098571028952849, -0.32360008629382064, 0.14076966108257558,
    -0.023872843162458468, -0.6095833580198289, -1.1589949905278558, -0.6256552873943414,
    -0.1387320258715336, 0.7117089178404791, 1.0627601599274006, -0.5952824108121825,
    -1.4851355568764144, -0.6842153677309318, 0.557635609074623, 1.0444084756106333,
    0.6829600983136596, 0.3671325690069051, 0.1715158613498271, 0.43709126691093836,
    0.3171294338241961, -0.48093068096675384, -0.3863974994862809, -0.4095976721208223,
    -0.6363469150632769, -0.6183542049141106, -0.7429772189751709, -0.7843054165091977,
    -0.8886195819771077, -0.03525267285536671, 0.11367115944885997, -0.8674313264442733,
    -0.9977454855212284, -0.8727397491217354, -0.13674807251835008, 1.170609230417168,
    0.8593186743492318, -0.24555585180842954, -0.4019030543148992, -0.7986009961324684,
    -0.9244136571556533, -0.40221606385929665, -0.29698579091787897, -0.4151249717311187,
    -0.6760214727851164, -0.4594964486797883, 0.2530621188653835, 0.44084357676676134,
    0.02703177262292994, -0.4302483791937005, -0.8242150568318073, -0.5613071175858801,
    -0.18242012613484115, -0.7864063306935144, -0.5699248015820677, -0.22777163691884944,
    -0.46302715105340697, -0.5577333737150487, -0.311615923169504, 0.001667198557366828,
    0.05392379743179307, -0.5421953122577658, -0.49688568085971485, 0.3861866826926287
};

constexpr std::array<double, 64> doubleOutputHigh05 = {
    -0.5498277006801517, -0.32005880443026635, 1.0925728376331951, -0.1638333428804025,
    -0.16545166560966562, -1.005969364105075, -0.09285390091097923, 1.159533307178008,
    -0.1856867841323923, 1.886568671556418, -1.1844661873825753, -2.131618954096591,
    0.35191266196812676, 1.2499277163228384, 1.2337742372882716, -0.26022850421625077,
    -0.462668250377697, -0.16898680823581208, -0.222246607078344, 0.7533974182005667,
    -0.993321084374051, -0.602799145207849, 0.7918655081687949, -0.838265853437878,
    0.38476736755296864, -0.3487819472546362, 0.0995359191325157, -0.18219231420056936,
    -0.026436016735250534, 1.7331698349787326, -1.4353221703702792, -0.5268828014159875,
    0.2662544832620771, -0.016243010463090846, 1.4882263636698616, 1.126488242201175,
    -1.7490693543370472, -0.46067969797827546, 0.14798529296533613, -0.9413811766004746,
    0.689755854554105, 0.35463933203860837, -0.144178786155773, -0.09209957547070641,
    -0.42969342663728893, 0.8627434748479452, 0.5623736602423985, -0.18681074443964268,
    -0.6408128638480202, -0.27374743978524096, -0.5141859154909728, 1.040001793982827,
    -0.2822278110807488, -0.9257445980365978, 1.358707656259491, -0.6744013269330547,
    0.20389029866393948, -0.3933027439872231, 0.8855376450783126, -0.2589714016245709,
    0.36348459937342337, -1.5557228187525416, 1.6463420815486431, 0.11980264555604392
};

constexpr std::array<double, 64> doubleInput09 = {
    -0.9629663717342508, 1.054078826032172, -1.0644939081323097, -0.05328934531304567,
    -0.04857086206002074, 1.612607856597715, 1.0513263960877668, -1.4323863476593215,
    2.2461810968138463, -0.6561891523704232, 0.022772627664592485, 0.07616465991959669,
    0.8305193318990887, -0.4888237081549593, 0.8564039983858606, 1.4871994957279644,
    0.22673465240234947, 1.658079098180724, -1.7453062858413877, -0.11612580324446467,
    -0.20232260689840872, -1.1476998404072543, -0.6202811352543974, 1.545975326252028,
    1.0442436933320733, -1.0968040236666232, 0.7595527972844077, 1.2073698123442007,
    -0.8873573213734941, -0.17122644896880435, -1.7574830431070918, 0.19907680046299245,
    1.27872961557419, -0.7422656051046687, -0.6846620117838057, -0.13384854423135875,
    0.9007202159691193, 1.1254806648626967, -0.04344693397840567, 0.7948730146831712,
    1.1781603468141004, 0.1875496039927383, 1.692965002836772, -0.04201566153548597,
    1.1210100661199038, -0.7501096833348359, 0.020210228191464837, 1.9979804313376157,
    0.7517403248613556, 1.1194691465807607, -1.1160170942539855, -1.0010374555669668,
    2.1609909686692763, -0.07213993925443297, -0.5083174992310037, -0.7489925703250175,
    0.5119124853257149, -0.33950253799120345, -0.26764774112191847, 0.10271208568438035,
    -0.09893862035889031, -0.4154625911342657, 0.11272544601558693, -0.6895075000870634
};

constexpr std::array<double, 64> doubleOutputLow09 = {
    -0.45614196555832937, 0.01915105911173487, -0.003925509462605503, -0.5296828837089897,
    -0.0761276184245572, 0.7368529122323524, 1.3006453256000892, -0.1120470651865213,
    0.37958450932653687, 0.7731322110167023, -0.25934823743872504, 0.033215123727314666,
    0.4312300552681833, 0.1845521404718603, 0.18383025013420895, 1.1198032472188755,
    0.8708005568627211, 0.9386381216900201, 0.008083864881265557, -0.8813055229942847,
    -0.19722848496211295, -0.6498647637217411, -0.8716680812987687, 0.39260945461473207,
    1.2476095068879813, 0.040766659677738515, -0.1576049672506421, 0.9234051852319387,
    0.20018513705096297, -0.49089835768577506, -0.9394359887562548, -0.7876364301343762,
    0.6585590165368562, 0.2887755321453973, -0.6607143694658354, -0.4224899670317008,
    0.34101868834779714, 0.9777277166228496, 0.5640016470832352, 0.38562296702242765,
    0.9548906958156775, 0.6971726448988013, 0.9274633740191786, 0.8308424971437238,
    0.5548311651791308, 0.204891295276039, -0.3349580947902264, 0.9383556758406053,
    1.3518864464016498, 0.9575142994410892, 0.0520306721253716, -1.0000768566454319,
    0.496816040067124, 1.015603963410564, -0.22150068331359823, -0.6072258583851468,
    -0.14426034859888792, 0.07407521986377441, -0.2836988048502276, -0.09305893177831953,
    -0.003110407570995219, -0.24382743742154736, -0.15623482860471877, -0.28143543764463197
};

constexpr std::array<double, 64> doubleOutputHigh09 = {
    -0.5068244061759215, 1.034927766920437, -1.0605683986697043, 0.47639353839594406,
    0.027556756364536465, 0.8757549443653627, -0.24931892951232237, -1.3203392824728002,
    1.8665965874873094, -1.4293213633871256, 0.2821208651033175, 0.04294953619228202,
    0.39928927663090535, -0.6733758486268195, 0.6725737482516516, 0.3673962485090889,
    -0.6440659044603716, 0.7194409764907039, -1.7533901507226533, 0.76517971974982,
    -0.00509412193629577, -0.4978350766855132, 0.2513869460443713, 1.1533658716372959,
    -0.203365813555908, -1.1375706833443617, 0.9171577645350498, 0.283964627112262,
    -1.087542458424457, 0.3196719087169707, -0.818047054350837, 0.9867132305973687,
    0.6201705990373338, -1.031041137250066, -0.023947642317970308, 0.288641422800342,
    0.5597015276213222, 0.14775294823984708, -0.6074485810616409, 0.4092500476607435,
    0.22326965099842289, -0.5096230409060629, 0.7655016288175934, -0.8728581586792098,
    0.5661789009407731, -0.9550009786108749, 0.35516832298169126, 1.0596247554970104,
    -0.6001461215402942, 0.16195484713967145, -1.1680477663793571, -0.0009605989215348831,
    1.6641749286021523, -1.087743902664997, -0.28681681591740543, -0.14176671193987078,
    0.6561728339246028, -0.41357775785497786, 0.016051063728309112, 0.19577101746269987,
    -0.09582821278789509, -0.17163515371271834, 0.2689602746203057, -0.40807206244243144
};

TEST_CASE("[OnePoleFilter] Tests")
{
    testFilter(floatInput01, floatOutputLow01, floatOutputHigh01, 0.1f);
    testFilter(floatInput05, floatOutputLow05, floatOutputHigh05, 0.5f);
    testFilter(floatInput09, floatOutputLow09, floatOutputHigh09, 0.9f);
    testFilter(doubleInput01, doubleOutputLow01, doubleOutputHigh01, 0.1);
    testFilter(doubleInput05, doubleOutputLow05, doubleOutputHigh05, 0.5);
    testFilter(doubleInput09, doubleOutputLow09, doubleOutputHigh09, 0.9);
}
