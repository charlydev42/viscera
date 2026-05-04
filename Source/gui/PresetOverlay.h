// PresetOverlay.h — Inline grid browser replacing main page content
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ParasiteProcessor;

class PresetOverlay : public juce::Component,
                      private juce::Timer
{
public:
    PresetOverlay(ParasiteProcessor& processor);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    std::function<void()> onClose;          // confirmed (double-click / Enter)
    std::function<void()> onCancel;         // cancelled (Back / Escape)
    std::function<void()> onPresetChanged;  // called on preview (single click)

    void refresh();
    void stopPreviewNote();
    int getSavedPresetIndex() const { return savedPresetIndex; }

private:
    ParasiteProcessor& proc;

    juce::String selectedCategory { "All" };
    juce::TextButton categoryButtons[9];  // "All" + 8 categories
    bool favFilterOn = false;
    juce::Rectangle<int> favToggleBounds;

    juce::ComboBox packSelector;
    juce::String selectedPack { "All" };

    struct CardRect {
        juce::Rectangle<int> bounds;
        int registryIndex = -1;
        bool isSeparator = false;
    };
    std::vector<CardRect> cards;
    int hoveredCard = -1;
    int focusedCard = -1;   // keyboard navigation
    int scrollOffset = 0;
    int totalContentHeight = 0;
    int gridTop = 0;
    int numCols = 4;

    void rebuildCards();
    int cardAtPoint(juce::Point<int> pt) const;
    void selectCard(int cardIndex);   // preview: load + trigger note
    void ensureCardVisible(int cardIndex);

    // Preview note management
    void triggerPreviewNote();
    void timerCallback() override;
    bool noteIsOn = false;

    // Saved preset to restore on cancel
    int savedPresetIndex = -1;

    // Delete confirmation state
    int confirmDeleteCard = -1;   // card index showing confirmation, -1 = none
    juce::Rectangle<int> deleteXBounds(int cardIndex) const;
    juce::Rectangle<int> favHeartBounds(int cardIndex) const;

    // Right-click "Send to user…" — opens an inline panel that paints
    // over the preset grid (no native AlertWindow). State below.
    void openSendPanel(int registryIndex);
    void closeSendPanel();
    void submitSendPanel();

    bool sendPanelOpen = false;
    juce::String sendPresetUuid;
    juce::String sendPresetName;
    juce::String sendStatusMessage;     // shown under the editor (errors / progress)
    bool         sendInFlight = false;  // disable buttons while request runs
    std::unique_ptr<juce::TextEditor> sendUsernameEditor;

    juce::Rectangle<int> sendPanelBounds;
    juce::Rectangle<int> sendEditorBounds;
    juce::Rectangle<int> sendButtonBounds;
    juce::Rectangle<int> sendCancelBounds;

    void layoutSendPanel();             // recompute bounds from current size
    void drawSendPanel(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetOverlay)
};
