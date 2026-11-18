#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

namespace
{
    const juce::Colour kBackground { 0xff0d0d0e };
    const juce::Colour kSurface    { 0xff19191b };
    const juce::Colour kOutline    { 0xff2c2c30 };
    const juce::Colour kText       { 0xffd8d4cc };
    const juce::Colour kAmber      { 0xffe08a2c };
}

BleepAudioProcessorEditor::BleepAudioProcessorEditor (BleepAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    lookAndFeel.setColour (juce::Slider::rotarySliderFillColourId, kAmber);
    lookAndFeel.setColour (juce::Slider::rotarySliderOutlineColourId, kOutline);
    lookAndFeel.setColour (juce::Slider::thumbColourId, kAmber);
    lookAndFeel.setColour (juce::Slider::textBoxTextColourId, kText);
    lookAndFeel.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lookAndFeel.setColour (juce::Label::textColourId, kText);
    setLookAndFeel (&lookAndFeel);

    namespace id = bleep::params::id;
    const std::pair<const char*, const char*> layout[] = {
        { id::attack, "ATTACK" }, { id::decay, "DECAY" }, { id::sustain, "SUSTAIN" },
        { id::release, "RELEASE" }, { id::fineTune, "FINE" }, { id::outputGain, "OUTPUT" }
    };

    for (const auto& [paramId, name] : layout)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        k->label.setText (name, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.getState(), paramId, k->slider);
        knobs.push_back (std::move (k));
    }

    setResizable (true, true);
    setResizeLimits (520, 220, 1400, 600);
    setSize (640, 240);
}

BleepAudioProcessorEditor::~BleepAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void BleepAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    auto header = getLocalBounds().removeFromTop (44).reduced (16, 0);
    g.setColour (kText);
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText ("BLEEP / 001", header, juce::Justification::centredLeft);
    g.setColour (kAmber);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("PHASE 1  -  SINE / ADSR", header, juce::Justification::centredRight);

    auto panel = getLocalBounds().reduced (12).withTrimmedTop (36);
    g.setColour (kSurface);
    g.fillRect (panel);
    g.setColour (kOutline);
    g.drawRect (panel, 1);
}

void BleepAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12).withTrimmedTop (36).reduced (8);
    const int w = area.getWidth() / static_cast<int> (knobs.size());

    for (auto& k : knobs)
    {
        auto cell = area.removeFromLeft (w).reduced (4);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell);
    }
}
