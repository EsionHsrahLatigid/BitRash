#include "TestSupport.h"
#include "ParameterIDs.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>

#include <string>

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
            const auto bounds = control->getBounds();
            test_support::check(! bounds.isEmpty(), std::string("initial parameter control bounds are non-empty: ") + id);
            test_support::check(bounds.getY() >= 80, std::string("initial parameter control stays below simple header: ") + id);
            test_support::check(bounds.getRight() <= editor->getWidth(), std::string("initial parameter control stays inside editor width: ") + id);
            test_support::check(bounds.getBottom() <= editor->getHeight(), std::string("initial parameter control stays inside editor height: ") + id);
        }

        juce::Image image(juce::Image::RGB, 320, 220, true);
        juce::Graphics g(image);
        editor->setBounds(0, 0, image.getWidth(), image.getHeight());
        editor->paint(g);
        const auto background = juce::Colour(0xff050505);
        const auto divider = juce::Colour(0xff2a2a2a);
        bool headerHasInk = false;
        bool separatorBandIsSimple = true;
        bool bodyIsPlain = true;
        int maxChannelSpread = 0;
        for (int y = 0; y < image.getHeight(); ++y)
        {
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto pixel = image.getPixelAt(x, y);
                headerHasInk = headerHasInk || (y < 64 && pixel != background);
                if (y >= 64 && y < 80)
                {
                    const bool onDivider = y == 72 && x >= 32 && x < image.getWidth() - 32;
                    separatorBandIsSimple = separatorBandIsSimple && pixel == (onDivider ? divider : background);
                }
                bodyIsPlain = bodyIsPlain && (y < 80 || pixel == background);
                const int red = pixel.getRed();
                const int green = pixel.getGreen();
                const int blue = pixel.getBlue();
                const int high = juce::jmax(juce::jmax(red, green), blue);
                const int low = juce::jmin(juce::jmin(red, green), blue);
                maxChannelSpread = juce::jmax(maxChannelSpread, high - low);
            }
        }
        test_support::check(headerHasInk, "simple header paints product text above divider");
        test_support::check(separatorBandIsSimple, "paint keeps only one divider in the 64px to 79px separator band");
        test_support::check(bodyIsPlain, "paint has no grid, motif, frame, or parameter-driven decoration below y=80");
        // The DHN9 paper/mid colours intentionally use a slight warm monochrome offset.
        test_support::check(maxChannelSpread <= 4,
                            "paint stays monochrome within DHN9 palette tolerance, max spread "
                                + std::to_string(maxChannelSpread));
    });
}
