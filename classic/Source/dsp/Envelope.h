#pragma once

#include <algorithm>
#include <cmath>

namespace bleep::dsp
{

/** ADSR envelope tuned for short percussive sounds.

    - Attack is linear (fast, punchy, predictable).
    - Decay and release are exponential, which sounds natural for bleeps.
      A segment "time" is the time to travel 80 dB, so release ends exactly
      when its time has elapsed.
    - Retriggering starts the attack from the current level: no clicks.
    - With sustain at 0 the envelope goes idle at the end of the decay, so a
      held note frees its voice (important for sequenced bleeps).
*/
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    static constexpr float kMinTimeSeconds = 0.0005f;  // 0.5 ms: avoids clicks
    static constexpr float kEndLevel       = 1.0e-4f;  // -80 dB

    void setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        updateCoefficients();
    }

    void setParameters (float attackSec, float decaySec, float sustainLevel, float releaseSec) noexcept
    {
        attack  = std::max (attackSec,  kMinTimeSeconds);
        decay   = std::max (decaySec,   kMinTimeSeconds);
        sustain = std::clamp (sustainLevel, 0.0f, 1.0f);
        release = std::max (releaseSec, kMinTimeSeconds);
        updateCoefficients();
    }

    void noteOn() noexcept   { stage = Stage::Attack; }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    void reset() noexcept    { stage = Stage::Idle; level = 0.0f; }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

    float process() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                return 0.0f;

            case Stage::Attack:
                level += attackIncrement;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                level = sustain + (level - sustain) * decayCoefficient;
                if (level - sustain <= kEndLevel)
                {
                    level = sustain;
                    if (sustain <= kEndLevel) { reset(); return 0.0f; }
                    stage = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                // Follow sustain changes smoothly rather than jumping.
                level = sustain + (level - sustain) * decayCoefficient;
                break;

            case Stage::Release:
                level *= releaseCoefficient;
                if (level <= kEndLevel) { reset(); return 0.0f; }
                break;
        }
        return level;
    }

private:
    void updateCoefficients() noexcept
    {
        const auto sr = static_cast<float> (sampleRate);
        attackIncrement    = 1.0f / (attack * sr);
        decayCoefficient   = coefficientFor (decay, sr);
        releaseCoefficient = coefficientFor (release, sr);
    }

    static float coefficientFor (float seconds, float sr) noexcept
    {
        // Multiply by this every sample to fall 80 dB in `seconds`.
        return std::exp (std::log (kEndLevel) / (seconds * sr));
    }

    double sampleRate = 44100.0;
    float attack = 0.001f, decay = 0.18f, sustain = 0.0f, release = 0.12f;
    float attackIncrement = 0.0f, decayCoefficient = 0.0f, releaseCoefficient = 0.0f;
    float level = 0.0f;
    Stage stage = Stage::Idle;
};

} // namespace bleep::dsp
