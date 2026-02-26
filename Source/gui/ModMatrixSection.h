// ModMatrixSection.h — LFO routing knobs (tremor/vein/flux)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class ModMatrixSection : public juce::Component
{
public:
    ModMatrixSection(juce::AudioProcessorValueTreeState& apvts);
    ~ModMatrixSection() override = default;
    void resized() override;

private:
    juce::Slider tremorKnob, veinKnob, fluxKnob;
    juce::Label tremorLabel, veinLabel, fluxLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tremorAttach, veinAttach, fluxAttach;

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixSection)
};
