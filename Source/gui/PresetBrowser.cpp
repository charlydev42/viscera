// PresetBrowser.cpp — Categorized preset browser (factory + user)
#include "PresetBrowser.h"
#include "../PluginProcessor.h"

PresetBrowser::PresetBrowser(VisceraProcessor& processor)
    : proc(processor)
{
    presetNameBtn.setName("presetDisplay");
    presetNameBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetNameBtn);

    prevButton.onClick  = [this] { navigatePreset(-1); };
    nextButton.onClick  = [this] { navigatePreset(+1); };
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);

    initButton.onClick = [this] {
        auto& registry = proc.getPresetRegistry();
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            if (registry[static_cast<size_t>(i)].category == "Init")
            {
                proc.loadPresetAt(i);
                updatePresetName();
                return;
            }
        }
    };
    addAndMakeVisible(initButton);

    randomButton.onClick = [this] {
        if (onRandomize) onRandomize();
        presetNameBtn.setButtonText("Random");
    };
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
                        proc.buildPresetRegistry();
                        refreshPresetList();
                    }
                }
                delete aw;
            }), false);
    };
    addAndMakeVisible(saveButton);

    updatePresetName();
}

void PresetBrowser::refreshPresetList()
{
    updatePresetName();
}

void PresetBrowser::updatePresetName()
{
    auto& registry = proc.getPresetRegistry();
    int idx = proc.getCurrentPresetIndex();

    juce::String displayName;
    if (proc.isUserPreset())
        displayName = proc.getUserPresetName();
    else if (idx >= 0 && idx < static_cast<int>(registry.size()))
        displayName = registry[static_cast<size_t>(idx)].name;
    else
        displayName = "Init";

    presetNameBtn.setButtonText(displayName);
}

void PresetBrowser::showPresetMenu()
{
    auto& registry = proc.getPresetRegistry();
    juce::PopupMenu menu;

    // Category order for factory presets
    static const juce::StringArray categoryOrder { "Init", "Bass", "Lead", "Pad", "FX", "Texture" };

    // Group factory presets by category
    juce::String lastCategory;
    bool hasUserPresets = false;
    int currentIdx = proc.getCurrentPresetIndex();

    for (int i = 0; i < static_cast<int>(registry.size()); ++i)
    {
        auto& entry = registry[static_cast<size_t>(i)];

        // Skip Init category — handled by dedicated Init button
        if (entry.isFactory && entry.category == "Init")
            continue;

        if (entry.isFactory)
        {
            if (entry.category != lastCategory)
            {
                if (lastCategory.isNotEmpty())
                    menu.addSeparator();
                menu.addSectionHeader(entry.category);
                lastCategory = entry.category;
            }
            menu.addItem(i + 1, entry.name, true, i == currentIdx);
        }
        else
        {
            if (!hasUserPresets)
            {
                menu.addSeparator();
                menu.addSectionHeader("User");
                hasUserPresets = true;
            }
            menu.addItem(i + 1, entry.name, true, i == currentIdx);
        }
    }

    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(&presetNameBtn)
        .withMinimumWidth(presetNameBtn.getWidth()),
        [this](int result) {
            if (result > 0)
            {
                int index = result - 1;
                proc.loadPresetAt(index);
                updatePresetName();
            }
        });
}

void PresetBrowser::navigatePreset(int direction)
{
    auto& registry = proc.getPresetRegistry();
    int total = static_cast<int>(registry.size());
    if (total == 0) return;

    int current = proc.getCurrentPresetIndex();

    // Step in direction, skipping Init-category entries
    for (int i = 0; i < total; ++i)
    {
        current = (current + direction + total) % total;
        if (!(registry[static_cast<size_t>(current)].isFactory
              && registry[static_cast<size_t>(current)].category == "Init"))
            break;
    }

    proc.loadPresetAt(current);
    updatePresetName();
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds();
    int btnW = 24;
    int sp = 2;

    // Left: [<]
    prevButton.setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(sp);

    // Right side: [+] [Random] [Init] [>]
    saveButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(sp);
    randomButton.setBounds(area.removeFromRight(48));
    area.removeFromRight(sp);
    initButton.setBounds(area.removeFromRight(32));
    area.removeFromRight(sp);
    nextButton.setBounds(area.removeFromRight(btnW));
    area.removeFromRight(sp);

    // Center: [preset name ▼]
    presetNameBtn.setBounds(area);
}
