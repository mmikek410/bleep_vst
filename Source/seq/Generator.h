#pragma once

#include "Pattern.h"

#include <cstdint>
#include <deque>
#include <random>
#include <vector>

namespace bleep::seq
{

struct GeneratorSettings
{
    float density    = 0.4f;  // 0..1: share of the steps that are active
    int   pitchRange = 12;    // semitones either side of the root (0 = root only)
};

/** Small deterministic RNG wrapper: same seed => same pattern (tests, recall). */
class Rng
{
public:
    explicit Rng (std::uint32_t seed) : engine (seed) {}
    float uniform() { return std::uniform_real_distribution<float> (0.0f, 1.0f) (engine); }
    int   between (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (engine); }
    bool  chance (float p) { return uniform() < p; }
    std::uint32_t next() { return static_cast<std::uint32_t> (engine()); }

private:
    std::mt19937 engine;
};

/** Pitch offsets the generator may use, within +/- range: minor-pentatonic
    degrees (0, 3, 5, 7, 10) in every octave. Keeps random lines coherent
    until the SCALE control arrives (Milestone 4). Always contains 0. */
std::vector<int> pitchPool (int pitchRange);

/** RANDOM: a new pattern for the first `numSteps` steps.

    Built for hypnotic techno rather than uniform noise:
    - a short motif (3, 4, 6, 8 or 16 steps) is generated and repeated, so the
      line loops and odd motif lengths give syncopated, polymetric phrasing;
    - step positions are weighted (off-beats and anticipations favoured);
    - exactly round(density * numSteps) steps are active (including locked ones);
    - pitches favour the root and small intervals; repeats vary occasionally;
    - velocities carry a light accent; some steps get probability < 100 %.
    Locked steps are never touched. Steps beyond `numSteps` are left as they are. */
Pattern generate (const Pattern& current, const GeneratorSettings& settings,
                  std::uint32_t seed, int numSteps = kVisibleSteps);

/** MUTATE: a variation of `current` that keeps its groove.
    `amount` 0..1. 0 changes nothing; low amounts make one or two small edits
    (mostly pitch/velocity); higher amounts make more edits and allow rhythm
    moves. Density is roughly preserved. Locked steps are never touched. */
Pattern mutate (const Pattern& current, float amount, const GeneratorSettings& settings,
                std::uint32_t seed, int numSteps = kVisibleSteps);

/** Bounded undo/redo history. Message-thread only; never used on audio. */
template <typename T>
class History
{
public:
    explicit History (size_t maxEntries = 64) : capacity (maxEntries) {}

    /** Call with the state *before* a change. Clears redo. */
    void push (const T& before)
    {
        undoStack.push_back (before);
        if (undoStack.size() > capacity)
            undoStack.pop_front();
        redoStack.clear();
    }

    /** Swaps `current` with the previous state. Returns false if nothing to undo. */
    bool undo (T& current)
    {
        if (undoStack.empty()) return false;
        redoStack.push_back (current);
        current = undoStack.back();
        undoStack.pop_back();
        return true;
    }

    bool redo (T& current)
    {
        if (redoStack.empty()) return false;
        undoStack.push_back (current);
        current = redoStack.back();
        redoStack.pop_back();
        return true;
    }

    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    size_t size() const noexcept  { return undoStack.size(); }
    void clear() { undoStack.clear(); redoStack.clear(); }

private:
    size_t capacity;
    std::deque<T> undoStack, redoStack;
};

} // namespace bleep::seq
