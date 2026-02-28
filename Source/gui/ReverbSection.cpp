// ReverbSection.cpp — Reverb knobs (On/Off, Size, Damp, Mix)
#include "ReverbSection.h"

ReverbSection::ReverbSection(juce::AudioProcessorValueTreeState& apvts)
{
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "REV_ON", onToggle);

    sizeKnob.initMod(apvts, bb::LFODest::RevSize);
    setupKnob(sizeKnob, sizeLabel, "Size");
    sizeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "REV_SIZE", sizeKnob);

    dampKnob.initMod(apvts, bb::LFODest::RevDamp);
    setupKnob(dampKnob, dampLabel, "Damp");
    dampAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "REV_DAMP", dampKnob);

    widthKnob.initMod(apvts, bb::LFODest::RevWidth);
    setupKnob(widthKnob, widthLabel, "Width");
    widthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "REV_WIDTH", widthKnob);

    pdlyKnob.initMod(apvts, bb::LFODest::RevPdly);
    setupKnob(pdlyKnob, pdlyLabel, "PDly");
    pdlyAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "REV_PDLY", pdlyKnob);

    revMixKnob.initMod(apvts, bb::LFODest::RevMix);
    setupKnob(revMixKnob, revMixLabel, "Mix");
    revMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "REV_MIX", revMixKnob);

    startTimerHz(5);
}

void ReverbSection::timerCallback()
{
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };
    showPct(sizeKnob, sizeLabel, "Size");
    showPct(dampKnob, dampLabel, "Damp");
    showPct(widthKnob, widthLabel, "Width");
    showPct(revMixKnob, revMixLabel, "Mix");

    if (pdlyKnob.isMouseOverOrDragging())
        pdlyLabel.setText(juce::String(static_cast<int>(pdlyKnob.getValue())) + "ms", juce::dontSendNotification);
    else
        pdlyLabel.setText("PDly", juce::dontSendNotification);
}

void ReverbSection::setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setSliderSnapsToMousePosition(false);
    knob.setMouseDragSensitivity(200);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void ReverbSection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(2);
    int knobSize = 36;
    int labelH = 12;

    auto knobRow = area.withSizeKeepingCentre(area.getWidth(), knobSize + labelH);
    int colW = knobRow.getWidth() / 6;

    // On/Off toggle
    auto onArea = knobRow.removeFromLeft(colW);
    onToggle.setBounds(onArea.reduced(4, 8));

    auto layout = [&](juce::Slider& knob, juce::Label& label)
    {
        auto col = knobRow.removeFromLeft(colW);
        label.setBounds(col.removeFromBottom(labelH));
        knob.setBounds(col);
    };

    layout(sizeKnob, sizeLabel);
    layout(dampKnob, dampLabel);
    layout(widthKnob, widthLabel);
    layout(pdlyKnob, pdlyLabel);
    layout(revMixKnob, revMixLabel);
}
