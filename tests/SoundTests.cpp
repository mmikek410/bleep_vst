// Тесты звукового движка Phase A (без JUCE): точность высоты, FM-спектр,
// алиасинг, огибающая высоты, фильтр, устойчивость, сглаживание, CPU.
// Спектральные проверки делаются алгоритмом Гёрцеля на конкретных частотах.

#include "dsp/SynthEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using bleep::dsp::EngineParams;
using bleep::dsp::SynthEngine;

static int failures = 0;
#define CHECK(cond, ...)                                        \
    do {                                                        \
        if (! (cond)) {                                         \
            ++failures;                                         \
            std::printf ("  FAIL %s:%d: ", __FILE__, __LINE__);  \
            std::printf (__VA_ARGS__);                          \
            std::printf ("\n");                                 \
        }                                                       \
    } while (0)

static double midiHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

// Амплитуда компоненты частоты f (Гёрцель с окном Ханна).
static double magnitudeAt (const std::vector<float>& x, size_t from, size_t len, double f, double sr)
{
    const double w = 2.0 * M_PI * f / sr;
    double s1 = 0, s2 = 0, norm = 0;
    for (size_t n = 0; n < len; ++n)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * (double) n / (double) (len - 1));
        const double s0 = x[from + n] * win + 2.0 * std::cos (w) * s1 - s2;
        s2 = s1; s1 = s0; norm += win;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / norm;
}

// Доля энергии вне гармоник f0 (± guard бинов): алиасы + шум. Полное FFT,
// окно Блэкмана–Харриса (боковые лепестки ≈ -92 дБ, чтобы не мерить утечку окна).
static double nonHarmonicRatio (const std::vector<float>& x, size_t from, double f0, double sr, double maxHz = 1e9)
{
    const size_t n = 16384;
    std::vector<double> re (n), im (n, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = 2.0 * M_PI * (double) i / (double) (n - 1);
        re[i] = x[from + i] * (0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t));
    }
    for (size_t i = 1, j = 0; i < n; ++i)   // перестановка бит
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        for (size_t i = 0; i < n; i += len)
            for (size_t k = 0; k < len / 2; ++k)
            {
                const double wr = std::cos (ang * (double) k), wi = std::sin (ang * (double) k);
                const double ur = re[i + k], ui = im[i + k];
                const double vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
                const double vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
                re[i + k] = ur + vr; im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
            }
    }
    const double binHz = sr / (double) n;
    const double guard = 6.0;   // бины вокруг каждой гармоники (главный лепесток BH = ±4)
    double total = 0, other = 0;
    for (size_t b = 1; b < n / 2; ++b)
    {
        const double p = re[b] * re[b] + im[b] * im[b];
        const double f = (double) b * binHz;
        const double nearest = std::round (f / f0) * f0;
        if (f > maxHz) continue;
        total += p;
        if (nearest < f0 * 0.5 || std::abs (f - nearest) > guard * binHz)
            other += p;
    }
    return other / total;
}

static double db (double ratio) { return 20.0 * std::log10 (std::max (ratio, 1e-12)); }

// Держит одну ноту numSamples сэмплов (с sustain = 1, чтобы тон был стабильным).
static std::vector<float> holdNote (EngineParams p, double sr, int note, float vel, int numSamples, int block = 256)
{
    p.sustain = 1.0f; p.attack = 0.001f; p.decay = 0.05f;
    SynthEngine e;
    e.setParams (p);
    e.prepare (sr);
    e.noteOn (note, vel);
    std::vector<float> out ((size_t) numSamples);
    for (int i = 0; i < numSamples; i += block)
        e.render (out.data() + i, std::min (block, numSamples - i));
    return out;
}

// Частота по пересечениям нуля с линейной интерполяцией. Порог отсекает
// микроскопический «звон» дециматора в первые сэмплы ноты (< -100 дБ).
static double zeroCrossFreq (const std::vector<float>& x, size_t from, size_t to, double sr)
{
    double first = -1, last = -1; int count = 0;
    for (size_t i = from + 1; i < to; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f && std::abs (x[i] - x[i - 1]) > 1.0e-4f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? sr * count / (last - first) : 0.0;
}

int main()
{
    std::printf ("высота тона: чистый синус совпадает с MIDI-нотой (< 0.5 цента)\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int note : { 36, 48, 60, 72, 84, 96 })
        {
            EngineParams p;   // нейтральные параметры = синус
            auto x = holdNote (p, sr, note, 1.0f, (int) sr);
            const double f = zeroCrossFreq (x, (size_t) (sr * 0.2), x.size(), sr);
            const double cents = 1200.0 * std::log2 (f / midiHz (note));
            CHECK (std::abs (cents) < 0.5, "sr=%.0f note=%d: %.3f cents", sr, note, cents);
        }

    std::printf ("FM: гармонические и негармонические отношения дают нужные боковые полосы\n");
    {
        const double sr = 48000.0, f0 = midiHz (57);  // A2 = 110 Гц
        EngineParams pure;
        auto x = holdNote (pure, sr, 57, 1.0f, 48000);
        const double h1 = magnitudeAt (x, 8000, 32768, f0, sr);
        CHECK (db (magnitudeAt (x, 8000, 32768, 2 * f0, sr) / h1) < -60, "у синуса есть 2-я гармоника");

        EngineParams fm; fm.fmRatio = 1.0f; fm.fmDepth = 2.0f; fm.fmDecay = 10.0f;
        x = holdNote (fm, sr, 57, 1.0f, 48000);
        const double a1 = magnitudeAt (x, 8000, 32768, f0, sr);
        for (int h = 2; h <= 4; ++h)
            CHECK (db (magnitudeAt (x, 8000, 32768, h * f0, sr) / a1) > -30, "ratio 1: нет гармоники %d", h);

        EngineParams inh = fm; inh.fmRatio = 1.41f;
        x = holdNote (inh, sr, 57, 1.0f, 48000);
        const double c = magnitudeAt (x, 8000, 32768, f0, sr);
        const double side = magnitudeAt (x, 8000, 32768, f0 * 2.41, sr);  // f + 1.41f
        const double harm2 = magnitudeAt (x, 8000, 32768, f0 * 2.0, sr);  // не должно быть
        std::printf ("  ratio 1.41: боковая 2.41f = %.1f дБ, 2f = %.1f дБ\n", db (side / c), db (harm2 / c));
        CHECK (db (side / c) > -30, "ratio 1.41: нет боковой полосы f+1.41f");
        CHECK (db (harm2 / c) < -45, "ratio 1.41 не должен давать гармонику 2f");
    }

    std::printf ("алиасинг: энергия вне гармоник (полоса до 16 кГц) при разных настройках\n");
    {
        // Без обратной связи и с умеренной (≤ 0.3) — должно быть чисто при любых нотах и индексах.
        struct Case { int note; float ratio, depth, fb; double limitDb; };
        const Case cases[] = {
            { 96, 8.0f, 10.0f, 0.0f, -45 }, { 84, 3.0f, 8.0f, 0.0f, -50 }, { 84, 7.0f, 5.0f, 0.0f, -45 },
            { 72, 3.0f, 8.0f, 0.3f, -45 }, { 60, 2.0f, 4.0f, 0.3f, -45 }, { 48, 1.0f, 10.0f, 0.3f, -45 } };
        // (Метод годится только для целых ratio: тогда все честные частоты кратны f0.)
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (const auto& c : cases)
            {
                EngineParams p; p.fmRatio = c.ratio; p.fmDepth = c.depth; p.fmFeedback = c.fb; p.fmDecay = 1000.0f;
                auto x = holdNote (p, sr, c.note, 1.0f, (int) sr);
                const double f0 = midiHz (c.note);
                const double aliasDb = 10.0 * std::log10 (nonHarmonicRatio (x, (size_t) (sr * 0.3), f0, sr, 16000.0));
                CHECK (aliasDb < c.limitDb, "sr=%.0f note=%d ratio=%.2f idx=%.1f fb=%.1f: %.1f дБ (порог %.0f)",
                           sr, c.note, c.ratio, c.depth, c.fb, aliasDb, c.limitDb);
            }
        // Для отчёта: полный спектр (до Найквиста) и сильная обратная связь — не порог, а информация.
        for (float fb : { 0.5f, 1.0f })
        {
            EngineParams p; p.fmRatio = 1.0f; p.fmDepth = 6.0f; p.fmFeedback = fb; p.fmDecay = 1000.0f;
            auto x = holdNote (p, 44100.0, 84, 1.0f, 44100);
            std::printf ("  справка: C6 ratio 1 idx 6 fb %.1f -> %.1f дБ вне гармоник (характер обратной связи)\n",
                         fb, 10.0 * std::log10 (nonHarmonicRatio (x, 13230, midiHz (84), 44100.0)));
        }
    }

    std::printf ("FM: насыщенность сохраняется — защита от алиасинга не душит тембр\n");
    {
        const double sr = 44100.0;
        EngineParams p; p.fmRatio = 3.0f; p.fmDepth = 8.0f; p.fmFeedback = 0.3f; p.fmDecay = 1000.0f;
        const int note = 84;
        const double f0 = midiHz (note);
        auto x = holdNote (p, sr, note, 1.0f, (int) sr);
        int partials = 0;
        const double fund = magnitudeAt (x, 13230, 16384, f0, sr);
        for (int h = 1; h * f0 < sr * 0.45; ++h)
            partials += db (magnitudeAt (x, 13230, 16384, h * f0, sr) / fund) > -40 ? 1 : 0;
        std::printf ("  C6 ratio 3 idx 8: %d слышимых гармоник\n", partials);
        CHECK (partials >= 8, "FM задавлен защитой от алиасинга");
    }

    std::printf ("CPU-калибровка: старый синусовый голос на этой машине\n");
    {
        const double sr = 96000.0;
        EngineParams p; p.sustain = 1.0f;
        SynthEngine e;
        e.setParams (p);
        e.prepare (sr);
        for (int n = 0; n < 8; ++n) e.noteOn (48 + n * 3, 1.0f);
        std::vector<float> buf (512);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < (int) (sr * 20.0 / 512); ++b) e.render (buf.data(), 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  нейтральные параметры (синус): %.1f%% одного ядра\n", secs / 20.0 * 100.0);
    }

    std::printf ("огибающая высоты: +12 полутонов в начале, затем базовая высота; знак работает\n");
    for (float amount : { 12.0f, -12.0f })
    {
        const double sr = 48000.0;
        EngineParams p; p.pitchAmount = amount; p.pitchDecay = 0.3f; p.pitchCurve = 0.0f;
        auto x = holdNote (p, sr, 60, 1.0f, 48000, 64);
        const double early = zeroCrossFreq (x, 0, 1440, sr);         // первые 30 мс
        const double late  = zeroCrossFreq (x, 24000, 48000, sr);    // после 500 мс
        const double base = midiHz (60);
        std::printf ("  amount %+.0f: начало %.1f Гц, конец %.1f Гц (база %.1f)\n", amount, early, late, base);
        CHECK (std::abs (late / base - 1.0) < 0.002, "после огибающей высота не вернулась");
        if (amount > 0) CHECK (early > base * 1.6, "+12: старт недостаточно высокий");
        else            CHECK (early < base * 0.65, "-12: старт недостаточно низкий");
    }

    std::printf ("огибающая FM и velocity: яркость в атаке выше, сильная нота ярче\n");
    {
        const double sr = 48000.0, f0 = midiHz (60);
        EngineParams p; p.fmRatio = 1.0f; p.fmDepth = 3.0f; p.fmDecay = 0.15f;
        auto x = holdNote (p, sr, 60, 1.0f, 48000);
        auto bright = [&] (const std::vector<float>& y, size_t from) {
            return magnitudeAt (y, from, 2048, 3 * f0, sr) / magnitudeAt (y, from, 2048, f0, sr);
        };
        CHECK (bright (x, 0) > bright (x, 30000) * 10.0, "огибающая индекса FM не затухает");
        auto soft = holdNote (p, sr, 60, 0.3f, 48000);
        CHECK (bright (x, 0) > bright (soft, 0) * 1.5, "velocity не влияет на яркость");
    }

    std::printf ("фильтр: LP режет верх, BP выделяет полосу\n");
    {
        const double sr = 48000.0, f0 = midiHz (45);  // 110 Гц, богатый спектр
        // Богатый спектр: ratio 1 + большая обратная связь ≈ пила.
        EngineParams rich; rich.fmRatio = 1.0f; rich.fmDepth = 6.0f; rich.fmFeedback = 1.0f; rich.fmDecay = 10.0f;
        auto open = holdNote (rich, sr, 45, 1.0f, 48000);
        EngineParams lp = rich; lp.cutoff = 200.0f;
        auto closed = holdNote (lp, sr, 45, 1.0f, 48000);
        const double at = 10 * f0;  // 1.1 кГц, на 2.5 октавы выше среза
        const double drop = db (magnitudeAt (closed, 8000, 16384, at, sr) / magnitudeAt (open, 8000, 16384, at, sr));
        std::printf ("  уровень 10-й гармоники в открытом звуке: %.1f дБ к основному\n",
                     db (magnitudeAt (open, 8000, 16384, at, sr) / magnitudeAt (open, 8000, 16384, f0, sr)));
        std::printf ("  LP 200 Гц: ослабление на 1.1 кГц = %.1f дБ (теория 2-полюсного ≈ -30)\n", drop);
        CHECK (drop < -22, "LP почти не режет");

        EngineParams bp = rich; bp.filterMode = 1; bp.cutoff = 1100.0f; bp.resonance = 0.6f;
        auto band = holdNote (bp, sr, 45, 1.0f, 48000);
        const double lowRatio  = magnitudeAt (band, 8000, 16384, f0, sr) / magnitudeAt (open, 8000, 16384, f0, sr);
        const double bandRatio = magnitudeAt (band, 8000, 16384, at, sr) / magnitudeAt (open, 8000, 16384, at, sr);
        std::printf ("  BP 1.1 кГц: основной тон %.1f дБ, полоса %.1f дБ\n", db (lowRatio), db (bandRatio));
        CHECK (db (bandRatio) - db (lowRatio) > 20, "BP не выделяет полосу");
    }

    std::printf ("устойчивость: случайные экстремальные параметры каждые 32 сэмпла\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        SynthEngine e;
        EngineParams p;
        e.setParams (p);
        e.prepare (sr);
        std::vector<float> buf (32);
        double peak = 0; bool finite = true;
        const int blocks = (int) (sr * 8.0 / 32);
        for (int b = 0; b < blocks; ++b)
        {
            p.fmRatio = 0.25f + 15.75f * u (rng);  p.fmDepth = 10.0f * u (rng);  p.fmFeedback = u (rng);
            p.fmDecay = 0.005f + 4.0f * u (rng);   p.pitchAmount = -48.0f + 96.0f * u (rng);
            p.pitchDecay = 0.001f + u (rng);       p.pitchCurve = u (rng);
            p.filterMode = u (rng) < 0.5f ? 0 : 1; p.cutoff = 20.0f + 19980.0f * u (rng) * u (rng);
            p.resonance = u (rng) < 0.3f ? 1.0f : u (rng);  p.filterEnv = -6.0f + 12.0f * u (rng);
            p.filterDecay = 0.005f + 2.0f * u (rng);        p.outputGainDb = 6.0f;
            p.decay = 0.01f + u (rng);
            e.setParams (p);
            if (b % 50 == 0)  e.noteOn (24 + (int) (u (rng) * 100), u (rng));
            if (b % 50 == 25) e.allNotesOff();
            e.render (buf.data(), 32);
            for (float v : buf) { finite &= std::isfinite (v); peak = std::max (peak, (double) std::abs (v)); }
        }
        std::printf ("  sr=%.0f пик %.3f\n", sr, peak);
        CHECK (finite, "sr=%.0f: NaN/Inf", sr);
        CHECK (peak <= 1.0, "sr=%.0f: пик %.3f выше 1.0", sr, peak);
    }

    std::printf ("сглаживание: резкий скачок среза не даёт щелчка\n");
    {
        const double sr = 48000.0;
        EngineParams p; p.fmDepth = 1.0f; p.fmDecay = 10.0f; p.cutoff = 300.0f; p.resonance = 0.5f;
        p.sustain = 1.0f;
        SynthEngine e;
        e.setParams (p);
        e.prepare (sr);
        e.noteOn (60, 1.0f);
        std::vector<float> a (4800), b (4800);
        e.render (a.data(), 4800);
        p.cutoff = 12000.0f;               // скачок на 5 октав
        e.setParams (p);
        e.render (b.data(), 4800);
        auto maxStep = [] (const std::vector<float>& v, size_t from, size_t to) {
            double m = 0; for (size_t i = from + 1; i < to; ++i) m = std::max (m, (double) std::abs (v[i] - v[i - 1])); return m; };
        const double around = maxStep (b, 0, 64);
        const double settled = maxStep (b, 2400, 4800);
        std::printf ("  макс. шаг сразу после скачка %.4f, в установившемся режиме %.4f\n", around, settled);
        CHECK (around <= settled * 1.2 + 1e-4, "скачок среза дал щелчок");
    }

    std::printf ("CPU: 8 голосов с FM + фильтром\n");
    {
        const double sr = 96000.0;
        EngineParams p; p.fmDepth = 4.0f; p.fmFeedback = 0.5f; p.fmRatio = 3.5f; p.cutoff = 3000.0f;
        p.resonance = 0.5f; p.filterEnv = 2.0f; p.pitchAmount = 12.0f; p.sustain = 1.0f;
        SynthEngine e;
        e.setParams (p);
        e.prepare (sr);
        for (int n = 0; n < 8; ++n) e.noteOn (48 + n * 3, 1.0f);
        std::vector<float> buf (512);
        const int blocks = (int) (sr * 20.0 / 512);   // 20 секунд аудио
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.render (buf.data(), 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  20 с аудио (96 кГц, 8 голосов) за %.3f с = %.1f%% одного ядра\n", secs, secs / 20.0 * 100.0);
        CHECK (e.getNumActiveVoices() == 8, "голоса потерялись");
        CHECK (secs / 20.0 < 0.25, "слишком тяжело для реального времени");
    }

    std::printf (failures == 0 ? "ALL SOUND TESTS PASSED\n" : "%d SOUND FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
