#include "TestSupport.h"
#include "ParameterIDs.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    return test_support::run("bitrash_editor_tests", [] {
        BitRashAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        auto* custom = dynamic_cast<BitRashAudioProcessorEditor*>(editor.get());
        test_support::check(custom != nullptr, "custom editor type, not GenericAudioProcessorEditor");
        test_support::check(dynamic_cast<juce::GenericAudioProcessorEditor*>(editor.get()) == nullptr, "not GenericAudioProcessorEditor");
        test_support::check(editor->getWidth() == BitRashAudioProcessorEditor::defaultWidth, "default width");
        test_support::check(editor->getHeight() == BitRashAudioProcessorEditor::defaultHeight, "default height");
        test_support::check(editor->getComponentID() == "bitrash-editor", "component id");
        test_support::check(editor->getName().isNotEmpty(), "accessible name");
        test_support::check(custom->getTooltip().isNotEmpty(), "editor tooltip");
        test_support::check(editor->getWantsKeyboardFocus(), "keyboard focus");

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
        {
            auto* control = editor->findChildWithID(juce::String("bitrash-") + id);
            test_support::check(control != nullptr, std::string("editor control exists: ") + id);
            test_support::check(control->getName().isNotEmpty(), std::string("control has accessible name: ") + id);
            auto* slider = dynamic_cast<juce::Slider*>(control);
            test_support::check(slider != nullptr && slider->getTooltip().isNotEmpty(), std::string("control has tooltip: ") + id);
        }

        juce::Image image(juce::Image::RGB, 320, 220, true);
        juce::Graphics g(image);
        editor->setBounds(0, 0, image.getWidth(), image.getHeight());
        editor->paint(g);
        const auto first = image.getPixelAt(0, 0);
        bool varied = false;
        bool hasBrightBit = false;
        for (int y = 0; y < image.getHeight(); y += 12)
            for (int x = 0; x < image.getWidth(); x += 12)
            {
                const auto pixel = image.getPixelAt(x, y);
                varied = varied || pixel != first;
                hasBrightBit = hasBrightBit || pixel.getBrightness() > 0.75f;
            }
        test_support::check(varied && hasBrightBit, "software paint uses monochrome bit-grid motif");
    });
}
