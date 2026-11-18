#pragma once

#include "BleepLookAndFeel.h"
#include "StepGrid.h"
#include "Wordmark.h"

#include <juce_audio_processors/juce_audio_processors.h>

class BleepAudioProcessor;

/** Editor layout:
      header     wordmark | mode | rate | status
      centre     SEQUENCER (length/root/gate + 16-step grid) | GENERATE (RANDOM, MUTATE, ...)
      bottom     SYNTH knobs
    Visual refinement of individual sections continues in Milestone 6. */
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
    void layoutKnobRow (std::vector<std::unique_ptr<Knob>>& group, juce::Rectangle<int> area, int maxKnob);
    void drawPanel (juce::Graphics&, juce::Rectangle<int> panel, const juce::String& title) const;
    void timerCallback() override;
    void updateModeButton();

    BleepAudioProcessor& processor;
    bleep::ui::BleepLookAndFeel lookAndFeel;   // declared first: outlives every child component
    juce::TooltipWindow tooltips { this, 600 };

    bleep::ui::Wordmark wordmark;
    juce::TextButton modeButton;
    std::unique_ptr<APVTS::ButtonAttachment> modeAttachment;
    juce::ComboBox rateBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> rateAttachment;
    juce::Label statusLabel;

    StepGrid grid;
    std::vector<std::unique_ptr<Knob>> seqKnobs;

    // Generative panel
    juce::TextButton randomButton { "RANDOM" };
    juce::TextButton mutateButton { "MUTATE" };
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::TextButton randSoundButton { "RANDOMIZE SOUND" };
    std::unique_ptr<APVTS::ButtonAttachment> randSoundAttachment;
    std::vector<std::unique_ptr<Knob>> genKnobs;

    // Панель SYNTH: группы ручек с подписями (FM / PITCH / FILTER / AMP / OUT).
    struct KnobGroup
    {
        juce::String title;
        std::vector<std::unique_ptr<Knob>> knobs;
        juce::Component* extra = nullptr;     // например, переключатель LP/BP
        juce::Rectangle<int> bounds;          // вычисляется в resized()
    };
    KnobGroup& addGroup (const juce::String& title);
    std::vector<std::unique_ptr<KnobGroup>> synthGroups;

    juce::TextButton filterModeButton;
    std::unique_ptr<APVTS::ButtonAttachment> filterModeAttachment;

    juce::Rectangle<int> headerArea, seqPanel, genPanel, synthPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepAudioProcessorEditor)
};
