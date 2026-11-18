#pragma once

#include "Envelope.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bleep::dsp
{

/** Параметры звука, общие для всех голосов (в обычных единицах).
    Значения по умолчанию здесь «нейтральные» и воспроизводят синус Phase 1:
    глубина FM 0, огибающая высоты 0, фильтр полностью открыт, без резонанса.
    (Звучащие значения по умолчанию для нового экземпляра задаются в Parameters.cpp.) */
struct VoiceParams
{
    // Амплитуда
    float attack = 0.001f, decay = 0.18f, sustain = 0.0f, release = 0.12f;   // с, с, 0..1, с
    float fineTune = 0.0f;                                                    // центы

    // FM на двух операторах: синус-модулятор (с обратной связью) -> синус-несущая
    float fmRatio = 1.0f;      // частота модулятора : несущей (0.25..16, гармонические и негармонические)
    float fmDepth = 0.0f;      // индекс модуляции в радианах при полной velocity (0..10)
    float fmFeedback = 0.0f;   // самообратная связь модулятора 0..1
    float fmDecay = 0.25f;     // с: огибающая индекса (яркая атака -> мягкий хвост)

    // Огибающая высоты
    float pitchAmount = 0.0f;  // полутоны в начале ноты, в обе стороны
    float pitchDecay = 0.03f;  // с
    float pitchCurve = 0.5f;   // 0 = линейный спад, 1 = очень быстрый спад («лазер»)

    // Резонансный фильтр (TPT state-variable, устойчив при модуляции)
    int   filterMode = 0;      // 0 = ФНЧ (low-pass), 1 = полосовой (band-pass)
    float cutoff = 20000.0f;   // Гц
    float resonance = 0.0f;    // 0..1 (ограничен ниже самовозбуждения)
    float filterEnv = 0.0f;    // октавы в начале ноты, в обе стороны
    float filterDecay = 0.2f;  // с
};

/** Один голос: пара FM-операторов -> резонансный фильтр -> огибающая амплитуды.

    Защита от алиасинга — в два слоя:
    1) FM-ядро (модулятор + несущая) работает с 2-кратной передискретизацией;
       результат прореживается полуполосным КИХ-фильтром (47 отводов, Кайзер β=8:
       ровно до 0.4·fs, подавление ≥ 84 дБ там, куда попадают отражения).
    2) Индекс FM ограничивается по правилу Карсона с учётом обратной связи
       (она расширяет спектр модулятора), так чтобы полоса сигнала оставалась
       ниже 1.2·fs — тогда всё, что отражается, попадает в полосу подавления.
    Обратная связь ограничена (β ≤ 0.9). Большая обратная связь — сознательный
    «грязный» тембр (как feedback 6–7 в DX7): петля даёт шумовую составляющую,
    которая растёт с fb и почти не зависит от частоты дискретизации. До fb ≈ 0.5
    она ниже -45 дБ. На высоких нотах обратная связь ослабляется автоматически.
    Фильтр и огибающие работают на базовой частоте. */
class Voice
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        env.setSampleRate (newSampleRate);
        env.reset();
        carrierPhase = modPhase = 0.0;
        fb1 = fb2 = 0.0f;
        ic1 = ic2 = 0.0f;
        decimator.reset();
        smoothCoef = std::exp (-1.0f / (0.005f * (float) sampleRate));  // сглаживание параметров 5 мс
    }

    void setParams (const VoiceParams& p) noexcept
    {
        params = p;
        env.setParameters (p.attack, p.decay, p.sustain, p.release);
        const float sr = (float) sampleRate;
        fmCoef     = decayCoefficient (p.fmDecay, sr);
        filterCoef = decayCoefficient (p.filterDecay, sr);
        pitchSamples = std::max (1.0f, p.pitchDecay * sr);
        pitchPower = 1.0f + 7.0f * std::clamp (p.pitchCurve, 0.0f, 1.0f);
    }

    /** Старт или перезапуск. При перезапуске фазы осцилляторов продолжаются: без ступеньки в форме волны. */
    void start (int midiNote, float velocity, std::uint64_t startOrder) noexcept
    {
        const bool wasIdle = ! env.isActive();
        if (wasIdle)
        {
            carrierPhase = modPhase = 0.0;
            fb1 = fb2 = 0.0f;
            ic1 = ic2 = 0.0f;
            decimator.reset();
            depthSmoothed = params.fmDepth;
            logCutoffSmoothed = std::log2 (std::clamp (params.cutoff, 20.0f, 20000.0f));
        }

        note     = midiNote;
        vel      = std::clamp (velocity, 0.0f, 1.0f);
        gain     = velocityToGain (vel);
        order    = startOrder;
        held     = true;
        fmLevel  = 1.0f;
        filterLevel = 1.0f;
        pitchPos = 0.0f;
        controlCounter = 0;   // сразу пересчитать частоту и фильтр для новой ноты
        env.noteOn();
    }

    void release() noexcept { held = false; env.noteOff(); }
    void kill() noexcept    { held = false; env.reset(); note = -1; }

    bool isActive() const noexcept    { return env.isActive(); }
    bool isHeld() const noexcept      { return held; }
    int getNote() const noexcept      { return note; }
    std::uint64_t getOrder() const noexcept { return order; }
    float getLevel() const noexcept   { return env.getLevel(); }
    bool isReleasing() const noexcept { return env.getStage() == Envelope::Stage::Release; }

    /** Добавляет звук в `out`. */
    void render (float* out, int numSamples) noexcept
    {
        if (! env.isActive())
            return;

        const float sr = (float) sampleRate;
        const float nyquistGuard = 0.45f * sr;
        const float controlSmoothCoef = std::pow (smoothCoef, (float) kControlInterval);
        const float baseSemis = (float) (note - 69) + params.fineTune * 0.01f;
        const float ratio = std::clamp (params.fmRatio, 0.125f, 32.0f);
        const float targetLogCutoff = std::log2 (std::clamp (params.cutoff, 20.0f, 20000.0f));
        const float velIndex = 0.4f + 0.6f * vel;   // сильнее удар — ярче звук
        const float res = std::clamp (params.resonance, 0.0f, 1.0f);
        const float k = 2.0f - 1.92f * res;          // демпфирование: Q 0.5 .. 12.5
        const float lpComp = 0.35f + 0.65f * (k * 0.5f);  // усмиряем резонансный пик
        const bool bandPass = params.filterMode == 1;

        for (int i = 0; i < numSamples; ++i)
        {
            // Управляющие величины (частота, предел индекса, коэффициенты фильтра)
            // пересчитываются раз в kControlInterval сэмплов — это неслышно,
            // но в несколько раз дешевле, чем считать exp2/pow/tan на каждом сэмпле.
            if (controlCounter == 0)
            {
                // Огибающая высоты: (1 - x)^p за время pitchDecay, p задаётся кривой.
                float pitchEnv = 0.0f;
                if (pitchPos < pitchSamples)
                    pitchEnv = std::pow (1.0f - pitchPos / pitchSamples, pitchPower);

                const float freq = 440.0f * std::exp2 ((baseSemis + params.pitchAmount * pitchEnv) * (1.0f / 12.0f));
                carrierHz = std::min (freq, nyquistGuard);
                modHz = carrierHz * ratio;
                carrierInc = carrierHz / (2.0f * sr);   // шаг фазы на удвоенной частоте
                modInc = modHz / (2.0f * sr);

                // Обратная связь делает модулятор круче (наклон ~ 1/(1-β)) и расширяет
                // его спектр — учитываем это в пределе Карсона.
                fbAmount = params.fmFeedback * std::min (1.0f, 1500.0f / modHz);   // на высоких нотах мягче
                const float beta = fbAmount * kFbScale;
                const float modBandwidth = modHz * (1.0f + beta / (1.0f - beta));
                maxIndex = std::max (0.0f, (kOversampledGuard * sr - carrierHz) / modBandwidth - 2.0f);   // Карсон + запас в одну боковую

                // Срез фильтра: сглаживание в log2-области + огибающая фильтра.
                logCutoffSmoothed = targetLogCutoff + (logCutoffSmoothed - targetLogCutoff) * controlSmoothCoef;
                const float fc = std::clamp (std::exp2 (logCutoffSmoothed + params.filterEnv * filterLevel), 20.0f, nyquistGuard);
                const float g = std::tan (kPi * fc / sr);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }
            controlCounter = (controlCounter + 1) % kControlInterval;
            pitchPos += 1.0f;
            filterLevel *= filterCoef;

            // Индекс FM: своя огибающая, velocity и предел по правилу Карсона.
            depthSmoothed = params.fmDepth + (depthSmoothed - params.fmDepth) * smoothCoef;
            const float index = std::min (depthSmoothed * fmLevel * velIndex, maxIndex);
            fmLevel *= fmCoef;

            // FM-ядро: два сэмпла на удвоенной частоте -> полуполосный дециматор.
            decimator.push (fmCore (index));
            decimator.push (fmCore (index));
            const float carrier = decimator.output();

            // SVF (Simper/Zavalishin): LP = v2, BP = v1.
            const float v3 = carrier - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            const float filtered = bandPass ? v1 * k : v2 * lpComp;   // BP: единичное усиление на центральной частоте

            const float e = env.process();
            out[i] += filtered * e * gain;

            if (! env.isActive())
            {
                note = -1;
                ic1 = ic2 = 0.0f;
                break;
            }
        }

        // Защита от денормалов и «разноса» (не должна срабатывать — тесты это проверяют).
        if (! std::isfinite (ic1) || ! std::isfinite (ic2) || std::abs (ic1) > 1.0e4f || std::abs (ic2) > 1.0e4f)
            ic1 = ic2 = 0.0f;
    }

private:
    /** Один сэмпл FM-ядра на удвоенной частоте: модулятор с усреднённой
        обратной связью (устойчивая схема, как в DX7) -> несущая. */
    float fmCore (float index) noexcept
    {
        const float mod = std::sin ((float) (kTwoPi * modPhase) + fbAmount * kFbScale * 0.5f * (fb1 + fb2));
        fb2 = fb1;
        fb1 = mod;
        const float out = std::sin ((float) (kTwoPi * carrierPhase) + index * mod);
        carrierPhase += carrierInc;
        if (carrierPhase >= 1.0) carrierPhase -= 1.0;
        modPhase += modInc;
        if (modPhase >= 1.0) modPhase -= std::floor (modPhase);
        return out;
    }

    /** Полуполосный дециматор 2:1 (47 отводов, ненулевые — центральный и
        каждый второй; симметрия позволяет обойтись 12 умножениями на пару). */
    struct HalfbandDecimator
    {
        static constexpr int kTaps = 47;
        static constexpr int kSize = 64;   // кольцевой буфер, степень двойки
        static constexpr float kCentre = 0.49999539684182204f;
        static constexpr float kSide[12] = {
            -3.2368e-05f, 0.000214604f, -0.000690004f, 0.001690651f, -0.003539468f, 0.006670848f,
            -0.011685384f, 0.019511683f, -0.031906212f, 0.053239599f, -0.099534584f, 0.316062936f };

        float buffer[kSize] {};
        int write = 0;

        void reset() noexcept { std::fill (buffer, buffer + kSize, 0.0f); write = 0; }
        void push (float x) noexcept { buffer[write] = x; write = (write + 1) & (kSize - 1); }

        float output() const noexcept
        {
            auto at = [this] (int t) { return buffer[(write - 1 - t) & (kSize - 1)]; };   // t сэмплов назад
            float y = kCentre * at (23);
            for (int j = 0; j < 12; ++j)
                y += kSide[j] * (at (2 * j) + at (kTaps - 1 - 2 * j));
            return y;
        }
    };

    static constexpr double kTwoPi = 6.283185307179586476925286766559;
    static constexpr float  kPi    = 3.14159265358979f;
    static constexpr int    kControlInterval = 8;
    static constexpr float  kFbScale = 0.9f;            // β обратной связи при fb = 1
    static constexpr float  kOversampledGuard = 1.2f;   // допустимая полоса FM, в долях fs

    static float velocityToGain (float v) noexcept { return 0.15f + 0.85f * v * v; }

    static float decayCoefficient (float seconds, float sr) noexcept
    {
        // Спад до -60 дБ (0.001) за `seconds`.
        return std::exp (std::log (0.001f) / (std::max (seconds, 0.001f) * sr));
    }

    Envelope env;
    VoiceParams params;
    double sampleRate = 44100.0;

    double carrierPhase = 0.0, modPhase = 0.0;
    float fb1 = 0.0f, fb2 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;                 // состояние SVF-фильтра
    HalfbandDecimator decimator;

    float fmLevel = 1.0f, fmCoef = 0.999f;
    float filterLevel = 1.0f, filterCoef = 0.999f;
    float pitchPos = 0.0f, pitchSamples = 1.0f, pitchPower = 4.5f;
    float depthSmoothed = 0.0f, logCutoffSmoothed = 14.0f, smoothCoef = 0.99f;

    // Управляющие величины, обновляемые раз в kControlInterval сэмплов.
    int controlCounter = 0;
    float carrierHz = 440.0f, modHz = 440.0f, carrierInc = 0.0f, modInc = 0.0f;
    float maxIndex = 0.0f, fbAmount = 0.0f;
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;

    float vel = 0.0f, gain = 0.0f;
    int note = -1;
    bool held = false;
    std::uint64_t order = 0;
};

} // namespace bleep::dsp
