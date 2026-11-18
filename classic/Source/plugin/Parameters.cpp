#include "Parameters.h"

namespace bleep::params
{

namespace
{
    using Float = juce::AudioParameterFloat;
    using Attributes = juce::AudioParameterFloatAttributes;

    // Time range with a skew so the short, percussive end gets most of the knob.
    juce::NormalisableRange<float> timeRange (float minMs, float maxMs, float centreMs)
    {
        juce::NormalisableRange<float> r { minMs, maxMs };
        r.setSkewForCentre (centreMs);
        return r;
    }

    Attributes msAttributes()
    {
        return Attributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int)
            {
                return v < 1000.0f ? juce::String (v, v < 10.0f ? 1 : 0) + " ms"
                                   : juce::String (v / 1000.0f, 2) + " s";
            });
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Float> (juce::ParameterID { id::attack, 1 }, "Attack",
                                         timeRange (0.5f, 2000.0f, 40.0f), 1.0f, msAttributes()));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::decay, 1 }, "Decay",
                                         timeRange (5.0f, 4000.0f, 250.0f), 180.0f, msAttributes()));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::sustain, 1 }, "Sustain",
                                         juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int)
                                             { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::release, 1 }, "Release",
                                         timeRange (5.0f, 4000.0f, 250.0f), 120.0f, msAttributes()));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::fineTune, 1 }, "Fine Tune",
                                         juce::NormalisableRange<float> { -100.0f, 100.0f }, 0.0f,
                                         Attributes().withLabel ("ct")));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::outputGain, 1 }, "Output",
                                         juce::NormalisableRange<float> { -48.0f, 6.0f }, -6.0f,
                                         Attributes().withLabel ("dB")));
    return layout;
}

} // namespace bleep::params
