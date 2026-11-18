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
}

/** Step length in quarter notes for each seq_rate choice. */
inline constexpr double kRatePpq[] = { 1.0, 0.5, 0.25, 0.125 };
inline constexpr int kDefaultRateIndex = 2;  // 1/16

/** Ableton-style note name (MIDI 60 = C3). */
juce::String noteName (int midiNote);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace bleep::params
