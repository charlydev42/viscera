// SaveOverlay.h — Inline save-preset overlay (name + category + Save/Cancel)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class VisceraProcessor;

class SaveOverlay : public juce::Component
{
public:
    SaveOverlay(VisceraProcessor& processor);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    std::function<void()> onSave;
    std::function<void()> onCancel;

    void refresh();

private:
    VisceraProcessor& proc;

    juce::TextEditor nameEditor;
    juce::TextButton categoryButtons[6];
    juce::String selectedCategory { "Bass" };
    juce::TextButton saveBtn  { "Save" };
    juce::TextButton cancelBtn { "Cancel" };

    static constexpr const char* kCategories[6] = { "Bass", "Lead", "Pad", "FX", "Drums", "Texture" };

    void doSave();
    void doCancel();
    void confirmOverwrite(const juce::String& name);

    bool awaitingOverwrite = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaveOverlay)
};
