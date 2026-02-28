// ModulatorSection.h — Panel d'un modulateur FM (réutilisé pour Mod1 et Mod2)
// Operator-style: Coarse/Freq knob (swaps based on Fixed), Fine knob, Level knob
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

class ModulatorSection : public juce::Component,
                         private juce::Timer
{
public:
    ModulatorSection(juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& prefix, const juce::String& envPrefix);
    ~ModulatorSection() override = default;

    void resized() override;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& state;
    juce::String paramPrefix;
    juce::String kbParamId;

    // Widgets
    juce::ComboBox waveCombo;
    juce::Label waveLabel;

    // Coarse knob (ratio mode — visible when Fixed OFF, LFO assignable)
    ModSlider coarseKnob;
    // Fixed freq knob (fixed mode — visible when Fixed ON)
    juce::Slider fixedFreqKnob;
    // Shared label that swaps between "Coarse" and "Freq"
    juce::Label mainKnobLabel;

    juce::ToggleButton fixedToggle;
    juce::ToggleButton onToggle;

    ModSlider fineKnob;
    // Multi knob (fixed mode — overlaps fineKnob position)
    juce::Slider multiKnob;
    juce::Label fineLabel; // shared label: shows "Fine" or "Multi"

    ModSlider levelKnob;
    juce::Label levelLabel;

    ModSlider adsrKnobs[4];
    juce::Label adsrLabels[4];

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> coarseAttach, fixedFreqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fineAttach, multiAttach, levelAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> adsrAttach[4];

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);
    void setupKnob(juce::Slider& knob); // no label variant

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatorSection)
};
