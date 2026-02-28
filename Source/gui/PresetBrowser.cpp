// PresetBrowser.cpp — Sélecteur de preset (factory + user)
#include "PresetBrowser.h"
#include "../PluginProcessor.h"

PresetBrowser::PresetBrowser(VisceraProcessor& processor)
    : proc(processor)
{
    refreshPresetList();

    presetCombo.onChange = [this] {
        int id = presetCombo.getSelectedId();
        if (id <= 0) return;
        if (id < kUserIdOffset)
            proc.loadPreset(id - 1);
        else if (id - kUserIdOffset < userPresetNames.size())
            proc.loadUserPreset(userPresetNames[id - kUserIdOffset]);
    };
    addAndMakeVisible(presetCombo);

    prevButton.onClick  = [this] { navigatePreset(-1); };
    nextButton.onClick  = [this] { navigatePreset(+1); };
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);

    randomButton.onClick = [this] { if (onRandomize) onRandomize(); };
    addAndMakeVisible(randomButton);

    saveButton.onClick = [this] {
        auto* aw = new juce::AlertWindow("Save Preset",
                                          "Enter a name for the new preset:",
                                          juce::AlertWindow::NoIcon, this);
        aw->addTextEditor("name", "", "Preset name:");
        aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, aw](int result) {
                if (result == 1)
                {
                    auto name = aw->getTextEditorContents("name").trim();
                    if (name.isNotEmpty())
                    {
                        proc.saveUserPreset(name);
                        refreshPresetList();
                        // Select the newly saved preset
                        int idx = userPresetNames.indexOf(name);
                        if (idx >= 0)
                            presetCombo.setSelectedId(kUserIdOffset + idx, juce::dontSendNotification);
                    }
                }
                delete aw;  // always delete, regardless of result
            }), false);
    };
    addAndMakeVisible(saveButton);
}

void PresetBrowser::refreshPresetList()
{
    presetCombo.clear(juce::dontSendNotification);

    // Factory presets
    auto& factoryNames = VisceraProcessor::getPresetNames();
    for (int i = 0; i < factoryNames.size(); ++i)
        presetCombo.addItem(factoryNames[i], i + 1);

    // User presets
    userPresetNames = proc.getUserPresetNames();
    if (userPresetNames.size() > 0)
    {
        presetCombo.addSeparator();
        for (int i = 0; i < userPresetNames.size(); ++i)
            presetCombo.addItem(userPresetNames[i], kUserIdOffset + i);
    }

    // Restore current selection
    if (proc.isUserPreset())
    {
        int idx = userPresetNames.indexOf(proc.getUserPresetName());
        if (idx >= 0)
            presetCombo.setSelectedId(kUserIdOffset + idx, juce::dontSendNotification);
    }
    else
    {
        presetCombo.setSelectedId(proc.getCurrentPresetIndex() + 1, juce::dontSendNotification);
    }
}

void PresetBrowser::navigatePreset(int direction)
{
    int total = getTotalPresetCount();
    if (total == 0) return;

    // Find current position in the flat list
    int current = 0;
    if (proc.isUserPreset())
    {
        int idx = userPresetNames.indexOf(proc.getUserPresetName());
        current = VisceraProcessor::kNumPresets + (idx >= 0 ? idx : 0);
    }
    else
    {
        current = proc.getCurrentPresetIndex();
    }

    current = (current + direction + total) % total;

    if (current < VisceraProcessor::kNumPresets)
    {
        proc.loadPreset(current);
        presetCombo.setSelectedId(current + 1, juce::dontSendNotification);
    }
    else
    {
        int userIdx = current - VisceraProcessor::kNumPresets;
        proc.loadUserPreset(userPresetNames[userIdx]);
        presetCombo.setSelectedId(kUserIdOffset + userIdx, juce::dontSendNotification);
    }
}

int PresetBrowser::getTotalPresetCount() const
{
    return VisceraProcessor::kNumPresets + userPresetNames.size();
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds();
    int btnW = 24;
    int sp = 2;

    prevButton.setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(sp);

    // Right side: [>] [?] [+]
    saveButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(sp);
    randomButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(sp);
    nextButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(sp);

    presetCombo.setBounds(area);
}
