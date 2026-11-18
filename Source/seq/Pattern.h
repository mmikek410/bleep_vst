#pragma once

#include <array>
#include <string>

namespace bleep::seq
{

constexpr int kMaxSteps     = 32;   // storage; the UI exposes 16 for now
constexpr int kVisibleSteps = 16;
constexpr int kMinPitch     = -24;  // semitones relative to the root note
constexpr int kMaxPitch     = 24;

/** One sequencer step. Fields beyond `active`/`pitch` are stored and saved now
    so later milestones (velocity, probability, gate, ratchet, lock) need no
    state migration. Defaults make them neutral. */
struct Step
{
    bool  active      = false;
    int   pitch       = 0;      // semitones from root, kMinPitch..kMaxPitch
    float velocity    = 100.0f / 127.0f;  // 0..1 (MIDI 100; exact on the 0-127 grid)
    float probability = 1.0f;   // 0..1
    float gate        = 1.0f;   // multiplier of the global gate, 0..1
    int   ratchet     = 1;      // 1..4
    bool  locked      = false;

    bool operator== (const Step& o) const noexcept
    {
        // Exact comparison is intended (detects any edit); written without
        // float == to keep -Wfloat-equal quiet.
        auto same = [] (float a, float b) { return ! (a < b) && ! (b < a); };
        return active == o.active && pitch == o.pitch && same (velocity, o.velocity)
            && same (probability, o.probability) && same (gate, o.gate)
            && ratchet == o.ratchet && locked == o.locked;
    }
};

struct Pattern
{
    std::array<Step, kMaxSteps> steps {};

    bool operator== (const Pattern& o) const noexcept { return steps == o.steps; }
    bool operator!= (const Pattern& o) const noexcept { return ! (*this == o); }

    /** A sparse, syncopated starting point: playable straight away. */
    static Pattern makeDefault() noexcept;

    /** Compact text form for plugin state. Not real-time safe (allocates). */
    std::string encode() const;

    /** Returns false (and leaves *this untouched) if the text is malformed. */
    bool decode (const std::string& text);
};

} // namespace bleep::seq
