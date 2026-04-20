// SaveOverlay.h — Inline save-preset overlay (name + category + Save/Cancel)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ParasiteProcessor;

class SaveOverlay : public juce::Component
{
public:
    SaveOverlay(ParasiteProcessor& processor);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    std::function<void()> onSave;
    std::function<void()> onCancel;

    void refresh();

private:
    ParasiteProcessor& proc;

    juce::TextEditor nameEditor;
    static constexpr int kNumCategories = 8;
    juce::TextButton categoryButtons[kNumCategories];
    juce::String selectedCategory { "Bass" };
    juce::TextButton saveBtn  { "Save" };
    juce::TextButton cancelBtn { "Cancel" };

    static constexpr const char* kCategories[kNumCategories] = {
        "Bass", "Lead", "Pluck", "Keys", "Pad", "Texture", "Drums", "FX"
    };

    void doSave();
    void doCancel();
    void confirmOverwrite(const juce::String& name);

    bool awaitingOverwrite = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaveOverlay)
};
