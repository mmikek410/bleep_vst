// Renders the real editor offscreen to PNG files (no window needed).
// Usage: bleep_ui_snapshot <output-dir>
#include "plugin/PluginProcessor.h"
#include "plugin/BleepLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    bleep::ui::Fonts fonts;
    std::printf ("embedded typeface: %s / %s\n", fonts.semiBold != nullptr ? fonts.semiBold->getName().toRawUTF8() : "MISSING",
                 fonts.semiBold != nullptr ? fonts.semiBold->getStyle().toRawUTF8() : "");
    if (fonts.semiBold == nullptr || ! fonts.semiBold->getName().containsIgnoreCase ("Barlow"))
        return 1;

    BleepAudioProcessor processor;
    processor.getState().getParameter ("seq_mode")->setValueNotifyingHost (1.0f);
    processor.randomizePattern (20261118);
    processor.getPatternStore().modify ([] (bleep::seq::Pattern& p) { p.steps[2].locked = true; });

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    for (auto size : { juce::Point<int> (1240, 700), juce::Point<int> (1060, 620), juce::Point<int> (1600, 900) })
    {
        editor->setSize (size.x, size.y);
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile ("bleep_ui_" + juce::String (size.x) + "x" + juce::String (size.y) + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
    }
    editor = nullptr;
    return 0;
}
