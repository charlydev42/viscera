// PluginEditor.h — Editeur principal de Viscera
// Layout 3x3 sombre avec sections tabbées
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "gui/VisceraLookAndFeel.h"
#include "gui/PresetBrowser.h"
#include "gui/ModulatorSection.h"
#include "gui/CarrierSection.h"
#include "gui/ModMatrixSection.h"
#include "gui/FilterSection.h"
#include "gui/GlobalSection.h"
#include "gui/PitchEnvSection.h"
#include "gui/TabbedEffectSection.h"
#include "gui/VolumeShaperSection.h"
#include "gui/VisualizerDisplay.h"
#include "gui/LFOSection.h"
#include "gui/ModSlider.h"

class VisceraEditor : public juce::AudioProcessorEditor,
                      public juce::DragAndDropContainer,
                      private juce::Timer
{
public:
    explicit VisceraEditor(VisceraProcessor& processor);
    ~VisceraEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    VisceraProcessor& proc;
    VisceraLookAndFeel lookAndFeel;

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
    VisualizerDisplay visualizerDisplay;
    LFOSection lfoSection;
    GlobalSection globalSection;

    // Clavier MIDI integre
    juce::MidiKeyboardComponent keyboard;

    // Label titre
    juce::Label titleLabel;

    // Logo / branding
    juce::ImageComponent logoImage;

    // FM algorithm selector (global, lives in top bar)
    juce::TextButton algoLeftBtn, algoRightBtn;
    juce::Label algoLabel;
    juce::StringArray algoNames;
    void updateAlgoLabel();

    // Randomize button
    juce::TextButton randomBtn;
    void randomizeParams();

    // Two-page UI state
    bool showAdvanced = false;
    juce::TextButton pageToggleBtn;
    void setPage(bool advanced);

    // Main page macro knobs (Volume, Drive, Cutoff, Res, Fold, Noise)
    ModSlider macroKnobs[6];
    juce::Label macroLabels[6];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> macroAttach[6];

    // Section header painting helper
    void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds,
                           const juce::String& title);

    // Stored bounds for section headers (set in resized, used in paint)
    juce::Rectangle<int> sectionBounds[11];
    // Main page section bounds: 0=visualizer, 1=macros, 2=effects
    juce::Rectangle<int> mainSectionBounds[3];
    // Individual knob card rects for main page painting
    juce::Rectangle<int> macroCardBounds[6];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisceraEditor)
};
