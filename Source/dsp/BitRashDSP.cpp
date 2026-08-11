#include "dsp/BitRashDSP.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bitrash::dsp
{
void BitRashDSP::prepare(double sampleRate, int maxBlockSize, int channels)
{
    sampleRate_ = std::isfinite(sampleRate) && sampleRate > 1000.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    channels_ = channels < 0 ? 0 : (channels > 2 ? 2 : channels);
    reset();
}

void BitRashDSP::reset() noexcept
{
    current_ = target_;
    for (auto& state : state_)
        state = {};
    reseedCurrentRngs();
}

void BitRashDSP::setTargets(const Parameters& parameters) noexcept
{
    target_.bits = clamp(sanitize(parameters.bits, 6.0f), 1.0f, 16.0f);
    target_.divisor = clamp(sanitize(parameters.divisor, 4.0f), 1.0f, 64.0f);
    target_.jitter = clamp(sanitize(parameters.jitter), 0.0f, 1.0f);
    target_.dither = clamp(sanitize(parameters.dither), 0.0f, 1.0f);
    target_.errorFeedback = clamp(sanitize(parameters.errorFeedback), 0.0f, 0.95f);
    target_.noiseShape = clamp(sanitize(parameters.noiseShape), 0.0f, 0.95f);
    target_.preFilter = clamp(sanitize(parameters.preFilter), 0.0f, 1.0f);
    target_.postFilter = clamp(sanitize(parameters.postFilter), 0.0f, 1.0f);
    target_.mode = parameters.mode;
    target_.seed = clamp(sanitize(parameters.seed, 0.173f), 0.0f, 1.0f);
    target_.mix = clamp(sanitize(parameters.mix), 0.0f, 1.0f);
    target_.trimDb = clamp(sanitize(parameters.trimDb, -3.0f), -36.0f, 12.0f);
}

void BitRashDSP::processBlock(float* const* channels, int numChannels, int numSamples) noexcept
{
    const int boundedChannels = std::min({ numChannels, channels_, maxChannels });
    if (boundedChannels <= 0 || numSamples <= 0)
        return;

    applyBlockBoundaryDiscreteTargets();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        current_.bits = smooth(current_.bits, target_.bits);
        current_.divisor = smooth(current_.divisor, target_.divisor);
        current_.jitter = smooth(current_.jitter, target_.jitter);
        current_.dither = smooth(current_.dither, target_.dither);
        current_.errorFeedback = smooth(current_.errorFeedback, target_.errorFeedback);
        current_.noiseShape = smooth(current_.noiseShape, target_.noiseShape);
        current_.preFilter = smooth(current_.preFilter, target_.preFilter);
        current_.postFilter = smooth(current_.postFilter, target_.postFilter);
        current_.mix = smooth(current_.mix, target_.mix);
        current_.trimDb = smooth(current_.trimDb, target_.trimDb);

        for (int channel = 0; channel < boundedChannels; ++channel)
        {
            if (channels[channel] != nullptr)
                channels[channel][sample] = processSampleForTest(channels[channel][sample], channel);
        }
    }
}

float BitRashDSP::processSampleForTest(float input, int channel) noexcept
{
    input = clamp(sanitize(input), -4.0f, 4.0f);
    const int index = channel <= 0 ? 0 : 1;
    auto& state = state_[static_cast<std::size_t>(index)];

    const float dry = clamp(input, -1.25f, 1.25f);
    const float preAlpha = 0.02f + (1.0f - current_.preFilter) * 0.38f;
    state.preLowpass += (input - state.preLowpass) * preAlpha;
    float crushedInput = input + (state.preLowpass - input) * current_.preFilter;

    const int bits = currentBits();
    const float step = stepForBits(bits);
    const float shapedError = state.lastError + current_.noiseShape * (state.lastError - state.previousError);
    crushedInput += shapedError * current_.errorFeedback;

    const bool silenceWithoutNoise = std::abs(crushedInput) < 1.0e-12f && current_.dither <= 0.00001f
        && current_.errorFeedback <= 0.00001f && state.holdRemaining <= 0;

    float quantized = 0.0f;
    if (!silenceWithoutNoise)
    {
        const float tpdf = (randomBipolar(state.rng) - randomBipolar(state.rng)) * step * 0.5f * current_.dither;
        quantized = quantize(crushedInput + tpdf, bits, current_.mode);
    }

    const float error = clamp(quantized - crushedInput, -2.0f, 2.0f);
    state.previousError = state.lastError;
    state.lastError = error;

    if (state.holdRemaining <= 0)
    {
        state.heldSample = quantized;
        state.holdRemaining = nextHoldLength(state, currentDivisor());
    }
    --state.holdRemaining;

    const float postAlpha = 0.025f + (1.0f - current_.postFilter) * 0.42f;
    state.postLowpass += (state.heldSample - state.postLowpass) * postAlpha;
    const float wet = state.heldSample + (state.postLowpass - state.heldSample) * current_.postFilter;
    const float mixed = dry + (wet - dry) * current_.mix;
    return clamp(sanitize(mixed * dbToGain(current_.trimDb)), -1.5f, 1.5f);
}

int BitRashDSP::levelCountForBits(int bits) noexcept
{
    bits = std::max(1, std::min(bits, 16));
    return 1 << bits;
}

float BitRashDSP::stepForBits(int bits) noexcept
{
    return 2.0f / static_cast<float>(levelCountForBits(bits) - 1);
}

float BitRashDSP::quantize(float input, int bits, ClipMode mode) noexcept
{
    input = sanitize(input);
    switch (mode)
    {
        case ClipMode::fold:
            input = fold(input);
            break;
        case ClipMode::wrap:
            input = wrap(input);
            break;
        case ClipMode::clamp:
        default:
            input = clamp(input, -1.0f, 1.0f);
            break;
    }

    if (bits <= 1 && std::abs(input) < 1.0e-12f)
        return 0.0f;

    const float step = stepForBits(bits);
    const float code = std::round((clamp(input, -1.0f, 1.0f) + 1.0f) / step);
    return clamp(code * step - 1.0f, -1.0f, 1.0f);
}

float BitRashDSP::sanitize(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float BitRashDSP::clamp(float value, float lo, float hi) noexcept
{
    return value < lo ? lo : (value > hi ? hi : value);
}

float BitRashDSP::dbToGain(float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}

float BitRashDSP::smooth(float current, float target) noexcept
{
    return current + (target - current) * 0.0025f;
}

std::uint32_t BitRashDSP::seedToState(float seed, int channel) noexcept
{
    const auto scaled = static_cast<std::uint32_t>(clamp(seed, 0.0f, 1.0f) * 4294967295.0f);
    auto state = scaled ^ (0x9e3779b9u + static_cast<std::uint32_t>(channel) * 0x85ebca6bu);
    state ^= state >> 16;
    state *= 0x7feb352du;
    state ^= state >> 15;
    state *= 0x846ca68bu;
    state ^= state >> 16;
    return state == 0u ? 0x6d2b79f5u : state;
}

std::uint32_t BitRashDSP::nextRandom(std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state == 0u ? 0x6d2b79f5u : state;
}

float BitRashDSP::randomBipolar(std::uint32_t& state) noexcept
{
    constexpr float scale = 1.0f / 2147483648.0f;
    return static_cast<float>(nextRandom(state) & 0x7fffffffu) * scale * 2.0f - 1.0f;
}

float BitRashDSP::fold(float input) noexcept
{
    input = std::fmod(input + 1.0f, 4.0f);
    if (input < 0.0f)
        input += 4.0f;
    return input <= 2.0f ? input - 1.0f : 3.0f - input;
}

float BitRashDSP::wrap(float input) noexcept
{
    input = std::fmod(input + 1.0f, 2.0f);
    if (input < 0.0f)
        input += 2.0f;
    return input - 1.0f;
}

void BitRashDSP::applyBlockBoundaryDiscreteTargets() noexcept
{
    current_.mode = target_.mode;
    if (std::abs(current_.seed - target_.seed) > 0.0000001f)
    {
        current_.seed = target_.seed;
        reseedCurrentRngs();
    }
}

void BitRashDSP::reseedCurrentRngs() noexcept
{
    for (int channel = 0; channel < maxChannels; ++channel)
    {
        auto& state = state_[static_cast<std::size_t>(channel)];
        state.rng = seedToState(current_.seed, channel);
        state.holdRemaining = 0;
    }
}

int BitRashDSP::currentBits() const noexcept
{
    return std::max(1, std::min(16, static_cast<int>(std::round(current_.bits))));
}

int BitRashDSP::currentDivisor() const noexcept
{
    return std::max(1, std::min(64, static_cast<int>(std::round(current_.divisor))));
}

int BitRashDSP::nextHoldLength(ChannelState& state, int divisor) const noexcept
{
    if (divisor <= 1)
        return 1;
    const float jitterSpan = current_.jitter * static_cast<float>(divisor) * 0.45f;
    const int offset = static_cast<int>(std::round(randomBipolar(state.rng) * jitterSpan));
    return std::max(1, divisor + offset);
}
} // namespace bitrash::dsp
