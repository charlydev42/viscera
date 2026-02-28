// RubberSection.cpp — Rubber comb filter knobs (On/Off, Tone, Stretch, Warp, Mix)
#include "RubberSection.h"

RubberSection::RubberSection(juce::AudioProcessorValueTreeState& apvts)
{
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "RUB_ON", onToggle);

    toneKnob.initMod(apvts, bb::LFODest::RubTone);
    setupKnob(toneKnob, toneLabel, "Tone");
    toneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "RUB_TONE", toneKnob);

    stretchKnob.initMod(apvts, bb::LFODest::RubStretch);
    setupKnob(stretchKnob, stretchLabel, "Strch");
    stretchAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "RUB_STRETCH", stretchKnob);

    feedKnob.initMod(apvts, bb::LFODest::RubFeed);
    setupKnob(feedKnob, feedLabel, "Feed");
    feedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "RUB_FEED", feedKnob);

    warpKnob.initMod(apvts, bb::LFODest::RubWarp);
    setupKnob(warpKnob, warpLabel, "Warp");
    warpAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "RUB_WARP", warpKnob);

    mixKnob.initMod(apvts, bb::LFODest::RubMix);
    setupKnob(mixKnob, mixLabel, "Mix");
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "RUB_MIX", mixKnob);

    startTimerHz(5);
}

void RubberSection::timerCallback()
{
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };
    showPct(toneKnob, toneLabel, "Tone");
    showPct(stretchKnob, stretchLabel, "Strch");
    showPct(feedKnob, feedLabel, "Feed");
    showPct(warpKnob, warpLabel, "Warp");
    showPct(mixKnob, mixLabel, "Mix");
}

void RubberSection::setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void RubberSection::resized()
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

    layout(toneKnob, toneLabel);
    layout(stretchKnob, stretchLabel);
    layout(feedKnob, feedLabel);
    layout(warpKnob, warpLabel);
    layout(mixKnob, mixLabel);
}
