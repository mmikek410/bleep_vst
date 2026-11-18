// JUCE-free tests for the BLEEP / 001 step sequencer, driven by a simulated host.

#include "seq/Sequencer.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

using namespace bleep::seq;

static int failures = 0;
#define CHECK(cond, ...)                                        \
    do {                                                        \
        if (! (cond)) {                                         \
            ++failures;                                         \
            std::printf ("  FAIL %s:%d: ", __FILE__, __LINE__);  \
            std::printf (__VA_ARGS__);                          \
            std::printf ("\n");                                 \
        }                                                       \
    } while (0)

struct AbsEvent { double sample; int note; bool on; };

/** Simulated host: advances PPQ per block exactly like a DAW does. */
struct Host
{
    double sr = 48000.0, bpm = 120.0, ppq = 0.0;
    bool playing = true, hasInfo = true, hasPpq = true;
    double absSample = 0.0;
    double ppqJitter = 0.0;  // tiny rounding noise some hosts produce

    Transport transport() const
    {
        Transport t;
        t.hasInfo = hasInfo; t.isPlaying = playing; t.hasPpq = hasPpq;
        t.ppq = ppq + ppqJitter; t.hasBpm = true; t.bpm = bpm;
        return t;
    }

    void run (Sequencer& s, int blockSize, int numSamples, std::vector<AbsEvent>& log)
    {
        NoteEvent ev[256];
        for (int done = 0; done < numSamples;)
        {
            const int n = std::min (blockSize, numSamples - done);
            const int c = s.process (transport(), n, ev, 256);
            for (int i = 0; i < c; ++i)
            {
                if (i > 0)
                    CHECK (ev[i].sample >= ev[i - 1].sample, "events not sorted");
                CHECK (ev[i].sample >= 0 && ev[i].sample < n, "event outside block");
                log.push_back ({ absSample + ev[i].sample, ev[i].note, ev[i].velocity > 0.0f });
            }
            if (playing) ppq += n * bpm / (60.0 * sr);
            absSample += n;
            done += n;
        }
    }
};

static Pattern allOn()
{
    Pattern p;
    for (int i = 0; i < kMaxSteps; ++i) { p.steps[(size_t) i].active = true; p.steps[(size_t) i].pitch = i % 12; }
    return p;
}

static Sequencer makeSeq (double sr, const Pattern& p, int length = 16, double stepPpq = 0.25, float gate = 0.5f)
{
    Sequencer s;
    s.prepare (sr);
    s.setPattern (p);
    Settings st; st.enabled = true; st.length = length; st.stepPpq = stepPpq; st.rootNote = 60; st.gate = gate;
    s.setSettings (st);
    return s;
}

static std::vector<AbsEvent> ons (const std::vector<AbsEvent>& log)
{
    std::vector<AbsEvent> r;
    for (auto& e : log) if (e.on) r.push_back (e);
    return r;
}

// Every note-on must be matched by a note-off, never two ons for one note without an off.
static bool balanced (const std::vector<AbsEvent>& log)
{
    std::map<int, int> held;
    for (auto& e : log)
    {
        int& h = held[e.note];
        if (e.on) { if (h != 0) return false; h = 1; }
        else h = 0;
    }
    for (auto& [n, h] : held) if (h != 0) return false;
    return true;
}

int main()
{
    std::printf ("timing: steps land on exact samples at every block size\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (double bpm : { 120.0, 128.0, 133.0 })
            for (int block : { 1, 17, 64, 511, 4096 })
            {
                Host h; h.sr = sr; h.bpm = bpm;
                auto s = makeSeq (sr, allOn());
                std::vector<AbsEvent> log;
                const double stepSamples = 0.25 * 60.0 / bpm * sr;
                h.run (s, block, (int) (stepSamples * 32 + 10), log);
                auto on = ons (log);
                CHECK (on.size() == 33, "sr=%.0f bpm=%.0f block=%d: %zu ons", sr, bpm, block, on.size());
                for (size_t k = 0; k < on.size(); ++k)
                {
                    CHECK (std::abs (on[k].sample - k * stepSamples) <= 0.51, "step %zu at %.1f, expected %.1f", k, on[k].sample, k * stepSamples);
                    CHECK (on[k].note == 60 + (int) (k % 16) % 12, "wrong pitch at step %zu", k);
                }
                // Gate 50%: each note-off half a step after its note-on.
                for (auto& e : log)
                    if (! e.on)
                    {
                        const double rel = std::fmod (e.sample, stepSamples);
                        CHECK (std::abs (rel - stepSamples * 0.5) <= 0.51, "note-off off-grid at %.1f", e.sample);
                        break;
                    }
            }

    std::printf ("start mid-song\n");
    {
        Pattern p; p.steps[6].active = true; p.steps[6].pitch = 5;
        Host h; h.ppq = 37.3;
        auto s = makeSeq (h.sr, p);
        std::vector<AbsEvent> log;
        h.run (s, 256, 48000, log);
        auto on = ons (log);
        CHECK (! on.empty() && on[0].note == 65, "first note should be step 6");
        // step 150 starts at ppq 37.5 -> 0.2 ppq = 0.1 s after start
        CHECK (! on.empty() && std::abs (on[0].sample - 4800.0) <= 0.51, "first note at %.1f", on.empty() ? -1.0 : on[0].sample);
        CHECK (s.getCurrentStep() >= 0, "playhead not reported");
    }

    std::printf ("loop / jump back releases notes and retriggers\n");
    {
        Host h;
        auto s = makeSeq (h.sr, allOn(), 16, 0.25, 0.9f);
        std::vector<AbsEvent> log;
        h.run (s, 512, 93000, log);         // ~1.9 bars, mid-note
        h.ppq = 0.0;                        // loop back to bar 1
        size_t before = log.size();
        h.run (s, 512, 12000, log);
        CHECK (log.size() > before && ! log[before].on, "jump must start with a note-off");
        bool step0 = false;
        for (size_t i = before; i < log.size(); ++i) if (log[i].on && log[i].note == 60) { step0 = true; break; }
        CHECK (step0, "step 0 not retriggered after loop");
        h.playing = false;
        h.run (s, 512, 512, log);
        CHECK (balanced (log), "stuck or doubled notes after loop");
    }

    std::printf ("tempo change\n");
    {
        Host h;
        auto s = makeSeq (h.sr, allOn());
        std::vector<AbsEvent> log;
        h.run (s, 480, 96000, log);          // 1 bar at 120 = 2 s = 96000 samples
        h.bpm = 140.0;
        const double t0 = h.absSample;
        h.run (s, 480, 96000, log);
        auto on = ons (log);
        CHECK (on.size() > 16 && std::abs (on[16].sample - 96000.0) <= 0.51, "bar 2 downbeat misplaced");
        const double step140 = 0.25 * 60.0 / 140.0 * 48000.0;
        CHECK (on.size() > 17 && std::abs ((on[17].sample - t0) - step140) <= 0.51 + 1.0, "new tempo spacing wrong");
    }

    std::printf ("transport stop and restart\n");
    {
        Host h;
        auto s = makeSeq (h.sr, allOn(), 16, 0.25, 1.0f);
        std::vector<AbsEvent> log;
        h.run (s, 512, 20000, log);
        h.playing = false;
        size_t before = log.size();
        h.run (s, 512, 20000, log);
        CHECK (ons (std::vector<AbsEvent> (log.begin() + (long) before, log.end())).empty(), "notes while stopped");
        CHECK (s.getCurrentStep() == -1, "playhead visible while stopped");
        CHECK (balanced (log), "stop left a stuck note");
        h.playing = true; h.ppq = 8.0;
        before = log.size();
        h.run (s, 512, 6000, log);
        CHECK (log.size() > before && log[before].on && log[before].note == 60, "restart at bar 3 should hit step 0");
    }

    std::printf ("no transport info: free running\n");
    {
        Host h; h.hasInfo = false;
        auto s = makeSeq (h.sr, allOn());
        std::vector<AbsEvent> log;
        h.run (s, 333, 6000 * 8 + 1, log);
        CHECK (ons (log).size() == 9, "free-run count %zu", ons (log).size());
    }

    std::printf ("length, inactive steps, divisions\n");
    {
        Pattern p; p.steps[0].active = true; p.steps[2].active = true; p.steps[2].pitch = 2;
        Host h;
        auto s = makeSeq (h.sr, p, 3);       // 3-step loop against 4/4: polymetric
        std::vector<AbsEvent> log;
        h.run (s, 256, 6000 * 9, log);
        auto on = ons (log);
        CHECK (on.size() == 6, "length 3: %zu ons", on.size());
        for (double div : { 1.0, 0.5, 0.25, 0.125 })
        {
            Host h2;
            auto s2 = makeSeq (h2.sr, allOn(), 16, div);
            std::vector<AbsEvent> l2;
            h2.run (s2, 512, 96000, l2);    // one bar
            CHECK ((int) ons (l2).size() == (int) std::lround (4.0 / div), "division %.3f: %zu ons", div, ons (l2).size());
        }
    }

    std::printf ("probability is deterministic per seed\n");
    {
        Pattern p = allOn();
        for (auto& st : p.steps) st.probability = 0.5f;
        auto count = [&] (std::uint32_t seed)
        {
            Host h;
            auto s = makeSeq (h.sr, p, 16, 0.125);
            s.setSeed (seed);
            std::vector<AbsEvent> log;
            h.run (s, 512, 3000 * 1000, log);
            return ons (log).size();
        };
        const auto a = count (42), b = count (42), c = count (7);
        CHECK (a == b, "same seed differs");
        CHECK (a > 400 && a < 600, "probability 0.5 gave %zu/1000", a);
        CHECK (a != c, "different seeds identical (unlikely)");
    }

    std::printf ("host rounding jitter does not double-trigger\n");
    {
        Host h;
        auto s = makeSeq (h.sr, allOn());
        std::vector<AbsEvent> log;
        for (int i = 0; i < 200; ++i)
        {
            h.ppqJitter = (i % 2 ? 1 : -1) * 1.0e-9;
            h.run (s, 6000, 6000, log);     // block boundaries exactly on step boundaries
        }
        CHECK (ons (log).size() == 200, "jitter: %zu ons for 200 steps", ons (log).size());
    }

    std::printf ("disable releases notes\n");
    {
        Host h;
        auto s = makeSeq (h.sr, allOn(), 16, 0.25, 1.0f);
        std::vector<AbsEvent> log;
        h.run (s, 512, 3000, log);
        Settings st; st.enabled = false;
        s.setSettings (st);
        h.run (s, 512, 3000, log);
        CHECK (balanced (log), "disable left a stuck note");
    }

    std::printf ("pattern encode / decode\n");
    {
        Pattern p = Pattern::makeDefault();
        p.steps[5] = { true, -24, 64.0f / 127.0f, 0.25f, 0.75f, 3, true };
        Pattern q;
        CHECK (q.decode (p.encode()), "decode failed");
        CHECK (q == p, "round trip mismatch");
        Pattern r = Pattern::makeDefault();
        CHECK (! r.decode ("garbage"), "garbage accepted");
        CHECK (r == Pattern::makeDefault(), "failed decode modified pattern");
        CHECK (r.decode ("P1;1,99"), "short record rejected");
        CHECK (r.steps[0].pitch == kMaxPitch && r.steps[0].active, "pitch not clamped");
    }

    std::printf (failures == 0 ? "ALL SEQUENCER TESTS PASSED\n" : "%d SEQUENCER FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
