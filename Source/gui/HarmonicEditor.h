// HarmonicEditor.h — 32-bar harmonic amplitude editor for Custom waveform
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/HarmonicTable.h"
#include <functional>

class HarmonicEditor : public juce::Component,
                        private juce::Timer
{
public:
    HarmonicEditor(bb::HarmonicTable& table);
    ~HarmonicEditor() override { stopTimer(); }

    // Called when user draws bars manually
    std::function<void()> onUserDraw;

    // Optional: route writes through APVTS params (for undo). If set, drawBar
    // calls this instead of HarmonicTable::setHarmonic directly. Takes (idx, amp01).
    std::function<void(int, float)> onSetHarmonic;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void drawBar(const juce::MouseEvent& e);

    bb::HarmonicTable& harmonicTable;

    juce::Rectangle<int> barArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicEditor)
};
