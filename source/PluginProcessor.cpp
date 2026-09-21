#include "PluginProcessor.h"
#include "PluginEditor.h"

ChorusAudioProcessor::ChorusAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    delayParam   = apvts.getRawParameterValue (ids::delay);
    depthParam   = apvts.getRawParameterValue (ids::depth);
    stereoParam  = apvts.getRawParameterValue (ids::stereo);
    shapeParam   = apvts.getRawParameterValue (ids::shape);
    mixParam     = apvts.getRawParameterValue (ids::mix);
    wetOnlyParam = apvts.getRawParameterValue (ids::wetOnly);

    for (int v = 0; v < chorus::ChorusEngine::numVoices; ++v)
    {
        rateParams[(size_t) v]    = apvts.getRawParameterValue (ids::voiceRate (v));
        enabledParams[(size_t) v] = apvts.getRawParameterValue (ids::voiceEnabled (v));
    }
}

chorus::ChorusEngine::Parameters ChorusAudioProcessor::collectParameters() const
{
    chorus::ChorusEngine::Parameters p;

    p.delayMs = delayParam->load();
    p.depthMs = depthParam->load();
    p.stereo  = stereoParam->load() * 0.01f;   // stored as a percentage
    p.mix     = mixParam->load()    * 0.01f;
    p.wetOnly = wetOnlyParam->load() > 0.5f;
    p.shape   = shapeParam->load() < 0.5f ? chorus::LfoShape::sine
                                          : chorus::LfoShape::triangle;

    for (int v = 0; v < chorus::ChorusEngine::numVoices; ++v)
    {
        p.rateHz[(size_t) v]  = rateParams[(size_t) v]->load();
        p.enabled[(size_t) v] = enabledParams[(size_t) v]->load() > 0.5f;
    }

    return p;
}

void ChorusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParameters (collectParameters());
    engine.prepare (sampleRate, samplesPerBlock);
    engine.reset();
}

void ChorusAudioProcessor::releaseResources()
{
    engine.reset();
}

double ChorusAudioProcessor::getTailLengthSeconds() const
{
    // The longest the wet path can still be ringing out after input stops.
    return (chorus::ChorusEngine::maxDelayMs + chorus::ChorusEngine::maxDepthMs) * 0.001;
}

bool ChorusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void ChorusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numChannels <= 0 || numSamples <= 0)
        return;

    engine.setParameters (collectParameters());

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    engine.process (left, right, numSamples);
}

juce::AudioProcessorEditor* ChorusAudioProcessor::createEditor()
{
    return new ChorusAudioProcessorEditor (*this);
}

void ChorusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChorusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChorusAudioProcessor();
}
