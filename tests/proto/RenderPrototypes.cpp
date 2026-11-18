// Рендер сравнения подходов A/B/C и рецептов на ОДНОМ и том же 16-шаговом паттерне.
// Использование: bleep_render_prototypes <папка>
//
// Все файлы выровнены по громкости (RMS сухого бипа = -21 dBFS), чтобы сравнение
// было честным: громче не значит лучше. Исходные уровни печатаются в отчёте.

#include "../DemoAudio.h"
#include "Prototypes.h"
#include "dsp/StereoDelay.h"
#include "dsp/SynthEngine.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace proto;

namespace
{
constexpr double kSr = demo::kSampleRate;
constexpr int kBars = 4;
constexpr float kTargetRmsDb = -21.0f;

// Гипнотический паттерн: офбиты и синкопы, в основном тоника, одна октава вниз,
// одна квинта; акценты velocity; разная длина нот (gate — доля шага).
struct Hit { int step, offset; float vel, gate; };
const Hit kPattern[] = {
    { 2, 0, 0.95f, 0.5f }, { 3, 0, 0.55f, 0.3f }, { 6, 0, 1.0f, 0.6f }, { 9, 0, 0.6f, 0.3f },
    { 10, 0, 0.9f, 0.5f }, { 11, 7, 0.5f, 0.3f }, { 13, -12, 0.85f, 0.8f }, { 14, 0, 0.75f, 0.4f },
};
constexpr int kRoot = 55;   // G2 по Ableton (196 Гц): над бочкой и басом

/** Абстрактный «инструмент»: noteOn/noteOff/render. */
struct Instrument
{
    std::function<void (int, float)> noteOn;
    std::function<void (int)> noteOff;
    std::function<void (float*, int)> render;
};

std::vector<float> playPattern (Instrument& inst, int bars)
{
    const double step = demo::samplesPerBeat() / 4.0;
    const size_t frames = (size_t) (step * 16 * bars) + (size_t) (kSr * 0.5);   // + хвост
    struct Ev { size_t at; int note; float vel; };
    std::vector<Ev> evs;
    for (int bar = 0; bar < bars; ++bar)
        for (const auto& h : kPattern)
        {
            const size_t on = (size_t) ((bar * 16 + h.step) * step);
            evs.push_back ({ on, kRoot + h.offset, h.vel });
            evs.push_back ({ on + (size_t) (h.gate * step), kRoot + h.offset, 0.0f });
        }
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.at < b.at || (a.at == b.at && a.vel < b.vel); });

    std::vector<float> out (frames, 0.0f);
    size_t pos = 0, e = 0;
    while (pos < frames)
    {
        const size_t next = e < evs.size() ? std::min (evs[e].at, frames) : frames;
        while (pos < next)
        {
            const int n = (int) std::min<size_t> (256, next - pos);
            inst.render (out.data() + pos, n);
            pos += (size_t) n;
        }
        while (e < evs.size() && evs[e].at <= pos)
        {
            if (evs[e].vel > 0.0f) inst.noteOn (evs[e].note, evs[e].vel);
            else                   inst.noteOff (evs[e].note);
            ++e;
        }
    }
    return out;
}

struct Stats { float peak, rmsDb; };
Stats measure (const std::vector<float>& x)
{
    double sum = 0; float peak = 0; size_t active = 0;
    for (float v : x) { peak = std::max (peak, std::abs (v)); if (std::abs (v) > 1e-4f) { sum += v * v; ++active; } }
    return { peak, active ? 10.0f * (float) std::log10 (sum / (double) active) : -120.0f };
}

struct Post { float busDriveDb = 0.0f, delayWet = 0.0f, delayFb = 0.45f, delayHp = 350.0f, delayLp = 2800.0f, spread = 0.6f; };

/** Стерео-рендер: (опционально) сатурация шины + дилэй + (опционально) ударные. */
void writeVersion (const std::string& path, const std::vector<float>& dry, const Post& post, bool drums, float gain)
{
    bleep::dsp::Saturator bus;
    bus.set (post.busDriveDb, 0.3f);
    bleep::dsp::StereoDelay delay;
    delay.prepare (kSr);
    delay.set ((float) (0.75 * 60.0 / demo::kBpm), post.delayFb, post.delayHp, post.delayLp, post.delayWet, post.spread);

    const auto loop = drums ? demo::drumLoop (dry.size(), kBars + 1) : std::vector<float> (dry.size(), 0.0f);
    std::vector<float> l (dry.size()), r (dry.size());
    int clipped = 0;
    for (size_t i = 0; i < dry.size(); ++i)
    {
        float x = dry[i] * gain;
        if (post.busDriveDb > 0.0f) x = bus.process (x);
        float dl = x, dr = x;
        if (post.delayWet > 0.0f) delay.process (x, dl, dr);
        l[i] = dl + loop[i] * 0.5f;
        r[i] = dr + loop[i] * 0.5f;
        clipped += (std::abs (l[i]) > 0.999f || std::abs (r[i]) > 0.999f) ? 1 : 0;
    }
    demo::writeWav (path, l, r);
    const auto sl = measure (l);
    std::printf ("    -> %-44s пик %.2f, RMS %.1f dBFS%s\n", path.substr (path.find_last_of ('/') + 1).c_str(),
                 sl.peak, sl.rmsDb, clipped ? "  КЛИППИНГ!" : "");
}

// --- Инструменты ---
Instrument makeCurrent()
{
    auto e = std::make_shared<bleep::dsp::SynthEngine>();
    bleep::dsp::EngineParams p;   // значения по умолчанию текущего плагина (Phase A)
    p.fmRatio = 2; p.fmDepth = 1.5f; p.fmDecay = 0.09f; p.pitchAmount = 7; p.pitchDecay = 0.012f; p.pitchCurve = 0.7f;
    p.cutoff = 4500; p.resonance = 0.15f; p.filterEnv = 1; p.filterDecay = 0.12f;
    p.attack = 0.001f; p.decay = 0.18f; p.release = 0.12f; p.outputGainDb = 0.0f;
    e->setParams (p);
    e->prepare (kSr);
    return { [e] (int n, float v) { e->noteOn (n, v); }, [e] (int n) { e->noteOff (n); },
             [e] (float* o, int n) { e->render (o, n); } };
}

template <typename V, typename P>
Instrument makeProto (const P& params)
{
    auto poly = std::make_shared<Poly<V, P>>();
    poly->prepare ((float) kSr, params);
    return { [poly] (int n, float v) { poly->noteOn (n, v); }, [poly] (int n) { poly->noteOff (n); },
             [poly] (float* o, int n) { poly->render (o, n); } };
}

struct Candidate
{
    std::string name, description;
    std::function<Instrument()> make;
    Post post;
};

double cpuPercent (const std::function<Instrument()>& make)
{
    // Плотная нагрузка: новая нота каждые 30 мс (до 8 звучащих голосов), 10 с аудио.
    auto inst = make();
    std::vector<float> buf (256);
    const int blocks = (int) (kSr * 10.0 / 256);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        if (b % 6 == 0) inst.noteOn (48 + (b / 6) % 24, 0.9f);
        if (b % 6 == 5) inst.noteOff (48 + (b / 6) % 24);
        inst.render (buf.data(), 256);
    }
    return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 10.0 * 100.0;
}
} // namespace

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const Post dryPost {};
    const Post fxPost { 4.0f, 0.22f, 0.45f, 350.0f, 2800.0f, 0.6f };

    // --- Базовые варианты подходов A / B / C ---
    DarkFmParams a;            // A: тёмный FM (значения по умолчанию структуры)
    ResonantParams b;          // B: «полое дерево» — гармонические моды 1:2:3, короткий шумовой удар
    b.ratios = { 1.0f, 2.0f, 3.0f }; b.gains = { 1.0f, 0.3f, 0.12f }; b.decay = 0.32f;
    HollowParams c;            // C: треугольник + импульс, резонансный ФНЧ

    std::vector<Candidate> bases = {
        { "current", "текущий движок (Phase A, значения по умолчанию)", [] { return makeCurrent(); }, dryPost },
        { "A_dark_fm", "A. Dark FM", [a] { return makeProto<DarkFmVoice> (a); }, dryPost },
        { "B_resonant", "B. Resonant (модальный)", [b] { return makeProto<ResonantVoice> (b); }, dryPost },
        { "C_hollow_analog", "C. Hollow Analog", [c] { return makeProto<HollowVoice> (c); }, dryPost },
    };

    // --- Рецепты (каждый — один из подходов с согласованными настройками) ---
    HollowParams darkHollow; darkHollow.cutoff = 480; darkHollow.resonance = 0.6f; darkHollow.filterEnvOct = 1.8f;
    darkHollow.filterDecay = 0.07f; darkHollow.pulseMix = 0.25f; darkHollow.decay = 0.3f; darkHollow.driveDb = 6;

    ResonantParams deep; deep.ratios = { 1.0f, 2.01f, 3.0f }; deep.gains = { 1.0f, 0.22f, 0.07f };
    deep.decays = { 1.0f, 0.5f, 0.3f }; deep.decay = 0.6f; deep.clickLevel = 0.6f; deep.noiseLevel = 0.3f;
    deep.noiseCutoff = 1200; deep.toneCutoff = 2500; deep.pitchAmount = -0.5f;

    ResonantParams metal; metal.ratios = { 1.0f, 2.76f, 5.40f }; metal.gains = { 1.0f, 0.5f, 0.25f };
    metal.decays = { 1.0f, 0.5f, 0.25f }; metal.decay = 0.18f; metal.noiseCutoff = 6000; metal.toneCutoff = 4000;
    metal.driveDb = 3; metal.pitchAmount = 0.0f;

    HollowParams rubber; rubber.pulseMix = 0.6f; rubber.pulseWidth = 0.25f; rubber.cutoff = 420; rubber.resonance = 0.7f;
    rubber.filterEnvOct = 2.8f; rubber.filterDecay = 0.045f; rubber.pitchAmount = -3.0f; rubber.pitchDecay = 0.025f;
    rubber.decay = 0.2f; rubber.driveDb = 8;

    DarkFmParams dub; dub.ratio = 1.0f; dub.indexPeak = 1.6f; dub.indexBody = 0.25f; dub.cutoff = 800; dub.resonance = 0.45f;
    dub.decay = 0.35f; dub.pitchAmount = 1.0f;

    DarkFmParams organic; organic.ratio = 1.41f; organic.indexPeak = 2.5f; organic.indexBody = 0.2f; organic.indexDecay = 0.025f;
    organic.feedback = 0.1f; organic.cutoff = 1500; organic.resonance = 0.3f; organic.curve = 0.6f; organic.driveDb = 4;

    std::vector<Candidate> recipes = {
        { "recipe_dark_hollow", "Dark Hollow (C)", [darkHollow] { return makeProto<HollowVoice> (darkHollow); },
          { 3.0f, 0.18f, 0.4f, 400.0f, 2200.0f, 0.5f } },
        { "recipe_deep_resonance", "Deep Resonance (B)", [deep] { return makeProto<ResonantVoice> (deep); },
          { 3.0f, 0.15f, 0.4f, 300.0f, 2000.0f, 0.5f } },
        { "recipe_muted_metallic", "Muted Metallic (B)", [metal] { return makeProto<ResonantVoice> (metal); },
          { 2.0f, 0.15f, 0.35f, 500.0f, 3500.0f, 0.6f } },
        { "recipe_rubber_pulse", "Rubber Pulse (C)", [rubber] { return makeProto<HollowVoice> (rubber); },
          { 4.0f, 0.12f, 0.35f, 350.0f, 2500.0f, 0.4f } },
        { "recipe_dub_signal", "Dub Signal (A)", [dub] { return makeProto<DarkFmVoice> (dub); },
          { 3.0f, 0.38f, 0.6f, 450.0f, 1800.0f, 0.9f } },
        { "recipe_organic_fm", "Organic FM (A)", [organic] { return makeProto<DarkFmVoice> (organic); },
          { 3.0f, 0.18f, 0.4f, 350.0f, 2800.0f, 0.6f } },
    };

    std::printf ("Паттерн: 16 шагов, корень G2 (MIDI %d), %d такта, %.0f BPM. Цель: RMS сухого бипа %.0f dBFS.\n\n",
                 kRoot, kBars, demo::kBpm, kTargetRmsDb);

    int fileNo = 1;
    auto num = [&fileNo] { char b[16]; std::snprintf (b, sizeof b, "%02d_", fileNo++); return std::string (b); };

    std::vector<std::vector<float>> baseDry;
    std::vector<float> baseGain;
    for (auto& cand : bases)
    {
        auto inst = cand.make();
        auto dry = playPattern (inst, kBars);
        const auto st = measure (dry);
        const float gain = std::pow (10.0f, (kTargetRmsDb - st.rmsDb) / 20.0f);
        std::printf ("%-36s исходно: пик %.2f, RMS %.1f dBFS, CPU %.1f%%\n", cand.description.c_str(), st.peak, st.rmsDb, cpuPercent (cand.make));
        baseDry.push_back (dry);
        baseGain.push_back (gain);
    }

    std::printf ("\nСухие (без эффектов):\n");
    for (size_t i = 0; i < bases.size(); ++i)
        writeVersion (dir + "/" + num() + bases[i].name + "_dry.wav", baseDry[i], dryPost, false, baseGain[i]);

    std::printf ("Обработанные (сатурация шины + фильтрованный дилэй):\n");
    for (size_t i = 1; i < bases.size(); ++i)
        writeVersion (dir + "/" + num() + bases[i].name + "_fx.wav", baseDry[i], fxPost, false, baseGain[i]);

    std::printf ("С ударной петлёй (140 BPM):\n");
    for (size_t i = 0; i < bases.size(); ++i)
        writeVersion (dir + "/" + num() + bases[i].name + "_drums.wav", baseDry[i], i == 0 ? dryPost : fxPost, true, baseGain[i]);

    std::printf ("A/B: текущий -> A -> B -> C подряд, по 2 такта, с ударными:\n");
    {
        std::vector<float> ab;
        const size_t twoBars = (size_t) (demo::samplesPerBeat() * 8);
        for (size_t i = 0; i < bases.size(); ++i)
            for (size_t k = 0; k < twoBars; ++k)
                ab.push_back (baseDry[i][k] * baseGain[i]);
        ab.resize (ab.size() + (size_t) (kSr * 0.5), 0.0f);
        // Ударные на всю длину (8 тактов); эффекты не применяем, чтобы сравнивать сам тембр.
        writeVersion (dir + "/" + num() + "AB_current_A_B_C_drums.wav", ab, dryPost, true, 1.0f);
    }

    std::printf ("Рецепты (с эффектами и ударными):\n");
    for (auto& rcp : recipes)
    {
        auto inst = rcp.make();
        auto dry = playPattern (inst, kBars);
        const auto st = measure (dry);
        const float gain = std::pow (10.0f, (kTargetRmsDb - st.rmsDb) / 20.0f);
        std::printf ("  %-24s исходно: пик %.2f, RMS %.1f dBFS, CPU %.1f%%\n", rcp.description.c_str(), st.peak, st.rmsDb, cpuPercent (rcp.make));
        writeVersion (dir + "/" + num() + rcp.name + "_drums.wav", dry, rcp.post, true, gain);
    }
    return 0;
}
