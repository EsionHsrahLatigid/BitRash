#include "TestSupport.h"
#include "dsp/BitRashDSP.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <vector>

namespace
{
using bitrash::dsp::BitRashDSP;

bool near(float a, float b, float tolerance)
{
    return std::abs(a - b) <= tolerance;
}

BitRashDSP makeDsp(const BitRashDSP::Parameters& parameters, int channels = 1)
{
    BitRashDSP dsp;
    dsp.prepare(48000.0, 512, channels);
    dsp.setTargets(parameters);
    dsp.reset();
    return dsp;
}

std::vector<float> render(BitRashDSP& dsp, int count, float frequency = 0.021f)
{
    std::vector<float> output;
    output.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        output.push_back(dsp.processSampleForTest(std::sin(static_cast<float>(i) * frequency) * 0.8f, 0));
    return output;
}

float adjacentDifferenceEnergy(const std::vector<float>& values)
{
    float energy = 0.0f;
    for (std::size_t i = 1; i < values.size(); ++i)
    {
        const float delta = values[i] - values[i - 1];
        energy += delta * delta;
    }
    return energy;
}

std::uint32_t hashSequence(const std::vector<float>& values)
{
    std::uint32_t hash = 2166136261u;
    for (float value : values)
    {
        const auto scaled = static_cast<std::int32_t>(std::round(value * 1000000.0f));
        hash ^= static_cast<std::uint32_t>(scaled);
        hash *= 16777619u;
    }
    return hash;
}
} // namespace

int main()
{
    return test_support::run("bitrash_dsp_tests", [] {
        test_support::check(BitRashDSP::levelCountForBits(1) == 2, "1-bit level count");
        test_support::check(BitRashDSP::levelCountForBits(8) == 256, "8-bit level count");
        test_support::check(near(BitRashDSP::stepForBits(4), 2.0f / 15.0f, 0.000001f), "4-bit step tolerance");

        std::set<int> codes;
        const float step4 = BitRashDSP::stepForBits(4);
        for (int i = 0; i < 4096; ++i)
        {
            const float x = -0.999f + static_cast<float>(i) * 1.998f / 4095.0f;
            const float y = BitRashDSP::quantize(x, 4, BitRashDSP::ClipMode::clamp);
            codes.insert(static_cast<int>(std::round((y + 1.0f) / step4)));
            test_support::check(std::abs((y + 1.0f) - std::round((y + 1.0f) / step4) * step4) < 0.00001f, "quantized value lands on 4-bit grid");
        }
        test_support::check(static_cast<int>(codes.size()) == BitRashDSP::levelCountForBits(4), "4-bit ramp reaches exact level count");
        test_support::check(BitRashDSP::quantize(0.0f, 1, BitRashDSP::ClipMode::clamp) == 0.0f, "1-bit zero silence guard");
        test_support::check(BitRashDSP::quantize(1.7f, 4, BitRashDSP::ClipMode::fold) < 1.0f, "fold mode folds out-of-range input");
        test_support::check(BitRashDSP::quantize(1.7f, 4, BitRashDSP::ClipMode::wrap) < 0.0f, "wrap mode wraps out-of-range input");

        BitRashDSP::Parameters holdParams;
        holdParams.bits = 8.0f;
        holdParams.divisor = 5.0f;
        holdParams.postFilter = 0.0f;
        holdParams.mix = 1.0f;
        holdParams.trimDb = 0.0f;
        auto hold = makeDsp(holdParams);
        std::vector<float> held;
        for (int i = 0; i < 20; ++i)
            held.push_back(hold.processSampleForTest(static_cast<float>(i) / 20.0f, 0));
        for (int start = 0; start < 20; start += 5)
            for (int i = start + 1; i < start + 5; ++i)
                test_support::check(held[static_cast<std::size_t>(i)] == held[static_cast<std::size_t>(start)], "sample-hold run length equals divisor");

        BitRashDSP::Parameters ditherParams;
        ditherParams.bits = 8.0f;
        ditherParams.divisor = 1.0f;
        ditherParams.dither = 1.0f;
        ditherParams.postFilter = 0.0f;
        ditherParams.seed = 0.421f;
        ditherParams.trimDb = 0.0f;
        auto ditherA = makeDsp(ditherParams);
        auto ditherB = makeDsp(ditherParams);
        std::vector<float> noiseA;
        std::vector<float> noiseB;
        float sum = 0.0f;
        float sumSquares = 0.0f;
        for (int i = 0; i < 4096; ++i)
        {
            const float a = ditherA.processSampleForTest(0.0f, 0);
            const float b = ditherB.processSampleForTest(0.0f, 0);
            noiseA.push_back(a);
            noiseB.push_back(b);
            sum += a;
            sumSquares += a * a;
        }
        test_support::check(hashSequence(noiseA) == hashSequence(noiseB), "TPDF dither is deterministic after seed/reset");
        test_support::check(std::abs(sum / 4096.0f) < 0.01f, "zero-input dither mean bounded");
        test_support::check(sumSquares / 4096.0f > 0.000001f && sumSquares / 4096.0f < 0.00008f, "zero-input dither variance bounded");

        BitRashDSP::Parameters plainError;
        plainError.bits = 5.0f;
        plainError.divisor = 1.0f;
        plainError.postFilter = 0.0f;
        plainError.trimDb = 0.0f;
        auto plain = makeDsp(plainError);
        auto shapedParams = plainError;
        shapedParams.errorFeedback = 0.85f;
        shapedParams.noiseShape = 0.85f;
        auto shaped = makeDsp(shapedParams);
        const auto plainOut = render(plain, 2048, 0.017f);
        const auto shapedOut = render(shaped, 2048, 0.017f);
        test_support::check(adjacentDifferenceEnergy(shapedOut) > adjacentDifferenceEnergy(plainOut) * 1.05f, "noise-shaping proxy moves error toward faster changes");

        BitRashDSP::Parameters jitterParams = holdParams;
        jitterParams.divisor = 9.0f;
        jitterParams.jitter = 1.0f;
        jitterParams.seed = 0.727f;
        auto jitterA = makeDsp(jitterParams);
        auto jitterB = makeDsp(jitterParams);
        auto noJitterParams = jitterParams;
        noJitterParams.jitter = 0.0f;
        auto noJitter = makeDsp(noJitterParams);
        const auto jitterOutA = render(jitterA, 256, 0.09f);
        const auto jitterOutB = render(jitterB, 256, 0.09f);
        const auto noJitterOut = render(noJitter, 256, 0.09f);
        test_support::check(hashSequence(jitterOutA) == hashSequence(jitterOutB), "jittered clock is deterministic under seed/reset");
        test_support::check(hashSequence(jitterOutA) != hashSequence(noJitterOut), "jittered clock differs from fixed divisor");

        BitRashDSP::Parameters filterParams;
        filterParams.bits = 12.0f;
        filterParams.divisor = 1.0f;
        filterParams.postFilter = 0.0f;
        filterParams.trimDb = 0.0f;
        auto unfiltered = makeDsp(filterParams);
        filterParams.preFilter = 1.0f;
        filterParams.postFilter = 1.0f;
        auto filtered = makeDsp(filterParams);
        std::vector<float> raw;
        std::vector<float> filt;
        for (int i = 0; i < 256; ++i)
        {
            const float input = (i & 1) == 0 ? 0.9f : -0.9f;
            raw.push_back(unfiltered.processSampleForTest(input, 0));
            filt.push_back(filtered.processSampleForTest(input, 0));
        }
        test_support::check(adjacentDifferenceEnergy(filt) < adjacentDifferenceEnergy(raw) * 0.5f, "pre/post filters reduce alternating high-frequency energy");

        BitRashDSP::Parameters silentParams;
        silentParams.bits = 1.0f;
        silentParams.divisor = 1.0f;
        silentParams.dither = 0.0f;
        silentParams.errorFeedback = 0.0f;
        silentParams.noiseShape = 0.0f;
        silentParams.postFilter = 0.0f;
        silentParams.trimDb = 0.0f;
        auto silent = makeDsp(silentParams, 2);
        for (int i = 0; i < 128; ++i)
            test_support::check(silent.processSampleForTest(0.0f, i & 1) == 0.0f, "reset silence stays silent with dither disabled");
        test_support::check(std::isfinite(silent.processSampleForTest(std::numeric_limits<float>::quiet_NaN(), 0)), "NaN input sanitized");
        test_support::check(std::isfinite(silent.processSampleForTest(std::numeric_limits<float>::infinity(), 1)), "Inf input sanitized");
        test_support::check(silent.preparedChannels() == 2, "stereo prepare accepted");
        BitRashDSP mono = makeDsp(silentParams, 1);
        test_support::check(mono.preparedChannels() == 1, "mono prepare accepted");

        BitRashDSP::Parameters maxParams;
        maxParams.bits = 16.0f;
        maxParams.divisor = 64.0f;
        maxParams.jitter = 1.0f;
        maxParams.dither = 1.0f;
        maxParams.errorFeedback = 0.9f;
        maxParams.noiseShape = 0.9f;
        maxParams.preFilter = 1.0f;
        maxParams.postFilter = 1.0f;
        maxParams.mode = BitRashDSP::ClipMode::wrap;
        maxParams.trimDb = 12.0f;
        auto hostile = makeDsp(maxParams, 2);
        for (int i = 0; i < 4096; ++i)
            test_support::check(std::isfinite(hostile.processSampleForTest(std::sin(static_cast<float>(i) * 0.61f) * 3.0f, i & 1)), "hostile max settings stay finite");
    });
}
