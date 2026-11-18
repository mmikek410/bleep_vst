#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"
#include "Theme.h"

using namespace bleep::theme;

BleepAudioProcessorEditor::BleepAudioProcessorEditor (BleepAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), grid (p)
{
    lookAndFeel.setColour (juce::Slider::rotarySliderFillColourId, amber);
    lookAndFeel.setColour (juce::Slider::rotarySliderOutlineColourId, outline);
    lookAndFeel.setColour (juce::Slider::thumbColourId, amber);
    lookAndFeel.setColour (juce::Slider::textBoxTextColourId, text);
    lookAndFeel.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lookAndFeel.setColour (juce::Label::textColourId, text);
    lookAndFeel.setColour (juce::TextButton::buttonColourId, surfaceHi);
    lookAndFeel.setColour (juce::TextButton::buttonOnColourId, amber);
    lookAndFeel.setColour (juce::TextButton::textColourOffId, text);
    lookAndFeel.setColour (juce::TextButton::textColourOnId, background);
    lookAndFeel.setColour (juce::ComboBox::backgroundColourId, surfaceHi);
    lookAndFeel.setColour (juce::ComboBox::outlineColourId, outline);
    lookAndFeel.setColour (juce::ComboBox::textColourId, text);
    lookAndFeel.setColour (juce::ComboBox::arrowColourId, amber);
    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId, surface);
    lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, amber);
    setLookAndFeel (&lookAndFeel);

    namespace id = bleep::params::id;

    // --- Sequencer header ---
    modeButton.setClickingTogglesState (true);
    modeButton.onStateChange = [this] { updateModeButton(); };
    addAndMakeVisible (modeButton);
    modeAttachment = std::make_unique<APVTS::ButtonAttachment> (p.getState(), id::seqMode, modeButton);
    updateModeButton();

    rateBox.addItemList ({ "1/4", "1/8", "1/16", "1/32" }, 1);
    rateBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rateBox);
    rateAttachment = std::make_unique<APVTS::ComboBoxAttachment> (p.getState(), id::seqRate, rateBox);

    clearButton.onClick = [this]
    {
        processor.getPatternStore().modify ([] (bleep::seq::Pattern& pat)
        {
            for (auto& s : pat.steps)
                if (! s.locked)
                    s = bleep::seq::Step {};
        });
    };
    addAndMakeVisible (clearButton);

    statusLabel.setFont (juce::FontOptions (11.0f));
    statusLabel.setColour (juce::Label::textColourId, textDim);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    addKnob (seqKnobs, id::seqLength, "LENGTH");
    addKnob (seqKnobs, id::seqRoot, "ROOT");
    addKnob (seqKnobs, id::seqGate, "GATE");

    addAndMakeVisible (grid);

    // --- Synth ---
    addKnob (synthKnobs, id::attack, "ATTACK");
    addKnob (synthKnobs, id::decay, "DECAY");
    addKnob (synthKnobs, id::sustain, "SUSTAIN");
    addKnob (synthKnobs, id::release, "RELEASE");
    addKnob (synthKnobs, id::fineTune, "FINE");
    addKnob (synthKnobs, id::outputGain, "OUTPUT");

    setResizable (true, true);
    setResizeLimits (760, 460, 1800, 1100);
    setSize (980, 560);
    startTimerHz (10);
}

BleepAudioProcessorEditor::~BleepAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

BleepAudioProcessorEditor::Knob& BleepAudioProcessorEditor::addKnob (std::vector<std::unique_ptr<Knob>>& group,
                                                                       const char* paramId, const char* name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (10.5f).withStyle ("Bold"));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<APVTS::SliderAttachment> (processor.getState(), paramId, k->slider);
    group.push_back (std::move (k));
    return *group.back();
}

void BleepAudioProcessorEditor::updateModeButton()
{
    modeButton.setButtonText (modeButton.getToggleState() ? "SEQ: INTERNAL" : "SEQ: EXTERNAL MIDI");
}

void BleepAudioProcessorEditor::timerCallback()
{
    juce::String status;
    if (! modeButton.getToggleState())
        status = "Playing from incoming MIDI. Switch to INTERNAL to use the sequencer.";
    else if (processor.getCurrentStep() < 0)
        status = "Internal sequencer armed. Press play in Live.";
    else
        status = "Step " + juce::String (processor.getCurrentStep() + 1);

    if (statusLabel.getText() != status)
        statusLabel.setText (status, juce::dontSendNotification);
}

void BleepAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto header = getLocalBounds().removeFromTop (44).reduced (16, 0);
    g.setColour (text);
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("BLEEP / 001", header, juce::Justification::centredLeft);

    for (auto panel : { seqPanel, synthPanel })
    {
        g.setColour (surface);
        g.fillRect (panel);
        g.setColour (outline);
        g.drawRect (panel, 1);
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    g.drawText ("SEQUENCER", seqPanel.reduced (10, 6).removeFromTop (12), juce::Justification::topLeft);
    g.drawText ("SYNTH", synthPanel.reduced (10, 6).removeFromTop (12), juce::Justification::topLeft);
}

void BleepAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (44).reduced (16, 8);
    header.removeFromLeft (150);  // title
    modeButton.setBounds (header.removeFromLeft (170));
    header.removeFromLeft (8);
    rateBox.setBounds (header.removeFromLeft (80));
    header.removeFromLeft (8);
    clearButton.setBounds (header.removeFromLeft (70));
    statusLabel.setBounds (header);

    area = area.reduced (12, 0);
    synthPanel = area.removeFromBottom (140);
    area.removeFromBottom (10);
    seqPanel = area;

    // Sequencer panel: knob column on the left, grid fills the rest.
    auto seq = seqPanel.reduced (10).withTrimmedTop (14);
    auto knobCol = seq.removeFromLeft (90);
    const int kh = knobCol.getHeight() / (int) seqKnobs.size();
    for (auto& k : seqKnobs)
    {
        auto cell = knobCol.removeFromTop (kh).reduced (2);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }
    seq.removeFromLeft (10);
    grid.setBounds (seq);

    // Synth row.
    auto synth = synthPanel.reduced (10).withTrimmedTop (14);
    const int w = synth.getWidth() / (int) synthKnobs.size();
    for (auto& k : synthKnobs)
    {
        auto cell = synth.removeFromLeft (w).reduced (4, 0);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }
}
