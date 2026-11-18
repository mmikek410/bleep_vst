#pragma once

#include "Envelope.h"

#include <cmath>
#include <cstdint>

namespace bleep::dsp
{

/** One synth voice. Phase 1: sine oscillator into an ADSR.
    Phase 2 will add FM, pitch envelope and filter here. */
class Voice
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        env.setSampleRate (newSampleRate);
        env.reset();
        phase = 0.0;
    }

    void setEnvelope (float a, float d, float s, float r) noexcept { env.setParameters (a, d, s, r); }

    /** Start or retrigger. Phase is kept on retrigger so there's no step in the waveform. */
    void start (int midiNote, float velocity, std::uint64_t startOrder) noexcept
    {
        if (! env.isActive())
            phase = 0.0;

        note  = midiNote;
        gain  = velocityToGain (velocity);
        order = startOrder;
        held  = true;
        env.noteOn();
    }

    void release() noexcept { held = false; env.noteOff(); }
    void kill() noexcept    { held = false; env.reset(); note = -1; }

    bool isActive() const noexcept  { return env.isActive(); }
    bool isHeld() const noexcept    { return held; }
    int getNote() const noexcept    { return note; }
    std::uint64_t getOrder() const noexcept { return order; }
    float getLevel() const noexcept { return env.getLevel(); }
    bool isReleasing() const noexcept { return env.getStage() == Envelope::Stage::Release; }

    /** Adds into `out`. tuningSemitones is the global fine tune. */
    void render (float* out, int numSamples, float tuningSemitones) noexcept
    {
        if (! env.isActive())
            return;

        const double freq = 440.0 * std::pow (2.0, (note - 69 + tuningSemitones) / 12.0);
        const double increment = freq / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            const float e = env.process();
            out[i] += static_cast<float> (std::sin (kTwoPi * phase)) * e * gain;

            phase += increment;
            if (phase >= 1.0)
                phase -= 1.0;

            if (! env.isActive())
            {
                note = -1;
                break;
            }
        }
    }

private:
    static constexpr double kTwoPi = 6.283185307179586476925286766559;

    static float velocityToGain (float velocity) noexcept
    {
        const float v = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
        return 0.15f + 0.85f * v * v;   // never fully silent, strong accent range
    }

    Envelope env;
    double sampleRate = 44100.0;
    double phase = 0.0;
    float gain = 0.0f;
    int note = -1;
    bool held = false;
    std::uint64_t order = 0;
};

} // namespace bleep::dsp
