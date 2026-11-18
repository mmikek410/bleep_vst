#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

#include <cmath>

BleepAudioProcessor::BleepAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BLEEP001", bleep::params::createLayout())
{
    namespace id = bleep::params::id;
    attack     = apvts.getRawParameterValue (id::attack);
    decay      = apvts.getRawParameterValue (id::decay);
    sustain    = apvts.getRawParameterValue (id::sustain);
    release    = apvts.getRawParameterValue (id::release);
    fineTune   = apvts.getRawParameterValue (id::fineTune);
    outputGain = apvts.getRawParameterValue (id::outputGain);
    jassert (attack && decay && sustain && release && fineTune && outputGain);
}

bool BleepAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BleepAudioProcessor::prepareToPlay (double sampleRate, int)
{
    updateEngineParams();
    engine.prepare (sampleRate);  // handles any sample rate (44.1 / 48 / 96 kHz ...)
}

void BleepAudioProcessor::updateEngineParams() noexcept
{
    bleep::dsp::EngineParams p;
    p.attack       = attack->load() * 0.001f;
    p.decay        = decay->load() * 0.001f;
    p.sustain      = sustain->load();
    p.release      = release->load() * 0.001f;
    p.fineTune     = fineTune->load();
    p.outputGainDb = outputGain->load();
    engine.setParams (p);
}

void BleepAudioProcessor::handleMidi (const juce::MidiMessage& msg) noexcept
{
    if (msg.isNoteOn())
        engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isNoteOff())
        engine.noteOff (msg.getNoteNumber());
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        engine.allNotesOff();
}

void BleepAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    updateEngineParams();

    // Render into channel 0, splitting the block at each MIDI event so notes
    // start sample-accurately regardless of the host buffer size.
    float* mono = buffer.getWritePointer (0);
    int position = 0;

    for (const auto meta : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, meta.samplePosition);
        if (eventPos > position)
        {
            engine.render (mono + position, eventPos - position);
            position = eventPos;
        }
        handleMidi (meta.getMessage());
    }

    if (position < numSamples)
        engine.render (mono + position, numSamples - position);

    // Safety net: never pass NaN/Inf to the host.
    for (int i = 0; i < numSamples; ++i)
    {
        if (! std::isfinite (mono[i]))
        {
            engine.reset();
            buffer.clear();
            return;
        }
    }

    for (int ch = 1; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* BleepAudioProcessor::createEditor()
{
    return new BleepAudioProcessorEditor (*this);
}

void BleepAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", kStateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BleepAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    // Future: migrate based on the "stateVersion" property here.
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepAudioProcessor();
}
