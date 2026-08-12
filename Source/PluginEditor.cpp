#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ParameterIDs.h"

namespace
{
struct ControlSpec
{
    const char* id;
    const char* name;
    const char* tip;
};

constexpr std::array<ControlSpec, 12> controls {{
    { bitrash::parameters::bits, "Bits", "Quantizer resolution from one brutal bit to sixteen finite digital bits." },
    { bitrash::parameters::divisor, "Divisor", "Sample-hold clock divisor; higher values repeat crushed samples longer." },
    { bitrash::parameters::jitter, "Jitter", "Deterministic clock-length jitter from the seeded generator." },
    { bitrash::parameters::dither, "Dither", "Seeded TPDF dither amount in quantizer steps." },
    { bitrash::parameters::errorFeedback, "Feedback", "Bounded quantization-error feedback amount." },
    { bitrash::parameters::noiseShape, "Shape", "High-frequency weighting of the error-feedback path." },
    { bitrash::parameters::preFilter, "Pre Filter", "One-pole filtering before quantization." },
    { bitrash::parameters::postFilter, "Post Filter", "One-pole filtering after sample-hold." },
    { bitrash::parameters::mode, "Mode", "Out-of-range input handling: clamp, fold, or wrap." },
    { bitrash::parameters::seed, "Seed", "Deterministic source for dither and clock jitter." },
    { bitrash::parameters::mix, "Mix", "Dry to crushed signal blend." },
    { bitrash::parameters::trim, "Trim", "Output gain in decibels after the wet mix." },
}};
} // namespace

BitRashAudioProcessorEditor::BitRashAudioProcessorEditor(BitRashAudioProcessor& p)
    : AudioProcessorEditor(&p), ownerProcessor(p),
      tooltipText("BitRash: deterministic quantization, sample-hold decimation, seeded dither, bounded error feedback, jitter, filters, mix, and trim.")
{
    setLookAndFeel(&ehlLookAndFeel);
    setResizeLimits(minimumWidth, minimumHeight,
                    ehl::juce_design::Metrics::maximumWidth,
                    ehl::juce_design::Metrics::maximumHeight);
    setResizable(true, true);
    setName("BitRash editor");
    setComponentID("bitrash-editor");
    setTitle("BitRash");
    setDescription("BitRash monochrome 8-bit custom editor");
    setWantsKeyboardFocus(true);

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        auto& slider = sliders[i];
        ehl::juce_design::styleSlider(slider);
        slider.setName(controls[i].name);
        slider.setComponentID(juce::String("bitrash-") + controls[i].id);
        slider.setTooltip(controls[i].tip);
        slider.setWantsKeyboardFocus(true);
        addAndMakeVisible(slider);

        auto& label = labels[i];
        label.setText(controls[i].name, juce::dontSendNotification);
        ehl::juce_design::styleLabel(label);
        label.setTooltip(controls[i].tip);
        label.setComponentID(juce::String("bitrash-") + controls[i].id + "-label");
        addAndMakeVisible(label);

        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ownerProcessor.parameters, controls[i].id, slider);
    }

    setSize(defaultWidth, defaultHeight);
}

BitRashAudioProcessorEditor::~BitRashAudioProcessorEditor()
{
    for (auto& slider : sliders)
        slider.setLookAndFeel(nullptr);
    for (auto& label : labels)
        label.setLookAndFeel(nullptr);
    tooltipWindow.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void BitRashAudioProcessorEditor::paint(juce::Graphics& g)
{
    ehl::juce_design::paintEditorChrome(g, getLocalBounds(), "BitRash", "BIT CRUSHER");
}

void BitRashAudioProcessorEditor::resized()
{
    for (std::size_t i = 0; i < sliders.size(); ++i)
        ehl::juce_design::layoutLabelledControl(labels[i], sliders[i],
                                                ehl::juce_design::controlCell(getLocalBounds(), i));
}
