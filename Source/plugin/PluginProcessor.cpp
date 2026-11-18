#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

#include <climits>
#include <cmath>

namespace
{
    const juce::Identifier kPatternTag { "PATTERN" };
    const juce::Identifier kPatternData { "data" };
    const juce::Identifier kStateVersionProp { "stateVersion" };
}

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
    seqMode    = apvts.getRawParameterValue (id::seqMode);
    seqRate    = apvts.getRawParameterValue (id::seqRate);
    seqLength  = apvts.getRawParameterValue (id::seqLength);
    seqRoot    = apvts.getRawParameterValue (id::seqRoot);
    seqGate    = apvts.getRawParameterValue (id::seqGate);
    jassert (attack && decay && sustain && release && fineTune && outputGain
             && seqMode && seqRate && seqLength && seqRoot && seqGate);

    audioPattern = patternStore.get();
    audioPatternVersion = patternStore.getVersion();
    sequencer.setPattern (audioPattern);
    sequencer.setSeed (0xB1EE9001u);
}

bool BleepAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BleepAudioProcessor::prepareToPlay (double sampleRate, int)
{
    updateEngineParams();
    engine.prepare (sampleRate);  // any sample rate (44.1 / 48 / 96 kHz ...)
    sequencer.prepare (sampleRate);
    updateSequencerSettings();
    currentStep.store (-1);
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

void BleepAudioProcessor::updateSequencerSettings() noexcept
{
    bleep::seq::Settings s;
    s.enabled  = seqMode->load() >= 0.5f;
    const int rateIndex = juce::jlimit (0, 3, (int) std::lround (seqRate->load()));
    s.stepPpq  = bleep::params::kRatePpq[rateIndex];
    s.length   = juce::jlimit (1, 16, (int) std::lround (seqLength->load()));
    s.rootNote = (int) std::lround (seqRoot->load());
    s.gate     = seqGate->load() * 0.01f;
    sequencer.setSettings (s);
}

bleep::seq::Transport BleepAudioProcessor::readTransport() const noexcept
{
    bleep::seq::Transport t;
    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto pos = hostPlayHead->getPosition())
        {
            t.hasInfo = true;
            t.isPlaying = pos->getIsPlaying();
            if (const auto ppq = pos->getPpqPosition())
            {
                t.hasPpq = true;
                t.ppq = *ppq;
            }
            if (const auto bpm = pos->getBpm())
            {
                t.hasBpm = true;
                t.bpm = *bpm;
            }
        }
    }
    return t;
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

void BleepAudioProcessor::handleSeqEvent (const bleep::seq::NoteEvent& e) noexcept
{
    if (e.velocity > 0.0f)
        engine.noteOn (e.note, e.velocity);
    else
        engine.noteOff (e.note);
}

void BleepAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    updateEngineParams();
    updateSequencerSettings();

    if (patternStore.tryCopyIfChanged (audioPattern, audioPatternVersion))
        sequencer.setPattern (audioPattern);

    const int numSeq = sequencer.process (readTransport(), numSamples, seqEvents.data(), kMaxSeqEvents);
    currentStep.store (sequencer.getCurrentStep(), std::memory_order_relaxed);

    // Merge host MIDI and sequencer events in time order, rendering audio
    // between events so every note starts on its exact sample.
    float* mono = buffer.getWritePointer (0);
    int position = 0;
    int seqIndex = 0;
    auto midiIt = midi.cbegin();
    const auto midiEnd = midi.cend();

    for (;;)
    {
        const int midiPos = midiIt != midiEnd ? juce::jlimit (0, numSamples, (*midiIt).samplePosition) : INT_MAX;
        const int seqPos  = seqIndex < numSeq ? seqEvents[(size_t) seqIndex].sample : INT_MAX;
        const int eventPos = std::min (midiPos, seqPos);
        if (eventPos == INT_MAX)
            break;

        if (eventPos > position)
        {
            engine.render (mono + position, eventPos - position);
            position = eventPos;
        }

        if (seqPos <= midiPos)
            handleSeqEvent (seqEvents[(size_t) seqIndex++]);
        else
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }
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
    state.setProperty (kStateVersionProp, kStateVersion, nullptr);

    juce::ValueTree patternTree (kPatternTag);
    patternTree.setProperty (kPatternData, juce::String (patternStore.get().encode()), nullptr);
    state.removeChild (state.getChildWithName (kPatternTag), nullptr);
    state.appendChild (patternTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BleepAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    // Pattern: present from state version 2. Phase 1 states keep the default pattern.
    bleep::seq::Pattern pattern = bleep::seq::Pattern::makeDefault();
    const auto patternTree = tree.getChildWithName (kPatternTag);
    if (patternTree.isValid())
        pattern.decode (patternTree.getProperty (kPatternData).toString().toStdString());
    tree.removeChild (patternTree, nullptr);

    // Parameters missing from older states (e.g. all seq_* in Phase 1) keep
    // their defaults, so old projects load with the sequencer off (External).
    apvts.replaceState (tree);
    patternStore.set (pattern);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepAudioProcessor();
}
