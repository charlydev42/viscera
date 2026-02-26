// PresetBrowser.h — Sélecteur de preset avec combo + boutons prev/next
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

private:
    VisceraProcessor& proc;
    juce::ComboBox presetCombo;
    juce::TextButton prevButton { "<" };
    juce::TextButton nextButton { ">" };

    void updateCombo();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
