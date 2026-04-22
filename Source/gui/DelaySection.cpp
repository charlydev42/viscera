// DelaySection.cpp — Delay knobs (On/Off, Time, Feedback, Damp, Mix, Ping-Pong)
#include "DelaySection.h"

DelaySection::DelaySection(juce::AudioProcessorValueTreeState& apvts)
    : state(apvts)
{
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "DLY_ON", onToggle);

    timeKnob.initMod(apvts, bb::LFODest::DlyTime);
    setupKnob(timeKnob, timeLabel, "Time");
    timeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "DLY_TIME", timeKnob);

    // Sync knob: same slot as timeKnob (only one visible at a time), stepped
    // across 9 beat divisions. Shares the DlyTime LFO destination so any
    // mapped LFO keeps working across Time <-> Sync mode transitions.
    syncKnob.initMod(apvts, bb::LFODest::DlyTime);
    syncKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    syncKnob.setSliderSnapsToMousePosition(false);
    syncKnob.setMouseDragSensitivity(180);
    syncKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    syncKnob.setRange(1.0, 9.0, 1.0);
    syncKnob.onValueChange = [this] {
        if (auto* p = state.getParameter("DLY_SYNC"))
        {
            const int idx = juce::jlimit(1, 9, static_cast<int>(syncKnob.getValue()));
            lastSyncIdx = idx;
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        }
    };
    addChildComponent(syncKnob);

    // Right-click menu: both Time and Sync entries always shown with a
    // checkmark on the current mode. Select the other to flip.
    static constexpr int kModeTime = ModSlider::kExtraMenuIdBase;
    static constexpr int kModeSync = ModSlider::kExtraMenuIdBase + 1;
    auto buildModeItems = [this](juce::PopupMenu& m, int base)
    {
        int current = 0;
        if (auto* p = state.getRawParameterValue("DLY_SYNC"))
            current = static_cast<int>(p->load());
        const bool synced = current > 0;
        m.addSeparator();
        m.addItem(base,     "Time mode", true, !synced);
        m.addItem(base + 1, "Sync mode", true,  synced);
    };
    auto handleMode = [this](int result)
    {
        if (auto* p = state.getParameter("DLY_SYNC"))
        {
            int target;
            if (result == kModeTime)        target = 0;
            else if (result == kModeSync)   target = lastSyncIdx;
            else                             return;
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(target)));
        }
    };
    timeKnob.buildExtraMenuItems  = buildModeItems;
    timeKnob.handleExtraMenuResult = handleMode;
    syncKnob.buildExtraMenuItems  = buildModeItems;
    syncKnob.handleExtraMenuResult = handleMode;

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

    updateSyncVisibility();
    startTimerHz(5);
}

void DelaySection::updateSyncVisibility()
{
    int syncIdx = 0;
    if (auto* p = state.getRawParameterValue("DLY_SYNC"))
        syncIdx = static_cast<int>(p->load());
    const bool synced = syncIdx > 0;

    timeKnob.setVisible(!synced);
    syncKnob.setVisible(synced);

    if (synced)
    {
        if (syncIdx != static_cast<int>(syncKnob.getValue()))
            syncKnob.setValue(static_cast<double>(syncIdx), juce::dontSendNotification);
        lastSyncIdx = syncIdx;
    }
}

void DelaySection::timerCallback()
{
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };

    int syncIdx = 0;
    if (auto* sp = state.getRawParameterValue("DLY_SYNC"))
        syncIdx = static_cast<int>(sp->load());

    // Keep knob visibility in sync with the param (covers external changes:
    // automation, preset load, undo).
    const bool currentlySynced = syncKnob.isVisible();
    if ((syncIdx > 0) != currentlySynced)
        updateSyncVisibility();

    if (syncIdx > 0)
    {
        // Sync mode — permanent readout so users know they're tempo-locked
        static const char* const labels[] = {
            "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T"
        };
        timeLabel.setText(labels[syncIdx - 1], juce::dontSendNotification);
    }
    else if (timeKnob.isMouseOverOrDragging())
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
    syncKnob.setBounds(timeKnob.getBounds());
    layout(feedKnob, feedLabel);
    layout(dampKnob, dampLabel);
    layout(spreadKnob, spreadLabel);
    layout(dlyMixKnob, dlyMixLabel);
}
