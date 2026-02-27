// PresetBrowser.h — Sélecteur de preset avec combo + boutons prev/next/random/save
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Forward declaration pour éviter l'inclusion circulaire
class VisceraProcessor;

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(VisceraProcessor& processor);
    ~PresetBrowser() override = default;
    void resized() override;

    // Called by PluginEditor to wire up the randomize logic
    std::function<void()> onRandomize;

    // Refresh the combo box (call after saving a new user preset)
    void refreshPresetList();

private:
    VisceraProcessor& proc;
    juce::ComboBox presetCombo;
    juce::TextButton prevButton  { "<" };
    juce::TextButton nextButton  { ">" };
    juce::TextButton randomButton { "?" };
    juce::TextButton saveButton  { "+" };

    // Track combined preset info
    juce::StringArray userPresetNames;
    static constexpr int kUserIdOffset = 1000;

    void navigatePreset(int direction);
    int getTotalPresetCount() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
