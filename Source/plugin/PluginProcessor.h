#pragma once

#include "dsp/SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

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

    static constexpr int kStateVersion = 1;

private:
    void updateEngineParams() noexcept;
    void handleMidi (const juce::MidiMessage& msg) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    bleep::dsp::SynthEngine engine;

    // Cached raw parameter pointers: atomic reads, no lookups on the audio thread.
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* decay = nullptr;
    std::atomic<float>* sustain = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* fineTune = nullptr;
    std::atomic<float>* outputGain = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BleepAudioProcessor)
};
