#include "PluginEditor.h"

namespace
{
const juce::Colour backgroundColour { 0xff14161c };
const juce::Colour panelColour      { 0xff1d212b };
const juce::Colour accentColour     { 0xff4fc3f7 };
const juce::Colour trackColour      { 0xff2c3240 };
const juce::Colour textColour       { 0xffc8cedb };
const juce::Colour dimTextColour    { 0xff7b8496 };

constexpr int knobSize    = 74;
constexpr int knobLabelH  = 30;
constexpr int padding     = 14;
} // namespace

// ---------------------------------------------------------------------------

ChorusLookAndFeel::ChorusLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId,    accentColour);
    setColour (juce::Slider::rotarySliderOutlineColourId, trackColour);
    setColour (juce::Slider::textBoxTextColourId,         textColour);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);

    setColour (juce::ToggleButton::textColourId,      textColour);
    setColour (juce::ToggleButton::tickColourId,      accentColour);
    setColour (juce::ToggleButton::tickDisabledColourId, trackColour);

    setColour (juce::ComboBox::backgroundColourId, trackColour);
    setColour (juce::ComboBox::textColourId,       textColour);
    setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId,      dimTextColour);

    setColour (juce::PopupMenu::backgroundColourId,        panelColour);
    setColour (juce::PopupMenu::textColourId,              textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId,   juce::Colours::white);
}

void ChorusLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius    = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float thickness = juce::jmax (3.0f, radius * 0.18f);
    const float angle     = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto centre     = bounds.getCentre();
    const float arcRadius = radius - thickness * 0.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId)
                           .withAlpha (slider.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Pointer.
    const float pointerLength = arcRadius * 0.62f;
    const juce::Point<float> tip { centre.x + pointerLength * std::sin (angle),
                                   centre.y - pointerLength * std::cos (angle) };

    g.setColour (textColour.withAlpha (slider.isEnabled() ? 1.0f : 0.4f));
    g.drawLine ({ centre, tip }, juce::jmax (2.0f, thickness * 0.4f));
    g.fillEllipse (juce::Rectangle<float> (thickness * 0.75f, thickness * 0.75f).withCentre (centre));
}

// ---------------------------------------------------------------------------

LabelledKnob::LabelledKnob (juce::AudioProcessorValueTreeState& state,
                            const juce::String& parameterID,
                            const juce::String& caption)
    : captionText (caption)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, parameterID, slider);
}

void LabelledKnob::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (16); // caption
    slider.setBounds (area);
}

void LabelledKnob::paint (juce::Graphics& g)
{
    g.setColour (dimTextColour);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (captionText, getLocalBounds().removeFromTop (16),
                juce::Justification::centred, false);
}

// ---------------------------------------------------------------------------

ChorusAudioProcessorEditor::ChorusAudioProcessorEditor (ChorusAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    auto& state = processorRef.getValueTreeState();

    // Per-voice LFO rate knobs plus their enable toggles.
    for (int v = 0; v < chorus::ChorusEngine::numVoices; ++v)
    {
        auto knob = std::make_unique<LabelledKnob> (state, ids::voiceRate (v),
                                                    "LFO " + juce::String (v + 1));
        addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));

        auto toggle = std::make_unique<juce::ToggleButton> ("On");
        addAndMakeVisible (*toggle);
        voiceToggleAttachments[(size_t) v] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                state, ids::voiceEnabled (v), *toggle);
        voiceToggles[(size_t) v] = std::move (toggle);
    }

    for (const auto& spec : { std::make_pair (juce::String (ids::delay),  juce::String ("Delay")),
                              std::make_pair (juce::String (ids::depth),  juce::String ("Depth")),
                              std::make_pair (juce::String (ids::stereo), juce::String ("Stereo")),
                              std::make_pair (juce::String (ids::mix),    juce::String ("Mix")) })
    {
        auto knob = std::make_unique<LabelledKnob> (state, spec.first, spec.second);
        addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));
    }

    shapeBox.addItemList ({ "Sine", "Triangle" }, 1);
    addAndMakeVisible (shapeBox);
    shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, ids::shape, shapeBox);

    addAndMakeVisible (wetOnlyButton);
    wetOnlyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, ids::wetOnly, wetOnlyButton);

    setResizable (true, true);
    setResizeLimits (520, 330, 1040, 660);
    setSize (560, 360);
}

ChorusAudioProcessorEditor::~ChorusAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ChorusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    g.setColour (panelColour);
    g.fillRoundedRectangle (lfoSection.toFloat(), 8.0f);
    g.fillRoundedRectangle (globalSection.toFloat(), 8.0f);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("CHORUS", getLocalBounds().removeFromTop (34).reduced (padding, 0),
                juce::Justification::centredLeft, false);

    g.setColour (dimTextColour);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("3-voice modulated delay",
                getLocalBounds().removeFromTop (34).reduced (padding, 0),
                juce::Justification::centredRight, false);

    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("MODULATION", lfoSection.reduced (10, 6).removeFromTop (14),
                juce::Justification::topLeft, false);
    g.drawText ("OUTPUT", globalSection.reduced (10, 6).removeFromTop (14),
                juce::Justification::topLeft, false);
}

void ChorusAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (padding);
    area.removeFromTop (34 - padding); // title strip

    const int knobHeight = knobSize + knobLabelH;

    lfoSection = area.removeFromTop (knobHeight + 52);
    area.removeFromTop (padding);
    globalSection = area;

    // --- modulation section: three LFO knobs, each with an enable toggle ---
    auto lfoArea = lfoSection.reduced (10);
    lfoArea.removeFromTop (18); // section caption

    const int columnWidth = lfoArea.getWidth() / chorus::ChorusEngine::numVoices;

    for (int v = 0; v < chorus::ChorusEngine::numVoices; ++v)
    {
        auto column = lfoArea.removeFromLeft (columnWidth);
        knobs[(size_t) v]->setBounds (column.removeFromTop (knobHeight));
        voiceToggles[(size_t) v]->setBounds (column.removeFromTop (24).withSizeKeepingCentre (60, 24));
    }

    // --- output section: delay, depth, stereo, mix, then shape and wet-only ---
    auto globalArea = globalSection.reduced (10);
    globalArea.removeFromTop (18);

    auto bottomRow = globalArea.removeFromBottom (26);
    shapeBox.setBounds (bottomRow.removeFromLeft (130).reduced (0, 2));
    bottomRow.removeFromLeft (16);
    wetOnlyButton.setBounds (bottomRow.removeFromLeft (110));

    globalArea.removeFromBottom (6);

    const int numGlobalKnobs = 4;
    const int globalColumn   = globalArea.getWidth() / numGlobalKnobs;

    for (int i = 0; i < numGlobalKnobs; ++i)
    {
        const auto index = static_cast<size_t> (chorus::ChorusEngine::numVoices + i);
        knobs[index]->setBounds (globalArea.removeFromLeft (globalColumn));
    }
}
