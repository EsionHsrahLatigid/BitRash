#pragma once

#include <ehl/juce_design/EhlDesign.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

class BitRashAudioProcessor;

class BitRashAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit BitRashAudioProcessorEditor(BitRashAudioProcessor&);
    ~BitRashAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() { return tooltipText; }

    static constexpr int defaultWidth = ehl::juce_design::Metrics::defaultWidth;
    static constexpr int defaultHeight = ehl::juce_design::Metrics::defaultHeight;
    static constexpr int minimumWidth = ehl::juce_design::Metrics::minimumWidth;
    static constexpr int minimumHeight = ehl::juce_design::Metrics::minimumHeight;

private:
    BitRashAudioProcessor& ownerProcessor;
    ehl::juce_design::LookAndFeel ehlLookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 700 };
    juce::String tooltipText;
    std::array<juce::Slider, 12> sliders;
    std::array<juce::Label, 12> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 12> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BitRashAudioProcessorEditor)
};
