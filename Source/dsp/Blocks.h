#pragma once

// Общие DSP-блоки для «тёмных» бипов: фильтр, сатурация, модальный резонатор,
// осцилляторы с polyBLEP, перкуссионная огибающая. Без JUCE, без аллокаций.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bleep::dsp
{

constexpr float kPiF = 3.14159265358979f;
constexpr double kTwoPiD = 6.283185307179586476925286766559;

/** TPT state-variable фильтр (Zavalishin/Simper): устойчив при быстрой модуляции.
    Даёт сразу LP, BP и HP. Резонанс 0..1 -> Q 0.5..~25 (без самовозбуждения). */
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 2.0f;

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void set (float cutoffHz, float resonance, float sampleRate) noexcept
    {
        const float fc = std::clamp (cutoffHz, 15.0f, 0.45f * sampleRate);
        const float g = std::tan (kPiF * fc / sampleRate);
        k = 2.0f - 1.96f * std::clamp (resonance, 0.0f, 1.0f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    struct Out { float lp, bp, hp; };

    Out process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }

    /** Полосовой с единичным усилением на центральной частоте. */
    float bandUnity (float x) noexcept { return process (x).bp * k; }
};

/** Мягкая «аналоговая» сатурация: tanh с небольшой асимметрией (чётные
    гармоники, «теплота»). Усиление малого сигнала = 1 при любом drive, поэтому
    громкость тихих мест не меняется — сжимаются только пики. */
struct Saturator
{
    float drive = 1.0f, bias = 0.0f, offset = 0.0f, comp = 1.0f;
    float dcX = 0.0f, dcY = 0.0f;   // ФВЧ ~10 Гц после асимметрии

    void set (float driveDb, float asymmetry) noexcept
    {
        drive = std::pow (10.0f, std::clamp (driveDb, 0.0f, 24.0f) / 20.0f);
        bias = 0.35f * std::clamp (asymmetry, 0.0f, 1.0f);
        offset = std::tanh (bias);
        comp = 1.0f / (drive * (1.0f - offset * offset));   // производная в нуле -> 1
    }

    float process (float x) noexcept
    {
        const float y = (std::tanh (drive * x + bias) - offset) * comp;
        const float out = y - dcX + 0.9987f * dcY;   // убираем постоянную составляющую
        dcX = y;
        dcY = out;
        return out;
    }
};

/** Модальный резонатор (затухающий синус, 2 полюса): точная частота и время
    затухания T60. Возбуждается коротким импульсом/шумом — естественный
    «деревянный/полый» тон без обрезанной огибающей. */
struct Resonator
{
    float b1 = 0.0f, b2 = 0.0f, y1 = 0.0f, y2 = 0.0f, gain = 1.0f;

    void reset() noexcept { y1 = y2 = 0.0f; }

    void set (float freqHz, float t60Seconds, float sampleRate) noexcept
    {
        const float f = std::clamp (freqHz, 20.0f, 0.45f * sampleRate);
        const float r = std::exp (-6.9078f / (std::max (t60Seconds, 0.005f) * sampleRate));
        const float w = 2.0f * kPiF * f / sampleRate;
        b1 = 2.0f * r * std::cos (w);
        b2 = -r * r;
        gain = std::sin (w);   // импульс единичной амплитуды даёт синус амплитудой ~1
    }

    float process (float x) noexcept
    {
        const float y = gain * x + b1 * y1 + b2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
};

/** polyBLEP-коррекция разрыва (Välimäki): убирает алиасинг у пилы/импульса. */
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)        { t /= dt;          return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

/** Осциллятор: импульс с ШИМ и треугольник (интегрированный BLEP-меандр). */
struct BlepOsc
{
    double phase = 0.0;
    float triState = 0.0f;

    void reset (double startPhase = 0.0) noexcept { phase = startPhase; triState = 0.0f; }

    /** Возвращает {треугольник, импульс}. inc = f/fs, pw = 0.05..0.95 */
    struct Out { float tri, pulse; };

    Out process (float inc, float pw) noexcept
    {
        const float t = (float) phase;
        float sq = t < 0.5f ? 1.0f : -1.0f;
        sq += polyBlep (t, inc);
        sq -= polyBlep ((float) std::fmod (phase + 0.5, 1.0), inc);
        // Треугольник = интеграл меандра (с утечкой против дрейфа).
        triState = 0.999f * triState + 4.0f * inc * sq;

        float pulse = t < pw ? 1.0f : -1.0f;
        pulse += polyBlep (t, inc);
        pulse -= polyBlep ((float) std::fmod (phase + 1.0 - pw, 1.0), inc);
        pulse -= (2.0f * pw - 1.0f);   // убираем постоянную составляющую

        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
        return { triState, pulse };
    }
};

/** Перкуссионная огибающая: мягкая атака (полукосинус, без щелчка) ->
    короткая «полка» -> спад с регулируемой кривой -> release по note-off.
    curve 0 = чистая экспонента (резкий старт спада), 1 = «округлое плечо». */
struct PercEnvelope
{
    enum class Stage { Idle, Attack, Hold, Decay, Release };

    Stage stage = Stage::Idle;
    float level = 0.0f, startLevel = 0.0f;
    float pos = 0.0f;
    float attackSamples = 96.0f, holdSamples = 0.0f, decaySamples = 9600.0f, releaseSamples = 4800.0f;
    float curve = 0.5f, sustain = 0.0f, releaseCoef = 0.999f;

    void set (float attackS, float holdS, float decayS, float releaseS, float curveAmount, float sr) noexcept
    {
        attackSamples  = std::max (1.0f, attackS * sr);
        holdSamples    = std::max (0.0f, holdS * sr);
        decaySamples   = std::max (1.0f, decayS * sr);
        releaseSamples = std::max (1.0f, releaseS * sr);
        curve = std::clamp (curveAmount, 0.0f, 1.0f);
        releaseCoef = std::exp (-6.9078f / releaseSamples);
    }

    void noteOn() noexcept { startLevel = level; stage = Stage::Attack; pos = 0.0f; }
    void noteOff() noexcept { if (stage != Stage::Idle) stage = Stage::Release; }
    bool isActive() const noexcept { return stage != Stage::Idle; }

    float process() noexcept
    {
        switch (stage)
        {
            case Stage::Idle: return 0.0f;
            case Stage::Attack:
            {
                const float x = pos / attackSamples;
                level = startLevel + (1.0f - startLevel) * (0.5f - 0.5f * std::cos (kPiF * x));
                if (++pos >= attackSamples) { stage = holdSamples > 0 ? Stage::Hold : Stage::Decay; pos = 0.0f; level = 1.0f; }
                break;
            }
            case Stage::Hold:
                if (++pos >= holdSamples) { stage = Stage::Decay; pos = 0.0f; }
                break;
            case Stage::Decay:
            {
                // level = exp(-6.9 · x^p): p = 1 — экспонента, p > 1 — «округлое плечо»
                // (звук держится чуть дольше и уходит мягче). В x = 1 всегда -60 дБ.
                const float x = pos / decaySamples;
                level = std::exp (-6.9078f * std::pow (x, 1.0f + 2.0f * curve));
                ++pos;
                if (x >= 1.0f) { stage = Stage::Idle; level = 0.0f; }
                break;
            }
            case Stage::Release:
                level *= releaseCoef;
                if (level < 1.0e-4f) { stage = Stage::Idle; level = 0.0f; }
                break;
        }
        return level;
    }
};

/** Простой генератор шума (xorshift), детерминированный. */
struct Noise
{
    std::uint32_t state = 0x12345678u;
    float next() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (state >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }
};

} // namespace bleep::dsp
