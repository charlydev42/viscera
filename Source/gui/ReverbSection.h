// ReverbSection.h — Reverb controls (On/Off, Size, Damp, Mix)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class ReverbSection : public juce::Component
{
public:
    ReverbSection(juce::AudioProcessorValueTreeState& apvts);
    ~ReverbSection() override = default;
    void resized() override;

private:
    juce::ToggleButton onToggle;
    ModSlider sizeKnob, revMixKnob;
    juce::Slider dampKnob, widthKnob, pdlyKnob;
    juce::Label sizeLabel, dampLabel, widthLabel, pdlyLabel, revMixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        sizeAttach, dampAttach, widthAttach, pdlyAttach, revMixAttach;

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbSection)
};
