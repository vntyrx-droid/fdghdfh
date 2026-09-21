#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

/** Flat dark rotary styling; keeps the look consistent across hosts that would
    otherwise impose their own default slider drawing.
*/
class ChorusLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChorusLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

/** A rotary control with its caption and live value underneath. */
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterID,
                  const juce::String& caption);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::String captionText;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ChorusAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChorusAudioProcessorEditor (ChorusAudioProcessor&);
    ~ChorusAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChorusAudioProcessor& processorRef;
    ChorusLookAndFeel     lookAndFeel;

    std::vector<std::unique_ptr<LabelledKnob>> knobs;

    std::array<std::unique_ptr<juce::ToggleButton>, chorus::ChorusEngine::numVoices> voiceToggles;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,
               chorus::ChorusEngine::numVoices> voiceToggleAttachments;

    juce::ComboBox    shapeBox;
    juce::ToggleButton wetOnlyButton { "Wet only" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   wetOnlyAttachment;

    // Layout regions, computed in resized() and reused by paint().
    juce::Rectangle<int> lfoSection, globalSection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusAudioProcessorEditor)
};
