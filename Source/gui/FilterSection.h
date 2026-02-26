// FilterSection.h — Cutoff + Resonance + Filter Type selector + On/Off
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class FilterSection : public juce::Component
{
public:
    FilterSection(juce::AudioProcessorValueTreeState& apvts);
    ~FilterSection() override = default;
    void resized() override;

private:
    juce::ToggleButton onToggle;
    juce::ComboBox typeBox;
    ModSlider cutoffKnob, resKnob;
    juce::Label typeLabel, cutoffLabel, resLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttach, resAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterSection)
};
