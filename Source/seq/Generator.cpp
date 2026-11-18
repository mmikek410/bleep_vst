#include "Generator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace bleep::seq
{

namespace
{
    float quantiseVelocity (float v) { return std::round (std::clamp (v, 0.05f, 1.0f) * 127.0f) / 127.0f; }
    float quantisePercent (float p)  { return std::round (std::clamp (p, 0.0f, 1.0f) * 100.0f) / 100.0f; }

    /** Rhythmic weight of a 16th position inside a beat. Off-beats ("and") and
        anticipations ("a") score high so bleeps sit around the kick rather than
        on it; the "e" is the rarest, most syncopated spot. */
    float metricWeight (int step)
    {
        switch (step % 4)
        {
            case 0:  return 0.80f;   // on the beat
            case 2:  return 1.00f;   // off-beat
            case 3:  return 0.85f;   // anticipation
            default: return 0.55f;   // "e"
        }
    }

    int pickMotifLength (Rng& rng, int numSteps)
    {
        // length, weight
        const int lengths[] = { 3, 4, 6, 8, 16 };
        const float weights[] = { 0.14f, 0.18f, 0.10f, 0.33f, 0.25f };
        float r = rng.uniform() * std::accumulate (std::begin (weights), std::end (weights), 0.0f);
        for (size_t i = 0; i < 5; ++i)
        {
            if (r < weights[i])
                return std::min (lengths[i], numSteps);
            r -= weights[i];
        }
        return std::min (8, numSteps);
    }

    int pickPitch (Rng& rng, const std::vector<int>& pool)
    {
        // Root heavily favoured, then octaves, then small intervals.
        std::vector<float> w (pool.size());
        for (size_t i = 0; i < pool.size(); ++i)
        {
            const int p = pool[i];
            w[i] = p == 0 ? 3.0f : (std::abs (p) % 12 == 0 ? 1.4f : 1.0f / (1.0f + std::abs (p) / 7.0f));
        }
        float r = rng.uniform() * std::accumulate (w.begin(), w.end(), 0.0f);
        for (size_t i = 0; i < pool.size(); ++i)
        {
            if (r < w[i]) return pool[i];
            r -= w[i];
        }
        return 0;
    }

    /** Nearest pool neighbour `distance` places up or down from `pitch`. */
    int neighbourPitch (Rng& rng, const std::vector<int>& pool, int pitch, int distance)
    {
        auto it = std::lower_bound (pool.begin(), pool.end(), pitch);
        auto index = static_cast<int> (it - pool.begin());
        if (it == pool.end() || *it != pitch)
            index = std::clamp (index, 0, (int) pool.size() - 1);
        const int dir = rng.chance (0.5f) ? 1 : -1;
        int target = index + dir * distance;
        if (target < 0 || target >= (int) pool.size())
            target = index - dir * distance;
        return pool[(size_t) std::clamp (target, 0, (int) pool.size() - 1)];
    }

    std::vector<int> unlockedSteps (const Pattern& p, int numSteps)
    {
        std::vector<int> r;
        for (int i = 0; i < numSteps; ++i)
            if (! p.steps[(size_t) i].locked)
                r.push_back (i);
        return r;
    }
}

std::vector<int> pitchPool (int pitchRange)
{
    pitchRange = std::clamp (pitchRange, 0, kMaxPitch);
    const int degrees[] = { 0, 3, 5, 7, 10 };
    std::vector<int> pool;
    for (int octave = -2; octave <= 2; ++octave)
        for (int d : degrees)
        {
            const int p = octave * 12 + d;
            if (std::abs (p) <= pitchRange)
                pool.push_back (p);
        }
    std::sort (pool.begin(), pool.end());
    pool.erase (std::unique (pool.begin(), pool.end()), pool.end());
    return pool;  // always contains 0
}

Pattern generate (const Pattern& current, const GeneratorSettings& settings, std::uint32_t seed, int numSteps)
{
    numSteps = std::clamp (numSteps, 1, kMaxSteps);
    Rng rng (seed);
    Pattern out = current;
    const auto pool = pitchPool (settings.pitchRange);
    const auto free = unlockedSteps (current, numSteps);

    int lockedActive = 0;
    for (int i = 0; i < numSteps; ++i)
        lockedActive += (current.steps[(size_t) i].locked && current.steps[(size_t) i].active) ? 1 : 0;

    const int wanted = (int) std::lround (std::clamp (settings.density, 0.0f, 1.0f) * (float) numSteps);
    const int target = std::clamp (wanted - lockedActive, 0, (int) free.size());

    // Motif: per-position rhythm preference, pitch and velocity, repeated across the bar.
    const int motifLength = pickMotifLength (rng, numSteps);
    std::vector<float> motifRhythm ((size_t) motifLength), motifVelocity ((size_t) motifLength);
    std::vector<int> motifPitch ((size_t) motifLength);
    for (int j = 0; j < motifLength; ++j)
    {
        motifRhythm[(size_t) j]   = rng.uniform();
        motifPitch[(size_t) j]    = pickPitch (rng, pool);
        motifVelocity[(size_t) j] = 0.55f + 0.35f * rng.uniform();
    }

    // Score each free step; the `target` best become active.
    std::vector<std::pair<float, int>> scored;
    for (int i : free)
    {
        const float score = metricWeight (i) * (0.35f + 0.65f * motifRhythm[(size_t) (i % motifLength)])
                          + 0.08f * rng.uniform();
        scored.push_back ({ score, i });
    }
    std::sort (scored.begin(), scored.end(), [] (auto& a, auto& b) { return a.first > b.first; });

    const int anchor = scored.empty() || target == 0 ? -1 : scored.front().second;

    for (size_t rank = 0; rank < scored.size(); ++rank)
    {
        const int i = scored[rank].second;
        auto& s = out.steps[(size_t) i];
        const int m = i % motifLength;

        s = Step {};
        s.active = (int) rank < target;

        // Pitch: repeat the motif; later repeats sometimes vary (organic, not chaotic).
        s.pitch = motifPitch[(size_t) m];
        if (i >= motifLength && rng.chance (0.12f))
            s.pitch = pickPitch (rng, pool);

        const float accent = (i % 4 == 0) ? 0.08f : (i % 4 == 2 ? 0.04f : 0.0f);
        s.velocity = quantiseVelocity (motifVelocity[(size_t) m] + accent + 0.1f * (rng.uniform() - 0.5f));

        // A few steps become probabilistic so the loop breathes; the anchor never does.
        if (s.active && i != anchor && rng.chance (0.22f))
        {
            const float choices[] = { 0.5f, 0.6f, 0.7f, 0.8f };
            s.probability = quantisePercent (choices[rng.between (0, 3)]);
        }
    }
    return out;
}

Pattern mutate (const Pattern& current, float amount, const GeneratorSettings& settings,
                std::uint32_t seed, int numSteps)
{
    amount = std::clamp (amount, 0.0f, 1.0f);
    numSteps = std::clamp (numSteps, 1, kMaxSteps);
    if (amount <= 0.0f)
        return current;

    Rng rng (seed);
    Pattern out = current;
    const auto pool = pitchPool (std::max (settings.pitchRange, 0));
    const auto free = unlockedSteps (current, numSteps);
    if (free.empty())
        return out;

    auto activeFree = [&] {
        std::vector<int> r;
        for (int i : free) if (out.steps[(size_t) i].active) r.push_back (i);
        return r;
    };
    auto inactiveFree = [&] {
        std::vector<int> r;
        for (int i : free) if (! out.steps[(size_t) i].active) r.push_back (i);
        return r;
    };

    // Number of edits grows with amount: 0.1 -> 1, 0.25 -> 2, 0.5 -> 4, 1.0 -> 8 (for 16 steps).
    const int edits = std::max (1, (int) std::lround (amount * (float) numSteps * 0.5f));
    const float rhythmWeight = 0.1f + 0.3f * amount;

    for (int e = 0; e < edits; ++e)
    {
        const float r = rng.uniform() * (0.4f + 0.2f + 0.15f + rhythmWeight);
        auto active = activeFree();

        if (r < 0.4f && ! active.empty())                        // pitch
        {
            auto& s = out.steps[(size_t) active[(size_t) rng.between (0, (int) active.size() - 1)]];
            s.pitch = neighbourPitch (rng, pool, s.pitch, amount > 0.6f && rng.chance (0.5f) ? 2 : 1);
        }
        else if (r < 0.6f && ! active.empty())                   // velocity
        {
            auto& s = out.steps[(size_t) active[(size_t) rng.between (0, (int) active.size() - 1)]];
            const float delta = (0.08f + 0.2f * amount) * (rng.chance (0.5f) ? 1.0f : -1.0f);
            s.velocity = quantiseVelocity (std::clamp (s.velocity + delta, 0.3f, 1.0f));
        }
        else if (r < 0.75f && ! active.empty())                  // probability
        {
            auto& s = out.steps[(size_t) active[(size_t) rng.between (0, (int) active.size() - 1)]];
            s.probability = s.probability < 1.0f ? 1.0f : quantisePercent (0.5f + 0.1f * (float) rng.between (0, 3));
        }
        else                                                     // rhythm
        {
            auto inactive = inactiveFree();
            if (! active.empty() && ! inactive.empty() && rng.chance (0.6f))
            {
                // Nudge a hit to a free neighbouring step: same density, shifted feel.
                const int from = active[(size_t) rng.between (0, (int) active.size() - 1)];
                const int dir = rng.chance (0.5f) ? 1 : -1;
                for (int d : { dir, -dir })
                {
                    const int to = (from + d + numSteps) % numSteps;
                    auto& dst = out.steps[(size_t) to];
                    if (! dst.locked && ! dst.active)
                    {
                        dst = out.steps[(size_t) from];
                        out.steps[(size_t) from].active = false;
                        break;
                    }
                }
            }
            else if (active.size() > 1 && (inactive.empty() || rng.chance (0.5f)))
            {
                out.steps[(size_t) active[(size_t) rng.between (0, (int) active.size() - 1)]].active = false;
            }
            else if (! inactive.empty())
            {
                auto& s = out.steps[(size_t) inactive[(size_t) rng.between (0, (int) inactive.size() - 1)]];
                s.active = true;
                if (! std::binary_search (pool.begin(), pool.end(), s.pitch))
                    s.pitch = 0;
            }
        }
    }
    return out;
}

} // namespace bleep::seq
