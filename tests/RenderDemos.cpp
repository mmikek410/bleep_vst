// Рендер демо-WAV для оценки звука на слух (без JUCE).
// Использует настоящий движок, секвенсор и генератор плагина, плюс простую
// оригинальную ударную петлю (бочка, хэты, клэп, перкуссия) — всё синтезировано
// здесь же, без сэмплов.
//
// Использование: bleep_render_demos <папка>   -> несколько *.wav (48 кГц, 16 бит, стерео)

#include "dsp/SynthEngine.h"
#include "seq/Generator.h"
#include "seq/Sequencer.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace bleep;

#include "DemoAudio.h"

using namespace demo;

namespace
{
struct Demo
{
    const char* name;
    const char* description;
    dsp::EngineParams sound;
    seq::GeneratorSettings gen;
    std::uint32_t seed;
    int root;
    float gate;
};

/** Рендерит бипы секвенсором плагина (с настоящей «хост»-позицией в PPQ). */
std::vector<float> renderBleeps (const Demo& d, size_t frames)
{
    dsp::SynthEngine engine;
    engine.setParams (d.sound);
    engine.prepare (kSampleRate);

    seq::Sequencer sequencer;
    sequencer.prepare (kSampleRate);
    sequencer.setPattern (seq::generate (seq::Pattern {}, d.gen, d.seed));
    sequencer.setSeed (d.seed);
    seq::Settings st;
    st.enabled = true; st.length = 16; st.stepPpq = 0.25; st.rootNote = d.root; st.gate = d.gate;
    sequencer.setSettings (st);

    std::vector<float> out (frames, 0.0f);
    seq::NoteEvent events[256];
    const int block = 256;
    double ppq = 0.0;
    for (size_t start = 0; start < frames; start += block)
    {
        const int n = (int) std::min<size_t> (block, frames - start);
        seq::Transport t;
        t.hasInfo = t.isPlaying = t.hasPpq = t.hasBpm = true;
        t.ppq = ppq; t.bpm = kBpm;
        const int count = sequencer.process (t, n, events, 256);
        int pos = 0;
        for (int i = 0; i < count; ++i)
        {
            if (events[i].sample > pos) { engine.render (out.data() + start + pos, events[i].sample - pos); pos = events[i].sample; }
            if (events[i].velocity > 0.0f) engine.noteOn (events[i].note, events[i].velocity);
            else                           engine.noteOff (events[i].note);
        }
        if (pos < n) engine.render (out.data() + start + pos, n - pos);
        ppq += n * kBpm / (60.0 * kSampleRate);
    }
    return out;
}
} // namespace

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const size_t frames = (size_t) (samplesPerBeat() * 4 * kBars);
    const auto drums = drumLoop (frames);

    auto base = [] { dsp::EngineParams p; p.outputGainDb = -3.0f; return p; };

    std::vector<Demo> demos;
    {   // 0. Как было: голый синус (нейтральные параметры) — для сравнения.
        Demo d { "00_before_sine", "старый движок: синус + ADSR", base(), { 0.4f, 12 }, 11, 60, 0.5f };
        demos.push_back (d);
    }
    {   // 1. ROUND: мягкий FM, гармоническое отношение, тёплый ФНЧ.
        auto p = base();
        p.fmRatio = 1.0f; p.fmDepth = 1.4f; p.fmDecay = 0.16f;
        p.pitchAmount = 5.0f; p.pitchDecay = 0.008f; p.pitchCurve = 0.7f;
        p.cutoff = 2800.0f; p.resonance = 0.12f; p.filterEnv = 1.2f; p.filterDecay = 0.12f;
        p.decay = 0.16f; p.release = 0.08f;
        demos.push_back ({ "01_round_fm", "ROUND: тёплый FM-бип", p, { 0.4f, 7 }, 11, 60, 0.45f });
    }
    {   // 2. Металлический: негармоническое отношение, короткий, BP.
        auto p = base();
        p.fmRatio = 2.76f; p.fmDepth = 3.2f; p.fmDecay = 0.05f; p.fmFeedback = 0.15f;
        p.filterMode = 1; p.cutoff = 2400.0f; p.resonance = 0.35f; p.filterEnv = 0.8f; p.filterDecay = 0.06f;
        p.decay = 0.09f; p.release = 0.05f;
        demos.push_back ({ "02_metallic", "METALLIC: негармонический перкуссионный FM", p, { 0.45f, 5 }, 23, 67, 0.3f });
    }
    {   // 3. Лазер: сильная быстрая огибающая высоты.
        auto p = base();
        p.fmRatio = 2.0f; p.fmDepth = 1.0f; p.fmDecay = 0.08f;
        p.pitchAmount = 24.0f; p.pitchDecay = 0.035f; p.pitchCurve = 0.85f;
        p.cutoff = 6000.0f; p.resonance = 0.2f; p.filterEnv = 0.5f;
        p.decay = 0.14f; p.release = 0.06f;
        demos.push_back ({ "03_laser", "лазерные бипы: +24 полутона, быстрый спад", p, { 0.35f, 7 }, 37, 60, 0.4f });
    }
    {   // 4. Резонансный: низкий срез, высокий резонанс, огибающая фильтра.
        auto p = base();
        p.fmRatio = 1.0f; p.fmDepth = 0.6f; p.fmDecay = 0.3f;
        p.cutoff = 700.0f; p.resonance = 0.82f; p.filterEnv = 2.4f; p.filterDecay = 0.11f;
        p.decay = 0.22f; p.release = 0.1f;
        demos.push_back ({ "04_resonant", "RESONANT: звенящий фильтр", p, { 0.4f, 5 }, 41, 55, 0.5f });
    }
    {   // 5. «Резиновый»: отрицательная огибающая высоты, полый BP.
        auto p = base();
        p.fmRatio = 1.0f; p.fmDepth = 2.2f; p.fmDecay = 0.07f;
        p.pitchAmount = -12.0f; p.pitchDecay = 0.06f; p.pitchCurve = 0.3f;
        p.filterMode = 1; p.cutoff = 520.0f; p.resonance = 0.45f; p.filterEnv = 1.5f; p.filterDecay = 0.09f;
        p.decay = 0.15f; p.release = 0.07f;
        demos.push_back ({ "05_rubber", "ALIEN: резиновый, падающая высота", p, { 0.35f, 7 }, 53, 62, 0.5f });
    }

    for (const auto& d : demos)
    {
        const auto bleeps = renderBleeps (d, frames);
        std::vector<float> left (frames), right (frames);
        float peak = 0.0f;
        for (size_t i = 0; i < frames; ++i)
        {
            // Бипы немного в стороны (как будто трек с панорамой), ударные по центру.
            left[i]  = drums[i] * 0.55f + bleeps[i] * 0.95f;
            right[i] = drums[i] * 0.55f + bleeps[i] * 0.80f;
            peak = std::max ({ peak, std::abs (left[i]), std::abs (right[i]) });
        }
        const std::string path = dir + "/" + d.name + ".wav";
        writeWav (path, left, right);
        writeWav (dir + "/dry_" + d.name + ".wav", bleeps, bleeps);   // только бипы, без ударных

        float bleepPeak = 0.0f;
        for (float v : bleeps) bleepPeak = std::max (bleepPeak, std::abs (v));
        std::printf ("%-18s пик бипов %.2f, пик микса %.2f — %s\n", d.name, bleepPeak, peak, d.description);
    }
    return 0;
}
