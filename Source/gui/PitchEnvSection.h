// PitchEnvSection.h — Pitch Envelope: visual display + knobs
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

// Visual-only ADSR display for pitch envelope
class PitchEnvDisplay : public juce::Component,
                        private juce::Timer
{
public:
    PitchEnvDisplay(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    juce::AudioProcessorValueTreeState& state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchEnvDisplay)
};

class PitchEnvSection : public juce::Component,
                        private juce::Timer
{
public:
    PitchEnvSection(juce::AudioProcessorValueTreeState& apvts);
    ~PitchEnvSection() override = default;
    void resized() override;
    void timerCallback() override;

private:
    juce::ToggleButton onToggle;
    ModSlider amtKnob;
    juce::Slider adsrKnobs[4];
    juce::Label amtLabel;
    juce::Label adsrLabels[4];
    PitchEnvDisplay envDisplay;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> adsrAttach[4];

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchEnvSection)
};
