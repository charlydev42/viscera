// RubberSection.h — Rubber comb filter controls (On/Off, Tone, Stretch, Warp, Mix)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class RubberSection : public juce::Component
{
public:
    RubberSection(juce::AudioProcessorValueTreeState& apvts);
    ~RubberSection() override = default;
    void resized() override;

private:
    juce::ToggleButton onToggle;
    juce::Slider toneKnob, stretchKnob, feedKnob;
    ModSlider warpKnob, mixKnob;
    juce::Label toneLabel, stretchLabel, feedLabel, warpLabel, mixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        toneAttach, stretchAttach, feedAttach, warpAttach, mixAttach;

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RubberSection)
};
