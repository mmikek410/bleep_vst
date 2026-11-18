#pragma once

// Прототипы трёх подходов к «тёмному» бипу. Один патч на весь секвенсор:
// каждый шаг — тот же звук, меняются только высота и velocity.
//
//   A. Dark FM        — 2-оп FM с коротким индексом, округлая огибающая, тёмный ФНЧ, мягкая сатурация
//   B. Resonant       — короткий импульс/шум возбуждает 3 модальных резонатора (дерево/труба/металл)
//   C. Hollow Analog  — треугольник + узкий импульс с лёгким взаимодействием, резонансный фильтр, сатурация
//
// Это экспериментальный код для сравнения на слух; финальная версия будет
// встроена в плагин после выбора.

#include "dsp/Blocks.h"

#include <array>
#include <cmath>
#include <vector>

namespace proto
{
using namespace bleep::dsp;

inline float midiHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

// ---------------------------------------------------------------------------
// A. Dark FM
// ---------------------------------------------------------------------------
struct DarkFmParams
{
    float ratio = 1.0f, indexPeak = 2.2f, indexBody = 0.35f, indexDecay = 0.035f;  // индекс: короткий пик -> тело
    float feedback = 0.0f;                                                       // 0..0.4
    float pitchAmount = 2.0f, pitchDecay = 0.008f;                               // лёгкий «удар», не лазер
    float cutoff = 1100.0f, resonance = 0.35f, filterEnvOct = 1.4f, filterDecay = 0.05f, keytrack = 0.5f;
    float attack = 0.003f, hold = 0.004f, decay = 0.28f, release = 0.08f, curve = 0.45f;
    float driveDb = 5.0f, asym = 0.4f;
};

struct DarkFmVoice
{
    DarkFmParams p;
    float sr = 48000.0f, f = 220.0f, vel = 1.0f, t = 0.0f;
    double cPhase = 0.0, mPhase = 0.0;
    float fbPrev = 0.0f;
    Svf filter;
    Saturator sat;
    PercEnvelope env;
    int ctl = 0;

    void prepare (float sampleRate) { sr = sampleRate; }
    void noteOn (int note, float velocity)
    {
        if (! env.isActive()) { cPhase = mPhase = 0.0; filter.reset(); fbPrev = 0.0f; }
        f = midiHz ((float) note); vel = velocity; t = 0.0f; ctl = 0;
        env.set (p.attack, p.hold, p.decay, p.release, p.curve, sr);
        sat.set (p.driveDb, p.asym);
        env.noteOn();
    }
    void noteOff() { env.noteOff(); }
    bool active() const { return env.isActive(); }

    void render (float* out, int n)
    {
        for (int i = 0; i < n && env.isActive(); ++i, t += 1.0f / sr)
        {
            const float pe = std::exp (-t / std::max (p.pitchDecay, 1e-4f));
            const float fc = f * std::exp2 (p.pitchAmount * pe / 12.0f);
            const float fm = fc * p.ratio;
            if (ctl++ % 8 == 0)
            {
                const float fe = std::exp (-t / p.filterDecay);
                const float cut = p.cutoff * std::pow (f / 261.6f, p.keytrack) * std::exp2 (p.filterEnvOct * fe + 0.6f * vel - 0.3f);
                filter.set (cut, p.resonance, sr);
            }
            float index = (p.indexBody + (p.indexPeak - p.indexBody) * std::exp (-t / p.indexDecay)) * (0.55f + 0.45f * vel);
            index = std::min (index, std::max (0.0f, (0.45f * sr - fc) / fm - 1.0f));   // Карсон
            const float mod = std::sin ((float) (kTwoPiD * mPhase) + p.feedback * fbPrev);
            fbPrev = mod;
            const float car = std::sin ((float) (kTwoPiD * cPhase) + index * mod);
            cPhase += fc / sr; cPhase -= std::floor (cPhase);
            mPhase += fm / sr; mPhase -= std::floor (mPhase);

            const float y = sat.process (filter.process (car).lp * 0.9f);
            out[i] += y * env.process() * (0.3f + 0.7f * vel * vel);
        }
    }
};

// ---------------------------------------------------------------------------
// B. Resonant (модальный синтез)
// ---------------------------------------------------------------------------
struct ResonantParams
{
    std::array<float, 3> ratios { 1.0f, 2.0f, 3.0f };   // частоты мод относительно ноты
    std::array<float, 3> gains  { 1.0f, 0.35f, 0.15f };
    std::array<float, 3> decays { 1.0f, 0.55f, 0.3f };  // доля от decay
    float decay = 0.35f;            // T60 основной моды, с
    float clickLevel = 1.0f;        // импульс
    float noiseLevel = 0.6f, noiseMs = 4.0f, noiseCutoff = 3500.0f;
    float pitchAmount = -0.8f, pitchDecay = 0.03f;      // «резиновый» лёгкий спад высоты
    float toneCutoff = 5000.0f;     // мягкий ФНЧ на выходе
    float release = 0.06f;          // глушение по note-off (как демпфер)
    float driveDb = 4.0f, asym = 0.3f;
};

struct ResonantVoice
{
    ResonantParams p;
    float sr = 48000.0f, f = 220.0f, vel = 1.0f, t = 0.0f;
    std::array<Resonator, 3> modes;
    Svf exciteFilter, tone;
    Saturator sat;
    Noise noise;
    float damp = 1.0f, dampCoef = 1.0f;
    bool on = false, released = false;
    int ctl = 0, sample = 0;

    void prepare (float sampleRate) { sr = sampleRate; }
    void noteOn (int note, float velocity)
    {
        f = midiHz ((float) note); vel = velocity; t = 0.0f; ctl = 0; sample = 0;
        on = true; released = false; damp = 1.0f;
        exciteFilter.set (p.noiseCutoff * (0.6f + 0.6f * vel), 0.1f, sr);
        tone.set (p.toneCutoff, 0.1f, sr);
        sat.set (p.driveDb, p.asym);
        dampCoef = std::exp (-6.9078f / (p.release * sr));
    }
    void noteOff() { released = true; }
    bool active() const { return on; }

    void updateModes()
    {
        const float pitch = std::exp2 (p.pitchAmount * std::exp (-t / p.pitchDecay) / 12.0f);
        const float lengthScale = std::pow (261.6f / f, 0.3f);   // высокие ноты звенят короче
        for (size_t m = 0; m < 3; ++m)
            modes[m].set (f * pitch * p.ratios[m], p.decay * p.decays[m] * lengthScale, sr);
    }

    void render (float* out, int n)
    {
        for (int i = 0; i < n && on; ++i, t += 1.0f / sr, ++sample)
        {
            if (ctl++ % 8 == 0) updateModes();

            // Возбуждение: импульс + короткий окрашенный шум.
            float x = (sample == 0 ? p.clickLevel : 0.0f);
            const float burst = std::exp (-t / (p.noiseMs * 0.001f));
            x += exciteFilter.process (noise.next()).lp * p.noiseLevel * burst;
            x *= 0.4f + 0.6f * vel;

            float y = 0.0f;
            for (size_t m = 0; m < 3; ++m)
                y += modes[m].process (x) * p.gains[m];

            if (released) damp *= dampCoef;
            y = tone.process (sat.process (y * 0.22f)).lp * damp;   // уровень на входе сатурации: мягко, без «квадрата»
            out[i] += y;

            if ((released && damp < 1e-4f) || t > p.decay * 1.6f) on = false;
        }
    }
};

// ---------------------------------------------------------------------------
// C. Hollow Analog
// ---------------------------------------------------------------------------
struct HollowParams
{
    float pulseMix = 0.3f, pulseWidth = 0.32f, detuneCents = 7.0f, crossMod = 0.04f;
    float subMix = 0.0f;                                  // суб-октава треугольником
    bool bandPass = false;
    float cutoff = 650.0f, resonance = 0.55f, filterEnvOct = 2.2f, filterDecay = 0.06f, keytrack = 0.6f;
    float pitchAmount = 0.0f, pitchDecay = 0.01f;
    float attack = 0.002f, hold = 0.0f, decay = 0.24f, release = 0.07f, curve = 0.5f;
    float driveDb = 7.0f, asym = 0.5f;
};

struct HollowVoice
{
    HollowParams p;
    float sr = 48000.0f, f = 220.0f, vel = 1.0f, t = 0.0f;
    BlepOsc osc1, osc2, sub;
    Svf filter;
    Saturator sat;
    PercEnvelope env;
    int ctl = 0;

    void prepare (float sampleRate) { sr = sampleRate; }
    void noteOn (int note, float velocity)
    {
        if (! env.isActive()) { osc1.reset (0.0); osc2.reset (0.37); sub.reset (0.0); filter.reset(); }
        f = midiHz ((float) note); vel = velocity; t = 0.0f; ctl = 0;
        env.set (p.attack, p.hold, p.decay, p.release, p.curve, sr);
        sat.set (p.driveDb, p.asym);
        env.noteOn();
    }
    void noteOff() { env.noteOff(); }
    bool active() const { return env.isActive(); }

    void render (float* out, int n)
    {
        const float detune = std::exp2 (p.detuneCents / 1200.0f);
        for (int i = 0; i < n && env.isActive(); ++i, t += 1.0f / sr)
        {
            const float fc = f * std::exp2 (p.pitchAmount * std::exp (-t / p.pitchDecay) / 12.0f);
            if (ctl++ % 8 == 0)
            {
                const float fe = std::exp (-t / p.filterDecay);
                const float cut = p.cutoff * std::pow (f / 261.6f, p.keytrack) * std::exp2 (p.filterEnvOct * fe * (0.5f + 0.5f * vel));
                filter.set (cut, p.resonance, sr);
            }
            const auto a = osc1.process (fc / sr, 0.5f);
            // Лёгкое взаимодействие: треугольник чуть модулирует частоту второго осциллятора.
            const auto b = osc2.process (fc * detune * (1.0f + p.crossMod * a.tri) / sr, p.pulseWidth);
            const auto s = sub.process (0.5f * fc / sr, 0.5f);
            float x = a.tri * (1.0f - p.pulseMix) + b.pulse * p.pulseMix * 0.6f + s.tri * p.subMix;
            x = sat.process (x * 0.7f);   // «аналоговый» драйв перед фильтром
            const auto fo = filter.process (x);
            const float y = p.bandPass ? fo.bp * filter.k : fo.lp;
            out[i] += y * env.process() * (0.3f + 0.7f * vel * vel);
        }
    }
};

// ---------------------------------------------------------------------------
// Полифонический контейнер (8 голосов, кража самого старого).
// ---------------------------------------------------------------------------
template <typename Voice, typename Params>
struct Poly
{
    std::array<Voice, 8> voices;
    std::array<int, 8> notes {};
    std::array<long, 8> ages {};
    long counter = 0;

    void prepare (float sr, const Params& params)
    {
        for (auto& v : voices) { v.prepare (sr); v.p = params; }
        notes.fill (-1);
    }
    void noteOn (int note, float vel)
    {
        size_t best = 0;
        for (size_t i = 0; i < voices.size(); ++i)
        {
            if (! voices[i].active()) { best = i; break; }
            if (ages[i] < ages[best]) best = i;
        }
        voices[best].noteOn (note, vel);
        notes[best] = note;
        ages[best] = ++counter;
    }
    void noteOff (int note)
    {
        for (size_t i = 0; i < voices.size(); ++i)
            if (notes[i] == note && voices[i].active()) voices[i].noteOff();
    }
    void render (float* out, int n)
    {
        for (int i = 0; i < n; ++i) out[i] = 0.0f;
        for (auto& v : voices) if (v.active()) v.render (out, n);
    }
};

} // namespace proto
