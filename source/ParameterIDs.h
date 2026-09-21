#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ChorusEngine.h"

namespace ids
{
// Bump this if a parameter ID ever has to change meaning; hosts key saved
// automation to these strings, so they are part of the plugin's contract.
inline constexpr int version = 1;

inline constexpr const char* delay   = "delay";
inline constexpr const char* depth   = "depth";
inline constexpr const char* stereo  = "stereo";
inline constexpr const char* shape   = "shape";
inline constexpr const char* mix     = "mix";
inline constexpr const char* wetOnly = "wetonly";

inline juce::String voiceRate (int index)    { return "lfo" + juce::String (index + 1) + "rate"; }
inline juce::String voiceEnabled (int index) { return "lfo" + juce::String (index + 1) + "on"; }
} // namespace ids

/** Builds the parameter layout. Ranges are chosen so that the default position
    of every control is a usable, musical chorus rather than a null setting.
*/
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto msRange = [] (float max)
    {
        return NormalisableRange<float> { 0.5f, max, 0.01f, 0.5f }; // skewed toward short delays
    };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::delay, ids::version }, "Delay",
        msRange (chorus::ChorusEngine::maxDelayMs), 12.0f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::depth, ids::version }, "Depth",
        NormalisableRange<float> { 0.0f, chorus::ChorusEngine::maxDepthMs, 0.01f, 0.6f }, 6.0f,
        AudioParameterFloatAttributes{}.withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::stereo, ids::version }, "Stereo",
        NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
        AudioParameterFloatAttributes{}.withLabel ("%")));

    // Per-voice LFO rate and on/off, matching the three-oscillator layout of
    // the classic FL chorus.
    const std::array<float, chorus::ChorusEngine::numVoices> defaultRates { 0.35f, 0.83f, 1.61f };
    const std::array<bool,  chorus::ChorusEngine::numVoices> defaultOn    { true,  true,  false };

    for (int v = 0; v < chorus::ChorusEngine::numVoices; ++v)
    {
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ids::voiceRate (v), ids::version },
            "LFO " + String (v + 1) + " Rate",
            NormalisableRange<float> { 0.0f, chorus::ChorusEngine::maxRateHz, 0.001f, 0.35f },
            defaultRates[(size_t) v],
            AudioParameterFloatAttributes{}.withLabel ("Hz")));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ids::voiceEnabled (v), ids::version },
            "LFO " + String (v + 1) + " On",
            defaultOn[(size_t) v]));
    }

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::shape, ids::version }, "Shape",
        StringArray { "Sine", "Triangle" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::mix, ids::version }, "Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 50.0f,
        AudioParameterFloatAttributes{}.withLabel ("%")));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ids::wetOnly, ids::version }, "Wet Only", false));

    return layout;
}
