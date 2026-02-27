// GlobalSection.h — Volume + Drive + Disperser + mono/retrig toggles
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class GlobalSection : public juce::Component,
                      private juce::Timer
{
public:
    GlobalSection(juce::AudioProcessorValueTreeState& apvts);
    ~GlobalSection() override = default;
    void resized() override;
    void timerCallback() override;

private:
    ModSlider volumeKnob, driveKnob, disperserKnob;
    ModSlider portaKnob;
    juce::Label volumeLabel, driveLabel, disperserLabel, portaLabel;
    juce::ToggleButton monoToggle, retrigToggle;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttach, driveAttach, disperserAttach, portaAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttach, retrigAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalSection)
};
