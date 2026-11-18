// JUCE-free tests for the BLEEP / 001 DSP core.
// Run: ./build-tests/bleep_tests [--wav out.wav]

#include "dsp/SynthEngine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using bleep::dsp::EngineParams;
using bleep::dsp::SynthEngine;

static int failures = 0;

#define CHECK(cond, ...)                                       \
    do {                                                       \
        if (! (cond)) {                                        \
            ++failures;                                        \
            std::printf ("  FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf (__VA_ARGS__);                         \
            std::printf ("\n");                                \
        }                                                      \
    } while (0)

struct Event { int sample; int note; float velocity; };  // velocity 0 = note off

// Renders events with a given (possibly irregular) block size, splitting at events
// exactly like the plugin's processBlock does.
static std::vector<float> renderSequence (double sr, int blockSize, int totalSamples,
                                          const std::vector<Event>& events, const EngineParams& p,
                                          SynthEngine* engineOut = nullptr)
{
    SynthEngine local;
    SynthEngine& e = engineOut ? *engineOut : local;
    e.setParams (p);
    e.prepare (sr);

    std::vector<float> out (static_cast<size_t> (totalSamples), 0.0f);
    size_t nextEvent = 0;
    int blockIndex = 0;

    for (int start = 0; start < totalSamples;)
    {
        // Vary block size to imitate hosts that send odd buffers.
        const int thisBlock = std::min (totalSamples - start, blockSize + (blockIndex++ % 3) * 7);
        int pos = start;
        const int end = start + thisBlock;

        while (nextEvent < events.size() && events[nextEvent].sample < end)
        {
            const auto& ev = events[nextEvent++];
            if (ev.sample > pos) { e.render (out.data() + pos, ev.sample - pos); pos = ev.sample; }
            if (ev.velocity > 0.0f) e.noteOn (ev.note, ev.velocity);
            else                    e.noteOff (ev.note);
        }
        if (pos < end) e.render (out.data() + pos, end - pos);
        start = end;
    }
    return out;
}

static double maxStep (const std::vector<float>& x)
{
    double m = 0.0;
    for (size_t i = 1; i < x.size(); ++i)
        m = std::max (m, (double) std::fabs (x[i] - x[i - 1]));
    return m;
}

static double peak (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    double m = 0.0;
    for (size_t i = from; i < std::min (to, x.size()); ++i)
        m = std::max (m, (double) std::fabs (x[i]));
    return m;
}

static bool allFinite (const std::vector<float>& x)
{
    for (float v : x) if (! std::isfinite (v)) return false;
    return true;
}

static void writeWav (const std::string& path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t dataBytes = (uint32_t) x.size() * 2;
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (1); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 2); u16 (2); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (float v : x) { auto s = (int16_t) std::lrint (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f); std::fwrite (&s, 2, 1, f); }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    EngineParams defaults;  // percussive defaults: 1 ms / 180 ms / 0 / 120 ms

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        for (int block : { 1, 17, 64, 512, 2048 })
        {
            std::printf ("sr=%.0f block=%d\n", sr, block);
            const int n = (int) (sr * 1.0);

            // Single bleep, A4 full velocity.
            auto x = renderSequence (sr, block, n, { { 100, 69, 1.0f }, { (int) (sr * 0.05), 69, 0.0f } }, defaults);
            CHECK (allFinite (x), "non-finite output");
            CHECK (peak (x) > 0.05, "too quiet: peak %f", peak (x));
            CHECK (peak (x) < 1.0, "clipping: peak %f", peak (x));
            CHECK (peak (x, 0, 100) == 0.0, "sound before note-on");
            // Sine at 440 Hz with gain g has max step 2*pi*440/sr*g. Allow margin for the attack.
            const double sineStep = 2.0 * M_PI * 440.0 / sr * peak (x);
            CHECK (maxStep (x) < sineStep * 1.6 + 1e-4, "click: max step %f vs sine %f", maxStep (x), sineStep);
            CHECK (peak (x, (size_t) (sr * 0.5)) == 0.0, "envelope didn't finish");

            // Note off before attack finished, rapid retriggers, and voice stealing (12 notes > 8 voices).
            std::vector<Event> ev;
            for (int i = 0; i < 12; ++i) ev.push_back ({ 50 + i * (int) (sr * 0.01), 48 + i * 3, 0.3f + 0.05f * i });
            for (int i = 0; i < 6; ++i)  ev.push_back ({ (int) (sr * 0.2) + i * 37, 60, 0.8f });
            ev.push_back ({ (int) (sr * 0.25), 60, 0.0f });
            for (int i = 0; i < 12; ++i) ev.push_back ({ (int) (sr * 0.3), 48 + i * 3, 0.0f });
            SynthEngine e;
            auto y = renderSequence (sr, block, n, ev, defaults, &e);
            CHECK (allFinite (y), "non-finite in stress");
            CHECK (peak (y) < 1.0, "clipping in stress: %f", peak (y));
            CHECK (e.getNumActiveVoices() == 0, "voices leaked: %d", e.getNumActiveVoices());
        }
    }

    // Sustain > 0 holds; note off releases; voice freed.
    {
        EngineParams p; p.sustain = 0.5f; p.release = 0.05f;
        SynthEngine e;
        auto x = renderSequence (48000.0, 256, 48000, { { 0, 60, 1.0f } }, p, &e);
        CHECK (peak (x, 40000) > 0.05, "sustain not holding");
        CHECK (e.getNumActiveVoices() == 1, "sustained voice not active");
        e.noteOff (60);
        std::vector<float> tail (48000 / 2);
        e.render (tail.data(), (int) tail.size());
        CHECK (e.getNumActiveVoices() == 0, "release didn't free voice");
    }

    // Extreme parameters stay finite.
    {
        EngineParams p; p.attack = 0.0f; p.decay = 0.0f; p.release = 0.0f; p.fineTune = 100.0f; p.outputGainDb = 6.0f;
        auto x = renderSequence (96000.0, 64, 9600, { { 0, 127, 1.0f }, { 10, 0, 1.0f } }, p);
        CHECK (allFinite (x), "non-finite at extremes");
    }

    // Optional audible check: a little bleep pattern.
    if (argc > 2 && std::strcmp (argv[1], "--wav") == 0)
    {
        std::vector<Event> ev;
        const int sr = 48000, step = sr * 60 / 128 / 4;  // 16ths at 128 BPM
        const int pattern[] = { 72, -1, 79, -1, -1, 72, -1, 84, 72, -1, 79, -1, -1, 75, -1, -1 };
        for (int bar = 0; bar < 2; ++bar)
            for (int i = 0; i < 16; ++i)
                if (pattern[i] >= 0)
                {
                    const int t = (bar * 16 + i) * step;
                    ev.push_back ({ t, pattern[i], i % 4 == 0 ? 1.0f : 0.6f });
                    ev.push_back ({ t + step / 2, pattern[i], 0.0f });
                }
        writeWav (argv[2], renderSequence (sr, 512, step * 34, ev, EngineParams {}), sr);
        std::printf ("wrote %s\n", argv[2]);
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
