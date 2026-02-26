// DelaySection.h — Delay controls (On/Off, Time, Feedback, Damp, Mix, Ping-Pong)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class DelaySection : public juce::Component
{
public:
    DelaySection(juce::AudioProcessorValueTreeState& apvts);
    ~DelaySection() override = default;
    void resized() override;

private:
    juce::ToggleButton onToggle, ppToggle;
    ModSlider timeKnob, feedKnob, dlyMixKnob;
    juce::Slider dampKnob, spreadKnob;
    juce::Label timeLabel, feedLabel, dampLabel, spreadLabel, dlyMixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach, ppAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        timeAttach, feedAttach, dampAttach, spreadAttach, dlyMixAttach;

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelaySection)
};
