#include "StepGrid.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theme.h"

using bleep::seq::kMinPitch;
using bleep::seq::kMaxPitch;
using bleep::seq::kVisibleSteps;

namespace
{
    constexpr float kPixelsPerSemitone = 6.0f;
}

StepGrid::StepGrid (BleepAudioProcessor& p) : processor (p)
{
    pattern = processor.getPatternStore().get();
    patternVersion = processor.getPatternStore().getVersion();
    startTimerHz (30);
}

StepGrid::~StepGrid() { stopTimer(); }

int StepGrid::visibleLength() const
{
    return juce::jlimit (1, kVisibleSteps,
                         (int) std::lround (processor.getState().getRawParameterValue (bleep::params::id::seqLength)->load()));
}

int StepGrid::rootNote() const
{
    return (int) std::lround (processor.getState().getRawParameterValue (bleep::params::id::seqRoot)->load());
}

void StepGrid::timerCallback()
{
    bool dirty = false;

    const auto v = processor.getPatternStore().getVersion();
    if (v != patternVersion)
    {
        pattern = processor.getPatternStore().get();
        patternVersion = v;
        dirty = true;
    }

    const int step = processor.getCurrentStep();
    const int len = visibleLength();
    const int r = rootNote();
    if (step != playingStep || len != length || r != root)
    {
        playingStep = step;
        length = len;
        root = r;
        dirty = true;
    }

    if (dirty)
        repaint();
}

juce::Rectangle<float> StepGrid::cellBounds (int step) const
{
    // 4 groups of 4 with a wider gap between groups (beats).
    const float groupGap = 10.0f, cellGap = 4.0f;
    const float total = (float) getWidth() - 3.0f * groupGap - 12.0f * cellGap;
    const float w = total / (float) kVisibleSteps;
    const float x = (float) step * (w + cellGap) + (float) (step / 4) * (groupGap - cellGap);
    return { x, 0.0f, w, (float) getHeight() };
}

juce::Rectangle<float> StepGrid::toggleBounds (int step) const
{
    auto c = cellBounds (step);
    return c.removeFromTop (juce::jmin (c.getWidth(), c.getHeight() * 0.32f));
}

juce::Rectangle<float> StepGrid::pitchBounds (int step) const
{
    auto c = cellBounds (step);
    c.removeFromTop (toggleBounds (step).getHeight() + 6.0f);
    return c;
}

int StepGrid::stepAt (juce::Point<float> p) const
{
    for (int i = 0; i < kVisibleSteps; ++i)
        if (cellBounds (i).expanded (2.0f, 0.0f).contains (p))
            return i;
    return -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    using namespace bleep::theme;

    for (int i = 0; i < kVisibleSteps; ++i)
    {
        const auto& s = pattern.steps[(size_t) i];
        const bool inRange = i < length;
        const float alpha = inRange ? 1.0f : 0.25f;

        // On/off square.
        auto t = toggleBounds (i).reduced (1.0f);
        g.setColour ((s.active ? amber : surfaceHi).withMultipliedAlpha (alpha));
        g.fillRect (t);
        g.setColour (outline.withMultipliedAlpha (alpha));
        g.drawRect (t, 1.0f);

        // Pitch lane: bipolar bar from the centre line.
        auto pb = pitchBounds (i).reduced (1.0f);
        auto labelArea = pb.removeFromBottom (16.0f);
        g.setColour (surface.withMultipliedAlpha (alpha));
        g.fillRect (pb);
        g.setColour (outline.withMultipliedAlpha (alpha));
        g.drawRect (pb, 1.0f);

        const float centreY = pb.getCentreY();
        g.setColour (outline.withMultipliedAlpha (alpha));
        g.drawHorizontalLine ((int) centreY, pb.getX(), pb.getRight());

        if (s.pitch != 0)
        {
            const float h = (float) std::abs (s.pitch) / (float) kMaxPitch * (pb.getHeight() * 0.5f - 2.0f);
            auto bar = juce::Rectangle<float> (pb.getX() + 3.0f, s.pitch > 0 ? centreY - h : centreY,
                                               pb.getWidth() - 6.0f, h);
            g.setColour ((s.active ? amber : textDim).withMultipliedAlpha (alpha * 0.85f));
            g.fillRect (bar);
        }

        g.setColour ((s.active ? text : textDim).withMultipliedAlpha (alpha));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (bleep::params::noteName (root + s.pitch), labelArea, juce::Justification::centred, false);

        // Playhead.
        if (i == playingStep && inRange)
        {
            g.setColour (amber);
            g.drawRect (cellBounds (i).reduced (0.0f, 0.0f).withTrimmedBottom (0.0f), 2.0f);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int step = stepAt (e.position);
    if (step < 0)
        return;

    if (toggleBounds (step).contains (e.position))
    {
        dragMode = DragMode::Paint;
        paintValue = ! pattern.steps[(size_t) step].active;
        processor.getPatternStore().modify ([&] (bleep::seq::Pattern& p) { p.steps[(size_t) step].active = paintValue; });
        pattern.steps[(size_t) step].active = paintValue;
        dragStep = step;
    }
    else
    {
        dragMode = DragMode::Pitch;
        dragStep = step;
        dragStartPitch = pattern.steps[(size_t) step].pitch;
        dragStartY = e.position.y;
    }
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::Paint)
    {
        const int step = stepAt (e.position);
        if (step >= 0 && step != dragStep && pattern.steps[(size_t) step].active != paintValue)
        {
            dragStep = step;
            processor.getPatternStore().modify ([&] (bleep::seq::Pattern& p) { p.steps[(size_t) step].active = paintValue; });
            pattern.steps[(size_t) step].active = paintValue;
            repaint();
        }
    }
    else if (dragMode == DragMode::Pitch && dragStep >= 0)
    {
        const int delta = (int) std::lround ((dragStartY - e.position.y) / kPixelsPerSemitone);
        const int newPitch = juce::jlimit (kMinPitch, kMaxPitch, dragStartPitch + delta);
        if (newPitch != pattern.steps[(size_t) dragStep].pitch)
        {
            const int step = dragStep;
            processor.getPatternStore().modify ([&] (bleep::seq::Pattern& p) { p.steps[(size_t) step].pitch = newPitch; });
            pattern.steps[(size_t) step].pitch = newPitch;
            repaint();
        }
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::None;
    dragStep = -1;
    patternVersion = processor.getPatternStore().getVersion();
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int step = stepAt (e.position);
    if (step < 0 || ! pitchBounds (step).contains (e.position))
        return;
    processor.getPatternStore().modify ([&] (bleep::seq::Pattern& p) { p.steps[(size_t) step].pitch = 0; });
    pattern.steps[(size_t) step].pitch = 0;
    repaint();
}
