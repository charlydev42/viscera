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
    addAndMakeVisible(visualizerDisplay);
    addAndMakeVisible(lfoSection);
    addAndMakeVisible(globalSection);

    // Clavier MIDI integre
    keyboard.setMidiChannel(1);
    keyboard.setOctaveForMiddleC(4);
    addAndMakeVisible(keyboard);

    // Titre
    titleLabel.setText("Viscera", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    // Logo image from BinaryData
    {
        auto img = juce::ImageCache::getFromMemory(BinaryData::viscera_logo_fat_png, BinaryData::viscera_logo_fat_pngSize);
        logoImage.setImage(img, juce::RectanglePlacement::centred);
    }
    addAndMakeVisible(logoImage);

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

    // Randomize button
    randomBtn.setButtonText("?");
    randomBtn.onClick = [this] { randomizeParams(); };
    addAndMakeVisible(randomBtn);

    // Page toggle button
    pageToggleBtn.setButtonText("Edit");
    pageToggleBtn.onClick = [this] { setPage(!showAdvanced); };
    addAndMakeVisible(pageToggleBtn);

    // Macro knobs for main page
    {
        struct MacroDef { const char* paramId; const char* label; bb::LFODest dest; };
        const MacroDef defs[6] = {
            { "VOLUME",      "Volume", bb::LFODest::Volume       },
            { "DRIVE",       "Drive",  bb::LFODest::Drive        },
            { "FILT_CUTOFF", "Cutoff", bb::LFODest::FilterCutoff },
            { "FILT_RES",    "Res",    bb::LFODest::FilterRes    },
            { "DISP_AMT",    "Fold",   bb::LFODest::FoldAmt      },
            { "CAR_NOISE",   "Noise",  bb::LFODest::CarNoise     },
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

    startTimerHz(5);

    setSize(920, 660);

    // Start on main (perform) page
    setPage(false);
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

    // Main-only components
    for (int i = 0; i < 6; ++i)
    {
        macroKnobs[i].setVisible(!advanced);
        macroLabels[i].setVisible(!advanced);
    }

    // Visualizer only on main page
    visualizerDisplay.setVisible(!advanced);

    // Effects: tabbed on main, stacked on edit
    tabbedEffects.setStacked(advanced);

    // Both pages: effects, preset, keyboard, logo, algo, randomize, toggle

    resized();
    repaint();
}

// Section header: colored bar at top + panel body below
void VisceraEditor::drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds,
                                       const juce::String& title)
{
    int headerH = 16;
    auto headerBar = bounds.removeFromTop(headerH);
    auto body = bounds;

    // Header bar background (rounded top corners)
    juce::Path headerPath;
    headerPath.addRoundedRectangle(headerBar.getX(), headerBar.getY(),
                                    headerBar.getWidth(), headerBar.getHeight(),
                                    3.0f, 3.0f, true, true, false, false);
    g.setColour(juce::Colour(VisceraLookAndFeel::kHeaderBg));
    g.fillPath(headerPath);

    // Title text centered in header bar
    if (title.isNotEmpty())
    {
        g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        g.drawText(title, headerBar, juce::Justification::centred);
    }

    // Body panel (rounded bottom corners)
    juce::Path bodyPath;
    bodyPath.addRoundedRectangle(body.getX(), body.getY(),
                                  body.getWidth(), body.getHeight(),
                                  3.0f, 3.0f, false, false, true, true);
    g.setColour(juce::Colour(VisceraLookAndFeel::kPanelColor));
    g.fillPath(bodyPath);
}

void VisceraEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(VisceraLookAndFeel::kBgColor));

    if (!showAdvanced)
    {
        // Minimal main page: no panel backgrounds, just the effects header
        drawSectionHeader(g, mainSectionBounds[2], "Effects");
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

    // === Top bar: [Logo] [< Algo >] [?] [Preset Browser] [Edit] ===
    int barH = 26;
    int sp = 4;
    auto topBar = area.removeFromTop(barH);

    logoImage.setBounds(topBar.removeFromLeft(36).reduced(1));
    topBar.removeFromLeft(sp);

    algoLeftBtn.setBounds(topBar.removeFromLeft(22));
    algoLabel.setBounds(topBar.removeFromLeft(58));
    algoRightBtn.setBounds(topBar.removeFromLeft(22));
    topBar.removeFromLeft(sp);

    randomBtn.setBounds(topBar.removeFromLeft(22));
    topBar.removeFromLeft(sp);

    pageToggleBtn.setBounds(topBar.removeFromRight(40));
    topBar.removeFromRight(sp);

    presetBrowser.setBounds(topBar);
    area.removeFromTop(4);

    // === Keyboard at bottom ===
    keyboard.setBounds(area.removeFromBottom(50));
    area.removeFromBottom(4);

    int gap = 6;
    int headerH = 16;

    if (!showAdvanced)
    {
        // =============================================
        // MAIN (PERFORM) PAGE — Dashboard: large viz left, stacked knobs right
        // =============================================
        int effectsH = 80;
        int cardGap = 6;

        // Effects strip: full width at bottom
        auto effectsStrip = area.removeFromBottom(effectsH);
        mainSectionBounds[2] = effectsStrip;
        tabbedEffects.setBounds(effectsStrip.withTrimmedTop(headerH).reduced(4, 0));

        area.removeFromBottom(gap);

        // Split remaining area: 70% left (visualizer), 30% right (knob cards)
        int leftW = static_cast<int>(area.getWidth() * 0.70f);
        auto leftCol = area.removeFromLeft(leftW);
        area.removeFromLeft(gap);
        auto rightCol = area;

        // Left: visualizer fills the column
        mainSectionBounds[0] = leftCol;
        visualizerDisplay.setBounds(leftCol.reduced(8));

        // Right: 6 stacked knob cards
        int cardH = (rightCol.getHeight() - 5 * cardGap) / 6;
        for (int i = 0; i < 6; ++i)
        {
            auto card = rightCol.removeFromTop(cardH);
            macroCardBounds[i] = card;

            // Knob: card area minus bottom label strip, with padding
            auto knobArea = card.withTrimmedBottom(14).reduced(6, 4);
            macroKnobs[i].setBounds(knobArea);

            // Label: bottom 14px of card
            macroLabels[i].setBounds(card.getX(), card.getBottom() - 14, card.getWidth(), 14);

            if (i < 5)
                rightCol.removeFromTop(cardGap);
        }
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

        // === LEFT COLUMN: Mod1 → Mod2 → Carrier (FM chain) ===
        // Mod sections are the reference: headerH + 6 + 28 + 2 + 48 + 2 + 48 + 4 = 154
        {
            int modH = 154;
            placeSection(leftCol, modH, mod1Section, 0);
            leftCol.removeFromTop(gap);
            placeSection(leftCol, modH, mod2Section, 1);
            leftCol.removeFromTop(gap);
            sectionBounds[2] = leftCol;
            carrierSection.setBounds(leftCol.withTrimmedTop(headerH).reduced(4, 0));
        }

        // === CENTRE COLUMN: Vibrato | LFO Assign | Filter + Pitch Env ===
        {
            int vibratoH = 70;
            int filterH = 80;
            int pitchH = 150;
            int lfoH = totalH - vibratoH - filterH - pitchH - gap * 3;

            placeSection(centreCol, vibratoH, modMatrixSection, 3);
            centreCol.removeFromTop(gap);
            placeSection(centreCol, lfoH, lfoSection, 4);
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
