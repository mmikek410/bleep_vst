#pragma once

// Общие части для демо-рендеров: запись WAV и оригинальная ударная петля.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace demo
{
constexpr double kSampleRate = 48000.0;
constexpr double kBpm = 140.0;
constexpr int kBars = 8;

inline double samplesPerBeat() { return kSampleRate * 60.0 / kBpm; }

inline void writeWav (const std::string& path, const std::vector<float>& left, const std::vector<float>& right)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr) { std::printf ("не могу записать %s\n", path.c_str()); return; }
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t frames = (uint32_t) left.size(), dataBytes = frames * 4;
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) kSampleRate); u32 ((uint32_t) kSampleRate * 4); u16 (4); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
        for (float v : { left[i], right[i] })
        {
            const auto s = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f);
            std::fwrite (&s, 2, 1, f);
        }
    std::fclose (f);
}

/** Оригинальная простая техно-петля: бочка на каждую долю, закрытые хэты на
    шестнадцатые с акцентом на офбит, открытый хэт на офбит, клэп на 2 и 4,
    деревянная перкуссия с синкопой. */
inline std::vector<float> drumLoop (size_t frames, int bars = kBars)
{
    std::vector<float> out (frames, 0.0f);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    const double beat = samplesPerBeat(), six = beat / 4.0;

    auto addKick = [&] (size_t at)
    {
        double phase = 0.0;
        for (size_t i = 0; i < (size_t) (kSampleRate * 0.45) && at + i < frames; ++i)
        {
            const double t = (double) i / kSampleRate;
            const double f = 48.0 + 110.0 * std::exp (-t * 38.0);
            phase += f / kSampleRate;
            const double amp = std::exp (-t * 7.5) * (1.0 - std::exp (-t * 2000.0));
            const double click = std::exp (-t * 900.0) * 0.25;
            out[at + i] += (float) (0.85 * std::tanh (1.6 * std::sin (2.0 * M_PI * phase) * amp) + click * noise (rng) * 0.3);
        }
    };
    auto addNoiseHit = [&] (size_t at, double decay, double level, double hp)
    {
        float prev = 0.0f, lp = 0.0f;
        for (size_t i = 0; i < (size_t) (kSampleRate * decay * 6) && at + i < frames; ++i)
        {
            const double t = (double) i / kSampleRate;
            const float n = noise (rng);
            const float high = n - prev; prev = n;                   // простой ФВЧ
            lp += (float) hp * (high - lp);                          // немного сглаживаем
            out[at + i] += (float) (level * std::exp (-t / decay)) * lp;
        }
    };
    auto addClap = [&] (size_t at)
    {
        for (int burst = 0; burst < 3; ++burst)
            addNoiseHit (at + (size_t) (burst * kSampleRate * 0.009), 0.012, 0.18, 0.5);
        addNoiseHit (at + (size_t) (kSampleRate * 0.027), 0.07, 0.16, 0.45);
    };
    auto addPerc = [&] (size_t at, double hz)
    {
        for (size_t i = 0; i < (size_t) (kSampleRate * 0.12) && at + i < frames; ++i)
        {
            const double t = (double) i / kSampleRate;
            out[at + i] += (float) (0.16 * std::exp (-t * 45.0) * std::sin (2.0 * M_PI * hz * t * (1.0 + 0.6 * std::exp (-t * 60.0))));
        }
    };

    for (int bar = 0; bar < bars; ++bar)
        for (int s = 0; s < 16; ++s)
        {
            const size_t at = (size_t) ((bar * 16 + s) * six);
            if (s % 4 == 0) addKick (at);
            if (s % 4 == 2) addNoiseHit (at, 0.045, 0.22, 0.9);                       // открытый хэт на офбит
            else            addNoiseHit (at, 0.012, s % 2 ? 0.07 : 0.11, 0.95);      // закрытые
            if (s == 4 || s == 12) addClap (at);
            if (s == 3 || s == 10 || (bar % 2 == 1 && s == 14)) addPerc (at, 620.0);  // синкопированная перкуссия
        }
    return out;
}

} // namespace demo
