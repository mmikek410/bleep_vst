// Headless tests of the real AudioProcessor: MIDI in -> audio out, internal
// sequencer driven by a simulated host playhead, state round trip and
// backward compatibility with Phase 1 states.
// Build with -DBLEEP_BUILD_PLUGIN_TESTS=ON.

#include "plugin/PluginProcessor.h"
#include "plugin/Parameters.h"

#include <juce_events/juce_events.h>

#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("  FAIL: %s\n", msg); } } while (0)

namespace id = bleep::params::id;

/** Simulated host transport. */
struct TestPlayHead final : juce::AudioPlayHead
{
    bool playing = true;
    double ppq = 0.0, bpm = 120.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        info.setBpm (bpm);
        return info;
    }
};

static void setParam (BleepAudioProcessor& p, const char* paramId, float plainValue)
{
    auto* param = p.getState().getParameter (paramId);
    param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

static bool near (float a, float b) { return std::abs (a - b) < 1.0e-4f; }
static bool silent (float peak) { return peak <= 0.0f; }

static float getParam (BleepAudioProcessor& p, const char* paramId)
{
    auto* param = p.getState().getParameter (paramId);
    return param->convertFrom0to1 (param->getValue());
}

/** Renders `blocks` blocks, advancing the playhead like a host. Returns the peak. */
static float run (BleepAudioProcessor& p, TestPlayHead* ph, int blocks, int blockSize, double sr,
                  juce::MidiBuffer firstBlockMidi = {})
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    float peakValue = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        juce::MidiBuffer m;
        if (b == 0) m = firstBlockMidi;
        p.processBlock (buffer, m);
        for (int ch = 0; ch < 2; ++ch)
            peakValue = std::max (peakValue, buffer.getMagnitude (ch, 0, blockSize));
        CHECK (near (buffer.getMagnitude (0, 0, blockSize), buffer.getMagnitude (1, 0, blockSize)), "L/R differ");
        if (ph != nullptr && ph->playing)
            ph->ppq += blockSize * ph->bpm / (60.0 * sr);
    }
    return peakValue;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("external MIDI (Phase 1 behaviour)\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        BleepAudioProcessor p;
        TestPlayHead ph;
        ph.playing = false;                  // transport stopped: MIDI must still play
        p.setPlayHead (&ph);
        p.setPlayConfigDetails (0, 2, sr, 256);
        p.prepareToPlay (sr, 256);

        CHECK (silent (run (p, &ph, 4, 256, sr)), "noise without notes");
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 31);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 200);
        const float pk = run (p, &ph, 8, 256, sr, midi);
        std::printf ("  sr=%.0f note peak=%.3f\n", sr, pk);
        CHECK (pk > 0.05f && pk < 1.0f, "note level out of range");
        run (p, &ph, int (sr / 256), 256, sr);
        CHECK (silent (run (p, &ph, 16, 256, sr)), "note did not end");
    }

    std::printf ("internal sequencer follows the host transport\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        BleepAudioProcessor p;
        TestPlayHead ph;
        p.setPlayHead (&ph);
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        setParam (p, id::seqMode, 1.0f);    // Internal

        const int blocksPerBar = int (2.0 * sr / 512);  // 120 BPM, 4/4
        const float pk = run (p, &ph, blocksPerBar, 512, sr);
        std::printf ("  sr=%.0f seq peak=%.3f step=%d\n", sr, pk, p.getCurrentStep());
        CHECK (pk > 0.05f && pk < 1.0f, "sequencer silent or clipping");
        CHECK (p.getCurrentStep() >= 0, "playhead not reported while playing");

        // Loop back mid-bar and keep playing: must stay sane.
        ph.ppq = 0.0;
        CHECK (run (p, &ph, blocksPerBar / 2, 512, sr) < 1.0f, "clipping after loop jump");

        // Stop: everything must fall silent (no stuck notes) and playhead hides.
        ph.playing = false;
        run (p, &ph, int (sr / 512) + 1, 512, sr);
        CHECK (silent (run (p, &ph, 8, 512, sr)), "stuck note after stop");
        CHECK (p.getCurrentStep() == -1, "playhead still shown after stop");

        // External mode ignores the sequencer even while playing.
        ph.playing = true;
        setParam (p, id::seqMode, 0.0f);
        run (p, &ph, int (sr / 512) + 1, 512, sr);
        CHECK (silent (run (p, &ph, 16, 512, sr)), "external mode still sequencing");
    }

    std::printf ("empty pattern is silent; tempo change is safe\n");
    {
        BleepAudioProcessor p;
        TestPlayHead ph;
        p.setPlayHead (&ph);
        p.setPlayConfigDetails (0, 2, 48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        setParam (p, id::seqMode, 1.0f);
        p.getPatternStore().set (bleep::seq::Pattern {});
        CHECK (silent (run (p, &ph, 400, 256, 48000.0)), "empty pattern made sound");
        p.getPatternStore().set (bleep::seq::Pattern::makeDefault());
        ph.bpm = 174.0;
        const float pk = run (p, &ph, 400, 256, 48000.0);
        CHECK (pk > 0.05f && pk < 1.0f, "no sound after tempo change");
    }

    std::printf ("no playhead (standalone): sequencer free-runs\n");
    {
        BleepAudioProcessor p;
        p.setPlayConfigDetails (0, 2, 48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        setParam (p, id::seqMode, 1.0f);
        CHECK (run (p, nullptr, 400, 256, 48000.0) > 0.05f, "free-run silent");
    }

    std::printf ("state round trip\n");
    {
        BleepAudioProcessor a;
        setParam (a, id::decay, 700.0f);
        setParam (a, id::seqMode, 1.0f);
        setParam (a, id::seqRate, 3.0f);
        setParam (a, id::seqLength, 7.0f);
        setParam (a, id::seqRoot, 48.0f);
        auto pattern = bleep::seq::Pattern::makeDefault();
        pattern.steps[1] = { true, -12, 64.0f / 127.0f, 0.5f, 0.25f, 2, true };
        a.getPatternStore().set (pattern);

        juce::MemoryBlock blob;
        a.getStateInformation (blob);

        BleepAudioProcessor b;
        b.setStateInformation (blob.getData(), (int) blob.getSize());
        CHECK (std::abs (getParam (b, id::decay) - 700.0f) < 0.5f, "decay not restored");
        CHECK (near (getParam (b, id::seqMode), 1.0f), "seq mode not restored");
        CHECK (near (getParam (b, id::seqRate), 3.0f), "seq rate not restored");
        CHECK (near (getParam (b, id::seqLength), 7.0f), "seq length not restored");
        CHECK (near (getParam (b, id::seqRoot), 48.0f), "seq root not restored");
        CHECK (b.getPatternStore().get() == pattern, "pattern not restored");

        // Saving twice must not duplicate the pattern node.
        juce::MemoryBlock blob2;
        b.getStateInformation (blob2);
        auto xml = juce::AudioProcessor::getXmlFromBinary (blob2.getData(), (int) blob2.getSize());
        int patternNodes = 0;
        for (auto* child : xml->getChildIterator())
            patternNodes += child->hasTagName ("PATTERN") ? 1 : 0;
        CHECK (patternNodes == 1, "pattern node duplicated");

        b.setStateInformation ("garbage", 7);  // must be ignored, not crash
        CHECK (std::abs (getParam (b, id::decay) - 700.0f) < 0.5f, "garbage state changed params");
        std::printf ("  state blob: %d bytes\n", (int) blob.getSize());
    }

    std::printf ("Phase 1 state loads with sequencer off and default pattern\n");
    {
        // Exactly what Phase 1 wrote: APVTS params only, stateVersion 1, no PATTERN node.
        juce::XmlElement phase1 ("BLEEP001");
        phase1.setAttribute ("stateVersion", 1);
        const std::pair<const char*, double> values[] = {
            { "amp_attack", 3.0 }, { "amp_decay", 420.0 }, { "amp_sustain", 0.0 },
            { "amp_release", 90.0 }, { "osc_fine", -12.0 }, { "out_gain", -3.0 } };
        for (const auto& [pid, v] : values)
        {
            auto* e = phase1.createNewChildElement ("PARAM");
            e->setAttribute ("id", pid);
            e->setAttribute ("value", v);
        }
        juce::MemoryBlock blob;
        juce::AudioProcessor::copyXmlToBinary (phase1, blob);

        BleepAudioProcessor p;
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        CHECK (std::abs (getParam (p, id::decay) - 420.0f) < 0.5f, "phase 1 decay lost");
        CHECK (std::abs (getParam (p, id::fineTune) + 12.0f) < 0.01f, "phase 1 fine tune lost");
        CHECK (near (getParam (p, id::seqMode), 0.0f), "phase 1 set should load in External mode");
        CHECK (near (getParam (p, id::seqLength), 16.0f), "phase 1 set should get default length");
        CHECK (p.getPatternStore().get() == bleep::seq::Pattern::makeDefault(), "phase 1 set should get default pattern");
    }

    std::printf (failures == 0 ? "PLUGIN TESTS PASSED\n" : "%d PLUGIN TEST FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
