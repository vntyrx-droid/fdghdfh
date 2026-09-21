#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIDs.h"
#include "dsp/ChorusEngine.h"

#include <array>

class ChorusAudioProcessor : public juce::AudioProcessor
{
public:
    ChorusAudioProcessor();
    ~ChorusAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    // Keep the double-precision overload visible; declaring only the float
    // version would hide it and trip -Woverloaded-virtual.
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

private:
    /** Reads the current parameter values into a plain DSP parameter struct. */
    chorus::ChorusEngine::Parameters collectParameters() const;

    juce::AudioProcessorValueTreeState apvts;
    chorus::ChorusEngine               engine;

    // Cached atomic pointers: looking parameters up by string on the audio
    // thread would hash on every block.
    std::atomic<float>* delayParam   = nullptr;
    std::atomic<float>* depthParam   = nullptr;
    std::atomic<float>* stereoParam  = nullptr;
    std::atomic<float>* shapeParam   = nullptr;
    std::atomic<float>* mixParam     = nullptr;
    std::atomic<float>* wetOnlyParam = nullptr;

    std::array<std::atomic<float>*, chorus::ChorusEngine::numVoices> rateParams    { nullptr, nullptr, nullptr };
    std::array<std::atomic<float>*, chorus::ChorusEngine::numVoices> enabledParams { nullptr, nullptr, nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusAudioProcessor)
};
