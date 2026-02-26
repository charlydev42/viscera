// DelaySection.cpp — Delay knobs (On/Off, Time, Feedback, Damp, Mix, Ping-Pong)
#include "DelaySection.h"

DelaySection::DelaySection(juce::AudioProcessorValueTreeState& apvts)
{
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "DLY_ON", onToggle);

    timeKnob.initMod(apvts, bb::LFODest::DlyTime);
    setupKnob(timeKnob, timeLabel, "Time");
    timeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DLY_TIME", timeKnob);

    feedKnob.initMod(apvts, bb::LFODest::DlyFeed);
    setupKnob(feedKnob, feedLabel, "Fdbk");
    feedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DLY_FEED", feedKnob);

    setupKnob(dampKnob, dampLabel, "Damp");
    dampAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DLY_DAMP", dampKnob);

    setupKnob(spreadKnob, spreadLabel, "Sprd");
    spreadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DLY_SPREAD", spreadKnob);

    dlyMixKnob.initMod(apvts, bb::LFODest::DlyMix);
    setupKnob(dlyMixKnob, dlyMixLabel, "Mix");
    dlyMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DLY_MIX", dlyMixKnob);

    ppToggle.setButtonText("PP");
    addAndMakeVisible(ppToggle);
    ppAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "DLY_PING", ppToggle);
}

void DelaySection::setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void DelaySection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(8);
    int knobSize = 36;
    int labelH = 12;

    auto knobRow = area.withSizeKeepingCentre(area.getWidth(), knobSize + labelH);
    int colW = knobRow.getWidth() / 6;

    // Col 1: On + PP stacked
    auto toggleCol = knobRow.removeFromLeft(colW);
    auto topHalf = toggleCol.removeFromTop(toggleCol.getHeight() / 2);
    onToggle.setBounds(topHalf.reduced(4, 1));
    ppToggle.setBounds(toggleCol.reduced(4, 1));

    auto layout = [&](juce::Slider& knob, juce::Label& label)
    {
        auto col = knobRow.removeFromLeft(colW);
        label.setBounds(col.removeFromBottom(labelH));
        knob.setBounds(col);
    };

    layout(timeKnob, timeLabel);
    layout(feedKnob, feedLabel);
    layout(dampKnob, dampLabel);
    layout(spreadKnob, spreadLabel);
    layout(dlyMixKnob, dlyMixLabel);
}
