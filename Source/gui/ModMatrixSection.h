// ModMatrixSection.h — FM macro knobs (Cortex/Ichor/Plasma/Time)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class ModMatrixSection : public juce::Component,
                         private juce::Timer
{
public:
    ModMatrixSection(juce::AudioProcessorValueTreeState& apvts);
    ~ModMatrixSection() override { stopTimer(); }
    void resized() override;
    void timerCallback() override;

private:
    ModSlider vortexKnob, helixKnob, plasmaKnob, timeKnob;
    juce::Label vortexLabel, helixLabel, plasmaLabel, timeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        vortexAttach, helixAttach, plasmaAttach, timeAttach;

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixSection)
};
