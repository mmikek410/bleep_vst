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

juce::String noteName (int midiNote)
{
    return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, midiNote), true, true, 3);
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
                                         Attributes().withLabel ("ct")
                                             .withStringFromValueFunction ([] (float v, int)
                                                 { const int c = juce::roundToInt (v); return (c > 0 ? "+" : "") + juce::String (c) + " ct"; })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::outputGain, 1 }, "Output",
                                         juce::NormalisableRange<float> { -48.0f, 6.0f }, -6.0f,
                                         Attributes().withLabel ("dB")
                                             .withStringFromValueFunction ([] (float v, int)
                                                 { return juce::String (v, 1) + " dB"; })));

    // --- Sequencer (new in Phase 2; IDs above are unchanged) ---
    // Default External keeps Phase 1 projects behaving exactly as before.
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::seqMode, 2 }, "Seq Mode",
                                                              juce::StringArray { "External MIDI", "Internal" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::seqRate, 2 }, "Seq Rate",
                                                              juce::StringArray { "1/4", "1/8", "1/16", "1/32" },
                                                              kDefaultRateIndex));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::seqLength, 2 }, "Seq Length",
                                                           1, 16, 16));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::seqRoot, 2 }, "Seq Root",
                                                           24, 96, 60,
                                                           juce::AudioParameterIntAttributes()
                                                               .withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::seqGate, 2 }, "Seq Gate",
                                         juce::NormalisableRange<float> { 5.0f, 100.0f }, 50.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int)
                                             { return juce::String (juce::roundToInt (v)) + " %"; })));

    // --- Generative (new in Milestone 2) ---
    auto percent = Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; });
    layout.add (std::make_unique<Float> (juce::ParameterID { id::genDensity, 3 }, "Density",
                                         juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 40.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::genRange, 3 }, "Pitch Range",
                                                           0, 24, 12,
                                                           juce::AudioParameterIntAttributes()
                                                               .withStringFromValueFunction ([] (int v, int) { return "+/-" + juce::String (v) + " st"; })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::genMutate, 3 }, "Mutation",
                                         juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 25.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::genRandSound, 3 }, "Randomize Sound", false));

    // --- Sound Engine 2.0 / Phase A (версия параметров 4) ---
    // Значения по умолчанию — короткий яркий бип с мягким хвостом, а не голый синус.
    auto ratioRange = juce::NormalisableRange<float> { 0.25f, 16.0f, 0.01f };
    ratioRange.setSkewForCentre (2.0f);
    layout.add (std::make_unique<Float> (juce::ParameterID { id::fmRatio, 4 }, "FM Ratio", ratioRange, 2.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));
    auto depthRange = juce::NormalisableRange<float> { 0.0f, 10.0f, 0.01f };
    depthRange.setSkewForCentre (2.0f);
    layout.add (std::make_unique<Float> (juce::ParameterID { id::fmDepth, 4 }, "FM Depth", depthRange, 1.5f,
                                         Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::fmFeedback, 4 }, "FM Feedback",
                                         juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 0.0f, percent));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::fmDecay, 4 }, "FM Decay",
                                         timeRange (5.0f, 4000.0f, 200.0f), 90.0f, msAttributes()));

    layout.add (std::make_unique<Float> (juce::ParameterID { id::pitchAmount, 4 }, "Pitch Env",
                                         juce::NormalisableRange<float> { -48.0f, 48.0f, 0.1f }, 7.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int)
                                             { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st"; })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::pitchDecay, 4 }, "Pitch Decay",
                                         timeRange (1.0f, 1000.0f, 60.0f), 12.0f, msAttributes()));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::pitchCurve, 4 }, "Pitch Curve",
                                         juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 70.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::filterMode, 4 }, "Filter Mode",
                                                              juce::StringArray { "LP", "BP" }, 0));
    auto cutoffRange = juce::NormalisableRange<float> { 20.0f, 20000.0f };
    cutoffRange.setSkewForCentre (1000.0f);
    layout.add (std::make_unique<Float> (juce::ParameterID { id::cutoff, 4 }, "Cutoff", cutoffRange, 4500.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int)
                                             { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz"
                                                                  : juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz"; })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::resonance, 4 }, "Resonance",
                                         juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 15.0f, percent));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::filterEnv, 4 }, "Filter Env",
                                         juce::NormalisableRange<float> { -6.0f, 6.0f, 0.01f }, 1.0f,
                                         Attributes().withStringFromValueFunction ([] (float v, int)
                                             { return (v > 0.005f ? "+" : "") + juce::String (v, 2) + " oct"; })));
    layout.add (std::make_unique<Float> (juce::ParameterID { id::filterDecay, 4 }, "Filter Decay",
                                         timeRange (5.0f, 4000.0f, 250.0f), 120.0f, msAttributes()));
    return layout;
}

} // namespace bleep::params
