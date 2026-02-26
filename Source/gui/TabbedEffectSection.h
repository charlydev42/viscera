// TabbedEffectSection.h — Delay/Reverb/Liquid/Rubber: tabbed (main) or stacked (edit)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "DelaySection.h"
#include "ReverbSection.h"
#include "LiquidSection.h"
#include "RubberSection.h"

class TabbedEffectSection : public juce::Component
{
public:
    TabbedEffectSection(juce::AudioProcessorValueTreeState& apvts);
    ~TabbedEffectSection() override = default;

    void setStacked(bool stacked);
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void switchTab(int tab);

    bool stackedMode = false;
    int activeTab = 0;
    juce::TextButton tabButtons[4];

    DelaySection   delaySection;
    ReverbSection  reverbSection;
    LiquidSection  liquidSection;
    RubberSection  rubberSection;

    juce::Rectangle<int> panelBounds[4];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabbedEffectSection)
};
