// PresetBrowser.h — Categorized preset browser with popup menu
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class VisceraProcessor;

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(VisceraProcessor& processor);
    ~PresetBrowser() override = default;
    void resized() override;

    // Called by PluginEditor to wire up the randomize logic
    std::function<void()> onRandomize;

    // Called when the user clicks the preset name (opens overlay browser)
    std::function<void()> onBrowse;

    // Called when the user clicks the save (floppy) button
    std::function<void()> onSave;

    // Called after any preset change (load, navigate, init, random)
    std::function<void()> onPresetChanged;

    // Rebuild the preset registry and refresh display
    void refreshPresetList();

    // Fallback: show the classic popup menu
    void showMenu() { showPresetMenu(); }

private:
    VisceraProcessor& proc;

    juce::TextButton presetNameBtn;
    juce::TextButton prevButton  { "<" };
    juce::TextButton nextButton  { ">" };
    juce::TextButton initButton  { "Init" };
    juce::TextButton randomButton { "Random" };
    juce::TextButton saveButton  { "+" };

    void showPresetMenu();
    void navigatePreset(int direction);
    void updatePresetName();
    std::vector<int> buildSortedOrder();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
