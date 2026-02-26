// FilterSection.cpp — Filtre Cutoff + Resonance + Type selector + On/Off
#include "FilterSection.h"

FilterSection::FilterSection(juce::AudioProcessorValueTreeState& apvts)
{
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "FILT_ON", onToggle);

    // Filter type ComboBox
    typeBox.addItemList({ "LP", "HP", "BP", "Notch" }, 1);
    addAndMakeVisible(typeBox);
    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(typeLabel);
    typeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "FILT_TYPE", typeBox);

    cutoffKnob.initMod(apvts, bb::LFODest::FilterCutoff);
    cutoffKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    cutoffKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(cutoffKnob);
    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(cutoffLabel);
    cutoffAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "FILT_CUTOFF", cutoffKnob);

    resKnob.initMod(apvts, bb::LFODest::FilterRes);
    resKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    resKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(resKnob);
    resLabel.setText("Res", juce::dontSendNotification);
    resLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(resLabel);
    resAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "FILT_RES", resKnob);
}

void FilterSection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(2);
    int knobSize = 36;
    int labelH = 12;
    auto knobRow = area.withSizeKeepingCentre(area.getWidth(), knobSize + labelH);
    int colW = knobRow.getWidth() / 4;

    // On/Off toggle
    auto onArea = knobRow.removeFromLeft(colW);
    onToggle.setBounds(onArea.reduced(4, 8));

    // Type selector
    auto typeArea = knobRow.removeFromLeft(colW);
    typeLabel.setBounds(typeArea.removeFromBottom(labelH));
    typeBox.setBounds(typeArea.reduced(2, 4));

    // Cutoff
    auto cutArea = knobRow.removeFromLeft(colW);
    cutoffLabel.setBounds(cutArea.removeFromBottom(labelH));
    cutoffKnob.setBounds(cutArea);

    // Resonance
    auto resArea = knobRow;
    resLabel.setBounds(resArea.removeFromBottom(labelH));
    resKnob.setBounds(resArea);
}
