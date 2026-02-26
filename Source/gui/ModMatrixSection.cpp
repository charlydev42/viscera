// ModMatrixSection.cpp — LFO routing: tremor/vein/flux
#include "ModMatrixSection.h"

ModMatrixSection::ModMatrixSection(juce::AudioProcessorValueTreeState& apvts)
{
    setupKnob(tremorKnob, tremorLabel, "Tremor");
    tremorAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "TREMOR", tremorKnob);

    setupKnob(veinKnob, veinLabel, "Vein");
    veinAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "VEIN", veinKnob);

    setupKnob(fluxKnob, fluxLabel, "Flux");
    fluxAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "FLUX", fluxKnob);
}

void ModMatrixSection::setupKnob(juce::Slider& knob, juce::Label& label,
                                  const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(0.0, 1.0, 0.01);
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void ModMatrixSection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(2);
    int knobSize = 36;
    int labelH = 12;
    auto knobRow = area.withSizeKeepingCentre(area.getWidth(), knobSize + labelH);
    int colW = knobRow.getWidth() / 3;

    auto tremorArea = knobRow.removeFromLeft(colW);
    tremorLabel.setBounds(tremorArea.removeFromBottom(labelH));
    tremorKnob.setBounds(tremorArea);

    auto veinArea = knobRow.removeFromLeft(colW);
    veinLabel.setBounds(veinArea.removeFromBottom(labelH));
    veinKnob.setBounds(veinArea);

    auto fluxArea = knobRow;
    fluxLabel.setBounds(fluxArea.removeFromBottom(labelH));
    fluxKnob.setBounds(fluxArea);
}
