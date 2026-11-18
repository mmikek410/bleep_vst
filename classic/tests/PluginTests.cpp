// Headless test of the real AudioProcessor: MIDI in -> audio out, and state round trip.
// Build with -DBLEEP_BUILD_PLUGIN_TESTS=ON.

#include "plugin/PluginProcessor.h"
#include "plugin/Parameters.h"

#include <juce_events/juce_events.h>

#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("  FAIL: %s\n", msg); } } while (0)

static float renderPeak (BleepAudioProcessor& p, int blocks, int blockSize, juce::MidiBuffer midi)
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    float peakValue = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        juce::MidiBuffer m;
        if (b == 0) m = midi;
        p.processBlock (buffer, m);
        for (int ch = 0; ch < 2; ++ch)
            peakValue = std::max (peakValue, buffer.getMagnitude (ch, 0, blockSize));
        CHECK (buffer.getMagnitude (0, 0, blockSize) == buffer.getMagnitude (1, 0, blockSize), "L/R differ");
    }
    return peakValue;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        BleepAudioProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 256);
        p.prepareToPlay (sr, 256);

        CHECK (renderPeak (p, 4, 256, {}) == 0.0f, "noise without notes");

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 31);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 200);
        const float pk = renderPeak (p, 8, 256, midi);
        std::printf ("sr=%.0f note peak=%.3f\n", sr, pk);
        CHECK (pk > 0.05f && pk < 1.0f, "note level out of range");
        renderPeak (p, int (sr / 256), 256, {});                       // let the tail finish (1 s)
        CHECK (renderPeak (p, 16, 256, {}) == 0.0f, "note did not end");
        p.releaseResources();
    }

    // State round trip.
    {
        BleepAudioProcessor a;
        a.getState().getParameter (bleep::params::id::decay)->setValueNotifyingHost (0.77f);
        a.getState().getParameter (bleep::params::id::fineTune)->setValueNotifyingHost (0.25f);
        juce::MemoryBlock blob;
        a.getStateInformation (blob);

        BleepAudioProcessor b;
        b.setStateInformation (blob.getData(), (int) blob.getSize());
        const auto get = [] (BleepAudioProcessor& p, const char* id) { return p.getState().getParameter (id)->getValue(); };
        CHECK (std::abs (get (b, bleep::params::id::decay) - 0.77f) < 1e-4f, "decay not restored");
        CHECK (std::abs (get (b, bleep::params::id::fineTune) - 0.25f) < 1e-4f, "fine tune not restored");

        b.setStateInformation ("garbage", 7);  // must be ignored, not crash
        CHECK (std::abs (get (b, bleep::params::id::decay) - 0.77f) < 1e-4f, "garbage state changed params");
        std::printf ("state blob: %d bytes\n", (int) blob.getSize());
    }

    std::printf (failures == 0 ? "PLUGIN TESTS PASSED\n" : "%d PLUGIN TEST FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
