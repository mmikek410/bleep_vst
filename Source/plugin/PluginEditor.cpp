#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"
#include "Theme.h"

using namespace bleep::theme;
using bleep::ui::BleepLookAndFeel;

namespace
{
    std::uint32_t freshSeed() { return static_cast<std::uint32_t> (juce::Random::getSystemRandom().nextInt()); }
}

BleepAudioProcessorEditor::BleepAudioProcessorEditor (BleepAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), grid (p)
{
    setLookAndFeel (&lookAndFeel);
    namespace id = bleep::params::id;

    // --- Header ---
    addAndMakeVisible (wordmark);

    modeButton.setClickingTogglesState (true);
    modeButton.setTooltip ("EXTERNAL: play from incoming MIDI. INTERNAL: the step sequencer plays with Live's transport.");
    modeButton.onStateChange = [this] { updateModeButton(); };
    addAndMakeVisible (modeButton);
    modeAttachment = std::make_unique<APVTS::ButtonAttachment> (p.getState(), id::seqMode, modeButton);
    updateModeButton();

    rateBox.addItemList ({ "1/4", "1/8", "1/16", "1/32" }, 1);
    rateBox.setJustificationType (juce::Justification::centred);
    rateBox.setTooltip ("Step length");
    addAndMakeVisible (rateBox);
    rateAttachment = std::make_unique<APVTS::ComboBoxAttachment> (p.getState(), id::seqRate, rateBox);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setColour (juce::Label::textColourId, textDim);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    BleepLookAndFeel::makeCaption (statusLabel);
    addAndMakeVisible (statusLabel);

    // --- Sequencer ---
    addKnob (seqKnobs, id::seqLength, "LENGTH");
    addKnob (seqKnobs, id::seqRoot, "ROOT");
    addKnob (seqKnobs, id::seqGate, "GATE");
    addAndMakeVisible (grid);

    // --- Generate ---
    BleepLookAndFeel::makePrimary (randomButton);
    randomButton.setTooltip ("Generate a new pattern. Locked steps are kept. UNDO brings the previous one back.");
    randomButton.onClick = [this] { processor.randomizePattern (freshSeed()); };
    mutateButton.setTooltip ("Vary the current pattern by MUTATION amount while keeping its groove.");
    mutateButton.onClick = [this] { processor.mutatePattern (freshSeed()); };
    clearButton.setTooltip ("Turn off all unlocked steps.");
    clearButton.onClick = [this] { processor.clearPattern(); };
    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };

    randSoundButton.setClickingTogglesState (true);
    randSoundButton.setTooltip ("RANDOM also varies attack, decay and release. FM and pitch envelope join when the FM engine arrives.");
    randSoundAttachment = std::make_unique<APVTS::ButtonAttachment> (p.getState(), id::genRandSound, randSoundButton);

    for (auto* b : { &randomButton, &mutateButton, &clearButton, &undoButton, &redoButton, &randSoundButton })
        addAndMakeVisible (*b);

    addKnob (genKnobs, id::genDensity, "DENSITY");
    addKnob (genKnobs, id::genRange, "RANGE");
    addKnob (genKnobs, id::genMutate, "MUTATION");

    // --- Synth ---
    addKnob (synthKnobs, id::attack, "ATTACK");
    addKnob (synthKnobs, id::decay, "DECAY");
    addKnob (synthKnobs, id::sustain, "SUSTAIN");
    addKnob (synthKnobs, id::release, "RELEASE");
    addKnob (synthKnobs, id::fineTune, "FINE");
    addKnob (synthKnobs, id::outputGain, "OUTPUT");

    setResizable (true, true);
    setResizeLimits (940, 560, 1900, 1200);
    setSize (1120, 640);
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
    // Add first so the slider's value box is created by our look-and-feel.
    addAndMakeVisible (k->slider);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (11.5f));
    k->label.setColour (juce::Label::textColourId, textDim);
    BleepLookAndFeel::makeCaption (k->label);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<APVTS::SliderAttachment> (processor.getState(), paramId, k->slider);
    group.push_back (std::move (k));
    return *group.back();
}

void BleepAudioProcessorEditor::updateModeButton()
{
    modeButton.setButtonText (modeButton.getToggleState() ? "SEQ  INTERNAL" : "SEQ  EXTERNAL MIDI");
}

void BleepAudioProcessorEditor::timerCallback()
{
    juce::String status;
    if (! modeButton.getToggleState())
        status = "PLAYING FROM INCOMING MIDI";
    else if (processor.getCurrentStep() < 0)
        status = "SEQUENCER ARMED  -  PRESS PLAY IN LIVE";
    else
        status = "STEP " + juce::String (processor.getCurrentStep() + 1).paddedLeft ('0', 2);

    if (statusLabel.getText() != status)
        statusLabel.setText (status, juce::dontSendNotification);

    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
}

void BleepAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> panel, const juce::String& title) const
{
    g.setColour (surface);
    g.fillRect (panel);
    g.setColour (outline);
    g.drawRect (panel, 1);

    // Title with a short amber tick: the only ornament.
    auto t = panel.reduced (padding, 0).withHeight (30);
    g.setColour (amber);
    g.fillRect (t.getX(), t.getCentreY() - 1, 8, 2);
    juce::GlyphArrangement ga;
    ga.addLineOfText (lookAndFeel.fonts().title (9.5f), title, (float) t.getX() + 15.0f, (float) t.getCentreY() + 4.0f);
    g.setColour (textDim);
    ga.draw (g);
}

void BleepAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    g.setColour (outline);
    g.drawHorizontalLine (headerArea.getBottom() - 1, (float) gutter, (float) (getWidth() - gutter));

    drawPanel (g, seqPanel, "SEQUENCER");
    drawPanel (g, genPanel, "GENERATE");
    drawPanel (g, synthPanel, "SYNTH");
}

void BleepAudioProcessorEditor::layoutKnobRow (std::vector<std::unique_ptr<Knob>>& group, juce::Rectangle<int> area, int maxKnob)
{
    // Caption (16) + knob + value box (18), centred as one block.
    const int w = area.getWidth() / (int) group.size();
    const int knob = juce::jmin (maxKnob, w - 10, area.getHeight() - 16 - 18);
    const int blockH = 16 + knob + 18;
    for (auto& k : group)
    {
        auto cell = area.removeFromLeft (w).withSizeKeepingCentre (w, blockH);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell.withSizeKeepingCentre (juce::jmax (knob, 72), cell.getHeight()));
    }
}

void BleepAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header: wordmark left, controls right.
    headerArea = area.removeFromTop (header);
    auto h = headerArea.reduced (gutter + 6, 12);
    auto controls = h.removeFromRight (juce::jmin (600, h.getWidth() / 2 + 100));
    wordmark.setBounds (h.withTrimmedRight (gutter));
    modeButton.setBounds (controls.removeFromLeft (168));
    controls.removeFromLeft (8);
    rateBox.setBounds (controls.removeFromLeft (76));
    controls.removeFromLeft (gutter);
    statusLabel.setBounds (controls);

    area = area.reduced (gutter, 0);
    area.removeFromTop (gutter);
    area.removeFromBottom (gutter);

    synthPanel = area.removeFromBottom (150);
    area.removeFromBottom (gutter);
    genPanel = area.removeFromRight (252);
    area.removeFromRight (gutter);
    seqPanel = area;

    // Sequencer: knob column + grid.
    auto seq = seqPanel.reduced (padding).withTrimmedTop (24);
    auto knobCol = seq.removeFromLeft (82);
    const int kh = knobCol.getHeight() / (int) seqKnobs.size();
    for (auto& k : seqKnobs)
    {
        auto cell = knobCol.removeFromTop (kh).reduced (0, 2);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
    seq.removeFromLeft (padding);
    grid.setBounds (seq);

    // Generate: RANDOM, MUTATE | CLEAR, UNDO | REDO, knobs, RANDOMIZE SOUND.
    auto gen = genPanel.reduced (padding).withTrimmedTop (24);
    randomButton.setBounds (gen.removeFromTop (50));
    gen.removeFromTop (8);
    auto row = gen.removeFromTop (30);
    mutateButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
    row.removeFromLeft (8);
    clearButton.setBounds (row);
    gen.removeFromTop (6);
    row = gen.removeFromTop (24);
    undoButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
    row.removeFromLeft (8);
    redoButton.setBounds (row);
    randSoundButton.setBounds (gen.removeFromBottom (26));
    gen.removeFromBottom (8);
    layoutKnobRow (genKnobs, gen, 58);

    // Synth row.
    layoutKnobRow (synthKnobs, synthPanel.reduced (padding).withTrimmedTop (24), 62);
}
