// LiquidSection.cpp — Liquid knobs (On/Off, Rate, Depth, Tone, Feed, Mix)
#include "LiquidSection.h"

LiquidSection::LiquidSection(juce::AudioProcessorValueTreeState& apvts)
{
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "LIQ_ON", onToggle);

    rateKnob.initMod(apvts, bb::LFODest::LiqRate);
    setupKnob(rateKnob, rateLabel, "Rate");
    rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "LIQ_RATE", rateKnob);

    depthKnob.initMod(apvts, bb::LFODest::LiqDepth);
    setupKnob(depthKnob, depthLabel, "Depth");
    depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "LIQ_DEPTH", depthKnob);

    toneKnob.initMod(apvts, bb::LFODest::LiqTone);
    setupKnob(toneKnob, toneLabel, "Tone");
    toneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "LIQ_TONE", toneKnob);

    feedKnob.initMod(apvts, bb::LFODest::LiqFeed);
    setupKnob(feedKnob, feedLabel, "Feed");
    feedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "LIQ_FEED", feedKnob);

    mixKnob.initMod(apvts, bb::LFODest::LiqMix);
    setupKnob(mixKnob, mixLabel, "Mix");
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "LIQ_MIX", mixKnob);

    startTimerHz(5);
}

void LiquidSection::timerCallback()
{
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };
    showPct(rateKnob, rateLabel, "Rate");
    showPct(depthKnob, depthLabel, "Depth");
    showPct(toneKnob, toneLabel, "Tone");
    showPct(feedKnob, feedLabel, "Feed");
    showPct(mixKnob, mixLabel, "Mix");
}

void LiquidSection::setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void LiquidSection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(8);
    int knobSize = 36;
    int labelH = 12;

    auto knobRow = area.withSizeKeepingCentre(area.getWidth(), knobSize + labelH);
    int colW = knobRow.getWidth() / 6;

    // On/Off toggle
    auto onArea = knobRow.removeFromLeft(colW);
    onToggle.setBounds(onArea.reduced(4, 6));

    auto layout = [&](juce::Slider& knob, juce::Label& label)
    {
        auto col = knobRow.removeFromLeft(colW);
        label.setBounds(col.removeFromBottom(labelH));
        knob.setBounds(col);
    };

    layout(rateKnob, rateLabel);
    layout(depthKnob, depthLabel);
    layout(toneKnob, toneLabel);
    layout(feedKnob, feedLabel);
    layout(mixKnob, mixLabel);
}
