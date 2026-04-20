// LFOSection.h — 3 tabbed assignable LFOs with Serum-style curve editor + learn mode
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/LFO.h"
#include "ModSlider.h" // ModSliderContext + ModSliderContextProvider

class ParasiteProcessor; // forward declaration for phase getter

// Waveform preview + drag source (standard modes) / curve editor (Custom mode)
class LFOWaveDisplay : public juce::Component,
                       private juce::Timer
{
public:
    LFOWaveDisplay(int lfoIndex);
    ~LFOWaveDisplay() override { stopTimer(); }
    void paint(juce::Graphics& g) override;
    void setWaveType(int type)    { waveType = type; repaint(); }
    void setPhase(float p)        { phase = p; }
    void setLFOIndex(int idx)     { lfoIdx = idx; }

    void setLFOPointer(bb::LFO* ptr) { lfoPtr = ptr; }

    // Callback to switch wave type to Custom when double-clicking in standard mode
    std::function<void(int)> onWaveChange;

    // Fires once per committed curve edit (end of drag / after add / after
    // delete / wave→custom conversion). The owner pushes an UndoableAction
    // with the before/after control-point snapshots so Cmd+Z restores the
    // exact variable-length curve (Serum-style).
    std::function<void(std::vector<bb::CurvePoint>,
                       std::vector<bb::CurvePoint>)> onCurveCommit;

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
    std::vector<bb::CurvePoint> dragStartCurve; // snapshot at mouseDown for undo
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
    LFOSection(juce::AudioProcessorValueTreeState& apvts, ParasiteProcessor& proc);
    ~LFOSection() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void parentHierarchyChanged() override;
    void resetToTab(int tab = 0) { switchTab(tab); }

private:
    void timerCallback() override;
    void switchTab(int tab);
    void updateAssignmentLabels();
    void showSlotPopup(int slotIdx);
    void showAssignmentsPopup(juce::Component* anchor = nullptr);
    void updateSyncDisplay();
    int getSyncParam() const;
    void setSyncParam(int idx);

    // Push an UndoableAction capturing the before/after control-point
    // snapshots so Cmd+Z restores the exact curve topology.
    void pushCurveEdit(int lfoIdx,
                       const std::vector<bb::CurvePoint>& before,
                       const std::vector<bb::CurvePoint>& after);

    // Learn mode
    void enterLearnMode(int slotIdx);
    void cancelLearnMode();
    int learnSlotIndex = -1; // slot in learn mode, -1 = inactive

    // Processor's stateGeneration value as of the last poll. A mismatch
    // means a preset was loaded (or setStateInformation ran) — the armed
    // learn click is stale and must be cancelled.
    uint32_t lastSeenStateGen = 0;

    juce::AudioProcessorValueTreeState& state;
    ParasiteProcessor& processor;

    // Per-editor context — resolved on attach, used to read/write
    // onLearnClick and showDropTargets without touching static state.
    ModSliderContext* ctx = nullptr;

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

    static constexpr int kNumSlots = 8;

    // "+" button to add a new routing, "-" to remove last, count label, hint
    juce::TextButton addSlotBtn;
    juce::TextButton removeSlotBtn;
    juce::Label countLabel;
    juce::Label learnHintLabel;
    juce::Rectangle<int> slotArea; // cached from resized()
    void layoutSlots(); // dynamic layout of visible slots

    // Retrigger toggle
    juce::ToggleButton retrigToggle;

    // Velocity → Rate toggle
    juce::ToggleButton velToggle;

    // APVTS attachments (re-created on tab switch)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retrigAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> velAttach;

    juce::StringArray syncNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOSection)
};
