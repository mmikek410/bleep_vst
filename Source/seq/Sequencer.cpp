#include "Sequencer.h"

#include <algorithm>
#include <cmath>

namespace bleep::seq
{

namespace
{
    constexpr double kJumpTolerancePpq = 0.01;  // ~5 ms at 120 BPM
    constexpr double kEpsilon = 1.0e-7;

    int wrap (std::int64_t k, int length) noexcept
    {
        const auto m = k % length;
        return static_cast<int> (m < 0 ? m + length : m);
    }
}

void Sequencer::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    running = false;
    numPending = 0;
    currentStep = -1;
    lastTriggered = -1;
}

void Sequencer::setSettings (const Settings& s) noexcept
{
    settings = s;
    settings.length   = std::clamp (s.length, 1, kMaxSteps);
    settings.stepPpq  = s.stepPpq > 0.0 ? s.stepPpq : 0.25;
    settings.rootNote = std::clamp (s.rootNote, 0, 127);
    settings.gate     = std::clamp (s.gate, 0.01f, 1.0f);
}

std::int64_t Sequencer::firstStepAtOrAfter (double ppq, double stepPpq) noexcept
{
    return static_cast<std::int64_t> (std::ceil (ppq / stepPpq - kEpsilon));
}

float Sequencer::nextRandom() noexcept
{
    // xorshift32: deterministic for a given seed, which keeps tests reproducible.
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float> (rng >> 8) * (1.0f / 16777216.0f);
}

void Sequencer::emit (NoteEvent* out, int& count, int maxEvents, NoteEvent e) noexcept
{
    if (count < maxEvents)
        out[count++] = e;
}

void Sequencer::releaseAll (NoteEvent* out, int& count, int maxEvents) noexcept
{
    for (int i = 0; i < numPending; ++i)
        emit (out, count, maxEvents, { 0, pending[(size_t) i].note, 0.0f });
    numPending = 0;
}

int Sequencer::process (const Transport& t, int numSamples, NoteEvent* out, int maxEvents) noexcept
{
    int count = 0;
    if (numSamples <= 0)
        return 0;

    const bool freeRunning = ! t.hasInfo || ! t.hasPpq;
    const bool playing = settings.enabled && (t.hasInfo ? t.isPlaying : true);

    if (! playing)
    {
        releaseAll (out, count, maxEvents);
        running = false;
        currentStep = -1;
        return count;
    }

    const double bpm = (t.hasBpm && t.bpm > 0.0) ? std::clamp (t.bpm, 20.0, 999.0) : 120.0;
    const double ppqPerSample = bpm / (60.0 * sampleRate);
    const double stepPpq = settings.stepPpq;
    const double ppqStart = freeRunning ? freeRunPpq : t.ppq;
    const double ppqEnd = ppqStart + numSamples * ppqPerSample;

    const bool jumped = std::abs (ppqStart - expectedPpq) > kJumpTolerancePpq;
    const bool rateChanged = stepPpq != lastStepPpq;

    if (! running || jumped || rateChanged)
    {
        // (Re)start: silence anything sounding, then play from the first
        // step boundary at or after the current position.
        if (! running || jumped)
            releaseAll (out, count, maxEvents);
        lastTriggered = firstStepAtOrAfter (ppqStart, stepPpq) - 1;
        running = true;
    }
    lastStepPpq = stepPpq;

    auto toSample = [&] (double ppq) noexcept
    {
        const auto s = static_cast<int> (std::lround ((ppq - ppqStart) / ppqPerSample));
        return std::clamp (s, 0, numSamples - 1);
    };

    // A time belongs to this block if it rounds to a sample inside it; times
    // rounding to `numSamples` or later are handled at sample 0 of the next block.
    auto rawSample = [&] (double ppq) noexcept
    {
        return std::lround ((ppq - ppqStart) / ppqPerSample);
    };

    // Emit pending note-offs due at or before `limitSample`.
    auto flushOffs = [&] (long limitSample) noexcept
    {
        int kept = 0;
        for (int i = 0; i < numPending; ++i)
        {
            const auto p = pending[(size_t) i];
            if (rawSample (p.offPpq) <= limitSample)
                emit (out, count, maxEvents, { toSample (p.offPpq), p.note, 0.0f });
            else
                pending[(size_t) kept++] = p;
        }
        numPending = kept;
    };

    for (std::int64_t k = lastTriggered + 1; ; ++k)
    {
        const double stepStart = static_cast<double> (k) * stepPpq;
        if (rawSample (stepStart) >= numSamples)
            break;

        const int onSample = toSample (stepStart);
        flushOffs (onSample);
        lastTriggered = k;

        const int index = wrap (k, settings.length);
        const Step& step = pattern.steps[(size_t) index];
        if (! step.active)
            continue;
        if (step.probability < 1.0f && nextRandom() >= step.probability)
            continue;

        const int note = std::clamp (settings.rootNote + step.pitch, 0, 127);
        const float gate = std::clamp (settings.gate * step.gate, 0.01f, 1.0f);

        // Same note still sounding (shouldn't happen with gate <= 1): end it first.
        for (int i = 0; i < numPending; ++i)
            if (pending[(size_t) i].note == note)
            {
                emit (out, count, maxEvents, { onSample, note, 0.0f });
                pending[(size_t) i] = pending[(size_t) --numPending];
                break;
            }

        emit (out, count, maxEvents, { onSample, note, std::max (step.velocity, 0.01f) });
        if (numPending < kMaxPending)
            pending[(size_t) numPending++] = { stepStart + stepPpq * gate, note };
    }

    flushOffs (numSamples - 1);

    // Guarantee ordering: by sample, note-offs before note-ons. (Tiny n: insertion sort.)
    for (int i = 1; i < count; ++i)
    {
        const NoteEvent e = out[i];
        auto before = [] (const NoteEvent& a, const NoteEvent& b)
        {
            return a.sample < b.sample || (a.sample == b.sample && a.velocity <= 0.0f && b.velocity > 0.0f);
        };
        int j = i - 1;
        while (j >= 0 && before (e, out[j]))
        {
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = e;
    }

    expectedPpq = ppqEnd;
    if (freeRunning)
        freeRunPpq = ppqEnd;

    const auto stepUnderHead = static_cast<std::int64_t> (std::floor (ppqEnd / stepPpq - kEpsilon));
    currentStep = wrap (stepUnderHead, settings.length);
    return count;
}

} // namespace bleep::seq
