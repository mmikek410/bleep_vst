#pragma once

// Сдержанная темпо-синхронная задержка в духе даба: ФВЧ и ФНЧ в петле
// обратной связи (каждый повтор темнее и тоньше), лёгкое стерео за счёт
// разного времени в каналах и перекрёстной связи. Сухой сигнал остаётся моно
// и не трогается. Буферы выделяются в prepare(), в process() аллокаций нет.

#include <algorithm>
#include <cmath>
#include <vector>

namespace bleep::dsp
{

class StereoDelay
{
public:
    void prepare (double sampleRate, double maxSeconds = 2.0)
    {
        sr = (float) sampleRate;
        const size_t n = (size_t) (maxSeconds * sampleRate) + 4;
        bufL.assign (n, 0.0f);
        bufR.assign (n, 0.0f);
        write = 0;
        lpL = lpR = hpL = hpR = hpInL = hpInR = 0.0f;
    }

    /** time — в секундах; spread — 0..1 (разница времени L/R и перекрёстная связь). */
    void set (float timeSeconds, float feedback, float hpHz, float lpHz, float wet, float spread) noexcept
    {
        const float maxT = (float) (bufL.size() - 4) / sr;
        timeL = std::clamp (timeSeconds, 0.001f, maxT);
        timeR = std::clamp (timeSeconds * (1.0f + 0.06f * spread), 0.001f, maxT);
        fb = std::clamp (feedback, 0.0f, 0.9f);
        cross = 0.35f * std::clamp (spread, 0.0f, 1.0f);
        lpCoef = 1.0f - std::exp (-2.0f * 3.14159265f * std::clamp (lpHz, 200.0f, 18000.0f) / sr);
        hpCoef = std::exp (-2.0f * 3.14159265f * std::clamp (hpHz, 20.0f, 4000.0f) / sr);
        wetGain = std::clamp (wet, 0.0f, 1.0f);
    }

    /** Моно вход -> стерео выход (сухой + мокрый). */
    void process (float in, float& outL, float& outR) noexcept
    {
        const float dL = read (bufL, timeL);
        const float dR = read (bufR, timeR);

        // Фильтры в петле: ФНЧ (темнее с каждым повтором) и ФВЧ (без гула).
        lpL += lpCoef * (dL - lpL);
        lpR += lpCoef * (dR - lpR);
        const float hL = hpCoef * (hpL + lpL - hpInL); hpInL = lpL; hpL = hL;
        const float hR = hpCoef * (hpR + lpR - hpInR); hpInR = lpR; hpR = hR;

        // Мягкое ограничение в петле: обратная связь не «разносится».
        const float fbL = std::tanh (fb * ((1.0f - cross) * hL + cross * hR));
        const float fbR = std::tanh (fb * ((1.0f - cross) * hR + cross * hL));

        bufL[write] = in + fbL;
        bufR[write] = in * (1.0f - 0.5f * cross) + fbR;
        write = (write + 1) % bufL.size();

        outL = in + wetGain * hL;
        outR = in + wetGain * hR;
    }

private:
    float read (const std::vector<float>& buf, float seconds) const noexcept
    {
        const float d = seconds * sr;
        const size_t n = buf.size();
        const float pos = (float) write - d + (float) n * 2.0f;
        const size_t i0 = (size_t) pos % n;
        const float frac = pos - std::floor (pos);
        return buf[i0] + frac * (buf[(i0 + 1) % n] - buf[i0]);
    }

    std::vector<float> bufL, bufR;
    size_t write = 0;
    float sr = 48000.0f;
    float timeL = 0.3f, timeR = 0.3f, fb = 0.4f, cross = 0.1f, wetGain = 0.2f;
    float lpCoef = 0.2f, hpCoef = 0.99f;
    float lpL = 0, lpR = 0, hpL = 0, hpR = 0, hpInL = 0, hpInR = 0;
};

} // namespace bleep::dsp
