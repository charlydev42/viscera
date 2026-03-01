// PluginEditor.cpp — Dark 3x3 layout with section header bars
#include "PluginEditor.h"
#include "gui/ModSlider.h"
#include "BinaryData.h"

VisceraEditor::VisceraEditor(VisceraProcessor& processor)
    : AudioProcessorEditor(processor),
      proc(processor),
      presetBrowser(processor),
      mod1Section(processor.apvts, "MOD1", "ENV1"),
      mod2Section(processor.apvts, "MOD2", "ENV2"),
      carrierSection(processor.apvts),
      modMatrixSection(processor.apvts),
      filterSection(processor.apvts),
      pitchEnvSection(processor.apvts),
      tabbedEffects(processor.apvts),
      shaperSection(processor.apvts, processor.getVolumeShaper()),
      visualizerDisplay(processor.getVisualBuffer(), processor.getVisualBufferR()),
      flubberVisualizer(processor.getVisualBuffer(), processor.getVisualBufferR()),
      lfoSection(processor.apvts, processor),
      globalSection(processor.apvts),
      keyboard(processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

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
    visualizerDisplay.setVisible(false);
    addAndMakeVisible(lfoSection);
    addAndMakeVisible(globalSection);

    // Clavier MIDI integre (standalone only)
#if JUCE_STANDALONE_APPLICATION
    keyboard.setMidiChannel(1);
    keyboard.setOctaveForMiddleC(4);
    addAndMakeVisible(keyboard);
#endif

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

    // Page toggle button
    pageToggleBtn.setButtonText("Edit");
    pageToggleBtn.onClick = [this] { setPage(!showAdvanced); };
    addAndMakeVisible(pageToggleBtn);

    // Keyboard toggle for main page (standalone only)
#if JUCE_STANDALONE_APPLICATION
    kbToggleBtn.setButtonText("KB");
    kbToggleBtn.onClick = [this] {
        showKeyboardOnMain = !showKeyboardOnMain;
        keyboard.setVisible(showAdvanced || showKeyboardOnMain);
        mainLogoImage.setVisible(!showAdvanced && !showKeyboardOnMain);
        resized();
    };
    addAndMakeVisible(kbToggleBtn);
#endif

    // Dark mode toggle
    darkModeBtn.setButtonText("Dark");
    darkModeBtn.onClick = [this] {
        // 1. Hide viz — paint() will fill with bg color
        flubberVisualizer.setVisible(false);

        // 2. Switch colours
        VisceraLookAndFeel::setDarkMode(!VisceraLookAndFeel::darkMode);
        lookAndFeel.refreshJuceColours();
        darkModeBtn.setButtonText(VisceraLookAndFeel::darkMode ? "Light" : "Dark");

        // Swap logos (advanced + main)
        {
            auto img = VisceraLookAndFeel::darkMode
                ? juce::ImageCache::getFromMemory(BinaryData::viscera_logo_dark_nodolph_png, BinaryData::viscera_logo_dark_nodolph_pngSize)
                : juce::ImageCache::getFromMemory(BinaryData::viscera_logo_light_nodolph_png, BinaryData::viscera_logo_light_nodolph_pngSize);
            logoImage.setImage(img, juce::RectanglePlacement::centred);

            auto mainImg = VisceraLookAndFeel::darkMode
                ? juce::ImageCache::getFromMemory(BinaryData::viscera_logo_neutral_dark_png, BinaryData::viscera_logo_neutral_dark_pngSize)
                : juce::ImageCache::getFromMemory(BinaryData::viscera_logo_neutral_png, BinaryData::viscera_logo_neutral_pngSize);
            mainLogoImage.setImage(mainImg, juce::RectanglePlacement::centred);
        }

        // 3. Repaint everything (viz area shows correct bg via parent)
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
            if (safeThis != nullptr)
                safeThis->flubberVisualizer.setVisible(!safeThis->showAdvanced);
        });
    };
    addAndMakeVisible(darkModeBtn);

    // Macro knobs for main page
    {
        struct MacroDef { const char* paramId; const char* label; bb::LFODest dest; };
        const MacroDef defs[6] = {
            { "VOLUME",      "Volume", bb::LFODest::Volume       },
            { "DRIVE",       "Drive",  bb::LFODest::Drive        },
            { "FILT_CUTOFF", "Cutoff", bb::LFODest::FilterCutoff },
            { "FILT_RES",    "Reso",   bb::LFODest::FilterRes    },
            { "DISP_AMT",    "Fold",   bb::LFODest::FoldAmt      },
            { "CAR_SPREAD",  "Spread", bb::LFODest::CarSpread    },
        };

        for (int i = 0; i < 6; ++i)
        {
            macroKnobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            macroKnobs[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            macroKnobs[i].initMod(processor.apvts, defs[i].dest);
            addChildComponent(macroKnobs[i]); // hidden initially (main page not shown by default... we call setPage below)

            macroAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.apvts, defs[i].paramId, macroKnobs[i]);

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

    setSize(920, 660);

    // Start on main (perform) page
    setPage(false);

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
    darkModeBtn.setPaintingIsUnclipped(true);
    pageToggleBtn.setPaintingIsUnclipped(true);
    kbToggleBtn.setPaintingIsUnclipped(true);

    // Load first preset so sound matches displayed name
    // (done here after all attachments are created)
    if (proc.getCurrentPresetIndex() == 0 && !proc.isUserPreset())
        proc.loadPresetAt(0);
}

VisceraEditor::~VisceraEditor()
{
    setLookAndFeel(nullptr);
}

void VisceraEditor::timerCallback()
{
    updateAlgoLabel();
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
    randFloat("CAR_NOISE", 0.0f, 0.3f);
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

    // Keep volume safe
    randFloat("VOLUME", 0.5f, 0.8f);
    randFloat("DRIVE", 0.0f, 0.4f);
    randFloat("DISP_AMT", 0.0f, 0.3f);

    // Global LFOs — moderate randomization
    for (int n = 1; n <= 3; ++n)
    {
        auto pfx = "LFO" + juce::String(n) + "_";
        randFloat(pfx + "RATE", 0.2f, 8.0f);
        randInt(pfx + "WAVE", 0, 4);
        // 30% chance of one active assignment per LFO
        for (int s = 1; s <= 4; ++s)
        {
            if (s == 1 && rng.nextFloat() < 0.3f)
            {
                randInt(pfx + "DEST" + juce::String(s), 1, 10);
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

void VisceraEditor::setPage(bool advanced)
{
    showAdvanced = advanced;
    pageToggleBtn.setButtonText(advanced ? "Back" : "Edit");

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

    // Flubber visualizer only on main page
    flubberVisualizer.setVisible(!advanced);

    // Effects: hidden on main (we use mini-controls), stacked on edit
    tabbedEffects.setVisible(advanced);
    tabbedEffects.setLayout(TabbedEffectSection::Stacked);

    // Keyboard: standalone only
#if JUCE_STANDALONE_APPLICATION
    keyboard.setVisible(advanced || showKeyboardOnMain);
    kbToggleBtn.setVisible(!advanced);
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
    else
    {
        // Advanced page: 10 section headers (no visualizer)
        static const char* titles[] = {
            "Mod 1", "Mod 2", "Carrier", "Vibrato",
            "LFO", "Filter", "Pitch Env",
            "Effects", "Vol Shaper", "Global"
        };
        for (int i = 0; i < 10; ++i)
            drawSectionHeader(g, sectionBounds[i], titles[i]);
    }
}

void VisceraEditor::resized()
{
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

    pageToggleBtn.setBounds(topBar.removeFromRight(40));
    topBar.removeFromRight(sp);
    darkModeBtn.setBounds(topBar.removeFromRight(40));
    topBar.removeFromRight(sp);
#if JUCE_STANDALONE_APPLICATION
    if (!showAdvanced)
    {
        kbToggleBtn.setBounds(topBar.removeFromRight(28));
        topBar.removeFromRight(sp);
    }
#endif

    presetBrowser.setBounds(topBar);
    area.removeFromTop(4);

    // === Keyboard at bottom (standalone only) ===
#if JUCE_STANDALONE_APPLICATION
    if (showAdvanced || showKeyboardOnMain)
    {
        if (showAdvanced)
        {
            keyboard.setBounds(area.removeFromBottom(50));
            area.removeFromBottom(4);
        }
        else
        {
            // On main page: overlay at bottom, no layout impact
            auto kbBounds = area;
            keyboard.setBounds(kbBounds.removeFromBottom(50));
        }
    }
#endif

    int gap = 6;
    int headerH = 16;

    if (!showAdvanced)
    {
        // =============================================
        // MAIN (PERFORM) PAGE — Oval viz + macro knobs + FX controls around ellipse
        // =============================================
        mainPanelBounds = area;
        int knobSize = 58;
        int fxKnobSize = 44;
        int labelH = 14;

        // Rectangular flubber visualizer centered
        int vizW = static_cast<int>(area.getWidth() * 0.54f);
        int vizH = static_cast<int>(area.getHeight() * 0.58f);
        auto vizBounds = area.withSizeKeepingCentre(vizW, vizH);
        vizBounds.translate(0, -62);
        mainSectionBounds[0] = vizBounds;
        flubberVisualizer.setBounds(vizBounds);

        float cx = static_cast<float>(vizBounds.getCentreX());
        float cy = static_cast<float>(vizBounds.getCentreY());
        constexpr float pi = juce::MathConstants<float>::pi;

        // --- Macro knobs: 3 left, 3 right (along sides of ellipse) ---
        float macroRx = static_cast<float>(vizW) * 0.5f + static_cast<float>(knobSize) * 1.4f;
        float macroRy = static_cast<float>(vizH) * 0.5f + static_cast<float>(knobSize) * 0.85f;

        // Left: Cutoff(2), Res(3), Spread(5)  |  Right: Drive(1), Fold(4), Volume(0)
        float leftAngles[3]  = { 150.0f * pi / 180.0f, 180.0f * pi / 180.0f, 210.0f * pi / 180.0f };
        float rightAngles[3] = {  30.0f * pi / 180.0f,   0.0f * pi / 180.0f, 330.0f * pi / 180.0f };
        int leftIdx[3]  = { 2, 3, 5 };
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
        float fxRx = macroRx + 28.0f;
        float fxRy = macroRy + 22.0f;
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
    }
    else
    {
        // =============================================
        // ADVANCED (EDIT) PAGE
        // =============================================
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
        int vibratoH = 70;
        int filterH = 80;
        int pitchH = 150;
        int logoH = 60;
        int lfoH = totalH - vibratoH - filterH - pitchH - logoH - gap * 4;

        // Filter top Y in centre column = vibratoH + gap + lfoH + gap + logoH + gap
        int filterTopOffset = vibratoH + gap + lfoH + gap + logoH + gap;

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

        // === CENTRE COLUMN: Vibrato | LFO Assign | Logo | Filter + Pitch Env ===
        {
            placeSection(centreCol, vibratoH, modMatrixSection, 3);
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
            int globalH = 70;
            int shaperH = 160;
            int effectsH = totalH - globalH - shaperH - gap * 2;

            placeSection(rightCol, effectsH, tabbedEffects, 7);
            rightCol.removeFromTop(gap);
            placeSection(rightCol, shaperH, shaperSection, 8);
            rightCol.removeFromTop(gap);
            auto globalBlock = rightCol.removeFromTop(globalH);
            sectionBounds[9] = globalBlock;
            globalSection.setBounds(globalBlock.withTrimmedTop(headerH).reduced(4, 0));
        }
    }
}
