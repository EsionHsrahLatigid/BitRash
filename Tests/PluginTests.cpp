#include "TestSupport.h"
#include "ParameterIDs.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <cmath>

namespace
{
juce::AudioProcessor::BusesLayout layout(juce::AudioChannelSet input, juce::AudioChannelSet output)
{
    juce::AudioProcessor::BusesLayout buses;
    buses.inputBuses.add(input);
    buses.outputBuses.add(output);
    return buses;
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
        processor.parameters.getParameter(bitrash::parameters::bits)->setValueNotifyingHost(bits->convertTo0to1(2.0f));
        processor.parameters.getParameter(bitrash::parameters::divisor)->setValueNotifyingHost(divisor->convertTo0to1(4.0f));
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
    });
}
