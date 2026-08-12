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
    setResizeLimits(minimumWidth, minimumHeight, defaultWidth * 2, defaultHeight * 2);
    setResizable(true, true);
    setName("BitRash editor");
    setComponentID("bitrash-editor");
    setTitle("BitRash");
    setDescription("BitRash monochrome 8-bit custom editor");
    setWantsKeyboardFocus(true);

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 112, 24);
        slider.setName(controls[i].name);
        slider.setComponentID(juce::String("bitrash-") + controls[i].id);
        slider.setTooltip(controls[i].tip);
        slider.setWantsKeyboardFocus(true);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff8a8a86));
        slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff2a2a2a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff2f2f0));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f0));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff050505));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff8a8a86));
        addAndMakeVisible(slider);

        auto& label = labels[i];
        label.setText(controls[i].name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f0));
        label.setTooltip(controls[i].tip);
        label.attachToComponent(&slider, true);
        addAndMakeVisible(label);

        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ownerProcessor.parameters, controls[i].id, slider);
    }

    setSize(defaultWidth, defaultHeight);
}

void BitRashAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds();
    g.fillAll(juce::Colour(0xff050505));

    g.setColour(juce::Colour(0xfff2f2f0));
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("BitRash", 32, 16, area.getWidth() - 64, 32, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff8a8a86));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("BIT CRUSHER", 32, 48, area.getWidth() - 64, 16, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawHorizontalLine(72, 32.0f, static_cast<float>(area.getWidth() - 32));
}

void BitRashAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(32);
    area.removeFromTop(48);

    const int rows = 6;
    const int columns = 2;
    const int rowHeight = area.getHeight() / rows;
    const int colWidth = area.getWidth() / columns;

    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const int row = static_cast<int>(i) % rows;
        const int column = static_cast<int>(i) / rows;
        auto cell = juce::Rectangle<int>(area.getX() + column * colWidth,
                                         area.getY() + row * rowHeight,
                                         colWidth,
                                         rowHeight).reduced(8, 8);
        sliders[i].setBounds(cell.withTrimmedLeft(132));
    }
}
