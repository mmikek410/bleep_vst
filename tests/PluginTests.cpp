// Headless tests of the real AudioProcessor: MIDI in -> audio out, internal
// sequencer driven by a simulated host playhead, state round trip and
// backward compatibility with Phase 1 states.
// Build with -DBLEEP_BUILD_PLUGIN_TESTS=ON.

#include "plugin/PluginProcessor.h"
#include "plugin/Parameters.h"

#include <juce_events/juce_events.h>

#include <cstdio>
#include <vector>

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


    std::printf ("RANDOM / MUTATE while the host is playing\n");
    {
        BleepAudioProcessor p;
        TestPlayHead ph;
        p.setPlayHead (&ph);
        p.setPlayConfigDetails (0, 2, 48000.0, 128);
        p.prepareToPlay (48000.0, 128);
        setParam (p, id::seqMode, 1.0f);
        setParam (p, id::genRandSound, 1.0f);
        float peak = 0.0f;
        for (int i = 0; i < 60; ++i)
        {
            if (i % 3 == 0) p.randomizePattern ((std::uint32_t) i + 1);
            else            p.mutatePattern ((std::uint32_t) i + 100);
            peak = std::max (peak, run (p, &ph, 50, 128, 48000.0));   // ~0.13 s between presses
        }
        std::printf ("  peak over 60 presses: %.3f\n", peak);
        CHECK (peak > 0.02f && peak < 1.0f, "level out of range while generating");
        ph.playing = false;
        run (p, &ph, 400, 128, 48000.0);
        CHECK (silent (run (p, &ph, 16, 128, 48000.0)), "stuck note after generating during playback");
    }

    std::printf ("RANDOM is deterministic per seed and respects density / locks\n");
    {
        BleepAudioProcessor a, b;
        for (auto* p : { &a, &b }) { setParam (*p, id::genDensity, 50.0f); setParam (*p, id::genRange, 7.0f); }
        a.getPatternStore().modify ([] (bleep::seq::Pattern& pat) { pat.steps[3] = { true, 5, 0.5f, 1.0f, 1.0f, 1, true }; });
        b.getPatternStore().set (a.getPatternStore().get());
        a.randomizePattern (1234);
        b.randomizePattern (1234);
        const auto pa = a.getPatternStore().get();
        CHECK (pa == b.getPatternStore().get(), "same seed differs between instances");
        CHECK (pa.steps[3].locked && pa.steps[3].pitch == 5, "locked step changed by RANDOM");
        int active = 0;
        for (int i = 0; i < 16; ++i) { active += pa.steps[(size_t) i].active ? 1 : 0; CHECK (std::abs (pa.steps[(size_t) i].pitch) <= 7, "pitch outside range"); }
        CHECK (active == 8, "density 50% wrong active count");
    }

    std::printf ("UNDO / REDO restore pattern and randomized sound\n");
    {
        BleepAudioProcessor p;
        setParam (p, id::genRandSound, 1.0f);
        const auto original = p.getPatternStore().get();
        const float decay0 = getParam (p, id::decay), out0 = getParam (p, id::outputGain), fine0 = getParam (p, id::fineTune);
        CHECK (! p.canUndo(), "fresh instance has undo");

        p.randomizePattern (7);
        const auto randomised = p.getPatternStore().get();
        CHECK (randomised != original, "RANDOM did not change the pattern");
        CHECK (! near (getParam (p, id::decay), decay0), "RANDOMIZE SOUND did not change decay");
        CHECK (near (getParam (p, id::outputGain), out0) && near (getParam (p, id::fineTune), fine0),
               "RANDOMIZE SOUND touched output or tuning");

        setParam (p, id::genMutate, 50.0f);
        p.mutatePattern (8);
        const auto mutated = p.getPatternStore().get();
        CHECK (mutated != randomised, "MUTATE did not change the pattern");

        CHECK (p.undo() && p.getPatternStore().get() == randomised, "undo of MUTATE");
        CHECK (p.undo() && p.getPatternStore().get() == original, "undo of RANDOM");
        CHECK (std::abs (getParam (p, id::decay) - decay0) < 0.5f, "undo did not restore decay");
        CHECK (! p.undo(), "undo past the beginning");
        CHECK (p.redo() && p.getPatternStore().get() == randomised, "redo RANDOM");
        CHECK (p.redo() && p.getPatternStore().get() == mutated, "redo MUTATE");
        CHECK (! p.redo(), "redo past the end");

        p.clearPattern();
        CHECK (p.undo() && p.getPatternStore().get() == mutated, "undo CLEAR");

        // Grid gesture without a change adds no undo step; with a change it adds one.
        const auto before = p.getPatternStore().get();
        p.beginPatternEdit(); p.endPatternEdit();
        p.undo();
        CHECK (p.getPatternStore().get() != before, "empty edit should not have been an undo step");
        p.redo();
        p.beginPatternEdit();
        p.getPatternStore().modify ([] (bleep::seq::Pattern& pat) { pat.steps[0].active = ! pat.steps[0].active; });
        p.endPatternEdit();
        CHECK (p.undo() && p.getPatternStore().get() == before, "grid edit not undoable");
    }

    std::printf ("generated pattern and generator settings survive save / reload\n");
    {
        BleepAudioProcessor a;
        setParam (a, id::genDensity, 63.0f);
        setParam (a, id::genRange, 5.0f);
        setParam (a, id::genMutate, 80.0f);
        setParam (a, id::genRandSound, 1.0f);
        a.randomizePattern (99);
        a.mutatePattern (100);
        juce::MemoryBlock blob;
        a.getStateInformation (blob);
        BleepAudioProcessor b;
        b.setStateInformation (blob.getData(), (int) blob.getSize());
        CHECK (b.getPatternStore().get() == a.getPatternStore().get(), "generated pattern not restored");
        CHECK (near (getParam (b, id::genDensity), 63.0f) && near (getParam (b, id::genRange), 5.0f)
               && near (getParam (b, id::genMutate), 80.0f) && near (getParam (b, id::genRandSound), 1.0f),
               "generator settings not restored");
        auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
        CHECK (xml->getIntAttribute ("stateVersion") == BleepAudioProcessor::kStateVersion, "state version not written");
    }

    std::printf ("Milestone 1 (v2) state loads with generator defaults\n");
    {
        BleepAudioProcessor a;
        juce::MemoryBlock blob;
        a.getStateInformation (blob);
        auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
        xml->setAttribute ("stateVersion", 2);
        for (auto* pid : { "gen_density", "gen_range", "gen_mutate", "gen_rand_sound" })
            if (auto* e = xml->getChildByAttribute ("id", pid))
                xml->removeChildElement (e, true);
        juce::MemoryBlock v2;
        juce::AudioProcessor::copyXmlToBinary (*xml, v2);
        BleepAudioProcessor b;
        b.setStateInformation (v2.getData(), (int) v2.getSize());
        CHECK (near (getParam (b, id::genDensity), 40.0f) && near (getParam (b, id::genRandSound), 0.0f), "v2 state: generator defaults wrong");
    }


    std::printf ("Phase A: старые проекты (v1–v3) звучат как раньше — FM и фильтр нейтральны\n");
    {
        // Состояние версии 3 без новых параметров (как сохранял предыдущий билд).
        BleepAudioProcessor a;
        juce::MemoryBlock blob;
        a.getStateInformation (blob);
        auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
        xml->setAttribute ("stateVersion", 3);
        for (const auto& legacy : bleep::params::kPreSoundEngineValues)
            if (auto* e = xml->getChildByAttribute ("id", legacy.id))
                xml->removeChildElement (e, true);
        juce::MemoryBlock v3;
        juce::AudioProcessor::copyXmlToBinary (*xml, v3);

        BleepAudioProcessor b;
        b.setStateInformation (v3.getData(), (int) v3.getSize());
        for (const auto& legacy : bleep::params::kPreSoundEngineValues)
            CHECK (near (getParam (b, legacy.id), legacy.plainValue) || std::abs (getParam (b, legacy.id) - legacy.plainValue) < 0.01f,
                   "старый проект получил не нейтральное значение");

        // Новый экземпляр — с новым звуком по умолчанию (FM включён).
        BleepAudioProcessor fresh;
        CHECK (getParam (fresh, id::fmDepth) > 0.5f, "новый экземпляр без FM");

        // v4 сохраняет и восстанавливает звуковые параметры как есть.
        setParam (fresh, id::fmRatio, 2.76f);
        setParam (fresh, id::cutoff, 900.0f);
        setParam (fresh, id::filterMode, 1.0f);
        juce::MemoryBlock v4;
        fresh.getStateInformation (v4);
        BleepAudioProcessor c;
        c.setStateInformation (v4.getData(), (int) v4.getSize());
        CHECK (std::abs (getParam (c, id::fmRatio) - 2.76f) < 0.01f && std::abs (getParam (c, id::cutoff) - 900.0f) < 1.0f
               && near (getParam (c, id::filterMode), 1.0f), "v4: звуковые параметры не восстановились");
    }

    std::printf ("Phase A: RANDOMIZE SOUND меняет FM/фильтр/высоту, UNDO возвращает всё\n");
    {
        BleepAudioProcessor p;
        setParam (p, id::genRandSound, 1.0f);
        std::vector<float> before;
        for (auto* pid : BleepAudioProcessor::kSoundParams) before.push_back (getParam (p, pid));
        const float out0 = getParam (p, id::outputGain), fine0 = getParam (p, id::fineTune), sus0 = getParam (p, id::sustain);

        int changed = 0;
        for (std::uint32_t seed = 1; seed <= 20; ++seed)
        {
            p.randomizePattern (seed);
            for (size_t i = 0; i < before.size(); ++i)
                changed += std::abs (getParam (p, BleepAudioProcessor::kSoundParams[i]) - before[i]) > 1e-3f ? 1 : 0;
            CHECK (near (getParam (p, id::outputGain), out0) && near (getParam (p, id::fineTune), fine0)
                   && near (getParam (p, id::sustain), sus0), "RANDOMIZE SOUND тронул громкость/строй/sustain");
            CHECK (getParam (p, id::resonance) <= 50.0f, "резонанс вне безопасного диапазона");
        }
        CHECK (changed > 100, "RANDOMIZE SOUND почти ничего не меняет");
        for (int i = 0; i < 20; ++i) p.undo();
        for (size_t i = 0; i < before.size(); ++i)
            CHECK (std::abs (getParam (p, BleepAudioProcessor::kSoundParams[i]) - before[i]) < 1e-3f,
                   "UNDO не вернул звуковой параметр");
    }

    std::printf ("Phase A: случайные звуки во время воспроизведения — без NaN, пик <= 1\n");
    {
        BleepAudioProcessor p;
        TestPlayHead ph;
        p.setPlayHead (&ph);
        p.setPlayConfigDetails (0, 2, 44100.0, 256);
        p.prepareToPlay (44100.0, 256);
        setParam (p, id::seqMode, 1.0f);
        setParam (p, id::genRandSound, 1.0f);
        setParam (p, id::genDensity, 80.0f);
        float peak = 0.0f;
        for (int i = 0; i < 40; ++i)
        {
            p.randomizePattern ((std::uint32_t) (i * 7 + 3));
            if (i % 5 == 0) { setParam (p, id::resonance, 100.0f); setParam (p, id::fmDepth, 10.0f); setParam (p, id::fmFeedback, 100.0f); }
            peak = std::max (peak, run (p, &ph, 40, 256, 44100.0));
        }
        std::printf ("  пик: %.3f\n", peak);
        CHECK (peak > 0.01f && peak <= 1.0f, "уровень вне диапазона");
        ph.playing = false;
        run (p, &ph, 400, 256, 44100.0);
        CHECK (silent (run (p, &ph, 16, 256, 44100.0)), "зависшая нота");
    }

    std::printf (failures == 0 ? "PLUGIN TESTS PASSED\n" : "%d PLUGIN TEST FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
