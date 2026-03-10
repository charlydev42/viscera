// PluginEditor.cpp — Dark 3x3 layout with section header bars
#include "PluginEditor.h"
#include "gui/ModSlider.h"
#include "BinaryData.h"

VisceraEditor::VisceraEditor(VisceraProcessor& processor)
    : AudioProcessorEditor(processor),
      proc(processor),
      presetBrowser(processor),
      mod1Section(processor.apvts, "MOD1", "ENV1", processor.getHarmonicTable(0)),
      mod2Section(processor.apvts, "MOD2", "ENV2", processor.getHarmonicTable(1)),
      carrierSection(processor.apvts, processor.getHarmonicTable(2)),
      modMatrixSection(processor.apvts),
      filterSection(processor.apvts),
      pitchEnvSection(processor.apvts),
      tabbedEffects(processor.apvts),
      shaperSection(processor.apvts, processor.getVolumeShaper()),
      flubberVisualizer(processor.getVisualBuffer(), processor.getVisualBufferR()),
      lfoSection(processor.apvts, processor),
      globalSection(processor.apvts),
      presetOverlay(processor),
      saveOverlay(processor),
      licenseOverlay(processor.getLicenseManager())
{
    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    // Give ModSlider access to live LFO modulation values
    ModSlider::voiceParamsPtr = &processor.getVoiceParams();

    addAndMakeVisible(presetBrowser);
    addAndMakeVisible(mod1Section);
    addAndMakeVisible(mod2Section);
    addAndMakeVisible(carrierSection);
    addAndMakeVisible(modMatrixSection);
    addAndMakeVisible(filterSection);
    addAndMakeVisible(pitchEnvSection);
    addAndMakeVisible(tabbedEffects);
    addAndMakeVisible(shaperSection);
    addAndMakeVisible(flubberVisualizer);
    addAndMakeVisible(lfoSection);
    addAndMakeVisible(globalSection);

    // Clavier MIDI visuel supprimé — on utilise le clavier d'ordi (Ableton-style)

    // Titre
    titleLabel.setText("Viscera", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    // Logo image from BinaryData (light/dark variants for advanced page)
    {
        auto img = juce::ImageCache::getFromMemory(BinaryData::viscera_logo_light_nodolph_png, BinaryData::viscera_logo_light_nodolph_pngSize);
        logoImage.setImage(img, juce::RectanglePlacement::centred);
    }
    addAndMakeVisible(logoImage);

    // Neutral logo for main page
    {
        auto img = juce::ImageCache::getFromMemory(BinaryData::viscera_logo_neutral_png, BinaryData::viscera_logo_neutral_pngSize);
        mainLogoImage.setImage(img, juce::RectanglePlacement::centred);
    }
    addAndMakeVisible(mainLogoImage);

    // FM Algorithm selector
    algoNames = { "Series", "Parallel", "Stack", "Ring", "Feedback" };

    algoLeftBtn.setButtonText("<");
    algoLeftBtn.onClick = [this] {
        auto* p = proc.apvts.getParameter("FM_ALGO");
        int n = static_cast<int>(algoNames.size());
        int idx = static_cast<int>(p->convertFrom0to1(p->getValue()));
        idx = (idx - 1 + n) % n;
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        updateAlgoLabel();
    };
    addAndMakeVisible(algoLeftBtn);

    algoRightBtn.setButtonText(">");
    algoRightBtn.onClick = [this] {
        auto* p = proc.apvts.getParameter("FM_ALGO");
        int n = static_cast<int>(algoNames.size());
        int idx = static_cast<int>(p->convertFrom0to1(p->getValue()));
        idx = (idx + 1) % n;
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        updateAlgoLabel();
    };
    addAndMakeVisible(algoRightBtn);

    algoLabel.setJustificationType(juce::Justification::centred);
    algoLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    addAndMakeVisible(algoLabel);
    updateAlgoLabel();

    // Wire randomize to PresetBrowser's ? button
    presetBrowser.onRandomize = [this] { randomizeParams(); };

    // Reset LFO section to tab 1 on any preset change
    presetBrowser.onPresetChanged = [this] { lfoSection.resetToTab(0); };

    // Wire preset overlay
    presetBrowser.onBrowse = [this] {
        if (showPresetOverlay)
        {
            // Toggling off = cancel (restore saved preset)
            presetOverlay.stopPreviewNote();
            int saved = presetOverlay.getSavedPresetIndex();
            if (saved >= 0)
                proc.loadPresetAt(saved);
            setPresetOverlayVisible(false);
            presetBrowser.refreshPresetList();
        }
        else
        {
            setPresetOverlayVisible(true);
        }
    };
    presetOverlay.onClose = [this] {
        // Confirmed — keep new preset
        setPresetOverlayVisible(false);
        presetBrowser.refreshPresetList();
    };
    presetOverlay.onCancel = [this] {
        // Cancelled — preset already restored by overlay
        setPresetOverlayVisible(false);
        presetBrowser.refreshPresetList();
    };
    presetOverlay.onPresetChanged = [this] {
        presetBrowser.refreshPresetList();
        lfoSection.resetToTab(0);
    };
    addChildComponent(presetOverlay); // hidden by default

    // Wire save overlay
    presetBrowser.onSave = [this] {
        if (showSaveOverlay)
            setSaveOverlayVisible(false);
        else
            setSaveOverlayVisible(true);
    };
    saveOverlay.onSave = [this] {
        setSaveOverlayVisible(false);
        presetBrowser.refreshPresetList();
    };
    saveOverlay.onCancel = [this] {
        setSaveOverlayVisible(false);
    };
    addChildComponent(saveOverlay); // hidden by default

    // License overlay — shown when not licensed
    licenseOverlay.onLicensed = [this] { updateLicenseOverlay(); };
    addChildComponent(licenseOverlay);

    // Page toggle button
    pageToggleBtn.setButtonText("Advanced");
    pageToggleBtn.onClick = [this] {
        if (showSaveOverlay)
        {
            setSaveOverlayVisible(false);
        }
        else if (showPresetOverlay)
        {
            // Back button = cancel (restore saved preset)
            presetOverlay.stopPreviewNote();
            int saved = presetOverlay.getSavedPresetIndex();
            if (saved >= 0)
                proc.loadPresetAt(saved);
            setPresetOverlayVisible(false);
            presetBrowser.refreshPresetList();
        }
        else
        {
            setPage(!showAdvanced);
        }
    };
    addAndMakeVisible(pageToggleBtn);

    // Keyboard toggle for main page (standalone only)
#if JUCE_STANDALONE_APPLICATION
    // KB toggle removed — computer keyboard replaces it on standalone
#endif

    // Settings menu button
    menuBtn.setButtonText(juce::String::charToString(0x2630)); // ☰ hamburger
    menuBtn.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible(menuBtn);

    // Macro knobs for main page
    {
        struct MacroDef { const char* paramId; const char* label; bb::LFODest dest; };
        const MacroDef defs[6] = {
            { "VOLUME",      "Volume",  bb::LFODest::Volume       },
            { "DRIVE",       "Drive",   bb::LFODest::Drive        },
            { "CORTEX",      "Cortex",  bb::LFODest::Cortex       },
            { "PLASMA",      "Plasma",  bb::LFODest::Plasma       },
            { "DISP_AMT",    "Fold",    bb::LFODest::FoldAmt      },
            { "ICHOR",       "Ichor",   bb::LFODest::Ichor        },
        };

        for (int i = 0; i < 6; ++i)
        {
            macroKnobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            macroKnobs[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            macroKnobs[i].initMod(processor.apvts, defs[i].dest);
            addChildComponent(macroKnobs[i]);

            macroAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.apvts, defs[i].paramId, macroKnobs[i]);

            // Double-click resets to parameter default
            if (auto* param = processor.apvts.getParameter(defs[i].paramId))
                macroKnobs[i].setDoubleClickReturnValue(true,
                    param->convertFrom0to1(param->getDefaultValue()));

            macroLabels[i].setText(defs[i].label, juce::dontSendNotification);
            macroLabels[i].setJustificationType(juce::Justification::centred);
            macroLabels[i].setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
            macroLabels[i].setColour(juce::Label::textColourId, juce::Colour(VisceraLookAndFeel::kAccentColor));
            addChildComponent(macroLabels[i]);
        }
    }

    // Effect mini-controls for main page (On/Off toggle + Mix knob)
    {
        struct FxDef { const char* onId; const char* mixId; const char* name; };
        const FxDef fxDefs[4] = {
            { "DLY_ON", "DLY_MIX", "Delay" },
            { "REV_ON", "REV_MIX", "Reverb" },
            { "LIQ_ON", "LIQ_MIX", "Liquid" },
            { "RUB_ON", "RUB_MIX", "Rubber" },
        };

        for (int i = 0; i < 4; ++i)
        {
            fxToggle[i].setButtonText(fxDefs[i].name);
            addChildComponent(fxToggle[i]);
            fxToggleAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.apvts, fxDefs[i].onId, fxToggle[i]);

            fxMixKnob[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            fxMixKnob[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            {
                static const bb::LFODest fxMixDests[4] = {
                    bb::LFODest::DlyMix, bb::LFODest::RevMix,
                    bb::LFODest::LiqMix, bb::LFODest::RubMix
                };
                fxMixKnob[i].initMod(processor.apvts, fxMixDests[i]);
            }
            addChildComponent(fxMixKnob[i]);
            fxMixAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.apvts, fxDefs[i].mixId, fxMixKnob[i]);

            fxLabel[i].setText(fxDefs[i].name, juce::dontSendNotification);
            fxLabel[i].setJustificationType(juce::Justification::centred);
            fxLabel[i].setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
            fxLabel[i].setColour(juce::Label::textColourId, juce::Colour(VisceraLookAndFeel::kAccentColor));
            addChildComponent(fxLabel[i]);
        }
    }

    startTimerHz(5);

    setWantsKeyboardFocus(true);
    setSize(920, 615);

    // Start on main (perform) page
    setPage(false);

    // Check license state
    updateLicenseOverlay();

    // Allow neumorphic shadows to overflow on all interactive widgets
    std::function<void(juce::Component*)> enableUnclipped = [&](juce::Component* comp)
    {
        if (dynamic_cast<juce::Slider*>(comp) ||
            dynamic_cast<juce::ToggleButton*>(comp) ||
            dynamic_cast<juce::TextButton*>(comp) ||
            dynamic_cast<juce::ComboBox*>(comp) ||
            dynamic_cast<juce::Label*>(comp))
        {
            comp->setPaintingIsUnclipped(true);
        }
        for (auto* child : comp->getChildren())
            enableUnclipped(child);
    };
    enableUnclipped(this);

    // Top bar components: allow shadows to overflow their parent bounds
    presetBrowser.setPaintingIsUnclipped(true);
    algoLeftBtn.setPaintingIsUnclipped(true);
    algoRightBtn.setPaintingIsUnclipped(true);
    algoLabel.setPaintingIsUnclipped(true);
    menuBtn.setPaintingIsUnclipped(true);
    pageToggleBtn.setPaintingIsUnclipped(true);

    // Load first preset so sound matches displayed name
    // (done here after all attachments are created)
    if (proc.getCurrentPresetIndex() == 0 && !proc.isUserPreset())
        proc.loadPresetAt(0);
}

VisceraEditor::~VisceraEditor()
{
    stopTimer();
    ModSlider::voiceParamsPtr = nullptr;
    ModSlider::onLearnClick = nullptr;
    setLookAndFeel(nullptr);
}

void VisceraEditor::timerCallback()
{
    updateAlgoLabel();
    proc.getUndoManager().beginNewTransaction();

    // Keep macro label colors in sync with theme
    auto labelCol = VisceraLookAndFeel::darkMode
        ? juce::Colour(VisceraLookAndFeel::kAccentColor)
        : juce::Colour(VisceraLookAndFeel::kAccentColor).darker(0.25f);
    for (int i = 0; i < 6; ++i)
        macroLabels[i].setColour(juce::Label::textColourId, labelCol);
    for (int i = 0; i < 4; ++i)
        fxLabel[i].setColour(juce::Label::textColourId, labelCol);
}

void VisceraEditor::randomizeParams()
{
    auto& rng = juce::Random::getSystemRandom();
    auto& apvts = proc.apvts;

    auto randFloat = [&](const juce::String& id, float lo, float hi) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(lo + rng.nextFloat() * (hi - lo)));
    };
    auto randInt = [&](const juce::String& id, int lo, int hi) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(lo + rng.nextInt(hi - lo + 1))));
    };
    auto randBool = [&](const juce::String& id, float chance = 0.5f) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(rng.nextFloat() < chance ? 1.0f : 0.0f);
    };

    // Modulators
    for (auto& prefix : { juce::String("MOD1"), juce::String("MOD2") })
    {
        randInt(prefix + "_WAVE", 0, 4);
        randInt(prefix + "_COARSE", 0, 12);
        randFloat(prefix + "_FINE", -200.0f, 200.0f);
        randFloat(prefix + "_FIXED_FREQ", 50.0f, 4000.0f);
        randInt(prefix + "_MULTI", 3, 5); // x0.1 to x10
        randBool(prefix + "_KB", 0.8f);
        randFloat(prefix + "_LEVEL", 0.1f, 1.0f);
    }

    // Mod envelopes
    for (auto& env : { juce::String("ENV1"), juce::String("ENV2") })
    {
        randFloat(env + "_A", 0.001f, 0.5f);
        randFloat(env + "_D", 0.01f, 0.8f);
        randFloat(env + "_S", 0.0f, 1.0f);
        randFloat(env + "_R", 0.01f, 1.0f);
    }

    // Carrier
    randInt("CAR_WAVE", 0, 4);
    randInt("CAR_COARSE", 0, 4);
    randFloat("CAR_FINE", -100.0f, 100.0f);
    randBool("CAR_KB", 0.9f);
    randFloat("CAR_DRIFT", 0.0f, 0.3f);
    randFloat("CAR_NOISE", 0.0f, rng.nextFloat() < 0.15f ? 0.15f : 0.0f);
    randFloat("CAR_SPREAD", 0.0f, 0.5f);

    // Carrier envelope
    randFloat("ENV3_A", 0.001f, 0.3f);
    randFloat("ENV3_D", 0.01f, 0.6f);
    randFloat("ENV3_S", 0.2f, 1.0f);
    randFloat("ENV3_R", 0.05f, 1.5f);

    // Algorithm, XOR, Sync
    randInt("FM_ALGO", 0, 4);
    randBool("XOR_ON", 0.2f);
    randBool("SYNC", 0.15f);

    // LFO amounts
    randFloat("TREMOR", 0.0f, 0.3f);
    randFloat("VEIN", 0.0f, 0.4f);
    randFloat("FLUX", 0.0f, 0.4f);

    // Filter
    randBool("FILT_ON", 0.6f);
    randInt("FILT_TYPE", 0, 2);
    randFloat("FILT_CUTOFF", 200.0f, 15000.0f);
    randFloat("FILT_RES", 0.0f, 0.7f);

    // Effects — lower chance of being on
    randBool("DLY_ON", 0.3f);
    randFloat("DLY_TIME", 0.05f, 0.5f);
    randFloat("DLY_FEED", 0.1f, 0.6f);
    randFloat("DLY_MIX", 0.1f, 0.4f);

    randBool("REV_ON", 0.3f);
    randFloat("REV_SIZE", 0.2f, 0.9f);
    randFloat("REV_MIX", 0.1f, 0.4f);

    randBool("LIQ_ON", 0.2f);
    randBool("RUB_ON", 0.15f);

    // Pitch envelope
    randBool("PENV_ON", 0.25f);
    randFloat("PENV_AMT", -24.0f, 24.0f);
    randFloat("PENV_A", 0.001f, 0.3f);
    randFloat("PENV_D", 0.01f, 0.5f);
    randFloat("PENV_S", 0.0f, 1.0f);
    randFloat("PENV_R", 0.01f, 1.0f);

    // Macros
    randFloat("DRIVE", 0.0f, 0.4f);
    randFloat("DISP_AMT", 0.0f, 0.3f);
    randFloat("CORTEX", 0.0f, 0.5f);
    randFloat("PLASMA", 0.0f, 0.5f);
    randFloat("ICHOR", 0.0f, 0.5f);

    // Estimate loudness from patch parameters and compensate volume
    {
        float loudness = 1.0f;

        // FM modulation depth → more harmonics → louder
        float mod1Lvl = apvts.getRawParameterValue("MOD1_LEVEL")->load();
        float mod2Lvl = apvts.getRawParameterValue("MOD2_LEVEL")->load();
        loudness += (mod1Lvl + mod2Lvl) * 0.35f;

        // Drive adds gain
        float drive = apvts.getRawParameterValue("DRIVE")->load();
        loudness += drive * 0.5f;

        // XOR adds harmonics
        if (apvts.getRawParameterValue("XOR_ON")->load() > 0.5f)
            loudness += 0.25f;

        // Fold (DISP_AMT) adds harmonics
        float fold = apvts.getRawParameterValue("DISP_AMT")->load();
        loudness += fold * 0.35f;

        // Cortex / Plasma / Ichor can add energy
        float cortex = apvts.getRawParameterValue("CORTEX")->load();
        float plasma = apvts.getRawParameterValue("PLASMA")->load();
        float ichor  = apvts.getRawParameterValue("ICHOR")->load();
        loudness += (cortex + plasma + ichor) * 0.15f;

        // Effects add energy
        if (apvts.getRawParameterValue("DLY_ON")->load() > 0.5f)
            loudness += apvts.getRawParameterValue("DLY_MIX")->load() * 0.2f;
        if (apvts.getRawParameterValue("REV_ON")->load() > 0.5f)
            loudness += apvts.getRawParameterValue("REV_MIX")->load() * 0.3f;

        // Filter off = full spectrum = louder
        if (apvts.getRawParameterValue("FILT_ON")->load() < 0.5f)
            loudness += 0.15f;

        // Compensate: target consistent level
        float targetVol = juce::jlimit(0.15f, 0.8f, 0.65f / loudness);
        if (auto* p = apvts.getParameter("VOLUME"))
            p->setValueNotifyingHost(p->convertTo0to1(targetVol));
    }

    // Global LFOs — moderate randomization
    for (int n = 1; n <= 3; ++n)
    {
        auto pfx = "LFO" + juce::String(n) + "_";
        randFloat(pfx + "RATE", 0.2f, 8.0f);
        randInt(pfx + "WAVE", 0, 4);
        // 30% chance of one active assignment per LFO, clear all 8 slots
        // Never assign to Volume (index 6) — vol shaper handles that
        for (int s = 1; s <= 8; ++s)
        {
            if (s == 1 && rng.nextFloat() < 0.3f)
            {
                int dest;
                do { dest = 1 + rng.nextInt(10); } while (dest == 6);
                randInt(pfx + "DEST" + juce::String(s), dest, dest);
                randFloat(pfx + "AMT" + juce::String(s), -0.5f, 0.5f);
            }
            else
            {
                randInt(pfx + "DEST" + juce::String(s), 0, 0); // None
                randFloat(pfx + "AMT" + juce::String(s), 0.0f, 0.0f);
            }
        }
    }

    updateAlgoLabel();
}

void VisceraEditor::updateAlgoLabel()
{
    int idx = static_cast<int>(proc.apvts.getRawParameterValue("FM_ALGO")->load());
    if (idx >= 0 && idx < algoNames.size())
        algoLabel.setText(algoNames[idx], juce::dontSendNotification);
}

void VisceraEditor::dragOperationEnded(const juce::DragAndDropTarget::SourceDetails&)
{
    // Clean up drop-target glow when any drag finishes (success or cancel)
    ModSlider::showDropTargets = false;
}

void VisceraEditor::setPresetOverlayVisible(bool visible)
{
    // Close save overlay if opening preset overlay
    if (visible && showSaveOverlay)
        setSaveOverlayVisible(false);

    showPresetOverlay = visible;
    presetOverlay.setVisible(visible);

    if (visible)
        presetOverlay.refresh();

    // Update page toggle button text
    if (visible)
        pageToggleBtn.setButtonText("Back");
    else if (showAdvanced)
        pageToggleBtn.setButtonText("Home");
    else
        pageToggleBtn.setButtonText("Advanced");

    if (showAdvanced)
    {
        // Advanced page: hide/show edit sections behind overlay
        mod1Section.setVisible(!visible);
        mod2Section.setVisible(!visible);
        carrierSection.setVisible(!visible);
        modMatrixSection.setVisible(!visible);
        lfoSection.setVisible(!visible);
        filterSection.setVisible(!visible);
        pitchEnvSection.setVisible(!visible);
        tabbedEffects.setVisible(!visible);
        shaperSection.setVisible(!visible);
        globalSection.setVisible(!visible);
        logoImage.setVisible(!visible);
    }
    else
    {
        // Main page: hide/show perform content behind overlay
        flubberVisualizer.setVisible(!visible);
        if (visible)
            flubberVisualizer.setBounds(0, 0, 0, 0); // clear GL native view bounds
        for (int i = 0; i < 6; ++i)
        {
            macroKnobs[i].setVisible(!visible);
            macroLabels[i].setVisible(!visible);
        }
        for (int i = 0; i < 4; ++i)
        {
            fxToggle[i].setVisible(!visible);
            fxMixKnob[i].setVisible(!visible);
            fxLabel[i].setVisible(!visible);
        }
        mainLogoImage.setVisible(!visible);
    }

    if (visible)
    {
        presetOverlay.setBounds(mainPanelBounds);
        presetOverlay.toFront(false);
    }
    else
    {
        // Restore flubber bounds when closing overlay on main page
        resized();
    }

    repaint();
}

void VisceraEditor::setSaveOverlayVisible(bool visible)
{
    // Close preset overlay if opening save overlay
    if (visible && showPresetOverlay)
    {
        presetOverlay.stopPreviewNote();
        int saved = presetOverlay.getSavedPresetIndex();
        if (saved >= 0)
            proc.loadPresetAt(saved);
        setPresetOverlayVisible(false);
        presetBrowser.refreshPresetList();
    }

    showSaveOverlay = visible;
    saveOverlay.setVisible(visible);

    if (visible)
        saveOverlay.refresh();

    // Update page toggle button text
    if (visible)
        pageToggleBtn.setButtonText("Back");
    else if (showPresetOverlay)
        pageToggleBtn.setButtonText("Back");
    else if (showAdvanced)
        pageToggleBtn.setButtonText("Home");
    else
        pageToggleBtn.setButtonText("Advanced");

    if (showAdvanced)
    {
        mod1Section.setVisible(!visible);
        mod2Section.setVisible(!visible);
        carrierSection.setVisible(!visible);
        modMatrixSection.setVisible(!visible);
        lfoSection.setVisible(!visible);
        filterSection.setVisible(!visible);
        pitchEnvSection.setVisible(!visible);
        tabbedEffects.setVisible(!visible);
        shaperSection.setVisible(!visible);
        globalSection.setVisible(!visible);
        logoImage.setVisible(!visible);
    }
    else
    {
        flubberVisualizer.setVisible(!visible);
        if (visible)
            flubberVisualizer.setBounds(0, 0, 0, 0); // clear GL native view bounds
        for (int i = 0; i < 6; ++i)
        {
            macroKnobs[i].setVisible(!visible);
            macroLabels[i].setVisible(!visible);
        }
        for (int i = 0; i < 4; ++i)
        {
            fxToggle[i].setVisible(!visible);
            fxMixKnob[i].setVisible(!visible);
            fxLabel[i].setVisible(!visible);
        }
        mainLogoImage.setVisible(!visible);
    }

    if (visible)
    {
        saveOverlay.setBounds(mainPanelBounds);
        saveOverlay.toFront(false);
    }
    else
    {
        resized();
    }

    repaint();
}

void VisceraEditor::setPage(bool advanced)
{
    showAdvanced = advanced;
    pageToggleBtn.setButtonText(advanced ? "Home" : "Advanced");

    // Close overlays when switching pages
    if (showPresetOverlay)
    {
        showPresetOverlay = false;
        presetOverlay.setVisible(false);
    }
    if (showSaveOverlay)
    {
        showSaveOverlay = false;
        saveOverlay.setVisible(false);
    }

    // Advanced-only sections
    mod1Section.setVisible(advanced);
    mod2Section.setVisible(advanced);
    carrierSection.setVisible(advanced);
    modMatrixSection.setVisible(advanced);
    lfoSection.setVisible(advanced);
    filterSection.setVisible(advanced);
    pitchEnvSection.setVisible(advanced);
    shaperSection.setVisible(advanced);
    globalSection.setVisible(advanced);
    logoImage.setVisible(advanced);
    mainLogoImage.setVisible(!advanced);

    // Main-only components
    for (int i = 0; i < 6; ++i)
    {
        macroKnobs[i].setVisible(!advanced);
        macroLabels[i].setVisible(!advanced);
    }
    for (int i = 0; i < 4; ++i)
    {
        fxToggle[i].setVisible(!advanced);
        fxMixKnob[i].setVisible(!advanced);
        fxLabel[i].setVisible(!advanced);
    }
    // Flubber visualizer only on main page (and not during overlays)
    flubberVisualizer.setVisible(!advanced && !showPresetOverlay && !showSaveOverlay);
    if (advanced)
        flubberVisualizer.setBounds(0, 0, 0, 0); // clear stale bounds so GL native view doesn't intercept events

    // Effects: hidden on main (we use mini-controls), stacked on edit
    tabbedEffects.setVisible(advanced);
    tabbedEffects.setLayout(TabbedEffectSection::Stacked);

    // Keyboard: standalone only
#if JUCE_STANDALONE_APPLICATION
#endif

    // Both pages: effects, preset, keyboard, logo, algo, randomize, toggle

    resized();
    repaint();
}

// Section header: neumorphic raised panel with darker header bar
void VisceraEditor::drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds,
                                       const juce::String& title)
{
    float cr = 8.0f;

    // Neumorphic double shadow (raised, rounded to match panel)
    auto bf = bounds.toFloat();
    juce::Path panelPath;
    panelPath.addRoundedRectangle(bf, cr);
    juce::DropShadow lightSh(juce::Colour(VisceraLookAndFeel::kShadowLight).withAlpha(0.7f), 4, { -2, -2 });
    lightSh.drawForPath(g, panelPath);
    juce::DropShadow darkSh(juce::Colour(VisceraLookAndFeel::kShadowDark).withAlpha(0.5f), 6, { 3, 3 });
    darkSh.drawForPath(g, panelPath);

    // Fill whole panel
    g.setColour(juce::Colour(VisceraLookAndFeel::kBgColor));
    g.fillRoundedRectangle(bf, cr);

    int headerH = 16;
    auto headerBar = bounds.removeFromTop(headerH);

    // Header bar background (rounded top corners)
    juce::Path headerPath;
    headerPath.addRoundedRectangle(headerBar.getX(), headerBar.getY(),
                                    headerBar.getWidth(), headerBar.getHeight(),
                                    cr, cr, true, true, false, false);
    g.setColour(juce::Colour(VisceraLookAndFeel::kHeaderBg));
    g.fillPath(headerPath);

    // Title text centered in header bar
    if (title.isNotEmpty())
    {
        g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        g.drawText(title, headerBar, juce::Justification::centred);
    }
}

void VisceraEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(VisceraLookAndFeel::kBgColor));

    if (!showAdvanced)
    {
        // Main page: flat background, no neumorphic panel
    }
    else if (!showPresetOverlay)
    {
        // Advanced page: 10 section headers (no visualizer)
        static const char* titles[] = {
            "Mod 1", "Mod 2", "Carrier", "Macros",
            "LFO", "Filter", "Pitch Env",
            "Effects", "Vol Shaper", "Global"
        };
        for (int i = 0; i < 10; ++i)
            drawSectionHeader(g, sectionBounds[i], titles[i]);
    }
}

void VisceraEditor::resized()
{
    // License overlay covers entire window
    if (licenseOverlay.isVisible())
        licenseOverlay.setBounds(getLocalBounds());

    auto area = getLocalBounds().reduced(4);
    titleLabel.setBounds(0, 0, 0, 0); // hidden

    // === Top bar: [< Algo >] [Preset Browser (< combo > ? +)] [KB] [Edit] ===
    int barH = 26;
    int sp = 4;
    auto topBar = area.removeFromTop(barH);

    // Logo moved to centre column (advanced page)

    algoLeftBtn.setBounds(topBar.removeFromLeft(22));
    algoLabel.setBounds(topBar.removeFromLeft(58));
    algoRightBtn.setBounds(topBar.removeFromLeft(22));
    topBar.removeFromLeft(sp);

    menuBtn.setBounds(topBar.removeFromRight(40));
    topBar.removeFromRight(sp);
    pageToggleBtn.setBounds(topBar.removeFromRight(62));
    topBar.removeFromRight(sp);

    presetBrowser.setBounds(topBar);
    area.removeFromTop(4);


    int gap = 6;
    int headerH = 16;

    // Store content area for overlay (available on both pages)
    mainPanelBounds = area;

    if (showAdvanced)
        area.removeFromBottom(3);

    if (!showAdvanced)
    {
        // =============================================
        // MAIN (PERFORM) PAGE — Oval viz + macro knobs + FX controls around ellipse
        // =============================================
        // Layout constants
        constexpr int   kMacroKnobSize    = 58;
        constexpr int   kFxKnobSize       = 44;
        constexpr int   kLabelH           = 14;
        constexpr float kVizWidthRatio    = 0.62f;   // visualizer width as fraction of area
        constexpr float kVizHeightRatio   = 0.66f;   // visualizer height as fraction of area
        constexpr int   kVizVerticalShift = -50;      // shift viz upward
        constexpr float kMacroOrbitXMul   = 1.45f;    // macro orbit X padding multiplier
        constexpr float kMacroOrbitYMul   = 0.9f;     // macro orbit Y padding multiplier
        constexpr float kFxOrbitPadX      = 28.0f;    // FX orbit extra X beyond macro
        constexpr float kFxOrbitPadY      = 22.0f;    // FX orbit extra Y beyond macro

        int knobSize = kMacroKnobSize;
        int fxKnobSize = kFxKnobSize;
        int labelH = kLabelH;

        // Rectangular flubber visualizer centered
        int vizW = static_cast<int>(area.getWidth() * kVizWidthRatio);
        int vizH = static_cast<int>(area.getHeight() * kVizHeightRatio);
        auto vizBounds = area.withSizeKeepingCentre(vizW, vizH);
        vizBounds.translate(0, kVizVerticalShift);
        flubberVisualizer.setBounds(vizBounds);

        float cx = static_cast<float>(vizBounds.getCentreX());
        float cy = static_cast<float>(vizBounds.getCentreY());
        constexpr float pi = juce::MathConstants<float>::pi;

        // --- Macro knobs: 3 left, 3 right (along sides of ellipse) ---
        float macroRx = static_cast<float>(vizW) * 0.5f + static_cast<float>(knobSize) * kMacroOrbitXMul;
        float macroRy = static_cast<float>(vizH) * 0.5f + static_cast<float>(knobSize) * kMacroOrbitYMul;

        // Left: Cortex(2), Ichor(5), Plasma(3)  |  Right: Drive(1), Fold(4), Volume(0)
        float leftAngles[3]  = { 150.0f * pi / 180.0f, 180.0f * pi / 180.0f, 210.0f * pi / 180.0f };
        float rightAngles[3] = {  30.0f * pi / 180.0f,   0.0f * pi / 180.0f, 330.0f * pi / 180.0f };
        int leftIdx[3]  = { 2, 5, 3 };
        int rightIdx[3] = { 1, 4, 0 };

        auto placeKnob = [&](int idx, float angle)
        {
            int kx = static_cast<int>(cx + macroRx * std::cos(angle)) - knobSize / 2;
            int ky = static_cast<int>(cy - macroRy * std::sin(angle)) - knobSize / 2;
            macroKnobs[idx].setBounds(kx, ky, knobSize, knobSize);
            macroLabels[idx].setBounds(kx - 6, ky + knobSize, knobSize + 12, labelH);
            macroCardBounds[idx] = { kx, ky, knobSize, knobSize + labelH };
        };

        for (int i = 0; i < 3; ++i)
        {
            placeKnob(leftIdx[i], leftAngles[i]);
            placeKnob(rightIdx[i], rightAngles[i]);
        }

        // --- Effect mini-controls: outer orbit, bottom arc ---
        float fxRx = macroRx + kFxOrbitPadX;
        float fxRy = macroRy + kFxOrbitPadY;
        // 4 effects centered at bottom: ~252°, 264°, 276°, 288°
        float fxAngles[4] = { 252.0f * pi / 180.0f, 264.0f * pi / 180.0f,
                               276.0f * pi / 180.0f, 288.0f * pi / 180.0f };

        for (int i = 0; i < 4; ++i)
        {
            float angle = fxAngles[i];
            int kx = static_cast<int>(cx + fxRx * std::cos(angle)) - fxKnobSize / 2;
            int ky = static_cast<int>(cy - fxRy * std::sin(angle)) - fxKnobSize / 2;

            // Toggle (no text): small On/Off above the knob
            fxToggle[i].setButtonText("");
            fxToggle[i].setBounds(kx + fxKnobSize / 2 - 8, ky - 22, 16, 14);
            // Mix knob
            fxMixKnob[i].setBounds(kx, ky, fxKnobSize, fxKnobSize);
            // Label below knob
            fxLabel[i].setBounds(kx - 8, ky + fxKnobSize, fxKnobSize + 16, labelH);
        }

        // Neutral logo at bottom of main page
        int logoW = 180;
        int logoH = static_cast<int>(logoW * (1080.0f / 1920.0f)); // keep aspect ratio
        int logoX = area.getRight() - logoW - 8;
        int logoY = area.getBottom() - logoH + 4;
        mainLogoImage.setBounds(logoX, logoY, logoW, logoH);

        // Overlays cover the entire main panel area
        if (showPresetOverlay)
            presetOverlay.setBounds(mainPanelBounds);
        if (showSaveOverlay)
            saveOverlay.setBounds(mainPanelBounds);
    }
    else
    {
        // =============================================
        // ADVANCED (EDIT) PAGE
        // =============================================
        // Clear flubber bounds so its OpenGL native view doesn't intercept mouse events
        flubberVisualizer.setBounds(0, 0, 0, 0);

        int totalH = area.getHeight();

        int colW = (area.getWidth() - gap * 2) / 3;

        auto leftCol   = area.removeFromLeft(colW);
        area.removeFromLeft(gap);
        auto centreCol = area.removeFromLeft(colW);
        area.removeFromLeft(gap);
        auto rightCol  = area;

        // Helper: place a section with header space reserved above it
        auto placeSection = [&](juce::Rectangle<int>& col, int height,
                                juce::Component& section, int idx)
        {
            auto block = col.removeFromTop(height);
            sectionBounds[idx] = block;
            section.setBounds(block.withTrimmedTop(headerH).reduced(4, 0));
        };

        // === CENTRE COLUMN heights (computed first so left can align) ===
        int macrosH = 70;
        int filterH = 70;
        int pitchH = 160;
        int logoH = 60;
        int lfoH = totalH - macrosH - filterH - pitchH - logoH - gap * 4;

        // Filter top Y in centre column = macrosH + gap + lfoH + gap + logoH + gap
        int filterTopOffset = macrosH + gap + lfoH + gap + logoH + gap;

        // === LEFT COLUMN: Mod1 → Mod2 → Carrier (FM chain) ===
        // Carrier should align its top with filter in centre column
        {
            // mod1 + gap + mod2 + gap = filterTopOffset → carrier aligns with filter
            int modH = (filterTopOffset - gap * 2) / 2;

            placeSection(leftCol, modH, mod1Section, 0);
            leftCol.removeFromTop(gap);
            placeSection(leftCol, modH, mod2Section, 1);
            leftCol.removeFromTop(gap);
            sectionBounds[2] = leftCol;
            carrierSection.setBounds(leftCol.withTrimmedTop(headerH).reduced(4, 0));
        }

        // === CENTRE COLUMN: Macros | LFO Assign | Logo | Filter + Pitch Env ===
        {
            placeSection(centreCol, macrosH, modMatrixSection, 3);
            centreCol.removeFromTop(gap);
            placeSection(centreCol, lfoH, lfoSection, 4);
            centreCol.removeFromTop(gap);
            logoImage.setBounds(centreCol.removeFromTop(logoH).reduced(20, 6));
            centreCol.removeFromTop(gap);
            placeSection(centreCol, filterH, filterSection, 5);
            centreCol.removeFromTop(gap);
            sectionBounds[6] = centreCol;
            pitchEnvSection.setBounds(centreCol.withTrimmedTop(headerH).reduced(4, 0));
        }

        // === RIGHT COLUMN: Effects | Vol Shaper | Global ===
        {
            int globalH = 75;
            int shaperH = 155;
            int effectsH = totalH - globalH - shaperH - gap * 2;

            placeSection(rightCol, effectsH, tabbedEffects, 7);
            rightCol.removeFromTop(gap);
            placeSection(rightCol, shaperH, shaperSection, 8);
            rightCol.removeFromTop(gap);
            auto globalBlock = rightCol.removeFromTop(globalH);
            sectionBounds[9] = globalBlock;
            globalSection.setBounds(globalBlock.withTrimmedTop(headerH).reduced(4, 0));
        }

        // Overlays cover the entire content area
        if (showPresetOverlay)
            presetOverlay.setBounds(mainPanelBounds);
        if (showSaveOverlay)
            saveOverlay.setBounds(mainPanelBounds);
    }
}

// =============================================================================
// Key handling — undo/redo (all platforms) + computer MIDI keyboard (standalone)
// =============================================================================

#if JUCE_STANDALONE_APPLICATION
// Note mapping: supports both QWERTY and AZERTY physical layouts
struct NoteMap { int offset; juce::juce_wchar keys[2]; int numKeys; };
static const NoteMap kNoteMapping[] = {
    {  0, {'a','q'}, 2 },  // C
    {  1, {'w','z'}, 2 },  // C#
    {  2, {'s', 0 }, 1 },  // D
    {  3, {'e', 0 }, 1 },  // D#
    {  4, {'d', 0 }, 1 },  // E
    {  5, {'f', 0 }, 1 },  // F
    {  6, {'t', 0 }, 1 },  // F#
    {  7, {'g', 0 }, 1 },  // G
    {  8, {'y', 0 }, 1 },  // G#
    {  9, {'h', 0 }, 1 },  // A
    { 10, {'u', 0 }, 1 },  // A#
    { 11, {'j', 0 }, 1 },  // B
    { 12, {'k', 0 }, 1 },  // C+1
    { 13, {'o', 0 }, 1 },  // C#+1
    { 14, {'l', 0 }, 1 },  // D+1
    { 15, {'p', 0 }, 1 },  // D#+1
};
#endif

void VisceraEditor::parentHierarchyChanged()
{
#if JUCE_STANDALONE_APPLICATION
    if (auto* docWindow = findParentComponentOfClass<juce::DocumentWindow>())
    {
        if (!docWindow->isUsingNativeTitleBar())
        {
            docWindow->setUsingNativeTitleBar(true);
            // Re-set editor size so the window adapts
            auto safeThis = juce::Component::SafePointer<VisceraEditor>(this);
            juce::MessageManager::callAsync([safeThis] {
                if (safeThis != nullptr)
                    safeThis->setSize(920, 615);
            });
        }
    }
#endif
}

bool VisceraEditor::keyPressed(const juce::KeyPress& key)
{
    // Redo: Cmd+Shift+Z (Mac) / Ctrl+Shift+Z (PC) — check before undo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier
                                    | juce::ModifierKeys::shiftModifier, 0))
    {
        proc.getUndoManager().redo();
        return true;
    }

    // Undo: Cmd+Z (Mac) / Ctrl+Z (PC)
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        proc.getUndoManager().undo();
        return true;
    }

#if JUCE_STANDALONE_APPLICATION
    auto c = static_cast<juce::juce_wchar>(std::tolower(key.getTextCharacter()));

    // Octave shift: C = down, V = up
    if (c == 'c') { computerKeyOctave = juce::jmax(0, computerKeyOctave - 1); return true; }
    if (c == 'v') { computerKeyOctave = juce::jmin(8, computerKeyOctave + 1); return true; }

    // Consume note keys to prevent macOS system beep
    for (auto& m : kNoteMapping)
        for (int k = 0; k < m.numKeys; ++k)
            if (m.keys[k] == c)
                return true;
#endif

    return false;
}

#if JUCE_STANDALONE_APPLICATION

bool VisceraEditor::keyStateChanged(bool /*isKeyDown*/)
{
    bool handled = false;

    for (auto& m : kNoteMapping)
    {
        int note = computerKeyOctave * 12 + m.offset;
        if (note < 0 || note > 127) continue;

        bool anyDown = false;
        for (int k = 0; k < m.numKeys; ++k)
            if (juce::KeyPress::isKeyCurrentlyDown(static_cast<int>(m.keys[k])))
                anyDown = true;

        bool wasDown = computerKeysDown.count(note) > 0;

        if (anyDown && !wasDown)
        {
            proc.keyboardState.noteOn(1, note, 0.7f);
            computerKeysDown.insert(note);
            handled = true;
        }
        else if (!anyDown && wasDown)
        {
            proc.keyboardState.noteOff(1, note, 0.0f);
            computerKeysDown.erase(note);
            handled = true;
        }
    }

    return handled;
}

#endif

// =====================================================================
// License overlay — shown when plugin is not licensed
// =====================================================================

void VisceraEditor::updateLicenseOverlay()
{
    bool licensed = proc.getLicenseManager().isLicensed();
    licenseOverlay.setVisible(!licensed);

    if (!licensed)
    {
        // Hide OpenGL visualizer — native view renders above JUCE components
        flubberVisualizer.setVisible(false);
        flubberVisualizer.setBounds(0, 0, 0, 0);

        licenseOverlay.setBounds(getLocalBounds());
        licenseOverlay.toFront(true);
    }
    else
    {
        // Restore visualizer if on main page
        if (!showAdvanced && !showPresetOverlay && !showSaveOverlay)
            flubberVisualizer.setVisible(true);
        resized();
    }
}

void VisceraEditor::showSettingsMenu()
{
    // Toggle: if menu is already open, dismiss it
    if (juce::PopupMenu::dismissAllActiveMenus(); menuBtn.getToggleState())
    {
        menuBtn.setToggleState(false, juce::dontSendNotification);
        return;
    }
    menuBtn.setToggleState(true, juce::dontSendNotification);

    juce::PopupMenu menu;

    // 1. Dark mode (ticked checkbox)
    menu.addItem(1, "Dark Mode", true, VisceraLookAndFeel::darkMode);

    menu.addSeparator();

    // 2. License info
    auto& lm = proc.getLicenseManager();
    if (lm.isLicensed())
    {
        auto key = lm.getLicenseKey();
        juce::String masked = key.substring(0, 5) + "****-****-****-" + key.getLastCharacters(4);
        menu.addItem(-1, "License: " + masked, false);
        menu.addItem(2, "Deactivate License");
    }
    else
    {
        menu.addItem(-1, "Not licensed", false);
    }

    menu.addSeparator();

    // 3. Account & presets
    menu.addItem(3, "Manage Account & Presets");

    // Right-align: use the full button screen bounds as target.
    // JUCE centers the popup on the target, so we shift the target right
    // by half the menu width minus half the button width to right-align edges.
    int menuW = 220;
    auto btnScreen = menuBtn.getScreenBounds();
    int anchorX = btnScreen.getRight() - menuW - menuW / 6 - 3;
    int anchorY = btnScreen.getBottom();
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetScreenArea(juce::Rectangle<int>(anchorX, anchorY, 1, 1))
        .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards)
        .withMinimumWidth(menuW),
        [this](int result)
        {
            menuBtn.setToggleState(false, juce::dontSendNotification);

            if (result == 1)
            {
                // 1. Hide viz — paint() will fill with new bg color
                flubberVisualizer.setVisible(false);

                // 2. Switch colours
                VisceraLookAndFeel::setDarkMode(!VisceraLookAndFeel::darkMode);
                lookAndFeel.refreshJuceColours();

                // Swap logos
                auto img = VisceraLookAndFeel::darkMode
                    ? juce::ImageCache::getFromMemory(BinaryData::viscera_logo_dark_nodolph_png, BinaryData::viscera_logo_dark_nodolph_pngSize)
                    : juce::ImageCache::getFromMemory(BinaryData::viscera_logo_light_nodolph_png, BinaryData::viscera_logo_light_nodolph_pngSize);
                logoImage.setImage(img, juce::RectanglePlacement::centred);

                auto mainImg = VisceraLookAndFeel::darkMode
                    ? juce::ImageCache::getFromMemory(BinaryData::viscera_logo_neutral_dark_png, BinaryData::viscera_logo_neutral_dark_pngSize)
                    : juce::ImageCache::getFromMemory(BinaryData::viscera_logo_neutral_png, BinaryData::viscera_logo_neutral_pngSize);
                mainLogoImage.setImage(mainImg, juce::RectanglePlacement::centred);

                // 3. Repaint everything
                std::function<void(juce::Component*)> refreshAll = [&](juce::Component* c) {
                    c->sendLookAndFeelChange();
                    c->repaint();
                    for (auto* ch : c->getChildren()) refreshAll(ch);
                };
                refreshAll(this);

                // 4. Kick GL to render with new bg, then re-show after a frame
                flubberVisualizer.triggerGLRepaint();
                auto safeThis = juce::Component::SafePointer<VisceraEditor>(this);
                juce::MessageManager::callAsync([safeThis] {
                    if (safeThis != nullptr && !safeThis->showAdvanced
                        && !safeThis->showPresetOverlay && !safeThis->showSaveOverlay)
                        safeThis->flubberVisualizer.setVisible(true);
                });
            }
            else if (result == 2)
            {
                // Deactivate license
                proc.getLicenseManager().deactivate();
                licenseOverlay.reset();
                updateLicenseOverlay();
                return; // overlay handles viz visibility
            }
            else if (result == 3)
            {
                juce::URL("https://thunderdolphin.studio").launchInDefaultBrowser();
            }
        });
}
