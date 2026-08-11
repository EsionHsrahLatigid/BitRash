#pragma once

#include <array>
#include <cstdint>

namespace bitrash::dsp
{
class BitRashDSP
{
public:
    enum class ClipMode
    {
        clamp = 0,
        fold = 1,
        wrap = 2
    };

    struct Parameters
    {
        float bits { 6.0f };
        float divisor { 4.0f };
        float jitter { 0.0f };
        float dither { 0.0f };
        float errorFeedback { 0.0f };
        float noiseShape { 0.0f };
        float preFilter { 0.0f };
        float postFilter { 0.15f };
        ClipMode mode { ClipMode::clamp };
        float seed { 0.173f };
        float mix { 1.0f };
        float trimDb { -3.0f };
    };

    void prepare(double sampleRate, int maxBlockSize, int channels);
    void reset() noexcept;
    void setTargets(const Parameters& parameters) noexcept;
    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept;
    float processSampleForTest(float input, int channel) noexcept;
    int preparedChannels() const noexcept { return channels_; }
    int preparedBlockSize() const noexcept { return maxBlockSize_; }

    static int levelCountForBits(int bits) noexcept;
    static float stepForBits(int bits) noexcept;
    static float quantize(float input, int bits, ClipMode mode) noexcept;

private:
    struct ChannelState
    {
        float preLowpass { 0.0f };
        float postLowpass { 0.0f };
        float lastError { 0.0f };
        float previousError { 0.0f };
        float heldSample { 0.0f };
        int holdRemaining { 0 };
        std::uint32_t rng { 0x12345678u };
    };

    static constexpr int maxChannels = 2;

    static float sanitize(float value, float fallback = 0.0f) noexcept;
    static float clamp(float value, float lo, float hi) noexcept;
    static float dbToGain(float db) noexcept;
    static float smooth(float current, float target) noexcept;
    static std::uint32_t seedToState(float seed, int channel) noexcept;
    static std::uint32_t nextRandom(std::uint32_t& state) noexcept;
    static float randomBipolar(std::uint32_t& state) noexcept;
    static float fold(float input) noexcept;
    static float wrap(float input) noexcept;
    void applyBlockBoundaryDiscreteTargets() noexcept;
    void reseedCurrentRngs() noexcept;
    int currentBits() const noexcept;
    int currentDivisor() const noexcept;
    int nextHoldLength(ChannelState& state, int divisor) const noexcept;

    double sampleRate_ { 44100.0 };
    int channels_ { 0 };
    int maxBlockSize_ { 0 };
    Parameters current_ {};
    Parameters target_ {};
    std::array<ChannelState, maxChannels> state_ {};
};
} // namespace bitrash::dsp
