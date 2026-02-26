// LFOSection.h — 3 tabbed assignable LFOs with Serum-style curve editor + learn mode
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/LFO.h"

class VisceraProcessor; // forward declaration for phase getter

// Waveform preview + drag source (standard modes) / curve editor (Custom mode)
class LFOWaveDisplay : public juce::Component,
                       private juce::Timer
{
public:
    LFOWaveDisplay(int lfoIndex);
    void paint(juce::Graphics& g) override;
    void setWaveType(int type)    { waveType = type; repaint(); }
    void setPhase(float p)        { phase = p; }
    void setLFOIndex(int idx)     { lfoIdx = idx; }

    void setLFOPointer(bb::LFO* ptr) { lfoPtr = ptr; }

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void timerCallback() override { repaint(); }

    int lfoIdx = 0;
    int waveType = 0;
    float phase = 0.0f;
    bb::LFO* lfoPtr = nullptr;

    // Curve editor drag state
    int dragPointIndex = -1;
    bool isDraggingPoint = false;
    static constexpr float kPointRadius = 5.0f;
    static constexpr float kHitRadius = 8.0f;

    // Helper: convert curve point [0,1] to pixel coords within given bounds
    juce::Point<float> pointToPixel(const bb::CurvePoint& pt, juce::Rectangle<float> area) const;
    // Helper: convert pixel coords to curve point [0,1]
    bb::CurvePoint pixelToPoint(juce::Point<float> px, juce::Rectangle<float> area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOWaveDisplay)
};

class LFOSection : public juce::Component,
                   private juce::Timer
{
public:
    LFOSection(juce::AudioProcessorValueTreeState& apvts, VisceraProcessor& proc);
    ~LFOSection() override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void switchTab(int tab);
    void updateAssignmentLabels();
    void showSlotPopup(int slotIdx); // 0-3
    void updateSyncDisplay();
    int getSyncParam() const;
    void setSyncParam(int idx);

    // Learn mode
    void enterLearnMode(int slotIdx);
    void cancelLearnMode();
    int learnSlotIndex = -1; // slot in learn mode, -1 = inactive

    juce::AudioProcessorValueTreeState& state;
    VisceraProcessor& processor;

    int activeTab = 0;

    // Tab buttons
    juce::TextButton tabButtons[3];

    // Controls for active LFO
    juce::ComboBox waveCombo;
    juce::Slider rateKnob;
    juce::Label rateLabel;

    // Fixed / Sync controls
    juce::ToggleButton fixedToggle;
    juce::Slider syncKnob;
    juce::Label syncValueLabel;
    int lastSyncIdx = 3; // default 1/4

    // Waveform display (drag source / custom editor)
    LFOWaveDisplay waveDisplay;

    // Reset custom curve button (painted refresh arrow)
    struct RefreshButton : public juce::Component
    {
        std::function<void()> onClick;
        void paint(juce::Graphics& g) override;
        void mouseUp(const juce::MouseEvent& e) override;
    };
    RefreshButton resetCurveBtn;

    // Assignment slot buttons (clickable to edit / learn)
    juce::TextButton slotButtons[4];
    juce::TextButton slotClearBtns[4]; // small "x" to unmap

    // APVTS attachments (re-created on tab switch)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;

    juce::StringArray syncNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOSection)
};
