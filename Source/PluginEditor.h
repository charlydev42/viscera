// PluginEditor.h — Editeur principal de Parasite
// Layout 3x3 sombre avec sections tabbées
#pragma once
#include <set>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "gui/ParasiteLookAndFeel.h"
#include "gui/PresetBrowser.h"
#include "gui/ModulatorSection.h"
#include "gui/CarrierSection.h"
#include "gui/ModMatrixSection.h"
#include "gui/FilterSection.h"
#include "gui/GlobalSection.h"
#include "gui/PitchEnvSection.h"
#include "gui/TabbedEffectSection.h"
#include "gui/VolumeShaperSection.h"
#include "gui/FlubberVisualizer.h"
#include "gui/LFOSection.h"
#include "gui/ModSlider.h"
#include "gui/PresetOverlay.h"
#include "gui/SaveOverlay.h"
#include "gui/LicenseOverlay.h"

class ParasiteEditor : public juce::AudioProcessorEditor,
                      public juce::DragAndDropContainer,
                      private juce::Timer
{
public:
    explicit ParasiteEditor(ParasiteProcessor& processor);
    ~ParasiteEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void dragOperationEnded(const juce::DragAndDropTarget::SourceDetails&) override;

private:
    ParasiteProcessor& proc;
    ParasiteLookAndFeel lookAndFeel;

    // Sous-composants
    PresetBrowser presetBrowser;
    ModulatorSection mod1Section;
    ModulatorSection mod2Section;
    CarrierSection carrierSection;
    ModMatrixSection modMatrixSection;
    FilterSection filterSection;
    PitchEnvSection pitchEnvSection;
    TabbedEffectSection tabbedEffects;
    VolumeShaperSection shaperSection;
    FlubberVisualizer flubberVisualizer;
    LFOSection lfoSection;
    GlobalSection globalSection;

    // Label titre
    juce::Label titleLabel;

    // Logo / branding
    juce::ImageComponent logoImage;        // advanced page (light/dark)
    juce::ImageComponent mainLogoImage;    // main page (neutral)

    // FM algorithm selector (global, lives in top bar)
    juce::TextButton algoLeftBtn, algoRightBtn;
    juce::Label algoLabel;
    juce::StringArray algoNames;
    void updateAlgoLabel();

    // Global octave selector (top bar, next to algo)
    juce::TextButton octLeftBtn, octRightBtn;
    juce::Label octLabel;
    void updateOctaveLabel();

    // Randomize logic (wired to PresetBrowser's ? button)
    void randomizeParams();

    // Two-page UI state
    bool showAdvanced = false;
    juce::TextButton pageToggleBtn;
    void setPage(bool advanced);

    // Settings menu (hamburger button)
    juce::TextButton menuBtn;
    void showSettingsMenu();

    // Main page macro knobs (Volume, Drive, Cortex, Plasma, Fold, Ichor)
    ModSlider macroKnobs[6];
    juce::Label macroLabels[6];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroAttach[6];

    // Main page effect mini-controls (On/Off toggle + Mix knob per effect)
    juce::ToggleButton fxToggle[4];      // DLY, REV, LIQ, RUB
    ModSlider          fxMixKnob[4];
    juce::Label        fxLabel[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fxToggleAttach[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fxMixAttach[4];

    // Section header painting helper
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds,
                           const juce::String& title);

    // Stored bounds for section headers (set in resized, used in paint)
    juce::Rectangle<int> sectionBounds[11];
    // Individual knob card rects for main page painting
    juce::Rectangle<int> macroCardBounds[6];
    // White panel bounds for main page background
    juce::Rectangle<int> mainPanelBounds;

    // Inline preset overlay (replaces main page content)
    PresetOverlay presetOverlay;
    bool showPresetOverlay = false;
    void setPresetOverlayVisible(bool visible);

    // Inline save overlay (replaces main page content)
    SaveOverlay saveOverlay;
    bool showSaveOverlay = false;
    void setSaveOverlayVisible(bool visible);

    // License overlay (blocks UI when not licensed)
    LicenseOverlay licenseOverlay;
    void updateLicenseOverlay();

    // Key handler (undo/redo on all platforms + MIDI keyboard standalone-only)
    bool keyPressed(const juce::KeyPress& key) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParasiteEditor)
};
