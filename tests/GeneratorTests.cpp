// JUCE-free tests for RANDOM (generate), MUTATE and the undo history.

#include "seq/Generator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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

static int activeCount (const Pattern& p, int n = 16)
{
    int c = 0;
    for (int i = 0; i < n; ++i) c += p.steps[(size_t) i].active ? 1 : 0;
    return c;
}

static int rhythmDistance (const Pattern& a, const Pattern& b, int n = 16)
{
    int d = 0;
    for (int i = 0; i < n; ++i) d += a.steps[(size_t) i].active != b.steps[(size_t) i].active ? 1 : 0;
    return d;
}

static int stepsChanged (const Pattern& a, const Pattern& b, int n = 16)
{
    int d = 0;
    for (int i = 0; i < n; ++i) d += a.steps[(size_t) i] == b.steps[(size_t) i] ? 0 : 1;
    return d;
}

int main()
{
    std::printf ("pitch pool\n");
    {
        CHECK (pitchPool (0) == std::vector<int> { 0 }, "range 0 must be root only");
        for (int r = 0; r <= 24; ++r)
        {
            auto pool = pitchPool (r);
            CHECK (std::find (pool.begin(), pool.end(), 0) != pool.end(), "pool %d lacks root", r);
            for (int p : pool) CHECK (std::abs (p) <= r, "pool %d has %d", r, p);
            CHECK (std::is_sorted (pool.begin(), pool.end()), "pool not sorted");
        }
    }

    std::printf ("RANDOM: density, ranges, determinism\n");
    for (float density : { 0.0f, 0.1f, 0.25f, 0.4f, 0.6f, 0.8f, 1.0f })
        for (int range : { 0, 5, 12, 24 })
            for (std::uint32_t seed = 1; seed <= 60; ++seed)
            {
                GeneratorSettings gs { density, range };
                const auto p = generate (Pattern {}, gs, seed);
                const int expected = (int) std::lround (density * 16.0f);
                CHECK (activeCount (p) == expected, "density %.2f seed %u: %d active, want %d", density, seed, activeCount (p), expected);
                for (int i = 0; i < 16; ++i)
                {
                    const auto& s = p.steps[(size_t) i];
                    CHECK (std::abs (s.pitch) <= range, "pitch %d outside range %d", s.pitch, range);
                    CHECK (s.velocity >= 0.3f && s.velocity <= 1.0f, "velocity %.2f", s.velocity);
                    CHECK (s.probability >= 0.5f && s.probability <= 1.0f, "probability %.2f", s.probability);
                    CHECK (! s.locked, "generator must not lock steps");
                }
                CHECK (generate (Pattern {}, gs, seed) == p, "same seed must give same pattern");
            }

    std::printf ("RANDOM: variety across seeds, some probabilistic steps, repetition\n");
    {
        GeneratorSettings gs { 0.5f, 12 };
        int distinct = 0, withProbability = 0, halvesMatch = 0, rootHeavy = 0;
        Pattern previous;
        for (std::uint32_t seed = 1; seed <= 200; ++seed)
        {
            const auto p = generate (Pattern {}, gs, seed);
            distinct += p != previous ? 1 : 0;
            previous = p;
            bool anyProb = false;
            int roots = 0, actives = 0;
            for (int i = 0; i < 16; ++i)
            {
                const auto& s = p.steps[(size_t) i];
                anyProb |= s.active && s.probability < 1.0f;
                if (s.active) { ++actives; roots += s.pitch == 0 ? 1 : 0; }
            }
            withProbability += anyProb ? 1 : 0;
            rootHeavy += roots * 4 >= actives ? 1 : 0;
            // Hypnotic repetition: second half's rhythm mostly mirrors the first.
            int same = 0;
            for (int i = 0; i < 8; ++i) same += p.steps[(size_t) i].active == p.steps[(size_t) i + 8].active ? 1 : 0;
            halvesMatch += same >= 6 ? 1 : 0;
        }
        std::printf ("  distinct=%d/200 probabilistic=%d/200 repeating-halves=%d/200 root>=25%%=%d/200\n",
                     distinct, withProbability, halvesMatch, rootHeavy);
        CHECK (distinct >= 195, "patterns too similar across seeds");
        CHECK (withProbability > 60 && withProbability < 200, "probability variation off: %d", withProbability);
        CHECK (halvesMatch > 120, "not enough repetition: %d/200", halvesMatch);
        CHECK (rootHeavy > 120, "root not favoured: %d/200", rootHeavy);
    }

    std::printf ("RANDOM: locked steps are untouched, density counts them\n");
    for (std::uint32_t seed = 1; seed <= 100; ++seed)
    {
        Pattern base = Pattern::makeDefault();
        base.steps[0].locked = true;                       // active, locked
        base.steps[5] = { true, 7, 0.4f, 0.6f, 0.5f, 2, true };
        base.steps[9].locked = true;                       // inactive, locked
        base.steps[20].active = true;                      // beyond 16: must be left alone
        GeneratorSettings gs { 0.5f, 12 };
        const auto p = generate (base, gs, seed);
        CHECK (p.steps[0] == base.steps[0] && p.steps[5] == base.steps[5] && p.steps[9] == base.steps[9], "locked step changed");
        CHECK (p.steps[20] == base.steps[20], "step beyond length changed");
        CHECK (activeCount (p) == 8, "density with locks: %d", activeCount (p));
    }

    std::printf ("MUTATE: strength scales with amount, groove kept, locks kept\n");
    {
        GeneratorSettings gs { 0.5f, 12 };
        double distSmall = 0, distBig = 0, changedSmall = 0, changedBig = 0;
        for (std::uint32_t seed = 1; seed <= 200; ++seed)
        {
            auto base = generate (Pattern {}, gs, seed);
            base.steps[2].locked = true;
            base.steps[11].locked = true;

            CHECK (mutate (base, 0.0f, gs, seed) == base, "amount 0 must not change anything");

            const auto subtle = mutate (base, 0.15f, gs, seed + 1000);
            CHECK (rhythmDistance (base, subtle) <= 2, "subtle mutation moved %d steps", rhythmDistance (base, subtle));
            CHECK (std::abs (activeCount (subtle) - activeCount (base)) <= 1, "subtle mutation changed density");

            const auto quarter = mutate (base, 0.25f, gs, seed + 2000);
            CHECK (rhythmDistance (base, quarter) <= 4, "25%% mutation moved %d steps", rhythmDistance (base, quarter));

            const auto big = mutate (base, 1.0f, gs, seed + 3000);
            for (auto* m : { &subtle, &quarter, &big })
            {
                CHECK (m->steps[2] == base.steps[2] && m->steps[11] == base.steps[11], "mutation changed a locked step");
                for (int i = 0; i < 16; ++i)
                    CHECK (std::abs (m->steps[(size_t) i].pitch) <= 12, "mutation left pitch range");
            }
            CHECK (mutate (base, 0.5f, gs, 77) == mutate (base, 0.5f, gs, 77), "mutation not deterministic");

            distSmall += rhythmDistance (base, subtle);   distBig += rhythmDistance (base, big);
            changedSmall += stepsChanged (base, subtle);  changedBig += stepsChanged (base, big);
        }
        std::printf ("  avg rhythm moves: 15%%=%.2f 100%%=%.2f; avg steps changed: 15%%=%.2f 100%%=%.2f\n",
                     distSmall / 200, distBig / 200, changedSmall / 200, changedBig / 200);
        CHECK (changedSmall / 200 >= 0.7 && changedSmall / 200 <= 2.0, "subtle mutation should change ~1 step");
        CHECK (changedBig / 200 > 4.0, "full mutation should change many steps");
        CHECK (distBig > distSmall * 3, "rhythm changes should grow with amount");
    }

    std::printf ("MUTATE: everything locked => no change\n");
    {
        auto p = generate (Pattern {}, { 0.5f, 12 }, 9);
        for (auto& s : p.steps) s.locked = true;
        CHECK (mutate (p, 1.0f, { 0.5f, 12 }, 3) == p, "fully locked pattern changed");
        CHECK (generate (p, { 0.9f, 24 }, 3) == p, "fully locked pattern regenerated");
    }

    std::printf ("undo / redo history\n");
    {
        History<int> h (3);
        int current = 0;
        CHECK (! h.undo (current) && current == 0, "undo on empty");
        for (int v = 1; v <= 5; ++v) { h.push (current); current = v; }  // 0..4 pushed, cap 3 keeps 2,3,4
        CHECK (h.size() == 3, "capacity not enforced");
        CHECK (h.undo (current) && current == 4, "undo 1");
        CHECK (h.undo (current) && current == 3, "undo 2");
        CHECK (h.redo (current) && current == 4, "redo");
        h.push (current); current = 9;                                     // new action clears redo
        CHECK (! h.canRedo(), "redo not cleared by new action");
        CHECK (h.undo (current) && current == 4, "undo after new action");
    }

    std::printf (failures == 0 ? "ALL GENERATOR TESTS PASSED\n" : "%d GENERATOR FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
