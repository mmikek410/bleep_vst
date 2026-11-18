#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bleep::params
{

// Parameter IDs are part of the saved state and automation: never rename them.
namespace id
{
    inline constexpr const char* attack     = "amp_attack";
    inline constexpr const char* decay      = "amp_decay";
    inline constexpr const char* sustain    = "amp_sustain";
    inline constexpr const char* release    = "amp_release";
    inline constexpr const char* fineTune   = "osc_fine";
    inline constexpr const char* outputGain = "out_gain";

    // Phase 2 / Milestone 1: sequencer
    inline constexpr const char* seqMode    = "seq_mode";    // 0 = External MIDI, 1 = Internal
    inline constexpr const char* seqRate    = "seq_rate";    // 1/4, 1/8, 1/16, 1/32
    inline constexpr const char* seqLength  = "seq_length";
    inline constexpr const char* seqRoot    = "seq_root";
    inline constexpr const char* seqGate    = "seq_gate";

    // Phase 2 / Milestone 2: generative controls
    inline constexpr const char* genDensity   = "gen_density";    // %
    inline constexpr const char* genRange     = "gen_range";      // semitones
    inline constexpr const char* genMutate    = "gen_mutate";     // %
    inline constexpr const char* genRandSound = "gen_rand_sound"; // bool

    // Sound Engine 2.0, Phase A: FM, огибающая высоты, резонансный фильтр
    inline constexpr const char* fmRatio     = "fm_ratio";      // модулятор : несущая
    inline constexpr const char* fmDepth     = "fm_depth";      // индекс модуляции
    inline constexpr const char* fmFeedback  = "fm_feedback";   // %
    inline constexpr const char* fmDecay     = "fm_decay";      // мс, огибающая индекса
    inline constexpr const char* pitchAmount = "pitch_amount";  // полутоны, ±
    inline constexpr const char* pitchDecay  = "pitch_decay";   // мс
    inline constexpr const char* pitchCurve  = "pitch_curve";   // %
    inline constexpr const char* filterMode  = "flt_mode";      // 0 = LP, 1 = BP
    inline constexpr const char* cutoff      = "flt_cutoff";    // Гц
    inline constexpr const char* resonance   = "flt_res";       // %
    inline constexpr const char* filterEnv   = "flt_env";       // октавы, ±
    inline constexpr const char* filterDecay = "flt_decay";     // мс
}

/** Значения новых звуковых параметров, которые воспроизводят старый синус.
    Применяются при загрузке состояний версий 1–3 (сохранённых до Phase A),
    чтобы старые проекты звучали как раньше, а новый экземпляр — с новым звуком. */
struct LegacyValue { const char* id; float plainValue; };
inline constexpr LegacyValue kPreSoundEngineValues[] = {
    { id::fmRatio, 1.0f },    { id::fmDepth, 0.0f },     { id::fmFeedback, 0.0f },  { id::fmDecay, 250.0f },
    { id::pitchAmount, 0.0f }, { id::pitchDecay, 30.0f }, { id::pitchCurve, 50.0f },
    { id::filterMode, 0.0f },  { id::cutoff, 20000.0f },  { id::resonance, 0.0f },
    { id::filterEnv, 0.0f },   { id::filterDecay, 200.0f } };

/** Step length in quarter notes for each seq_rate choice. */
inline constexpr double kRatePpq[] = { 1.0, 0.5, 0.25, 0.125 };
inline constexpr int kDefaultRateIndex = 2;  // 1/16

/** Ableton-style note name (MIDI 60 = C3). */
juce::String noteName (int midiNote);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace bleep::params
