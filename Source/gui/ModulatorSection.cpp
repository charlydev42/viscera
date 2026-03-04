// ModulatorSection.cpp — Panel modulateur (Operator-style: Coarse/Freq swap + Fine + Level)
#include "ModulatorSection.h"

ModulatorSection::ModulatorSection(juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& prefix,
                                   const juce::String& envPrefix,
                                   bb::HarmonicTable& harmonics)
    : state(apvts),
      paramPrefix(prefix),
      kbParamId(prefix + "_KB"),
      harmonicTable(harmonics),
      harmonicEditor(harmonics)
{
    // Waveform combo
    waveCombo.addItemList({"Sine", "Saw", "Square", "Tri", "Pulse", "Custom"}, 1);
    waveCombo.setWantsKeyboardFocus(false);
    addAndMakeVisible(waveCombo);
    waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, prefix + "_WAVE", waveCombo);
    // waveLabel hidden (removed for cleaner look)

    // --- Coarse knob (ratio mode, LFO assignable) ---
    {
        bb::LFODest coarseDest = (prefix == "MOD1") ? bb::LFODest::Mod1Coarse : bb::LFODest::Mod2Coarse;
        coarseKnob.initMod(apvts, coarseDest);
    }
    setupKnob(coarseKnob);
    coarseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "_COARSE", coarseKnob);

    // --- Fixed freq knob (fixed mode) ---
    setupKnob(fixedFreqKnob);
    fixedFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "_FIXED_FREQ", fixedFreqKnob);

    // Shared label (value updated in timerCallback)
    mainKnobLabel.setText("x1", juce::dontSendNotification);
    mainKnobLabel.setJustificationType(juce::Justification::centred);
    mainKnobLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(mainKnobLabel);

    // --- On/Off toggle ---
    onToggle.setButtonText("On");
    onToggle.setClickingTogglesState(true);
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, prefix + "_ON", onToggle);

    // --- Fixed toggle ---
    fixedToggle.setButtonText("Fixed");
    fixedToggle.setClickingTogglesState(true);
    addAndMakeVisible(fixedToggle);

    bool kbOn = apvts.getRawParameterValue(kbParamId)->load() > 0.5f;
    fixedToggle.setToggleState(!kbOn, juce::dontSendNotification);

    fixedToggle.onClick = [this] {
        auto* param = state.getParameter(kbParamId);
        param->setValueNotifyingHost(fixedToggle.getToggleState() ? 0.0f : 1.0f);
    };

    // --- Fine knob (ratio mode) — LFO assignable ---
    bb::LFODest fineDest = (prefix == "MOD1") ? bb::LFODest::Mod1Fine : bb::LFODest::Mod2Fine;
    fineKnob.initMod(apvts, fineDest);
    setupKnob(fineKnob);
    fineAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "_FINE", fineKnob);

    // --- Multi knob (fixed mode — overlaps fine position) ---
    setupKnob(multiKnob);
    multiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "_MULTI", multiKnob);

    // Shared label (value updated in timerCallback)
    fineLabel.setText("Fine", juce::dontSendNotification);
    fineLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(fineLabel);

    // --- Level knob ---
    bb::LFODest levelDest = (prefix == "MOD1") ? bb::LFODest::Mod1Level : bb::LFODest::Mod2Level;
    levelKnob.initMod(apvts, levelDest);
    setupKnob(levelKnob, levelLabel, "Level");
    levelAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "_LEVEL", levelKnob);

    // --- ADSR knobs (LFO assignable) ---
    {
        static const char* adsrNames[] = { "A", "D", "S", "R" };
        bb::LFODest envDests1[] = { bb::LFODest::Env1A, bb::LFODest::Env1D, bb::LFODest::Env1S, bb::LFODest::Env1R };
        bb::LFODest envDests2[] = { bb::LFODest::Env2A, bb::LFODest::Env2D, bb::LFODest::Env2S, bb::LFODest::Env2R };
        auto* dests = (prefix == "MOD1") ? envDests1 : envDests2;
        for (int i = 0; i < 4; ++i)
        {
            adsrKnobs[i].initMod(apvts, dests[i]);
            setupKnob(adsrKnobs[i], adsrLabels[i], adsrNames[i]);
            juce::String paramId = envPrefix + "_" + adsrNames[i];
            adsrAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, paramId, adsrKnobs[i]);
        }
    }

    // --- Design mode (harmonic editor) ---
    designBtn.onClick = [this] {
        if (!designMode)
        {
            // Entering design mode: if wave != Custom, init harmonics from current wave
            int waveIdx = static_cast<int>(state.getRawParameterValue(paramPrefix + "_WAVE")->load());
            if (waveIdx != 5) // 5 = Custom
            {
                harmonicTable.initFromWaveType(waveIdx);
                lastDesignWave = waveIdx;
            }
            else
            {
                lastDesignWave = 5;
            }
        }
        setDesignMode(!designMode);
    };
    addAndMakeVisible(designBtn);

    // When user draws bars manually, switch wave to Custom
    harmonicEditor.onUserDraw = [this] {
        int waveIdx = static_cast<int>(state.getRawParameterValue(paramPrefix + "_WAVE")->load());
        if (waveIdx != 5)
        {
            auto* param = state.getParameter(paramPrefix + "_WAVE");
            param->setValueNotifyingHost(param->convertTo0to1(5.0f));
        }
        lastDesignWave = 5;
    };

    addChildComponent(harmonicEditor); // hidden by default

    startTimerHz(5);
}

void ModulatorSection::timerCallback()
{
    // Sync Fixed toggle with KB param
    bool kbOn = state.getRawParameterValue(kbParamId)->load() > 0.5f;
    if (fixedToggle.getToggleState() == kbOn)
        fixedToggle.setToggleState(!kbOn, juce::dontSendNotification);

    bool isFixed = fixedToggle.getToggleState();

    // Swap knob visibility: Coarse/Fine in ratio mode, Freq/Multi in fixed mode
    if (!designMode)
    {
        coarseKnob.setVisible(!isFixed);
        fixedFreqKnob.setVisible(isFixed);
        fineKnob.setVisible(!isFixed);
        multiKnob.setVisible(isFixed);
    }

    // Update main knob label with formatted value
    if (isFixed)
    {
        float freq = fixedFreqKnob.getValue();
        if (freq >= 1000.0f)
            mainKnobLabel.setText(juce::String(freq / 1000.0f, 1) + "k Hz", juce::dontSendNotification);
        else
            mainKnobLabel.setText(juce::String(static_cast<int>(freq)) + " Hz", juce::dontSendNotification);
    }
    else
    {
        int idx = static_cast<int>(coarseKnob.getValue());
        mainKnobLabel.setText(idx == 0 ? "x0.5" : "x" + juce::String(idx), juce::dontSendNotification);
    }

    // Update fine/multi label with formatted value
    if (isFixed)
    {
        int multiIdx = static_cast<int>(multiKnob.getValue());
        static const char* kMVLabels[] = { "x0", "x0.001", "x0.01", "x0.1", "x1", "x10" };
        fineLabel.setText(kMVLabels[juce::jlimit(0, 5, multiIdx)], juce::dontSendNotification);
    }
    else
    {
        float fine = fineKnob.getValue();
        if (fine > 0.5f)
            fineLabel.setText("+" + juce::String(static_cast<int>(fine)) + "ct", juce::dontSendNotification);
        else if (fine < -0.5f)
            fineLabel.setText(juce::String(static_cast<int>(fine)) + "ct", juce::dontSendNotification);
        else
            fineLabel.setText("0ct", juce::dontSendNotification);
    }

    // In design mode: if wave combo changed to a standard wave, update harmonics
    if (designMode)
    {
        int waveIdx = static_cast<int>(state.getRawParameterValue(paramPrefix + "_WAVE")->load());
        if (waveIdx != 5 && waveIdx != lastDesignWave)
        {
            harmonicTable.initFromWaveType(waveIdx);
            lastDesignWave = waveIdx;
        }
    }

    // Level label
    if (levelKnob.isMouseOverOrDragging())
        levelLabel.setText(juce::String(static_cast<int>(levelKnob.getValue() * 100)) + "%", juce::dontSendNotification);
    else
        levelLabel.setText("Level", juce::dontSendNotification);

    // ADSR labels: show name normally, precise value when dragging
    static const char* adsrNames[] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        if (adsrKnobs[i].isMouseOverOrDragging())
        {
            float v = static_cast<float>(adsrKnobs[i].getValue());
            if (i == 2)
                adsrLabels[i].setText(juce::String(v, 3), juce::dontSendNotification);
            else if (v < 1.0f)
                adsrLabels[i].setText(juce::String(v * 1000.0f, 1) + "ms", juce::dontSendNotification);
            else
                adsrLabels[i].setText(juce::String(v, 2) + "s", juce::dontSendNotification);
        }
        else
            adsrLabels[i].setText(adsrNames[i], juce::dontSendNotification);
    }
}

void ModulatorSection::setDesignMode(bool on)
{
    designMode = on;
    designBtn.setButtonText(on ? "Back" : "Harmo");

    // Toggle visibility: knobs hidden in design mode, editor shown
    onToggle.setVisible(!on);
    coarseKnob.setVisible(!on && !fixedToggle.getToggleState());
    fixedFreqKnob.setVisible(!on && fixedToggle.getToggleState());
    mainKnobLabel.setVisible(!on);
    fineKnob.setVisible(!on && !fixedToggle.getToggleState());
    multiKnob.setVisible(!on && fixedToggle.getToggleState());
    fineLabel.setVisible(!on);
    levelKnob.setVisible(!on);
    levelLabel.setVisible(!on);
    harmonicEditor.setVisible(on);

    // Hide ADSR in design mode
    for (int i = 0; i < 4; ++i)
    {
        adsrKnobs[i].setVisible(!on);
        adsrLabels[i].setVisible(!on);
    }

    resized();
    repaint();
}

void ModulatorSection::setupKnob(juce::Slider& knob, juce::Label& label,
                                  const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setSliderSnapsToMousePosition(false);
    knob.setMouseDragSensitivity(200);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void ModulatorSection::setupKnob(juce::Slider& knob)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setSliderSnapsToMousePosition(false);
    knob.setMouseDragSensitivity(200);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
}

void ModulatorSection::resized()
{
    auto area = getLocalBounds().reduced(4);
    int rowH = 28;
    int knobSize = 36;
    int labelH = 12;

    if (designMode)
    {
        // Design mode: top row + harmonic editor fills everything
        auto topRow = area.removeFromTop(rowH);
        fixedToggle.setBounds(topRow.removeFromRight(60).reduced(2));
        designBtn.setBounds(topRow.removeFromRight(42).reduced(2));
        waveCombo.setBounds(topRow.reduced(2));

        area.removeFromTop(2);
        harmonicEditor.setBounds(area);
    }
    else
    {
        // Normal mode: Row 1 pinned to top, distribute spacing below
        // Row 1: [Waveform combo] [H button] [Fixed toggle]
        auto topRow = area.removeFromTop(rowH);
        fixedToggle.setBounds(topRow.removeFromRight(60).reduced(2));
        designBtn.setBounds(topRow.removeFromRight(42).reduced(2));
        waveCombo.setBounds(topRow.reduced(2));

        int contentH = (knobSize + labelH) + (knobSize + labelH);
        int totalGap = area.getHeight() - contentH;
        int gap = std::max(2, totalGap / 3); // split gaps: above row2, between row2-3, below row3

        area.removeFromTop(gap);

        // Row 2: [On] [Coarse/Freq] [Fine/Multi] [Level]
        auto midRow = area.removeFromTop(knobSize + labelH);
        int adsrColW = midRow.getWidth() / 4;

        auto onArea = midRow.removeFromLeft(adsrColW);
        onToggle.setBounds(onArea.withTrimmedLeft(18).reduced(2, 8));

        int colW = midRow.getWidth() / 3;

        auto col1 = midRow.removeFromLeft(colW);
        mainKnobLabel.setBounds(col1.removeFromBottom(labelH));
        coarseKnob.setBounds(col1);
        fixedFreqKnob.setBounds(col1);

        auto col2 = midRow.removeFromLeft(colW);
        fineLabel.setBounds(col2.removeFromBottom(labelH));
        fineKnob.setBounds(col2);
        multiKnob.setBounds(col2);

        auto col3 = midRow;
        levelLabel.setBounds(col3.removeFromBottom(labelH));
        levelKnob.setBounds(col3);

        area.removeFromTop(gap);

        // Row 3: ADSR
        auto adsrRow = area.removeFromTop(knobSize + labelH);
        for (int i = 0; i < 4; ++i)
        {
            auto col = adsrRow.removeFromLeft(adsrColW);
            adsrLabels[i].setBounds(col.removeFromBottom(labelH));
            adsrKnobs[i].setBounds(col);
        }
    }
}
