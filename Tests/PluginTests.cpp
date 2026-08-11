#include "TestSupport.h"
#include "ParameterIDs.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
juce::AudioProcessor::BusesLayout layout(juce::AudioChannelSet input, juce::AudioChannelSet output)
{
    juce::AudioProcessor::BusesLayout buses;
    buses.inputBuses.add(input);
    buses.outputBuses.add(output);
    return buses;
}

std::uint32_t hashBuffer(const juce::AudioBuffer<float>& buffer)
{
    std::uint32_t hash = 2166136261u;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto scaled = static_cast<std::int32_t>(std::round(buffer.getSample(0, sample) * 1000000.0f));
        hash ^= static_cast<std::uint32_t>(scaled);
        hash *= 16777619u;
    }
    return hash;
}

void setParameter(juce::AudioProcessorValueTreeState& parameters, const char* id, float value)
{
    auto* parameter = parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    return test_support::run("bitrash_plugin_tests", [] {
        BitRashAudioProcessor processor;
        test_support::check(processor.getName() == "BitRash", "product name");
        test_support::check(!processor.acceptsMidi(), "audio effect does not require MIDI");
        test_support::check(!processor.isMidiEffect(), "not a MIDI effect");
        test_support::check(processor.getLatencySamples() == 0, "zero latency bitcrusher");
        test_support::check(processor.getTailLengthSeconds() == 0.0, "zero tail bitcrusher");

        test_support::check(processor.isBusesLayoutSupported(layout(juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())), "mono to mono bus supported");
        test_support::check(processor.isBusesLayoutSupported(layout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())), "stereo to stereo bus supported");
        test_support::check(!processor.isBusesLayoutSupported(layout(juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())), "mono to stereo bus rejected");
        test_support::check(!processor.isBusesLayoutSupported(layout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono())), "stereo to mono bus rejected");

        const char* ids[] {
            bitrash::parameters::bits,
            bitrash::parameters::divisor,
            bitrash::parameters::jitter,
            bitrash::parameters::dither,
            bitrash::parameters::errorFeedback,
            bitrash::parameters::noiseShape,
            bitrash::parameters::preFilter,
            bitrash::parameters::postFilter,
            bitrash::parameters::mode,
            bitrash::parameters::seed,
            bitrash::parameters::mix,
            bitrash::parameters::trim
        };
        for (const auto* id : ids)
            test_support::check(processor.parameters.getParameter(id) != nullptr, std::string("parameter exists: ") + id);

        auto* bits = processor.parameters.getParameter(bitrash::parameters::bits);
        auto* divisor = processor.parameters.getParameter(bitrash::parameters::divisor);
        auto* seed = processor.parameters.getParameter(bitrash::parameters::seed);
        bits->setValueNotifyingHost(bits->convertTo0to1(3.0f));
        divisor->setValueNotifyingHost(divisor->convertTo0to1(11.0f));
        seed->setValueNotifyingHost(seed->convertTo0to1(0.777f));

        juce::MemoryBlock state;
        processor.getStateInformation(state);
        bits->setValueNotifyingHost(bits->convertTo0to1(12.0f));
        divisor->setValueNotifyingHost(divisor->convertTo0to1(1.0f));
        seed->setValueNotifyingHost(seed->convertTo0to1(0.1f));
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        test_support::check(std::abs(processor.parameters.getRawParameterValue(bitrash::parameters::bits)->load() - 3.0f) < 0.1f, "bits state round-trip");
        test_support::check(std::abs(processor.parameters.getRawParameterValue(bitrash::parameters::divisor)->load() - 11.0f) < 0.1f, "divisor state round-trip");
        test_support::check(std::abs(processor.parameters.getRawParameterValue(bitrash::parameters::seed)->load() - 0.777f) < 0.01f, "seed state round-trip");

        const char invalid[] = "not xml";
        processor.setStateInformation(invalid, static_cast<int>(sizeof(invalid)));
        test_support::check(std::isfinite(processor.parameters.getRawParameterValue(bitrash::parameters::bits)->load()), "invalid state ignored safely");

        processor.prepareToPlay(48000.0, 64);
        setParameter(processor.parameters, bitrash::parameters::bits, 2.0f);
        setParameter(processor.parameters, bitrash::parameters::divisor, 4.0f);
        juce::AudioBuffer<float> buffer(2, 128);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            buffer.setSample(0, i, std::sin(static_cast<float>(i) * 0.23f));
            buffer.setSample(1, i, 0.0f);
        }
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        float rightEnergy = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                test_support::check(std::isfinite(buffer.getSample(ch, i)), "processed samples finite");
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            rightEnergy += std::abs(buffer.getSample(1, i));
        test_support::check(rightEnergy < 0.0001f, "silent right channel remains silent with dither disabled");

        BitRashAudioProcessor modeProcessor;
        setParameter(modeProcessor.parameters, bitrash::parameters::bits, 4.0f);
        setParameter(modeProcessor.parameters, bitrash::parameters::divisor, 1.0f);
        setParameter(modeProcessor.parameters, bitrash::parameters::postFilter, 0.0f);
        setParameter(modeProcessor.parameters, bitrash::parameters::mix, 1.0f);
        setParameter(modeProcessor.parameters, bitrash::parameters::trim, 0.0f);
        setParameter(modeProcessor.parameters, bitrash::parameters::mode, 0.0f);
        modeProcessor.prepareToPlay(48000.0, 16);
        juce::AudioBuffer<float> modeBuffer(1, 1);
        modeBuffer.setSample(0, 0, 1.7f);
        modeProcessor.processBlock(modeBuffer, midi);
        const float clampSample = modeBuffer.getSample(0, 0);
        setParameter(modeProcessor.parameters, bitrash::parameters::mode, 2.0f);
        modeBuffer.setSample(0, 0, 1.7f);
        modeProcessor.processBlock(modeBuffer, midi);
        test_support::check(clampSample > 0.9f, "plugin live mode regression starts in clamp");
        test_support::check(modeBuffer.getSample(0, 0) < -0.2f, "plugin live mode changes to wrap without reset");

        auto configureSeeded = [](BitRashAudioProcessor& p, float seedValue) {
            setParameter(p.parameters, bitrash::parameters::bits, 8.0f);
            setParameter(p.parameters, bitrash::parameters::divisor, 1.0f);
            setParameter(p.parameters, bitrash::parameters::dither, 1.0f);
            setParameter(p.parameters, bitrash::parameters::postFilter, 0.0f);
            setParameter(p.parameters, bitrash::parameters::mix, 1.0f);
            setParameter(p.parameters, bitrash::parameters::trim, 0.0f);
            setParameter(p.parameters, bitrash::parameters::seed, seedValue);
            p.prepareToPlay(48000.0, 64);
        };

        BitRashAudioProcessor seedA;
        BitRashAudioProcessor seedB;
        BitRashAudioProcessor seedNoChange;
        configureSeeded(seedA, 0.1f);
        configureSeeded(seedB, 0.1f);
        configureSeeded(seedNoChange, 0.1f);
        juce::AudioBuffer<float> seedBufferA(1, 64);
        juce::AudioBuffer<float> seedBufferB(1, 64);
        juce::AudioBuffer<float> seedBufferNoChange(1, 64);
        seedBufferA.clear();
        seedBufferB.clear();
        seedBufferNoChange.clear();
        seedA.processBlock(seedBufferA, midi);
        seedB.processBlock(seedBufferB, midi);
        seedNoChange.processBlock(seedBufferNoChange, midi);
        setParameter(seedA.parameters, bitrash::parameters::seed, 0.7f);
        setParameter(seedB.parameters, bitrash::parameters::seed, 0.7f);
        seedBufferA.clear();
        seedBufferB.clear();
        seedBufferNoChange.clear();
        seedA.processBlock(seedBufferA, midi);
        seedB.processBlock(seedBufferB, midi);
        seedNoChange.processBlock(seedBufferNoChange, midi);
        test_support::check(hashBuffer(seedBufferA) == hashBuffer(seedBufferB), "plugin live seed change is deterministic across processors");
        test_support::check(hashBuffer(seedBufferA) != hashBuffer(seedBufferNoChange), "plugin live seed change reseeds without reset");
    });
}
