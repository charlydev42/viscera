// ModMatrixSection.cpp — FM macros: Cortex/Ichor/Plasma/Time
#include "ModMatrixSection.h"

ModMatrixSection::ModMatrixSection(juce::AudioProcessorValueTreeState& apvts)
{
    cortexKnob.initMod(apvts, bb::LFODest::Cortex);
    setupKnob(cortexKnob, cortexLabel, "Vortex");
    cortexAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CORTEX", cortexKnob);

    ichorKnob.initMod(apvts, bb::LFODest::Ichor);
    setupKnob(ichorKnob, ichorLabel, "Helix");
    ichorAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "ICHOR", ichorKnob);

    plasmaKnob.initMod(apvts, bb::LFODest::Plasma);
    setupKnob(plasmaKnob, plasmaLabel, "Plasma");
    plasmaAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "PLASMA", plasmaKnob);

    // Plain juce::Slider — no initMod. See header for the rationale.
    setupKnob(timeKnob, timeLabel, "Time");
    timeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "MACRO_TIME", timeKnob);

    // Double-click resets to default
    cortexKnob.setDoubleClickReturnValue(true, 0.5);
    ichorKnob.setDoubleClickReturnValue(true, 0.0);
    plasmaKnob.setDoubleClickReturnValue(true, 0.5);
    timeKnob.setDoubleClickReturnValue(true, 0.5);

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
    showPct(cortexKnob, cortexLabel, "Vortex");
    showPct(ichorKnob, ichorLabel, "Helix");
    showPct(plasmaKnob, plasmaLabel, "Plasma");

    // Time: show multiplier value
    if (timeKnob.isMouseOverOrDragging())
    {
        float mul = std::pow(4.0f, static_cast<float>(timeKnob.getValue()) * 2.0f - 1.0f);
        timeLabel.setText(juce::String(mul, 2) + "x", juce::dontSendNotification);
    }
    else
        timeLabel.setText("Time", juce::dontSendNotification);
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
    int colW = knobRow.getWidth() / 4;

    auto placeKnob = [&](juce::Slider& knob, juce::Label& label) {
        auto col = knobRow.removeFromLeft(colW);
        label.setBounds(col.removeFromBottom(labelH));
        knob.setBounds(col);
    };

    placeKnob(cortexKnob, cortexLabel);
    placeKnob(ichorKnob, ichorLabel);
    placeKnob(plasmaKnob, plasmaLabel);
    placeKnob(timeKnob, timeLabel);
}
