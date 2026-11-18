#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class BleepAudioProcessor;

/** Phase 1 editor: functional, not final. Dark panel with a rotary knob per
    parameter, values shown under each knob. The real layout comes in Phase 5. */
class BleepAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit BleepAudioProcessorEditor (BleepAudioProcessor&);
    ~BleepAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    juce::LookAndFeel_V4 lookAndFeel;
    std::vector<std::unique_ptr<Knob>> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepAudioProcessorEditor)
};
