// PresetBrowser.cpp — Categorized preset browser (factory + user)
#include "PresetBrowser.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <vector>

PresetBrowser::PresetBrowser(ParasiteProcessor& processor)
    : proc(processor)
{
    presetNameBtn.setName("presetDisplay");
    presetNameBtn.onClick = [this] {
        if (onBrowse)
            onBrowse();
        else
            showPresetMenu();
    };
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
                if (onPresetChanged) onPresetChanged();
                return;
            }
        }
    };
    addAndMakeVisible(initButton);

    randomButton.onClick = [this] {
        if (onRandomize) onRandomize();
        proc.setDisplayName("Random");
        updatePresetName();
        if (onPresetChanged) onPresetChanged();
    };
    addAndMakeVisible(randomButton);

    saveButton.setButtonText("Save");
    saveButton.onClick = [this] {
        if (onSave) onSave();
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
    // If the processor has a display name override (e.g. "Random"), use it
    auto override = proc.getDisplayName();
    if (override.isNotEmpty())
    {
        presetNameBtn.setButtonText(override);
        return;
    }

    auto& registry = proc.getPresetRegistry();
    int idx = proc.getCurrentPresetIndex();

    juce::String name;
    if (proc.isUserPreset())
        name = proc.getUserPresetName();
    else if (idx >= 0 && idx < static_cast<int>(registry.size()))
        name = registry[static_cast<size_t>(idx)].name;
    else
        name = "Init";

    presetNameBtn.setButtonText(name);
}

void PresetBrowser::showPresetMenu()
{
    proc.buildPresetRegistry();
    auto& registry = proc.getPresetRegistry();
    juce::PopupMenu menu;

    // Category order for factory presets
    static const juce::StringArray categoryOrder { "Bass", "Lead", "Pluck", "Keys", "Pad", "Texture", "Drums", "FX" };
    int currentIdx = proc.getCurrentPresetIndex();

    // Collect indices per category, then sort alphabetically within each
    for (auto& cat : categoryOrder)
    {
        std::vector<int> indices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            auto& entry = registry[static_cast<size_t>(i)];
            if (entry.isFactory && entry.category == cat)
                indices.push_back(i);
        }
        if (indices.empty()) continue;

        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            return registry[static_cast<size_t>(a)].name.compareIgnoreCase(
                   registry[static_cast<size_t>(b)].name) < 0;
        });

        menu.addSeparator();
        menu.addSectionHeader(cat);
        for (int i : indices)
            menu.addItem(i + 1, registry[static_cast<size_t>(i)].name, true, i == currentIdx);
    }

    // User presets — sorted alphabetically
    {
        std::vector<int> userIndices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
            if (!registry[static_cast<size_t>(i)].isFactory)
                userIndices.push_back(i);

        if (!userIndices.empty())
        {
            std::sort(userIndices.begin(), userIndices.end(), [&](int a, int b) {
                return registry[static_cast<size_t>(a)].name.compareIgnoreCase(
                       registry[static_cast<size_t>(b)].name) < 0;
            });

            menu.addSeparator();
            menu.addSectionHeader("User");
            for (int i : userIndices)
                menu.addItem(i + 1, registry[static_cast<size_t>(i)].name, true, i == currentIdx);
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
                if (onPresetChanged) onPresetChanged();
            }
        });
}

std::vector<int> PresetBrowser::buildSortedOrder()
{
    auto& registry = proc.getPresetRegistry();
    std::vector<int> order;

    // Same category order as showPresetMenu
    static const juce::StringArray categoryOrder { "Bass", "Lead", "Pluck", "Keys", "Pad", "Texture", "Drums", "FX" };

    for (auto& cat : categoryOrder)
    {
        std::vector<int> catIndices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            auto& entry = registry[static_cast<size_t>(i)];
            if (entry.isFactory && entry.category == cat)
                catIndices.push_back(i);
        }
        std::sort(catIndices.begin(), catIndices.end(), [&](int a, int b) {
            return registry[static_cast<size_t>(a)].name.compareIgnoreCase(
                   registry[static_cast<size_t>(b)].name) < 0;
        });
        for (int i : catIndices)
            order.push_back(i);
    }

    // User presets sorted alphabetically
    std::vector<int> userIndices;
    for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        if (!registry[static_cast<size_t>(i)].isFactory)
            userIndices.push_back(i);
    std::sort(userIndices.begin(), userIndices.end(), [&](int a, int b) {
        return registry[static_cast<size_t>(a)].name.compareIgnoreCase(
               registry[static_cast<size_t>(b)].name) < 0;
    });
    for (int i : userIndices)
        order.push_back(i);

    return order;
}

void PresetBrowser::navigatePreset(int direction)
{
    auto order = buildSortedOrder();
    if (order.empty()) return;

    int current = proc.getCurrentPresetIndex();

    // Find current position in sorted order
    int pos = 0;
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
    {
        if (order[static_cast<size_t>(i)] == current)
        {
            pos = i;
            break;
        }
    }

    // Step in direction, wrapping around
    int n = static_cast<int>(order.size());
    pos = (pos + direction + n) % n;

    proc.loadPresetAt(order[static_cast<size_t>(pos)]);
    updatePresetName();
    if (onPresetChanged) onPresetChanged();
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds();
    int btnW = 24;
    int sp = 2;

    // Left: [<]
    prevButton.setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(sp);

    // Right side: [Save] [Random] [Init] [>]
    saveButton.setBounds(area.removeFromRight(36));
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
