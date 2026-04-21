// CarrierSection.h — Carrier: waveform, Coarse/Freq, Fixed, Fine, ADSR, XOR, sync
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModSlider.h"
#include "HarmonicEditor.h"

// Interactive ADSR display for carrier envelope (ENV3)
// Drag points to adjust A/D/S/R like Ableton Operator
class CarrierEnvDisplay : public juce::Component,
                          private juce::Timer
{
public:
    CarrierEnvDisplay(juce::AudioProcessorValueTreeState& apvts);
    ~CarrierEnvDisplay() override { stopTimer(); }
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

private:
    juce::AudioProcessorValueTreeState& state;

    // Dragging state: -1=none, 0=attack peak, 1=decay/sustain, 2=release end
    int dragPoint = -1;
    int hoveredPoint = -1;
    static constexpr float kHitRadius = 8.0f;

    // Cached ADSR points for hit-testing
    juce::Point<float> ptPeak, ptSustain, ptRelEnd;

    // Change-detection cache — timerCallback skips repaint if ADSR +
    // hovered point are identical to the last painted frame.
    float lastAdsrDigest    = -1.0f;
    int   lastHoveredPoint  = -2;

    int pointAtPosition(juce::Point<float> pos) const;
    void setParamNormalized(const juce::String& id, float newVal);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarrierEnvDisplay)
};

class CarrierSection : public juce::Component,
                       private juce::Timer
{
public:
    CarrierSection(juce::AudioProcessorValueTreeState& apvts,
                   bb::HarmonicTable& harmonics);
    ~CarrierSection() override { stopTimer(); }
    void resized() override;
    void timerCallback() override;

private:
    void setDesignMode(bool on);

    juce::AudioProcessorValueTreeState& state;
    bb::HarmonicTable& harmonicTable;

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

    // Fine knob (ratio mode) / Multi knob (fixed mode)
    ModSlider fineKnob;
    juce::Slider multiKnob;
    juce::Label fineLabel;

    // ADSR display + knobs
    CarrierEnvDisplay envDisplay;
    ModSlider adsrKnobs[4];
    juce::Label adsrLabels[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> adsrAttach[4];
    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);
    void setupKnob(juce::Slider& knob);

    // Push the 32 internal harmonics into the CAR_H## params so a subsequent
    // bar edit + Cmd+Z doesn't revert to a stale pre-sync value.
    void syncHarmonicsToParams();

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> coarseAttach, fixedFreqAttach, fineAttach, multiAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> xorAttach, syncAttach;

    // Design mode (harmonic editor)
    HarmonicEditor harmonicEditor;
    juce::TextButton designBtn { "Harmo" };
    bool designMode = false;
    int lastDesignWave = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarrierSection)
};
