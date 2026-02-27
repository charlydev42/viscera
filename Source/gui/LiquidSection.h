// LiquidSection.h — Liquid controls (On/Off, Rate, Depth, Tone, Feed, Mix)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class LiquidSection : public juce::Component,
                      private juce::Timer
{
public:
    LiquidSection(juce::AudioProcessorValueTreeState& apvts);
    ~LiquidSection() override = default;
    void resized() override;
    void timerCallback() override;

private:
    juce::ToggleButton onToggle;
    juce::Slider rateKnob, toneKnob, feedKnob;
    ModSlider depthKnob, mixKnob;
    juce::Label rateLabel, depthLabel, toneLabel, feedLabel, mixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        rateAttach, depthAttach, toneAttach, feedAttach, mixAttach;

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiquidSection)
};
