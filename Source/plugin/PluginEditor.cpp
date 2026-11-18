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

    // --- Synth: группы ---
    {
        auto& fm = addGroup ("FM");
        addKnob (fm.knobs, id::fmRatio, "RATIO");
        addKnob (fm.knobs, id::fmDepth, "DEPTH");
        addKnob (fm.knobs, id::fmFeedback, "FEEDBACK");
        addKnob (fm.knobs, id::fmDecay, "DECAY");

        auto& pitch = addGroup ("PITCH");
        addKnob (pitch.knobs, id::pitchAmount, "AMOUNT");
        addKnob (pitch.knobs, id::pitchDecay, "DECAY");
        addKnob (pitch.knobs, id::pitchCurve, "CURVE");

        auto& filter = addGroup ("FILTER");
        filterModeButton.setClickingTogglesState (true);
        filterModeButton.setTooltip ("LP: low-pass. BP: band-pass (hollow, woody).");
        filterModeButton.onStateChange = [this]
        {
            filterModeButton.setButtonText (filterModeButton.getToggleState() ? "BP" : "LP");
        };
        addAndMakeVisible (filterModeButton);
        filterModeAttachment = std::make_unique<APVTS::ButtonAttachment> (p.getState(), id::filterMode, filterModeButton);
        filterModeButton.setButtonText (filterModeButton.getToggleState() ? "BP" : "LP");
        filter.extra = &filterModeButton;
        addKnob (filter.knobs, id::cutoff, "CUTOFF");
        addKnob (filter.knobs, id::resonance, "RESO");
        addKnob (filter.knobs, id::filterEnv, "ENV");
        addKnob (filter.knobs, id::filterDecay, "DECAY");

        auto& amp = addGroup ("AMP");
        addKnob (amp.knobs, id::attack, "ATTACK");
        addKnob (amp.knobs, id::decay, "DECAY");
        addKnob (amp.knobs, id::sustain, "SUSTAIN");
        addKnob (amp.knobs, id::release, "RELEASE");

        auto& out = addGroup ("OUT");
        addKnob (out.knobs, id::fineTune, "FINE");
        addKnob (out.knobs, id::outputGain, "OUTPUT");
    }

    setResizable (true, true);
    setResizeLimits (1060, 620, 2000, 1250);
    setSize (1240, 700);
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

BleepAudioProcessorEditor::KnobGroup& BleepAudioProcessorEditor::addGroup (const juce::String& title)
{
    synthGroups.push_back (std::make_unique<KnobGroup>());
    synthGroups.back()->title = title;
    return *synthGroups.back();
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

    // Подписи групп и тонкие разделители между ними.
    for (size_t i = 0; i < synthGroups.size(); ++i)
    {
        const auto& grp = *synthGroups[i];
        juce::GlyphArrangement ga;
        ga.addLineOfText (lookAndFeel.fonts().title (8.5f), grp.title, (float) grp.bounds.getX() + 4.0f, (float) grp.bounds.getY() + 9.0f);
        g.setColour (amber.withAlpha (0.8f));
        ga.draw (g);
        if (i > 0)
        {
            g.setColour (outline);
            g.drawVerticalLine (grp.bounds.getX() - 8, (float) grp.bounds.getY() + 2.0f, (float) grp.bounds.getBottom() - 4.0f);
        }
    }
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

    synthPanel = area.removeFromBottom (172);
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

    // Synth: группы в ряд; ширина пропорциональна числу ручек, переключатель — половина ручки.
    {
        auto synthRow = synthPanel.reduced (padding).withTrimmedTop (22);
        const int groupGap = 16;
        float units = 0.0f;
        for (auto& grp : synthGroups)
            units += (float) grp->knobs.size() + (grp->extra != nullptr ? 0.6f : 0.0f);
        const float unitW = (float) (synthRow.getWidth() - groupGap * (int) (synthGroups.size() - 1)) / units;

        for (auto& grp : synthGroups)
        {
            const float u = (float) grp->knobs.size() + (grp->extra != nullptr ? 0.6f : 0.0f);
            grp->bounds = synthRow.removeFromLeft (juce::roundToInt (u * unitW));
            synthRow.removeFromLeft (groupGap);

            auto inner = grp->bounds.withTrimmedTop (14);
            if (grp->extra != nullptr)
            {
                auto col = inner.removeFromLeft (juce::roundToInt (0.6f * unitW));
                grp->extra->setBounds (col.withSizeKeepingCentre (juce::jmin (col.getWidth() - 6, 40), 26));
            }
            layoutKnobRow (grp->knobs, inner, 54);
        }
    }
}
