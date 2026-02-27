// CarrierSection.cpp — Carrier panel: wave, Coarse/Freq, Fixed, Fine, ADSR, XOR, sync
#include "CarrierSection.h"
#include "VisceraLookAndFeel.h"

// ============================================================
// CarrierEnvDisplay — visual ADSR curve for ENV3 (no drag)
// ============================================================

CarrierEnvDisplay::CarrierEnvDisplay(juce::AudioProcessorValueTreeState& apvts)
    : state(apvts)
{
    startTimerHz(15);
}

void CarrierEnvDisplay::timerCallback() { repaint(); }

void CarrierEnvDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour(juce::Colour(VisceraLookAndFeel::kDisplayBg));
    g.fillRoundedRectangle(b, 3.0f);

    auto inner = b.reduced(3.0f, 1.0f);   // horizontal padding only; vertical tight
    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float y0 = inner.getY();

    float attack  = *state.getRawParameterValue("ENV3_A");
    float decay   = *state.getRawParameterValue("ENV3_D");
    float sustain = *state.getRawParameterValue("ENV3_S");
    float release = *state.getRawParameterValue("ENV3_R");

    float sustainHold = 0.25f;
    float totalTime = attack + decay + sustainHold + release;
    if (totalTime < 0.01f) totalTime = 0.01f;
    float pps = w / totalTime;

    float baseline = y0 + h;
    float peakY = y0;
    float sustainY = y0 + h * (1.0f - sustain);

    float cx = x0;
    juce::Point<float> pStart  = { cx, baseline };
    cx += attack * pps;
    juce::Point<float> pPeak   = { cx, peakY };
    cx += decay * pps;
    juce::Point<float> pSusS   = { cx, sustainY };
    cx += sustainHold * pps;
    juce::Point<float> pSusE   = { cx, sustainY };
    cx += release * pps;
    juce::Point<float> pRelEnd = { cx, baseline };

    // Curve
    juce::Path path;
    path.startNewSubPath(pStart);
    path.lineTo(pPeak);
    path.lineTo(pSusS);
    path.lineTo(pSusE);
    path.lineTo(pRelEnd);

    g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    // Fill under curve
    juce::Path fill(path);
    fill.lineTo(pRelEnd.x, baseline);
    fill.lineTo(pStart.x, baseline);
    fill.closeSubPath();
    g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor).withAlpha(0.06f));
    g.fillPath(fill);
}

// ============================================================
// CarrierSection
// ============================================================

CarrierSection::CarrierSection(juce::AudioProcessorValueTreeState& apvts)
    : state(apvts), kbParamId("CAR_KB"), envDisplay(apvts)
{
    // Waveform combo
    waveCombo.addItemList({"Sine", "Saw", "Square", "Tri", "Pulse"}, 1);
    addAndMakeVisible(waveCombo);
    waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "CAR_WAVE", waveCombo);
    // waveLabel hidden (removed for cleaner look)

    // --- Coarse knob (ratio mode) ---
    setupKnob(coarseKnob);
    coarseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CAR_COARSE", coarseKnob);

    // --- Fixed freq knob (fixed mode) ---
    setupKnob(fixedFreqKnob);
    fixedFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CAR_FIXED_FREQ", fixedFreqKnob);

    // Shared label (value updated in timerCallback)
    mainKnobLabel.setText("x1", juce::dontSendNotification);
    mainKnobLabel.setJustificationType(juce::Justification::centred);
    mainKnobLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(mainKnobLabel);

    // --- Fixed toggle (inverted KB) ---
    fixedToggle.setButtonText("Fixed");
    fixedToggle.setClickingTogglesState(true);
    addAndMakeVisible(fixedToggle);

    bool kbOn = apvts.getRawParameterValue(kbParamId)->load() > 0.5f;
    fixedToggle.setToggleState(!kbOn, juce::dontSendNotification);

    fixedToggle.onClick = [this] {
        auto* param = state.getParameter(kbParamId);
        param->setValueNotifyingHost(fixedToggle.getToggleState() ? 0.0f : 1.0f);
    };

    // --- Fine knob (LFO assignable) ---
    fineKnob.initMod(apvts, bb::LFODest::CarFine);
    setupKnob(fineKnob, fineLabel, "Fine");
    fineAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CAR_FINE", fineKnob);

    // Env display
    addAndMakeVisible(envDisplay);

    // ADSR knobs
    const juce::String adsrIds[] = { "ENV3_A", "ENV3_D", "ENV3_S", "ENV3_R" };
    const juce::String adsrNamesList[] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        setupKnob(adsrKnobs[i], adsrLabels[i], adsrNamesList[i]);
        adsrAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, adsrIds[i], adsrKnobs[i]);
    }

    // Drift knob (LFO assignable)
    driftKnob.initMod(apvts, bb::LFODest::CarDrift);
    setupKnob(driftKnob, driftLabel, "Drift");
    driftAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CAR_DRIFT", driftKnob);

    // Noise knob
    noiseKnob.initMod(apvts, bb::LFODest::CarNoise);
    setupKnob(noiseKnob, noiseLabel, "Noise");
    noiseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CAR_NOISE", noiseKnob);

    // Spread knob (stereo unison detune)
    spreadKnob.initMod(apvts, bb::LFODest::CarSpread);
    setupKnob(spreadKnob, spreadLabel, "Spread");
    spreadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CAR_SPREAD", spreadKnob);

    // XOR + Sync toggles
    xorToggle.setButtonText("XOR");
    addAndMakeVisible(xorToggle);
    xorAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "XOR_ON", xorToggle);

    syncToggle.setButtonText("Sync");
    addAndMakeVisible(syncToggle);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "SYNC", syncToggle);

    // Timer to refresh labels on preset change
    startTimerHz(5);
}

void CarrierSection::timerCallback()
{
    // Sync Fixed toggle with KB param
    bool kbOn = state.getRawParameterValue(kbParamId)->load() > 0.5f;
    if (fixedToggle.getToggleState() == kbOn)
        fixedToggle.setToggleState(!kbOn, juce::dontSendNotification);

    bool isFixed = fixedToggle.getToggleState();
    coarseKnob.setVisible(!isFixed);
    fixedFreqKnob.setVisible(isFixed);

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

    // Update fine label with formatted value
    float fine = fineKnob.getValue();
    if (fine > 0.5f)
        fineLabel.setText("+" + juce::String(static_cast<int>(fine)) + "ct", juce::dontSendNotification);
    else if (fine < -0.5f)
        fineLabel.setText(juce::String(static_cast<int>(fine)) + "ct", juce::dontSendNotification);
    else
        fineLabel.setText("0ct", juce::dontSendNotification);

    // Drift / Noise / Spread labels
    auto showPct = [](juce::Slider& knob, juce::Label& label, const char* name) {
        if (knob.isMouseOverOrDragging())
            label.setText(juce::String(static_cast<int>(knob.getValue() * 100)) + "%", juce::dontSendNotification);
        else
            label.setText(name, juce::dontSendNotification);
    };
    showPct(driftKnob, driftLabel, "Drift");
    showPct(noiseKnob, noiseLabel, "Noise");
    showPct(spreadKnob, spreadLabel, "Spread");

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

void CarrierSection::setupKnob(juce::Slider& knob, juce::Label& label,
                                 const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(label);
}

void CarrierSection::setupKnob(juce::Slider& knob)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
}

void CarrierSection::resized()
{
    auto area = getLocalBounds().reduced(4);

    int labelH = 12;
    int knobH = 36;

    // Row 1: Wave combo + Fixed + XOR + Sync
    auto topBar = area.removeFromTop(26);
    waveCombo.setBounds(topBar.removeFromLeft(80).reduced(1));
    topBar.removeFromLeft(4);
    int toggleW = topBar.getWidth() / 3;
    fixedToggle.setBounds(topBar.removeFromLeft(toggleW).reduced(1));
    xorToggle.setBounds(topBar.removeFromLeft(toggleW).reduced(1));
    syncToggle.setBounds(topBar.reduced(1));

    area.removeFromTop(2);

    // Row 2 (pitch + character knobs): [Coarse/Freq] [Fine] [Drift] [Noise] [Spread]
    auto knobRow1 = area.removeFromTop(knobH + labelH);
    int colW = knobRow1.getWidth() / 5;

    auto coarseArea = knobRow1.removeFromLeft(colW);
    mainKnobLabel.setBounds(coarseArea.removeFromBottom(labelH));
    coarseKnob.setBounds(coarseArea.reduced(2, 0));
    fixedFreqKnob.setBounds(coarseArea.reduced(2, 0));

    auto fineArea = knobRow1.removeFromLeft(colW);
    fineLabel.setBounds(fineArea.removeFromBottom(labelH));
    fineKnob.setBounds(fineArea.reduced(2, 0));

    auto driftArea = knobRow1.removeFromLeft(colW);
    driftLabel.setBounds(driftArea.removeFromBottom(labelH));
    driftKnob.setBounds(driftArea.reduced(2, 0));

    auto noiseArea = knobRow1.removeFromLeft(colW);
    noiseLabel.setBounds(noiseArea.removeFromBottom(labelH));
    noiseKnob.setBounds(noiseArea.reduced(2, 0));

    auto spreadArea = knobRow1;
    spreadLabel.setBounds(spreadArea.removeFromBottom(labelH));
    spreadKnob.setBounds(spreadArea.reduced(2, 0));

    area.removeFromTop(2);

    // Bottom: ADSR knobs
    auto knobRow2 = area.removeFromBottom(knobH + labelH);
    int adsrColW = knobRow2.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto col = knobRow2.removeFromLeft(adsrColW);
        adsrLabels[i].setBounds(col.removeFromBottom(labelH));
        adsrKnobs[i].setBounds(col.reduced(2, 0));
    }

    area.removeFromBottom(2);

    // ENV3 display
    envDisplay.setBounds(area.reduced(0, 2));
}
