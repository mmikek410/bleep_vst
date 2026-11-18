#pragma once

#include "seq/Pattern.h"

#include <juce_gui_basics/juce_gui_basics.h>

class BleepAudioProcessor;

/** The 16-step sequencer grid.

    Each column is one step:
      - top square: click to switch the step on/off (drag across to paint)
      - lower area: drag up/down to set the pitch (semitones from root),
        double-click to reset it to the root
    The playing step is outlined in amber. Steps beyond the sequence
    length are dimmed. */
class StepGrid final : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (BleepAudioProcessor&);
    ~StepGrid() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum class DragMode { None, Paint, Pitch };

    void timerCallback() override;
    juce::Rectangle<float> cellBounds (int step) const;
    juce::Rectangle<float> toggleBounds (int step) const;
    juce::Rectangle<float> pitchBounds (int step) const;
    int stepAt (juce::Point<float> p) const;
    int visibleLength() const;
    int rootNote() const;

    BleepAudioProcessor& processor;
    bleep::seq::Pattern pattern;        // UI copy, refreshed when the store changes
    std::uint32_t patternVersion = 0;
    int playingStep = -1;
    int length = 16;
    int root = 60;

    DragMode dragMode = DragMode::None;
    bool paintValue = false;
    int dragStep = -1;
    int dragStartPitch = 0;
    float dragStartY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
};
