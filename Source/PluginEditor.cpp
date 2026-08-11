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
    setSize(defaultWidth, defaultHeight);
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
        addAndMakeVisible(slider);

        auto& label = labels[i];
        label.setText(controls[i].name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffeeeeee));
        label.setTooltip(controls[i].tip);
        label.attachToComponent(&slider, true);
        addAndMakeVisible(label);

        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ownerProcessor.parameters, controls[i].id, slider);
    }
}

void BitRashAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds();
    g.fillAll(juce::Colour(0xff050505));

    const auto grid = 8;
    g.setColour(juce::Colour(0xff202020));
    for (int x = 0; x < area.getWidth(); x += grid)
        g.drawVerticalLine(x, 0.0f, static_cast<float>(area.getHeight()));
    for (int y = 0; y < area.getHeight(); y += grid)
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(area.getWidth()));

    const auto bits = ownerProcessor.parameters.getRawParameterValue(bitrash::parameters::bits)->load();
    const auto divisor = ownerProcessor.parameters.getRawParameterValue(bitrash::parameters::divisor)->load();
    const auto jitter = ownerProcessor.parameters.getRawParameterValue(bitrash::parameters::jitter)->load();
    const auto dither = ownerProcessor.parameters.getRawParameterValue(bitrash::parameters::dither)->load();
    const auto feedback = ownerProcessor.parameters.getRawParameterValue(bitrash::parameters::errorFeedback)->load();
    const auto shape = ownerProcessor.parameters.getRawParameterValue(bitrash::parameters::noiseShape)->load();
    const auto mix = ownerProcessor.parameters.getRawParameterValue(bitrash::parameters::mix)->load();
    const float values[] {
        (bits - 1.0f) / 15.0f,
        (divisor - 1.0f) / 63.0f,
        jitter,
        dither,
        feedback / 0.95f,
        shape / 0.95f,
        mix
    };

    g.setColour(juce::Colour(0xffe8e8e8));
    g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    g.drawText("BitRash", 32, 24, area.getWidth() - 64, 48, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(16.0f));
    g.drawText("jp.ehl.bitrash / BtRh", 34, 74, area.getWidth() - 68, 24, juce::Justification::centredLeft);

    const int left = 32;
    const int top = 108;
    const int width = area.getWidth() - 64;
    for (int i = 0; i < 7; ++i)
    {
        const int y = top + i * 18;
        const int filled = static_cast<int>(static_cast<float>(width) * values[i]);
        g.setColour(juce::Colour(i % 2 == 0 ? 0xffd6d6d6 : 0xff8a8a8a));
        for (int x = 0; x < filled; x += 16)
            g.fillRect(left + x, y, 8, 10);
        g.setColour(juce::Colour(0xff404040));
        g.drawRect(left, y, width, 10, 1);
    }

    g.setColour(juce::Colour(0xfff2f2f2));
    for (int y = area.getHeight() - 112; y < area.getHeight() - 32; y += 16)
    {
        for (int x = 32; x < area.getWidth() - 32; x += 24)
        {
            const int bit = ((x / 24) + (y / 16) + static_cast<int>(bits)) & 7;
            if (bit == 0 || bit == 2 || bit == 5)
                g.fillRect(x, y, 8, 8);
        }
    }
}

void BitRashAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(32);
    area.removeFromTop(232);
    const int rowHeight = 22;
    const int gap = 4;
    for (auto& slider : sliders)
    {
        slider.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(132));
        area.removeFromTop(gap);
    }
}
