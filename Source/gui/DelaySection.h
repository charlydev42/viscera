// DelaySection.h — Delay controls (On/Off, Time, Feedback, Damp, Mix, Ping-Pong)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class DelaySection : public juce::Component,
                     private juce::Timer
{
public:
    DelaySection(juce::AudioProcessorValueTreeState& apvts);
    ~DelaySection() override { stopTimer(); }
    void resized() override;
    void timerCallback() override;

private:
    juce::ToggleButton onToggle, ppToggle;
    ModSlider timeKnob, feedKnob, dlyMixKnob;
    ModSlider dampKnob, spreadKnob;
    ModSlider syncKnob; // Stepped 1..9 — same LFO destination as timeKnob
    juce::Label timeLabel, feedLabel, dampLabel, spreadLabel, dlyMixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach, ppAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        timeAttach, feedAttach, dampAttach, spreadAttach, dlyMixAttach;

    juce::AudioProcessorValueTreeState& state;
    int lastSyncIdx = 1; // Remembered division when toggling Sync on

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);
    void updateSyncVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelaySection)
};
