#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
std::unique_ptr<juce::AudioParameterFloat> makeFloat(const char* id,
                                                     const char* name,
                                                     juce::NormalisableRange<float> range,
                                                     float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, range, defaultValue);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice(const char* id,
                                                       const char* name,
                                                       juce::StringArray choices,
                                                       int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, 1 }, name, choices, defaultIndex);
}
} // namespace

BitRashAudioProcessor::BitRashAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BitRashAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(12);
    params.push_back(makeFloat(bitrash::parameters::bits, "RASH Bits", { 1.0f, 16.0f, 1.0f }, 6.0f));
    params.push_back(makeFloat(bitrash::parameters::divisor, "CLOCK Divisor", { 1.0f, 64.0f, 1.0f }, 4.0f));
    params.push_back(makeFloat(bitrash::parameters::jitter, "CLOCK Jitter", { 0.0f, 1.0f, 0.001f }, 0.0f));
    params.push_back(makeFloat(bitrash::parameters::dither, "ERROR TPDF Dither", { 0.0f, 1.0f, 0.001f }, 0.0f));
    params.push_back(makeFloat(bitrash::parameters::errorFeedback, "ERROR Feedback", { 0.0f, 0.95f, 0.001f }, 0.0f));
    params.push_back(makeFloat(bitrash::parameters::noiseShape, "ERROR Shape", { 0.0f, 0.95f, 0.001f }, 0.0f));
    params.push_back(makeFloat(bitrash::parameters::preFilter, "FILTER Pre", { 0.0f, 1.0f, 0.001f }, 0.0f));
    params.push_back(makeFloat(bitrash::parameters::postFilter, "FILTER Post", { 0.0f, 1.0f, 0.001f }, 0.15f));
    params.push_back(makeChoice(bitrash::parameters::mode, "RASH Input Mode", { "Clamp", "Fold", "Wrap" }, 0));
    params.push_back(makeFloat(bitrash::parameters::seed, "ERROR Seed", { 0.0f, 1.0f, 0.001f }, 0.173f));
    params.push_back(makeFloat(bitrash::parameters::mix, "OUTPUT Wet Dry", { 0.0f, 1.0f, 0.001f }, 1.0f));
    params.push_back(makeFloat(bitrash::parameters::trim, "OUTPUT Trim (dB)", { -36.0f, 12.0f, 0.1f }, -3.0f));
    return { params.begin(), params.end() };
}

void BitRashAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    dsp.setTargets(readDspParameters());
    dsp.reset();
}

bool BitRashAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut)
        return false;
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void BitRashAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    dsp.setTargets(readDspParameters());

    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int channel = totalIn; channel < totalOut; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    std::array<float*, 2> channels { nullptr, nullptr };
    for (int channel = 0; channel < std::min(totalOut, 2); ++channel)
        channels[static_cast<std::size_t>(channel)] = buffer.getWritePointer(channel);
    dsp.processBlock(channels.data(), std::min(totalOut, 2), buffer.getNumSamples());
}

bitrash::dsp::BitRashDSP::Parameters BitRashAudioProcessor::readDspParameters() const noexcept
{
    bitrash::dsp::BitRashDSP::Parameters values;
    values.bits = parameters.getRawParameterValue(bitrash::parameters::bits)->load();
    values.divisor = parameters.getRawParameterValue(bitrash::parameters::divisor)->load();
    values.jitter = parameters.getRawParameterValue(bitrash::parameters::jitter)->load();
    values.dither = parameters.getRawParameterValue(bitrash::parameters::dither)->load();
    values.errorFeedback = parameters.getRawParameterValue(bitrash::parameters::errorFeedback)->load();
    values.noiseShape = parameters.getRawParameterValue(bitrash::parameters::noiseShape)->load();
    values.preFilter = parameters.getRawParameterValue(bitrash::parameters::preFilter)->load();
    values.postFilter = parameters.getRawParameterValue(bitrash::parameters::postFilter)->load();
    values.mode = static_cast<bitrash::dsp::BitRashDSP::ClipMode>(
        std::clamp(static_cast<int>(parameters.getRawParameterValue(bitrash::parameters::mode)->load()), 0, 2));
    values.seed = parameters.getRawParameterValue(bitrash::parameters::seed)->load();
    values.mix = parameters.getRawParameterValue(bitrash::parameters::mix)->load();
    values.trimDb = parameters.getRawParameterValue(bitrash::parameters::trim)->load();
    return values;
}

juce::AudioProcessorEditor* BitRashAudioProcessor::createEditor()
{
    return new BitRashAudioProcessorEditor(*this);
}

void BitRashAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void BitRashAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BitRashAudioProcessor();
}
