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
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace bleep::params
