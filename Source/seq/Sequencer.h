#pragma once

#include "Pattern.h"

#include <array>
#include <cstdint>

namespace bleep::seq
{

/** What the host told us about the transport for this block. */
struct Transport
{
    bool   hasInfo   = false;  // host gave any position info at all
    bool   isPlaying = false;
    bool   hasPpq    = false;
    double ppq       = 0.0;    // quarter notes since song start
    bool   hasBpm    = false;
    double bpm       = 120.0;
};

struct Settings
{
    bool   enabled  = false;
    int    length   = 16;      // 1..kMaxSteps
    double stepPpq  = 0.25;    // 1/16 = 0.25 quarter notes
    int    rootNote = 60;
    float  gate     = 0.5f;    // fraction of a step, before per-step gate
};

struct NoteEvent
{
    int   sample;     // offset inside the block
    int   note;
    float velocity;   // 0 => note off
};

/** Host-synchronised step sequencer.

    The step to play is derived from the host's absolute PPQ position every
    block (step k starts at k * stepPpq), so starting mid-song, loop jumps,
    timeline jumps and tempo changes need no extra state: we only remember
    the last step index we triggered, to avoid double or missed triggers
    across block boundaries. Any discontinuity releases sounding notes.

    Without host transport info (e.g. the standalone app) it free-runs at the
    host BPM or 120.

    Real-time safe: no allocation, fixed-size event output. */
class Sequencer
{
public:
    static constexpr int kMaxPending = 16;

    void prepare (double sampleRate) noexcept;
    void setPattern (const Pattern& p) noexcept { pattern = p; }
    void setSettings (const Settings& s) noexcept;
    void setSeed (std::uint32_t seed) noexcept { rng = seed ? seed : 1u; }

    /** Writes events (sorted by sample, note-offs before note-ons on the same
        sample) into `out`. Returns the number written. */
    int process (const Transport& t, int numSamples, NoteEvent* out, int maxEvents) noexcept;

    /** Step currently under the playhead, or -1 when not running. */
    int getCurrentStep() const noexcept { return currentStep; }
    bool isRunning() const noexcept { return running; }

private:
    struct Pending { double offPpq; int note; };

    void releaseAll (NoteEvent* out, int& count, int maxEvents) noexcept;
    void emit (NoteEvent* out, int& count, int maxEvents, NoteEvent e) noexcept;
    float nextRandom() noexcept;
    static std::int64_t firstStepAtOrAfter (double ppq, double stepPpq) noexcept;

    Pattern pattern = Pattern::makeDefault();
    Settings settings;
    double sampleRate = 44100.0;

    bool running = false;
    double expectedPpq = 0.0;     // where the next block should start
    double freeRunPpq = 0.0;
    double lastStepPpq = 0.25;
    std::int64_t lastTriggered = -1;
    int currentStep = -1;

    std::array<Pending, kMaxPending> pending {};
    int numPending = 0;

    std::uint32_t rng = 0x9e3779b9u;
};

} // namespace bleep::seq
