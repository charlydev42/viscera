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

    dampKnob.initMod(apvts, bb::LFODest::DlyDamp);
    setupKnob(dampKnob, dampLabel, "Damp");
    dampAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DLY_DAMP", dampKnob);

    spreadKnob.initMod(apvts, bb::LFODest::DlySpread);
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

    startTimerHz(5);
}

void DelaySection::timerCallback()
{
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };

    if (timeKnob.isMouseOverOrDragging())
    {
        float v = static_cast<float>(timeKnob.getValue());
        if (v < 1.0f)
            timeLabel.setText(juce::String(v * 1000.0f, 0) + "ms", juce::dontSendNotification);
        else
            timeLabel.setText(juce::String(v, 2) + "s", juce::dontSendNotification);
    }
    else
        timeLabel.setText("Time", juce::dontSendNotification);

    showPct(feedKnob, feedLabel, "Fdbk");
    showPct(dampKnob, dampLabel, "Damp");
    showPct(spreadKnob, spreadLabel, "Sprd");
    showPct(dlyMixKnob, dlyMixLabel, "Mix");
}

void DelaySection::setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text)
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

void DelaySection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(2);
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
