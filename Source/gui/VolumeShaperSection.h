// VolumeShaperSection.h — Drawable volume shaper with 32-step display
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/VolumeShaper.h"

// Interactive drawable display for the shaper table (32 or 8 bars)
class ShaperDisplay : public juce::Component,
                      private juce::Timer
{
public:
    ShaperDisplay(bb::VolumeShaper& shaper);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void timerCallback() override;

    void setCoarseMode(bool coarse) { coarseMode = coarse; repaint(); }
    bool isCoarseMode() const { return coarseMode; }

private:
    bb::VolumeShaper& volumeShaper;
    bool coarseMode = false; // false=32 steps, true=8 steps
    void applyMouse(const juce::MouseEvent& e);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShaperDisplay)
};

class VolumeShaperSection : public juce::Component,
                            private juce::Timer
{
public:
    VolumeShaperSection(juce::AudioProcessorValueTreeState& apvts,
                        bb::VolumeShaper& shaper);
    ~VolumeShaperSection() override = default;
    void resized() override;
    void timerCallback() override;

private:
    juce::AudioProcessorValueTreeState& state;
    bb::VolumeShaper& volumeShaper;

    juce::ToggleButton onToggle;
    juce::ComboBox shapePresetBox;
    juce::TextButton subdivBtn; // toggle 32/8 steps
    ShaperDisplay shaperDisplay;

    // Fixed toggle
    juce::ToggleButton fixedToggle;
    juce::Label fixedLabel;

    // Rate knob (free mode)
    juce::Slider rateKnob;
    juce::Label rateValueLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;

    // Sync knob (fixed mode)
    juce::Slider syncKnob;
    juce::Label syncValueLabel;

    // Depth knob
    juce::Slider depthKnob;
    juce::Label depthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;

    juce::StringArray syncNames;
    int lastSyncIdx = 3;
    void updateDisplay();
    void setSyncParam(int idx);
    int getSyncParam() const;
    void loadShapePreset(int presetIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VolumeShaperSection)
};
