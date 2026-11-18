#pragma once

#include "dsp/SynthEngine.h"
#include "seq/Sequencer.h"
#include "PatternStore.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

class BleepAudioProcessor final : public juce::AudioProcessor
{
public:
    BleepAudioProcessor();
    ~BleepAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    PatternStore& getPatternStore() noexcept { return patternStore; }

    /** Step under the playhead for the UI, or -1 when the sequencer is not running. */
    int getCurrentStep() const noexcept { return currentStep.load (std::memory_order_relaxed); }

    static constexpr int kStateVersion = 2;   // 1 = Phase 1 (no sequencer)

private:
    void updateEngineParams() noexcept;
    void updateSequencerSettings() noexcept;
    bleep::seq::Transport readTransport() const noexcept;
    void handleMidi (const juce::MidiMessage& msg) noexcept;
    void handleSeqEvent (const bleep::seq::NoteEvent& e) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    bleep::dsp::SynthEngine engine;
    bleep::seq::Sequencer sequencer;

    PatternStore patternStore;
    bleep::seq::Pattern audioPattern;           // audio thread's copy
    std::uint32_t audioPatternVersion = 0;

    static constexpr int kMaxSeqEvents = 256;
    std::array<bleep::seq::NoteEvent, kMaxSeqEvents> seqEvents {};

    std::atomic<int> currentStep { -1 };

    // Cached raw parameter pointers: atomic reads, no lookups on the audio thread.
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* decay = nullptr;
    std::atomic<float>* sustain = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* fineTune = nullptr;
    std::atomic<float>* outputGain = nullptr;
    std::atomic<float>* seqMode = nullptr;
    std::atomic<float>* seqRate = nullptr;
    std::atomic<float>* seqLength = nullptr;
    std::atomic<float>* seqRoot = nullptr;
    std::atomic<float>* seqGate = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepAudioProcessor)
};
