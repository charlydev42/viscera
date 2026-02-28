// CarrierSection.h — Carrier: waveform, Coarse/Freq, Fixed, Fine, ADSR, XOR, sync
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"

// Visual-only ADSR display for carrier envelope (ENV3)
class CarrierEnvDisplay : public juce::Component,
                          private juce::Timer
{
public:
    CarrierEnvDisplay(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    juce::AudioProcessorValueTreeState& state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarrierEnvDisplay)
};

class CarrierSection : public juce::Component,
                       private juce::Timer
{
public:
    CarrierSection(juce::AudioProcessorValueTreeState& apvts);
    ~CarrierSection() override = default;
    void resized() override;
    void timerCallback() override;

private:
    juce::AudioProcessorValueTreeState& state;

    juce::ComboBox waveCombo;
    juce::Label waveLabel;

    // Coarse knob (ratio mode — visible when Fixed OFF, LFO assignable)
    ModSlider coarseKnob;
    // Fixed freq knob (fixed mode — visible when Fixed ON)
    juce::Slider fixedFreqKnob;
    // Shared label
    juce::Label mainKnobLabel;

    // Fixed toggle (inverted KB)
    juce::ToggleButton fixedToggle;
    juce::String kbParamId;

    // Fine knob (LFO assignable)
    ModSlider fineKnob;
    juce::Label fineLabel;

    // ADSR display + knobs
    CarrierEnvDisplay envDisplay;
    ModSlider adsrKnobs[4];
    juce::Label adsrLabels[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> adsrAttach[4];
    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);
    void setupKnob(juce::Slider& knob);

    // Drift knob (LFO assignable)
    ModSlider driftKnob;
    juce::Label driftLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driftAttach;

    // Noise knob
    ModSlider noiseKnob;
    juce::Label noiseLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttach;

    // Spread knob (stereo unison detune)
    ModSlider spreadKnob;
    juce::Label spreadLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttach;

    // Toggles
    juce::ToggleButton xorToggle, syncToggle;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> coarseAttach, fixedFreqAttach, fineAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> xorAttach, syncAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarrierSection)
};
