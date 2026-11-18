#pragma once

#include "Smoother.h"
#include "Voice.h"

#include <array>
#include <cstdint>

namespace bleep::dsp
{

/** Снимок параметров, который плагин передаёт каждый блок.
    Звуковые параметры голоса + выходная громкость. */
struct EngineParams : VoiceParams
{
    float outputGainDb = -6.0f;
};

/** Polyphonic engine. Knows nothing about JUCE or MIDI byte formats:
    the plugin (and later the internal sequencer) feed it note events.
    No allocation after prepare(); safe to call from the audio thread. */
class SynthEngine
{
public:
    static constexpr int kNumVoices = 8;

    void prepare (double sampleRate);
    void setParams (const EngineParams& p) noexcept;

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;  // release everything
    void reset() noexcept;        // hard stop (transport jumps, non-finite output)

    /** Overwrites `out` with numSamples of mono audio. */
    void render (float* out, int numSamples) noexcept;

    int getNumActiveVoices() const noexcept;

private:
    Voice* findVoiceFor (int note) noexcept;

    std::array<Voice, kNumVoices> voices;
    EngineParams params;
    Smoother outputGain;
    double sampleRate = 44100.0;
    std::uint64_t noteCounter = 0;
};

} // namespace bleep::dsp
