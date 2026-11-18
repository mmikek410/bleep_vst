#include "SynthEngine.h"

#include <cmath>

namespace bleep::dsp
{

namespace
{
    float dbToGain (float db) noexcept { return db <= -100.0f ? 0.0f : std::pow (10.0f, db / 20.0f); }

    // Headroom so several overlapping voices don't clip the output.
    constexpr float kVoiceHeadroom = 0.35f;
}

void SynthEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    for (auto& v : voices)
    {
        v.prepare (sampleRate);
        v.setEnvelope (params.attack, params.decay, params.sustain, params.release);
    }

    outputGain.prepare (sampleRate, 0.02f);
    outputGain.snapTo (dbToGain (params.outputGainDb));
}

void SynthEngine::setParams (const EngineParams& p) noexcept
{
    params = p;
    for (auto& v : voices)
        v.setEnvelope (p.attack, p.decay, p.sustain, p.release);
    outputGain.setTarget (dbToGain (p.outputGainDb));
}

Voice* SynthEngine::findVoiceFor (int note) noexcept
{
    // 1. Same note already sounding: retrigger it (no doubled, phasey notes).
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note)
            return &v;

    // 2. A free voice.
    for (auto& v : voices)
        if (! v.isActive())
            return &v;

    // 3. Steal: prefer the quietest releasing voice, else the oldest one.
    Voice* best = nullptr;
    for (auto& v : voices)
        if (v.isReleasing() && (best == nullptr || v.getLevel() < best->getLevel()))
            best = &v;

    if (best != nullptr)
        return best;

    best = &voices[0];
    for (auto& v : voices)
        if (v.getOrder() < best->getOrder())
            best = &v;
    return best;
}

void SynthEngine::noteOn (int note, float velocity) noexcept
{
    if (note < 0 || note > 127)
        return;
    if (velocity <= 0.0f)
    {
        noteOff (note);
        return;
    }
    findVoiceFor (note)->start (note, velocity, ++noteCounter);
}

void SynthEngine::noteOff (int note) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.isHeld() && v.getNote() == note)
            v.release();
}

void SynthEngine::allNotesOff() noexcept
{
    for (auto& v : voices)
        v.release();
}

void SynthEngine::reset() noexcept
{
    for (auto& v : voices)
        v.kill();
}

void SynthEngine::render (float* out, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        out[i] = 0.0f;

    const float tuning = params.fineTune / 100.0f;
    for (auto& v : voices)
        v.render (out, numSamples, tuning);

    for (int i = 0; i < numSamples; ++i)
        out[i] *= kVoiceHeadroom * outputGain.next();
}

int SynthEngine::getNumActiveVoices() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        n += v.isActive() ? 1 : 0;
    return n;
}

} // namespace bleep::dsp
