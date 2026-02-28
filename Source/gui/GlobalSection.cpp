// GlobalSection.cpp — Volume + Drive + Disperser + mono/retrig
#include "GlobalSection.h"

GlobalSection::GlobalSection(juce::AudioProcessorValueTreeState& apvts)
{
    // Volume knob
    volumeKnob.initMod(apvts, bb::LFODest::Volume);
    volumeKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    volumeKnob.setSliderSnapsToMousePosition(false);
    volumeKnob.setMouseDragSensitivity(200);
    volumeKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(volumeKnob);
    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(volumeLabel);
    volumeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "VOLUME", volumeKnob);

    // Drive knob
    driveKnob.initMod(apvts, bb::LFODest::Drive);
    driveKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    driveKnob.setSliderSnapsToMousePosition(false);
    driveKnob.setMouseDragSensitivity(200);
    driveKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(driveKnob);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(driveLabel);
    driveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DRIVE", driveKnob);

    // Disperser knob
    disperserKnob.initMod(apvts, bb::LFODest::FoldAmt);
    disperserKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    disperserKnob.setSliderSnapsToMousePosition(false);
    disperserKnob.setMouseDragSensitivity(200);
    disperserKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(disperserKnob);
    disperserLabel.setText("Fold", juce::dontSendNotification);
    disperserLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(disperserLabel);
    disperserAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DISP_AMT", disperserKnob);

    // Portamento knob
    portaKnob.initMod(apvts, bb::LFODest::Porta);
    portaKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    portaKnob.setSliderSnapsToMousePosition(false);
    portaKnob.setMouseDragSensitivity(200);
    portaKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(portaKnob);
    portaLabel.setText("Porta", juce::dontSendNotification);
    portaLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(portaLabel);
    portaAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "PORTA", portaKnob);

    // Toggles
    monoToggle.setButtonText("Mono");
    addAndMakeVisible(monoToggle);
    monoAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "MONO", monoToggle);

    retrigToggle.setButtonText("Rtrg");
    addAndMakeVisible(retrigToggle);
    retrigAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "RETRIG", retrigToggle);

    startTimerHz(5);
}

void GlobalSection::timerCallback()
{
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };
    showPct(volumeKnob, volumeLabel, "Volume");
    if (driveKnob.isMouseOverOrDragging())
        driveLabel.setText(juce::String(driveKnob.getValue(), 1) + "x", juce::dontSendNotification);
    else
        driveLabel.setText("Drive", juce::dontSendNotification);
    showPct(disperserKnob, disperserLabel, "Fold");

    if (portaKnob.isMouseOverOrDragging())
    {
        float v = static_cast<float>(portaKnob.getValue());
        if (v < 1.0f)
            portaLabel.setText(juce::String(v * 1000.0f, 0) + "ms", juce::dontSendNotification);
        else
            portaLabel.setText(juce::String(v, 2) + "s", juce::dontSendNotification);
    }
    else
        portaLabel.setText("Porta", juce::dontSendNotification);
}

void GlobalSection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(2);
    int knobSize = 36;
    int labelH = 12;
    auto knobRow = area.withSizeKeepingCentre(area.getWidth(), knobSize + labelH);
    int colW = knobRow.getWidth() / 5;

    // Col 1: Mono + Retrig stacked
    auto toggleCol = knobRow.removeFromLeft(colW);
    auto topHalf = toggleCol.removeFromTop(toggleCol.getHeight() / 2);
    monoToggle.setBounds(topHalf.reduced(4, 1));
    retrigToggle.setBounds(toggleCol.reduced(4, 1));

    auto layout = [&](juce::Slider& knob, juce::Label& label)
    {
        auto col = knobRow.removeFromLeft(colW);
        label.setBounds(col.removeFromBottom(labelH));
        knob.setBounds(col);
    };

    layout(portaKnob, portaLabel);
    layout(driveKnob, driveLabel);
    layout(disperserKnob, disperserLabel);
    layout(volumeKnob, volumeLabel);
}
