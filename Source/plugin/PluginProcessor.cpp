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
    fmRatio     = apvts.getRawParameterValue (id::fmRatio);
    fmDepth     = apvts.getRawParameterValue (id::fmDepth);
    fmFeedback  = apvts.getRawParameterValue (id::fmFeedback);
    fmDecay     = apvts.getRawParameterValue (id::fmDecay);
    pitchAmount = apvts.getRawParameterValue (id::pitchAmount);
    pitchDecay  = apvts.getRawParameterValue (id::pitchDecay);
    pitchCurve  = apvts.getRawParameterValue (id::pitchCurve);
    filterMode  = apvts.getRawParameterValue (id::filterMode);
    cutoff      = apvts.getRawParameterValue (id::cutoff);
    resonance   = apvts.getRawParameterValue (id::resonance);
    filterEnv   = apvts.getRawParameterValue (id::filterEnv);
    filterDecay = apvts.getRawParameterValue (id::filterDecay);
    jassert (attack && decay && sustain && release && fineTune && outputGain
             && seqMode && seqRate && seqLength && seqRoot && seqGate
             && fmRatio && fmDepth && fmFeedback && fmDecay && pitchAmount && pitchDecay && pitchCurve
             && filterMode && cutoff && resonance && filterEnv && filterDecay);

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

    p.fmRatio     = fmRatio->load();
    p.fmDepth     = fmDepth->load();
    p.fmFeedback  = fmFeedback->load() * 0.01f;
    p.fmDecay     = fmDecay->load() * 0.001f;
    p.pitchAmount = pitchAmount->load();
    p.pitchDecay  = pitchDecay->load() * 0.001f;
    p.pitchCurve  = pitchCurve->load() * 0.01f;
    p.filterMode  = filterMode->load() >= 0.5f ? 1 : 0;
    p.cutoff      = cutoff->load();
    p.resonance   = resonance->load() * 0.01f;
    p.filterEnv   = filterEnv->load();
    p.filterDecay = filterDecay->load() * 0.001f;
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

// ---------------------------------------------------------------------------
// Generative actions (message thread)
// ---------------------------------------------------------------------------

bleep::seq::GeneratorSettings BleepAudioProcessor::generatorSettings() const
{
    namespace id = bleep::params::id;
    bleep::seq::GeneratorSettings g;
    g.density    = apvts.getRawParameterValue (id::genDensity)->load() * 0.01f;
    g.pitchRange = (int) std::lround (apvts.getRawParameterValue (id::genRange)->load());
    return g;
}

int BleepAudioProcessor::patternLength() const
{
    // Generate over the visible 16 steps so changing LENGTH later reveals musical material.
    return bleep::seq::kVisibleSteps;
}

void BleepAudioProcessor::setParamPlain (const char* paramId, float value)
{
    if (auto* p = apvts.getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

BleepAudioProcessor::Snapshot BleepAudioProcessor::captureSnapshot() const
{
    Snapshot s;
    s.pattern = patternStore.get();
    for (size_t i = 0; i < s.sound.size(); ++i)
        s.sound[i] = apvts.getRawParameterValue (kSoundParams[i])->load();
    return s;
}

void BleepAudioProcessor::applySnapshot (const Snapshot& s)
{
    patternStore.set (s.pattern);
    for (size_t i = 0; i < s.sound.size(); ++i)
        if (std::abs (apvts.getRawParameterValue (kSoundParams[i])->load() - s.sound[i]) > 1.0e-4f)
            setParamPlain (kSoundParams[i], s.sound[i]);
}

void BleepAudioProcessor::randomizeSound (bleep::seq::Rng& rng)
{
    // Согласованный «бип-патч», а не независимые случайные ручки. Все диапазоны
    // безопасны: громкость и подстройка не меняются, резонанс ограничен, индекс FM
    // всё равно проходит через защиту от алиасинга в движке.
    // (Правила по тембровым режимам ROUND / METALLIC / ... — в Phase D.)
    namespace id = bleep::params::id;
    auto pick = [&rng] (std::initializer_list<float> values)
    {
        const int i = rng.between (0, (int) values.size() - 1);
        return *(values.begin() + i);
    };
    const float u = rng.uniform();

    // Амплитуда: в основном коротко и щелчково.
    setParamPlain (id::attack,  0.5f + 4.0f * rng.uniform() * rng.uniform());
    setParamPlain (id::decay,   40.0f + 460.0f * u * u);
    setParamPlain (id::release, 30.0f + 220.0f * rng.uniform());

    // FM: преимущественно гармонические отношения, иногда «металл».
    const bool metallic = rng.chance (0.25f);
    setParamPlain (id::fmRatio,    metallic ? pick ({ 1.41f, 2.76f, 3.53f, 4.27f, 5.19f }) : pick ({ 0.5f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 4.0f }));
    setParamPlain (id::fmDepth,    0.3f + 3.7f * std::pow (rng.uniform(), 1.5f));
    setParamPlain (id::fmFeedback, rng.chance (0.7f) ? 0.0f : 40.0f * rng.uniform());
    setParamPlain (id::fmDecay,    30.0f + 370.0f * rng.uniform() * rng.uniform());

    // Огибающая высоты: в половине случаев выключена, иначе короткий «чирп».
    if (rng.chance (0.5f))
        setParamPlain (id::pitchAmount, 0.0f);
    else
    {
        setParamPlain (id::pitchAmount, pick ({ 5.0f, 7.0f, 12.0f, 12.0f, 24.0f, -12.0f }));
        setParamPlain (id::pitchDecay,  4.0f + 36.0f * rng.uniform());
        setParamPlain (id::pitchCurve,  50.0f + 40.0f * rng.uniform());
    }

    // Фильтр: срез над басом и бочкой, умеренный резонанс.
    setParamPlain (id::filterMode,  rng.chance (0.2f) ? 1.0f : 0.0f);
    setParamPlain (id::cutoff,      1500.0f * std::pow (6.0f, rng.uniform()));   // 1.5..9 кГц
    setParamPlain (id::resonance,   50.0f * rng.uniform());
    setParamPlain (id::filterEnv,   2.5f * rng.uniform());
    setParamPlain (id::filterDecay, 40.0f + 260.0f * rng.uniform());
}

void BleepAudioProcessor::randomizePattern (std::uint32_t seed)
{
    history.push (captureSnapshot());
    bleep::seq::Rng rng (seed);
    patternStore.set (bleep::seq::generate (patternStore.get(), generatorSettings(), rng.next(), patternLength()));
    if (apvts.getRawParameterValue (bleep::params::id::genRandSound)->load() >= 0.5f)
        randomizeSound (rng);
}

void BleepAudioProcessor::mutatePattern (std::uint32_t seed)
{
    history.push (captureSnapshot());
    const float amount = apvts.getRawParameterValue (bleep::params::id::genMutate)->load() * 0.01f;
    patternStore.set (bleep::seq::mutate (patternStore.get(), amount, generatorSettings(), seed, patternLength()));
}

void BleepAudioProcessor::clearPattern()
{
    history.push (captureSnapshot());
    patternStore.modify ([] (bleep::seq::Pattern& p)
    {
        for (auto& s : p.steps)
            if (! s.locked)
                s = bleep::seq::Step {};
    });
}

void BleepAudioProcessor::beginPatternEdit()
{
    if (! pendingEdit.has_value())
        pendingEdit = captureSnapshot();
}

void BleepAudioProcessor::endPatternEdit()
{
    if (pendingEdit.has_value() && pendingEdit->pattern != patternStore.get())
        history.push (*pendingEdit);
    pendingEdit.reset();
}

bool BleepAudioProcessor::undo()
{
    auto current = captureSnapshot();
    if (! history.undo (current))
        return false;
    applySnapshot (current);
    return true;
}

bool BleepAudioProcessor::redo()
{
    auto current = captureSnapshot();
    if (! history.redo (current))
        return false;
    applySnapshot (current);
    return true;
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

    // Parameters missing from older states (e.g. seq_* / gen_* in older sets) keep
    // their defaults, so old projects load with the sequencer off (External).
    const int loadedVersion = (int) tree.getProperty (kStateVersionProp, 1);
    apvts.replaceState (tree);
    patternStore.set (pattern);

    // Состояния до Phase A (версии 1–3) не знают о FM и фильтре: ставим
    // «нейтральные» значения, чтобы такие проекты звучали точно как раньше.
    if (loadedVersion < 4)
        for (const auto& legacy : bleep::params::kPreSoundEngineValues)
            if (auto* param = apvts.getParameter (legacy.id))
                param->setValueNotifyingHost (param->convertTo0to1 (legacy.plainValue));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BleepAudioProcessor();
}
