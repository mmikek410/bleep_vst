#pragma once

#include "StepGrid.h"

#include <juce_audio_processors/juce_audio_processors.h>

class BleepAudioProcessor;

/** Milestone 1 editor: header with sequencer controls, a large step grid,
    and the synth controls underneath. Visual refinement comes in Milestone 6. */
class BleepAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BleepAudioProcessorEditor (BleepAudioProcessor&);
    ~BleepAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    Knob& addKnob (std::vector<std::unique_ptr<Knob>>& group, const char* paramId, const char* name);
    void timerCallback() override;
    void updateModeButton();

    BleepAudioProcessor& processor;
    juce::LookAndFeel_V4 lookAndFeel;

    juce::TextButton modeButton;
    std::unique_ptr<APVTS::ButtonAttachment> modeAttachment;
    juce::ComboBox rateBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> rateAttachment;
    juce::TextButton clearButton { "CLEAR" };
    juce::Label statusLabel;

    StepGrid grid;
    std::vector<std::unique_ptr<Knob>> seqKnobs;
    std::vector<std::unique_ptr<Knob>> synthKnobs;

    juce::Rectangle<int> seqPanel, synthPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepAudioProcessorEditor)
};
