// PluginEditor.cpp — Dark 3x3 layout with section header bars
#include "PluginEditor.h"
#include "gui/ModSlider.h"
#include "BinaryData.h"

ParasiteEditor::ParasiteEditor(ParasiteProcessor& processor)
    : AudioProcessorEditor(processor),
      proc(processor),
      modSliderContext{&processor.getVoiceParams(), {}, false},
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
    // Load persisted dark mode preference before any rendering
    ParasiteLookAndFeel::loadDarkModePreference();
    lastSeenDarkMode = ParasiteLookAndFeel::darkMode.load(std::memory_order_acquire);

    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    // Push the loaded dark mode colors into the JUCE look and feel
    lookAndFeel.refreshJuceColours();

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
    titleLabel.setText("Parasite", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    // Error toast — hidden by default, shown briefly on preset load failures.
    errorToast.setJustificationType(juce::Justification::centred);
    errorToast.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
    errorToast.setColour(juce::Label::backgroundColourId, juce::Colour(0xCC8B2C2C)); // translucent red
    errorToast.setColour(juce::Label::textColourId,       juce::Colour(0xFFFFFFFF));
    errorToast.setInterceptsMouseClicks(false, false);
    errorToast.setVisible(false);
    addChildComponent(errorToast);

    // Advanced page logo (larger format, fills available space)
    {
        auto img = ParasiteLookAndFeel::darkMode
            ? juce::ImageCache::getFromMemory(BinaryData::parasite_advanced_dark_png, BinaryData::parasite_advanced_dark_pngSize)
            : juce::ImageCache::getFromMemory(BinaryData::parasite_advanced_light_png, BinaryData::parasite_advanced_light_pngSize);
        logoImage.setImage(img, juce::RectanglePlacement::centred);
    }
    addAndMakeVisible(logoImage);

    // Neutral logo for main page
    {
        auto img = ParasiteLookAndFeel::darkMode
            ? juce::ImageCache::getFromMemory(BinaryData::parasite_logo_neutral_dark_png, BinaryData::parasite_logo_neutral_dark_pngSize)
            : juce::ImageCache::getFromMemory(BinaryData::parasite_logo_neutral_png, BinaryData::parasite_logo_neutral_pngSize);
        mainLogoImage.setImage(img, juce::RectanglePlacement::centred);
    }
    addAndMakeVisible(mainLogoImage);

    // FM Algorithm selector
    algoNames = { "Series", "Parallel", "Stack", "Ring", "Feedback", "Mix" };

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

    // Global octave selector (< Oct 0 >)
    octLeftBtn.setButtonText("<");
    octLeftBtn.onClick = [this] {
        auto* p = proc.apvts.getParameter("OCTAVE");
        int cur = static_cast<int>(p->convertFrom0to1(p->getValue()));
        if (cur > -4) {
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(cur - 1)));
            updateOctaveLabel();
        }
    };
    addAndMakeVisible(octLeftBtn);

    octRightBtn.setButtonText(">");
    octRightBtn.onClick = [this] {
        auto* p = proc.apvts.getParameter("OCTAVE");
        int cur = static_cast<int>(p->convertFrom0to1(p->getValue()));
        if (cur < 4) {
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(cur + 1)));
            updateOctaveLabel();
        }
    };
    addAndMakeVisible(octRightBtn);

    octLabel.setJustificationType(juce::Justification::centred);
    octLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    addAndMakeVisible(octLabel);
    updateOctaveLabel();

    // Wire randomize to PresetBrowser's ? button
    presetBrowser.onRandomize = [this] { randomizeParams(); };

    // Reset LFO section to tab 1 on any preset change
    presetBrowser.onPresetChanged = [this] { lfoSection.resetToTab(0); };

    // Wire preset overlay
    presetBrowser.onBrowse = [this] {
        if (showPresetOverlay)
        {
            // Toggling off = commit currently auditioned preset
            presetOverlay.stopPreviewNote();
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
    licenseOverlay.onDemoRequested = [this] { updateLicenseOverlay(); };
    addChildComponent(licenseOverlay);

    // Demo banner — visible only when user has dismissed the overlay and the
    // plugin is still unlicensed. Clicking it clears the ack so the overlay
    // re-appears (letting the user type a key).
    demoBanner.setButtonText("DEMO - click to activate");
    demoBanner.onClick = [this] {
        LicenseOverlay::writeDemoAcknowledged(false);
        licenseOverlay.reset();
        updateLicenseOverlay();
    };
    addChildComponent(demoBanner);

    // Page toggle button
    pageToggleBtn.setButtonText("Advanced");
    pageToggleBtn.onClick = [this] {
        if (showSaveOverlay)
        {
            setSaveOverlayVisible(false);
        }
        else if (showPresetOverlay)
        {
            // Back button = commit currently auditioned preset
            presetOverlay.stopPreviewNote();
            setPresetOverlayVisible(false);
            presetBrowser.refreshPresetList();
        }
        else
        {
            setPage(!showAdvanced);
        }
    };
    addAndMakeVisible(pageToggleBtn);

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
            { "CORTEX",      "Vortex",  bb::LFODest::Cortex       },
            { "PLASMA",      "Plasma",  bb::LFODest::Plasma       },
            { "DISP_AMT",    "Fold",    bb::LFODest::FoldAmt      },
            { "ICHOR",       "Helix",   bb::LFODest::Ichor        },
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
            macroLabels[i].setColour(juce::Label::textColourId, juce::Colour(ParasiteLookAndFeel::kAccentColor));
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
            fxLabel[i].setColour(juce::Label::textColourId, juce::Colour(ParasiteLookAndFeel::kAccentColor));
            addChildComponent(fxLabel[i]);
        }
    }

    startTimerHz(5);

    setWantsKeyboardFocus(false);
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
    octLeftBtn.setPaintingIsUnclipped(true);
    octRightBtn.setPaintingIsUnclipped(true);
    octLabel.setPaintingIsUnclipped(true);
    menuBtn.setPaintingIsUnclipped(true);
    pageToggleBtn.setPaintingIsUnclipped(true);

    // Only load Init preset on very first editor open (fresh plugin instance).
    // Skip if the editor is being reopened — processor state is already valid.
    if (proc.getCurrentPresetIndex() < 0)
        proc.loadPresetAt(0);
}

ParasiteEditor::~ParasiteEditor()
{
    stopTimer();
    // Drop any lambda the LFOSection may have registered before children
    // start destroying — the lambda's SafePointer already handles stale
    // access but clearing here keeps the context in a clean state.
    modSliderContext.onLearnClick = nullptr;
    modSliderContext.showDropTargets = false;

    // Only un-set the global default LaF if we're still the current one.
    // Another plugin instance may have overwritten it in its constructor,
    // in which case we must NOT clobber the live reference.
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == &lookAndFeel)
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);

    setLookAndFeel(nullptr);
}

void ParasiteEditor::applyDarkModeChange(bool localToggle)
{
    const bool dark = ParasiteLookAndFeel::darkMode.load(std::memory_order_acquire);
    lastSeenDarkMode = dark;

    // For the local toggle UX we briefly hide the flubber so its backing
    // paint() fills with the new bg colour before GL repaints with the new
    // shader — avoids a single-frame flash of the old theme.
    if (localToggle)
        flubberVisualizer.setVisible(false);

    lookAndFeel.refreshJuceColours();

    auto img = dark
        ? juce::ImageCache::getFromMemory(BinaryData::parasite_advanced_dark_png,
                                           BinaryData::parasite_advanced_dark_pngSize)
        : juce::ImageCache::getFromMemory(BinaryData::parasite_advanced_light_png,
                                           BinaryData::parasite_advanced_light_pngSize);
    logoImage.setImage(img, juce::RectanglePlacement::centred);

    auto mainImg = dark
        ? juce::ImageCache::getFromMemory(BinaryData::parasite_logo_neutral_dark_png,
                                           BinaryData::parasite_logo_neutral_dark_pngSize)
        : juce::ImageCache::getFromMemory(BinaryData::parasite_logo_neutral_png,
                                           BinaryData::parasite_logo_neutral_pngSize);
    mainLogoImage.setImage(mainImg, juce::RectanglePlacement::centred);

    // Keep the license overlay + demo banner palette in sync
    licenseOverlay.refreshColors();
    demoBanner.repaint();

    std::function<void(juce::Component*)> refreshAll = [&](juce::Component* c) {
        c->sendLookAndFeelChange();
        c->repaint();
        for (auto* ch : c->getChildren()) refreshAll(ch);
    };
    refreshAll(this);

    flubberVisualizer.triggerGLRepaint();

    if (localToggle)
    {
        auto safeThis = juce::Component::SafePointer<ParasiteEditor>(this);
        juce::MessageManager::callAsync([safeThis] {
            if (safeThis != nullptr && !safeThis->showAdvanced
                && !safeThis->showPresetOverlay && !safeThis->showSaveOverlay)
                safeThis->flubberVisualizer.setVisible(true);
        });
    }
}

void ParasiteEditor::showErrorToast(const juce::String& msg)
{
    errorToast.setText(msg, juce::dontSendNotification);
    // 4 s at 10Hz timer = 40 ticks. setVisible + bring to front so any
    // overlay (preset browser, save dialog) doesn't eat the notification.
    errorToastCountdown = 40;
    errorToast.setVisible(true);
    errorToast.toFront(false);
    // Resize against current bounds — top-centered banner.
    auto b = getLocalBounds();
    int w = juce::jmin(520, b.getWidth() - 40);
    errorToast.setBounds(b.getCentreX() - w / 2, 8, w, 28);
    repaint();
}

void ParasiteEditor::timerCallback()
{
    updateAlgoLabel();
    updateOctaveLabel();
    proc.getUndoManager().beginNewTransaction();

    // Surface preset-load failures reported by the processor. Cleared by
    // the processor when read so two identical errors don't keep re-toasting.
    if (auto err = proc.getAndClearLastLoadError(); err.isNotEmpty())
        showErrorToast(err);
    if (errorToastCountdown > 0 && --errorToastCountdown == 0)
        errorToast.setVisible(false);

    // Detect dark-mode changes made by other plugin instances. Polling at
    // the editor timer rate (~10Hz) means remote toggles apply within ~100ms
    // — matches the flubber's per-frame atomic read so widgets can't lag
    // behind the visualizer anymore.
    const bool curDark = ParasiteLookAndFeel::darkMode.load(std::memory_order_acquire);
    if (curDark != lastSeenDarkMode)
        applyDarkModeChange(/*localToggle*/ false);

    // Keep macro label colors in sync with theme
    auto labelCol = ParasiteLookAndFeel::darkMode
        ? juce::Colour(ParasiteLookAndFeel::kAccentColor)
        : juce::Colour(ParasiteLookAndFeel::kAccentColor).darker(0.25f);
    for (int i = 0; i < 6; ++i)
        macroLabels[i].setColour(juce::Label::textColourId, labelCol);
    for (int i = 0; i < 4; ++i)
        fxLabel[i].setColour(juce::Label::textColourId, labelCol);
}

void ParasiteEditor::randomizeParams()
{
    auto& rng = juce::Random::getSystemRandom();
    auto& apvts = proc.apvts;

    // Silence any held notes + clear effect tails before the new patch takes
    // hold — without this, a note that was ringing when the user clicks "?"
    // keeps playing with the newly-randomised FM ratios / envelope / filter,
    // which sounds like a ghost retrigger.
    proc.requestVoicePanic();

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

    // Pick a random sound archetype for ADSR variety.
    // Distribution biased toward short/percussive patches — long sustained
    // pads and evolving textures are harder to play in a musical context
    // straight out of a random roll, so they get a smaller slice.
    //   Pluck 35 / Stab 25 / Lead 25 / Pad 10 / Evolving 5   (long = 15% total)
    enum Archetype { Pluck, Stab, Pad, Lead, Evolving };
    float roll = rng.nextFloat();
    Archetype arch = (roll < 0.35f) ? Pluck
                   : (roll < 0.60f) ? Stab
                   : (roll < 0.85f) ? Lead
                   : (roll < 0.95f) ? Pad
                   :                  Evolving;

    // Mod envelopes — shaped by archetype
    for (auto& env : { juce::String("ENV1"), juce::String("ENV2") })
    {
        switch (arch) {
        case Pluck:
            randFloat(env + "_A", 0.0f, 0.005f);
            randFloat(env + "_D", 0.02f, 0.25f);
            randFloat(env + "_S", 0.0f, 0.15f);
            randFloat(env + "_R", 0.01f, 0.15f);
            break;
        case Stab:
            randFloat(env + "_A", 0.0f, 0.01f);
            randFloat(env + "_D", 0.05f, 0.4f);
            randFloat(env + "_S", 0.0f, 0.3f);
            randFloat(env + "_R", 0.05f, 0.3f);
            break;
        case Pad:
            randFloat(env + "_A", 0.1f, 1.5f);
            randFloat(env + "_D", 0.2f, 1.0f);
            randFloat(env + "_S", 0.5f, 1.0f);
            randFloat(env + "_R", 0.3f, 2.0f);
            break;
        case Lead:
            randFloat(env + "_A", 0.0f, 0.02f);
            randFloat(env + "_D", 0.05f, 0.5f);
            randFloat(env + "_S", 0.3f, 0.9f);
            randFloat(env + "_R", 0.05f, 0.5f);
            break;
        case Evolving:
            randFloat(env + "_A", 0.3f, 2.0f);
            randFloat(env + "_D", 0.5f, 2.0f);
            randFloat(env + "_S", 0.2f, 0.8f);
            randFloat(env + "_R", 0.5f, 3.0f);
            break;
        }
    }

    // Carrier
    randInt("CAR_WAVE", 0, 4);
    randInt("CAR_COARSE", 0, 4);
    randFloat("CAR_FINE", -100.0f, 100.0f);
    randBool("CAR_KB", 0.9f);
    randFloat("CAR_DRIFT", 0.0f, arch == Pad || arch == Evolving ? 0.4f : 0.15f);
    randFloat("CAR_NOISE", 0.0f, rng.nextFloat() < 0.15f ? 0.15f : 0.0f);
    randFloat("CAR_SPREAD", 0.0f, arch == Pad ? 0.6f : 0.3f);

    // Carrier envelope — shaped by archetype
    switch (arch) {
    case Pluck:
        randFloat("ENV3_A", 0.0f, 0.003f);
        randFloat("ENV3_D", 0.05f, 0.4f);
        randFloat("ENV3_S", 0.0f, 0.05f);
        randFloat("ENV3_R", 0.01f, 0.1f);
        break;
    case Stab:
        randFloat("ENV3_A", 0.0f, 0.005f);
        randFloat("ENV3_D", 0.08f, 0.5f);
        randFloat("ENV3_S", 0.0f, 0.2f);
        randFloat("ENV3_R", 0.05f, 0.3f);
        break;
    case Pad:
        randFloat("ENV3_A", 0.15f, 1.5f);
        randFloat("ENV3_D", 0.3f, 1.5f);
        randFloat("ENV3_S", 0.6f, 1.0f);
        randFloat("ENV3_R", 0.5f, 3.0f);
        break;
    case Lead:
        randFloat("ENV3_A", 0.0f, 0.01f);
        randFloat("ENV3_D", 0.1f, 0.6f);
        randFloat("ENV3_S", 0.4f, 1.0f);
        randFloat("ENV3_R", 0.1f, 0.6f);
        break;
    case Evolving:
        randFloat("ENV3_A", 0.5f, 3.0f);
        randFloat("ENV3_D", 1.0f, 3.0f);
        randFloat("ENV3_S", 0.3f, 0.8f);
        randFloat("ENV3_R", 1.0f, 5.0f);
        break;
    }

    // Algorithm, XOR, Sync
    randInt("FM_ALGO", 0, 5);
    randBool("XOR_ON", 0.2f);
    randBool("SYNC", 0.15f);

    // Disable volume shaper on randomize (user's custom shape persists but is bypassed)
    if (auto* p = apvts.getParameter("SHAPER_ON"))
        p->setValueNotifyingHost(0.0f);

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

    // Reverb: percussive archetypes almost always want a small tail to sit
    // in their own space ("dans son jus"). Pluck is the most reverb-hungry,
    // Stab wants a discreet room, others get the default 30% chance.
    const float revChance = (arch == Pluck) ? 0.85f
                          : (arch == Stab)  ? 0.70f
                          :                   0.30f;
    const float revMixLo  = (arch == Pluck) ? 0.10f
                          : (arch == Stab)  ? 0.08f
                          :                   0.10f;
    const float revMixHi  = (arch == Pluck) ? 0.30f   // small/medium tail
                          : (arch == Stab)  ? 0.28f
                          :                   0.40f;  // pads/leads can go wetter
    randBool("REV_ON", revChance);
    randFloat("REV_SIZE", 0.2f, 0.9f);
    randFloat("REV_MIX", revMixLo, revMixHi);

    randBool("LIQ_ON", 0.2f);
    randBool("RUB_ON", 0.15f);

    // Pitch envelope — more likely on percussive archetypes
    {
        float penvChance = (arch == Pluck || arch == Stab) ? 0.45f : 0.15f;
        randBool("PENV_ON", penvChance);
        if (arch == Pluck || arch == Stab)
        {
            randFloat("PENV_AMT", 6.0f, 48.0f);
            randFloat("PENV_A", 0.0f, 0.005f);
            randFloat("PENV_D", 0.01f, 0.15f);
            randFloat("PENV_S", 0.0f, 0.1f);
            randFloat("PENV_R", 0.01f, 0.1f);
        }
        else
        {
            randFloat("PENV_AMT", -12.0f, 12.0f);
            randFloat("PENV_A", 0.0f, 0.3f);
            randFloat("PENV_D", 0.05f, 0.5f);
            randFloat("PENV_S", 0.0f, 0.5f);
            randFloat("PENV_R", 0.05f, 0.5f);
        }
    }

    // Macros
    randFloat("DRIVE", 0.0f, 0.4f);
    randFloat("DISP_AMT", 0.0f, 0.3f);

    // Vortex / Helix / Plasma shift harmonic ratios and detune — engaging all
    // three at once almost always produces an inharmonic mess. Each is opted
    // in independently at ~20% → ~51% of rolls have zero active, ~38% have
    // exactly one, the remaining tail very rarely stacks two or three.
    // Rests at the param's DEFAULT (CORTEX/PLASMA default to 0.5 = neutral)
    // and perturbs bipolarly when triggered.
    auto randMacro = [&](const juce::String& id, float chance) {
        if (auto* p = apvts.getParameter(id))
        {
            float value = p->getDefaultValue(); // normalised base
            if (rng.nextFloat() < chance)
            {
                const float spread = (rng.nextFloat() - 0.5f) * 0.6f; // ±0.3 around default
                value = juce::jlimit(0.0f, 1.0f, value + spread);
            }
            p->setValueNotifyingHost(value);
        }
    };
    randMacro("CORTEX", 0.20f);
    randMacro("PLASMA", 0.20f);
    randMacro("ICHOR",  0.20f);

    // Portamento: rarely engaged, small glide amount when it is (mostly for
    // mono leads). Defaults to 0 so poly patches don't get ghost pitch glides.
    if (rng.nextFloat() < 0.10f)
        randFloat("PORTA", 0.05f, 0.25f);
    else if (auto* p = apvts.getParameter("PORTA"))
        p->setValueNotifyingHost(0.0f);

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

        // Archetype adjustment: percussive sounds are naturally quieter (short envelope)
        float baseLevel = (arch == Pluck) ? 0.80f : (arch == Stab) ? 0.75f : 0.65f;

        // Compensate: target consistent level
        float targetVol = juce::jlimit(0.15f, 0.85f, baseLevel / loudness);
        if (auto* p = apvts.getParameter("VOLUME"))
            p->setValueNotifyingHost(p->convertTo0to1(targetVol));
    }

    // Global LFOs — moderate randomization
    for (int n = 1; n <= 3; ++n)
    {
        auto pfx = "LFO" + juce::String(n) + "_";
        randFloat(pfx + "RATE", 0.2f, 8.0f);
        randInt(pfx + "WAVE", 0, 4);
        // 30% chance of one active assignment per LFO, clear all 8 slots.
        // Curated destination list: only targets with a visible knob and a
        // musically useful effect when wiggled. Skips Pitch (no knob in the
        // UI), Volume (the shaper handles that), and niche per-envelope
        // time knobs that are too subtle to notice in a preview.
        static const int kRandomDests[] = {
             2,  3,            // Cutoff, Res
             4,  5,            // Mod1Lvl, Mod2Lvl
             7,  8,  9, 10,    // Drive, Noise, Spread, Fold
            11, 12, 13, 14,    // M1Fine, M2Fine, Drift, CarFine
            58, 59, 60,        // Tremor, Vein, Flux
            61, 62, 63         // Vortex, Helix, Plasma
        };
        constexpr int kNumRandomDests = sizeof(kRandomDests) / sizeof(kRandomDests[0]);

        for (int s = 1; s <= 8; ++s)
        {
            if (s == 1 && rng.nextFloat() < 0.3f)
            {
                const int dest = kRandomDests[rng.nextInt(kNumRandomDests)];
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

void ParasiteEditor::updateAlgoLabel()
{
    int idx = static_cast<int>(proc.apvts.getRawParameterValue("FM_ALGO")->load());
    if (idx >= 0 && idx < algoNames.size())
        algoLabel.setText(algoNames[idx], juce::dontSendNotification);
}

void ParasiteEditor::updateOctaveLabel()
{
    int oct = static_cast<int>(proc.apvts.getRawParameterValue("OCTAVE")->load());
    juce::String text = "Oct " + juce::String(oct);
    octLabel.setText(text, juce::dontSendNotification);
}

void ParasiteEditor::dragOperationEnded(const juce::DragAndDropTarget::SourceDetails&)
{
    // Clean up drop-target glow when any drag finishes (success or cancel)
    modSliderContext.showDropTargets = false;
}

void ParasiteEditor::setPresetOverlayVisible(bool visible)
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

void ParasiteEditor::setSaveOverlayVisible(bool visible)
{
    // Close preset overlay if opening save overlay (commit auditioned preset)
    if (visible && showPresetOverlay)
    {
        presetOverlay.stopPreviewNote();
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

void ParasiteEditor::setPage(bool advanced)
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

    // Both pages: effects, preset, logo, algo, randomize, toggle

    resized();
    repaint();
}

// Section header: neumorphic raised panel with darker header bar
void ParasiteEditor::drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds,
                                       const juce::String& title)
{
    float cr = 8.0f;

    // Neumorphic double shadow (raised, rounded to match panel)
    auto bf = bounds.toFloat();
    juce::Path panelPath;
    panelPath.addRoundedRectangle(bf, cr);
    juce::DropShadow lightSh(juce::Colour(ParasiteLookAndFeel::kShadowLight).withAlpha(0.7f), 4, { -2, -2 });
    lightSh.drawForPath(g, panelPath);
    juce::DropShadow darkSh(juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(0.5f), 6, { 3, 3 });
    darkSh.drawForPath(g, panelPath);

    // Fill whole panel
    g.setColour(juce::Colour(ParasiteLookAndFeel::kBgColor));
    g.fillRoundedRectangle(bf, cr);

    int headerH = 16;
    auto headerBar = bounds.removeFromTop(headerH);

    // Header bar background (rounded top corners)
    juce::Path headerPath;
    headerPath.addRoundedRectangle(headerBar.getX(), headerBar.getY(),
                                    headerBar.getWidth(), headerBar.getHeight(),
                                    cr, cr, true, true, false, false);
    g.setColour(juce::Colour(ParasiteLookAndFeel::kHeaderBg));
    g.fillPath(headerPath);

    // Title text centered in header bar
    if (title.isNotEmpty())
    {
        g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        g.drawText(title, headerBar, juce::Justification::centred);
    }
}

void ParasiteEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ParasiteLookAndFeel::kBgColor));

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

void ParasiteEditor::resized()
{
    // License overlay covers entire window
    if (licenseOverlay.isVisible())
    {
        licenseOverlay.setBounds(getLocalBounds());
        licenseOverlay.toFront(false);
    }

    // Demo banner floats centered at the very top when visible.
    // Component bounds are padded around the pill so the drop shadows aren't
    // clipped to a rectangular silhouette by JUCE's component clip region.
    if (demoBanner.isVisible())
    {
        const int bw = 236, bh = 28;
        demoBanner.setBounds((getWidth() - bw) / 2, 0, bw, bh);
        demoBanner.toFront(false);
    }

    auto area = getLocalBounds().reduced(4);
    titleLabel.setBounds(0, 0, 0, 0); // hidden

    // === Top bar: [< Algo >] [Preset Browser (< combo > ? +)] [Edit] ===
    int barH = 26;
    int sp = 4;
    auto topBar = area.removeFromTop(barH);

    // Logo moved to centre column (advanced page)

    algoLeftBtn.setBounds(topBar.removeFromLeft(22));
    algoLabel.setBounds(topBar.removeFromLeft(58));
    algoRightBtn.setBounds(topBar.removeFromLeft(22));
    topBar.removeFromLeft(sp);

    octLeftBtn.setBounds(topBar.removeFromLeft(18));
    octLabel.setBounds(topBar.removeFromLeft(42));
    octRightBtn.setBounds(topBar.removeFromLeft(18));
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
        int logoH = 50;
        int logoW = static_cast<int>(logoH * (1204.0f / 429.0f)); // keep aspect ratio
        int logoX = area.getRight() - logoW - 40;
        int logoY = area.getBottom() - logoH - 20;
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
        int logoH = 80;  // advanced page logo — trimmed to give LFO section more vertical room
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
            logoImage.setBounds(centreCol.removeFromTop(logoH).reduced(8, 4));
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
// Key handling — never consume, let the DAW handle everything
// =============================================================================

bool ParasiteEditor::keyPressed(const juce::KeyPress&)
{
    return false;
}

// =====================================================================
// License overlay — shown when plugin is not licensed
// =====================================================================

void ParasiteEditor::DemoBannerButton::paintButton(juce::Graphics& g, bool highlighted, bool)
{
    // Neumorphic raised pill matching the project's button language:
    // outer soft light shadow top-left + dark shadow bottom-right, fill in
    // kBgColor. A small accent dot + accent-coloured text carry the "demo"
    // signal without breaking the visual flow of the top bar.
    const juce::Colour bg          = juce::Colour(ParasiteLookAndFeel::kBgColor);
    const juce::Colour accent      = juce::Colour(ParasiteLookAndFeel::kAccentColor);
    const juce::Colour text        = juce::Colour(ParasiteLookAndFeel::kTextColor);
    const juce::Colour shadowLight = juce::Colour(ParasiteLookAndFeel::kShadowLight);
    const juce::Colour shadowDark  = juce::Colour(ParasiteLookAndFeel::kShadowDark);

    // Inset the pill so there's room around it for the shadow halo —
    // otherwise JUCE's per-component clip region cuts the blur to a
    // rectangle silhouette.
    auto bounds = getLocalBounds().toFloat().reduced(8.0f, 5.0f);
    const float cr = bounds.getHeight() * 0.5f;

    juce::Path pill;
    pill.addRoundedRectangle(bounds, cr);

    juce::DropShadow(shadowLight.withAlpha(0.55f), 5, { -2, -2 }).drawForPath(g, pill);
    juce::DropShadow(shadowDark .withAlpha(0.55f), 6, {  2,  2 }).drawForPath(g, pill);

    g.setColour(highlighted ? bg.brighter(0.04f) : bg);
    g.fillRoundedRectangle(bounds, cr);

    // Small accent dot on the left — subtle "attention" marker
    const float dotR = bounds.getHeight() * 0.22f;
    const float dotX = bounds.getX() + bounds.getHeight() * 0.55f;
    const float dotY = bounds.getCentreY();
    g.setColour(accent.withAlpha(highlighted ? 1.0f : 0.85f));
    g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

    g.setColour(text.interpolatedWith(accent, 0.35f));
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(),
                         10.5f, juce::Font::plain));
    g.drawText(getButtonText(), bounds, juce::Justification::centred, false);
}

void ParasiteEditor::updateLicenseOverlay()
{
    const bool licensed        = proc.getLicenseManager().isLicensed();
    const bool demoAcked       = LicenseOverlay::readDemoAcknowledged();
    // Overlay shows only when unlicensed AND the user has not chosen demo.
    const bool showOverlay     = (!licensed) && (!demoAcked);

    licenseOverlay.setVisible(showOverlay);
    demoBanner.setVisible((!licensed) && demoAcked);
    presetBrowser.refreshLicenseState();

    if (showOverlay)
    {
        // Re-sync colors in case dark mode changed while overlay was hidden
        licenseOverlay.refreshColors();

        // Hide OpenGL visualizer — native view renders above JUCE components
        flubberVisualizer.setVisible(false);
        flubberVisualizer.setBounds(0, 0, 0, 0);

        licenseOverlay.setBounds(getLocalBounds());
        licenseOverlay.toFront(true);
    }
    else
    {
        // Restore visualizer if on main page (works in both licensed + demo modes)
        if (!showAdvanced && !showPresetOverlay && !showSaveOverlay)
            flubberVisualizer.setVisible(true);
        resized();
    }
}

void ParasiteEditor::showSettingsMenu()
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
    menu.addItem(1, "Dark Mode", true, ParasiteLookAndFeel::darkMode);

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
        menu.addItem(5, "Activate License...");
    }

    menu.addSeparator();

    // 3. Sync presets
    menu.addItem(3, "Sync Presets");

    // 4. Account & presets
    menu.addItem(4, "Manage Account");

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
                // Flip the shared atomic once, then let applyDarkModeChange
                // handle the rest. Other plugin instances will detect the
                // change via their timerCallback and re-theme themselves.
                ParasiteLookAndFeel::setDarkMode(!ParasiteLookAndFeel::darkMode);
                applyDarkModeChange(/*localToggle*/ true);
            }
            else if (result == 2)
            {
                // Deactivate license — user is already educated about licensing,
                // so mark demo as acknowledged so they land straight in demo
                // mode (small banner) rather than the full activation overlay.
                proc.getLicenseManager().deactivate();
                LicenseOverlay::writeDemoAcknowledged(true);
                licenseOverlay.reset();
                updateLicenseOverlay();
                return; // overlay handles viz visibility
            }
            else if (result == 3)
            {
                proc.getCloudPresetManager().syncAll();
            }
            else if (result == 4)
            {
                juce::URL("https://voidscan-audio.com/dashboard").launchInDefaultBrowser();
            }
            else if (result == 5)
            {
                // Reopen the activation overlay from demo mode
                LicenseOverlay::writeDemoAcknowledged(false);
                licenseOverlay.reset();
                updateLicenseOverlay();
            }
        });
}
