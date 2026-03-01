// PresetBrowser.h — Categorized preset browser with popup menu
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class VisceraProcessor;

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(VisceraProcessor& processor);
    ~PresetBrowser() override = default;
    void resized() override;

    // Called by PluginEditor to wire up the randomize logic
    std::function<void()> onRandomize;

    // Rebuild the preset registry and refresh display
    void refreshPresetList();

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
