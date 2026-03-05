// ModMatrixSection.cpp — FM macros: Cortex/Ichor/Plasma
#include "ModMatrixSection.h"

ModMatrixSection::ModMatrixSection(juce::AudioProcessorValueTreeState& apvts)
{
    cortexKnob.initMod(apvts, bb::LFODest::Cortex);
    setupKnob(cortexKnob, cortexLabel, "Cortex");
    cortexAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CORTEX", cortexKnob);

    ichorKnob.initMod(apvts, bb::LFODest::Ichor);
    setupKnob(ichorKnob, ichorLabel, "Ichor");
    ichorAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "ICHOR", ichorKnob);

    plasmaKnob.initMod(apvts, bb::LFODest::Plasma);
    setupKnob(plasmaKnob, plasmaLabel, "Plasma");
    plasmaAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "PLASMA", plasmaKnob);

    // Double-click resets to default
    cortexKnob.setDoubleClickReturnValue(true, 0.5);
    ichorKnob.setDoubleClickReturnValue(true, 0.0);
    plasmaKnob.setDoubleClickReturnValue(true, 1.0);

    startTimerHz(5);
}

void ModMatrixSection::timerCallback()
{
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };
    showPct(cortexKnob, cortexLabel, "Cortex");
    showPct(ichorKnob, ichorLabel, "Ichor");
    showPct(plasmaKnob, plasmaLabel, "Plasma");
}

void ModMatrixSection::setupKnob(juce::Slider& knob, juce::Label& label,
                                  const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setSliderSnapsToMousePosition(false);
    knob.setMouseDragSensitivity(200);
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

    auto placeKnob = [&](juce::Slider& knob, juce::Label& label) {
        auto col = knobRow.removeFromLeft(colW);
        label.setBounds(col.removeFromBottom(labelH));
        knob.setBounds(col);
    };

    placeKnob(cortexKnob, cortexLabel);
    placeKnob(ichorKnob, ichorLabel);
    placeKnob(plasmaKnob, plasmaLabel);
}
