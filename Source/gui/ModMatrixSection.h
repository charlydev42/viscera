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
    ModSlider cortexKnob, ichorKnob, plasmaKnob;
    // timeKnob stays a plain juce::Slider — making it a ModSlider broke the
    // regular drag behaviour on this specific knob (release snapped the
    // value back). Couldn't pin down the root cause; rolled back the GUI
    // wiring. The LFODest::MacroTime backend is intact, so users can still
    // target the envelope-time macro via the LFO section's destination
    // dropdown — just no arc overlay on the knob itself.
    juce::Slider timeKnob;
    juce::Label cortexLabel, ichorLabel, plasmaLabel, timeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        cortexAttach, ichorAttach, plasmaAttach, timeAttach;

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixSection)
};
