// PresetBrowser.cpp — Sélecteur de preset
#include "PresetBrowser.h"
#include "../PluginProcessor.h"

PresetBrowser::PresetBrowser(VisceraProcessor& processor)
    : proc(processor)
{
    auto& names = VisceraProcessor::getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetCombo.addItem(names[i], i + 1);

    presetCombo.setSelectedId(proc.getCurrentPresetIndex() + 1, juce::dontSendNotification);
    presetCombo.onChange = [this] {
        int idx = presetCombo.getSelectedId() - 1;
        if (idx >= 0)
            proc.loadPreset(idx);
    };
    addAndMakeVisible(presetCombo);

    prevButton.onClick = [this] {
        int idx = proc.getCurrentPresetIndex() - 1;
        if (idx < 0) idx = VisceraProcessor::kNumPresets - 1;
        proc.loadPreset(idx);
        presetCombo.setSelectedId(idx + 1, juce::dontSendNotification);
    };
    addAndMakeVisible(prevButton);

    nextButton.onClick = [this] {
        int idx = proc.getCurrentPresetIndex() + 1;
        if (idx >= VisceraProcessor::kNumPresets) idx = 0;
        proc.loadPreset(idx);
        presetCombo.setSelectedId(idx + 1, juce::dontSendNotification);
    };
    addAndMakeVisible(nextButton);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(4);
    int btnW = 28;
    prevButton.setBounds(area.removeFromLeft(btnW));
    nextButton.setBounds(area.removeFromRight(btnW));
    presetCombo.setBounds(area.reduced(2, 0));
}
