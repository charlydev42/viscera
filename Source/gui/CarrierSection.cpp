// CarrierSection.cpp — Carrier panel: wave, Coarse/Freq, Fixed, Fine, ADSR, XOR, sync
#include "CarrierSection.h"
#include "ParasiteLookAndFeel.h"

// ============================================================
// CarrierEnvDisplay — interactive ADSR display (drag points)
// ============================================================

CarrierEnvDisplay::CarrierEnvDisplay(juce::AudioProcessorValueTreeState& apvts)
    : state(apvts)
{
    startTimerHz(15);
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void CarrierEnvDisplay::timerCallback() { repaint(); }

// Fixed-zone layout (like Ableton Operator): each ADSR segment has a
// maximum pixel budget.  When a param is 0, its segment collapses to 0px
// so adjacent points overlap naturally.
static constexpr float kMaxA   = 5.0f;   // max attack seconds
static constexpr float kMaxD   = 5.0f;   // max decay seconds
static constexpr float kSusHold = 0.14f; // sustain hold as fraction of width
static constexpr float kMaxR   = 8.0f;   // max release seconds

// Skewed 0-1 mapping (sqrt for log-ish feel)
static float paramToFrac(float val, float maxV)
{
    if (maxV <= 0.0f) return 0.0f;
    return std::sqrt(juce::jlimit(0.0f, 1.0f, val / maxV));
}
static float fracToParam(float frac, float maxV)
{
    float sq = juce::jlimit(0.0f, 1.0f, frac);
    return sq * sq * maxV;
}

void CarrierEnvDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour(juce::Colour(ParasiteLookAndFeel::kDisplayBg));
    g.fillRoundedRectangle(b, 3.0f);

    auto inner = b.reduced(6.0f, 4.0f);
    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float y0 = inner.getY();

    float attack  = *state.getRawParameterValue("ENV3_A");
    float decay   = *state.getRawParameterValue("ENV3_D");
    float sustain = *state.getRawParameterValue("ENV3_S");
    float release = *state.getRawParameterValue("ENV3_R");

    float baseline = y0 + h;
    float peakY = y0;
    float sustainY = y0 + h * (1.0f - sustain);

    // Budget: each segment gets a max pixel allocation, but collapses when param=0
    float maxADR = w * (1.0f - kSusHold);          // total px for A+D+R
    float budgetA = maxADR * (kMaxA / (kMaxA + kMaxD + kMaxR));  // proportional budgets
    float budgetD = maxADR * (kMaxD / (kMaxA + kMaxD + kMaxR));
    float budgetR = maxADR * (kMaxR / (kMaxA + kMaxD + kMaxR));
    float susW    = w * kSusHold;

    // Cumulative positions — points can overlap when params are 0
    float peakX     = x0 + paramToFrac(attack, kMaxA) * budgetA;
    float susStartX = peakX + paramToFrac(decay, kMaxD) * budgetD;
    float susEndX   = susStartX + susW;
    float relEndX   = susEndX + paramToFrac(release, kMaxR) * budgetR;

    juce::Point<float> pStart = { x0, baseline };
    ptPeak    = { peakX, peakY };
    ptSustain = { susStartX, sustainY };     // decay→sustain corner (draggable)
    ptRelEnd  = { relEndX, baseline };
    juce::Point<float> pSusEnd = { susEndX, sustainY };

    // Filled area
    juce::Path fill;
    fill.startNewSubPath(pStart);
    fill.lineTo(ptPeak);
    fill.lineTo(ptSustain);
    fill.lineTo(pSusEnd);
    fill.lineTo(ptRelEnd);
    fill.lineTo(ptRelEnd.x, baseline);
    fill.lineTo(pStart.x, baseline);
    fill.closeSubPath();
    g.setColour(juce::Colour(ParasiteLookAndFeel::kAccentColor).withAlpha(0.08f));
    g.fillPath(fill);

    // Curve stroke
    juce::Path path;
    path.startNewSubPath(pStart);
    path.lineTo(ptPeak);
    path.lineTo(ptSustain);
    path.lineTo(pSusEnd);
    path.lineTo(ptRelEnd);
    g.setColour(juce::Colour(ParasiteLookAndFeel::kAccentColor));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    // Draggable points
    auto accent = juce::Colour(ParasiteLookAndFeel::kAccentColor);
    auto drawPoint = [&](juce::Point<float> pt, int idx) {
        float r = (idx == dragPoint) ? 5.0f : (idx == hoveredPoint) ? 4.5f : 3.5f;
        if (idx == dragPoint || idx == hoveredPoint)
        {
            g.setColour(accent.withAlpha(0.25f));
            g.fillEllipse(pt.x - r - 2, pt.y - r - 2, (r + 2) * 2, (r + 2) * 2);
        }
        g.setColour(accent);
        g.fillEllipse(pt.x - r, pt.y - r, r * 2, r * 2);
    };

    drawPoint(ptPeak, 0);      // Attack peak (horizontal = attack time)
    drawPoint(ptSustain, 1);   // Decay/Sustain (horizontal = decay, vertical = sustain)
    drawPoint(ptRelEnd, 2);    // Release end (horizontal = release time)
}

int CarrierEnvDisplay::pointAtPosition(juce::Point<float> pos) const
{
    if (ptPeak.getDistanceFrom(pos) < kHitRadius)    return 0;
    if (ptSustain.getDistanceFrom(pos) < kHitRadius) return 1;
    if (ptRelEnd.getDistanceFrom(pos) < kHitRadius)  return 2;
    return -1;
}

void CarrierEnvDisplay::setParamNormalized(const juce::String& id, float newVal)
{
    if (auto* param = state.getParameter(id))
    {
        auto range = param->getNormalisableRange();
        float clamped = juce::jlimit(range.start, range.end, newVal);
        param->setValueNotifyingHost(range.convertTo0to1(clamped));
    }
}

void CarrierEnvDisplay::mouseDown(const juce::MouseEvent& e)
{
    dragPoint = pointAtPosition(e.position);
    if (dragPoint >= 0)
    {
        // Begin gesture for undo
        if (dragPoint == 0)
            state.getParameter("ENV3_A")->beginChangeGesture();
        else if (dragPoint == 1)
        {
            state.getParameter("ENV3_D")->beginChangeGesture();
            state.getParameter("ENV3_S")->beginChangeGesture();
        }
        else if (dragPoint == 2)
            state.getParameter("ENV3_R")->beginChangeGesture();
    }
}

void CarrierEnvDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragPoint < 0) return;

    auto inner = getLocalBounds().toFloat().reduced(6.0f, 4.0f);
    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float relX = e.position.x - x0;
    float relY = e.position.y - inner.getY();

    float maxADR = w * (1.0f - kSusHold);
    float budgetA = maxADR * (kMaxA / (kMaxA + kMaxD + kMaxR));
    float budgetD = maxADR * (kMaxD / (kMaxA + kMaxD + kMaxR));
    float budgetR = maxADR * (kMaxR / (kMaxA + kMaxD + kMaxR));

    if (dragPoint == 0)
    {
        // Attack: pixel offset from x0 → param
        float frac = juce::jlimit(0.0f, 1.0f, relX / budgetA);
        setParamNormalized("ENV3_A", fracToParam(frac, kMaxA));
    }
    else if (dragPoint == 1)
    {
        // Decay: pixel offset from current peak position → param
        float curAttack = *state.getRawParameterValue("ENV3_A");
        float peakPx = paramToFrac(curAttack, kMaxA) * budgetA;
        float dRelX = relX - peakPx;
        float frac = juce::jlimit(0.0f, 1.0f, dRelX / budgetD);
        setParamNormalized("ENV3_D", fracToParam(frac, kMaxD));

        // Sustain: vertical position
        float newSustain = juce::jlimit(0.0f, 1.0f, 1.0f - relY / h);
        setParamNormalized("ENV3_S", newSustain);
    }
    else if (dragPoint == 2)
    {
        // Release: pixel offset from sustain end → param
        float curAttack = *state.getRawParameterValue("ENV3_A");
        float curDecay  = *state.getRawParameterValue("ENV3_D");
        float susEndPx = paramToFrac(curAttack, kMaxA) * budgetA
                       + paramToFrac(curDecay, kMaxD) * budgetD
                       + w * kSusHold;
        float rRelX = relX - susEndPx;
        float frac = juce::jlimit(0.0f, 1.0f, rRelX / budgetR);
        setParamNormalized("ENV3_R", fracToParam(frac, kMaxR));
    }

    repaint();
}

void CarrierEnvDisplay::mouseUp(const juce::MouseEvent&)
{
    if (dragPoint == 0)
        state.getParameter("ENV3_A")->endChangeGesture();
    else if (dragPoint == 1)
    {
        state.getParameter("ENV3_D")->endChangeGesture();
        state.getParameter("ENV3_S")->endChangeGesture();
    }
    else if (dragPoint == 2)
        state.getParameter("ENV3_R")->endChangeGesture();

    dragPoint = -1;
}

void CarrierEnvDisplay::mouseMove(const juce::MouseEvent& e)
{
    int newHover = pointAtPosition(e.position);
    if (newHover != hoveredPoint)
    {
        hoveredPoint = newHover;
        setMouseCursor(hoveredPoint >= 0 ? juce::MouseCursor::DraggingHandCursor
                                         : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

// ============================================================
// CarrierSection
// ============================================================

CarrierSection::CarrierSection(juce::AudioProcessorValueTreeState& apvts,
                               bb::HarmonicTable& harmonics)
    : state(apvts), kbParamId("CAR_KB"), harmonicTable(harmonics),
      envDisplay(apvts), harmonicEditor(harmonics)
{
    // Waveform combo
    waveCombo.addItemList({"Sine", "Saw", "Square", "Tri", "Pulse", "Custom", "Noise"}, 1);
    waveCombo.setWantsKeyboardFocus(false);
    addAndMakeVisible(waveCombo);
    waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "CAR_WAVE", waveCombo);
    waveCombo.onChange = [this] {
        int waveIdx = waveCombo.getSelectedId() - 1;
        if (waveIdx == 5 && !designMode)
        {
            // Only auto-switch if user clicked the combo popup (mouse near it)
            auto mousePos = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
            if (waveCombo.getScreenBounds().toFloat().expanded(0, 200).contains(mousePos))
            {
                lastDesignWave = 5;
                setDesignMode(true);
            }
        }
    };

    // --- Coarse knob (ratio mode, LFO assignable) ---
    coarseKnob.initMod(apvts, bb::LFODest::CarCoarse);
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

    // --- Multi knob (fixed mode — same as modulators) ---
    setupKnob(multiKnob);
    multiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "CAR_MULTI", multiKnob);

    // Env display
    addAndMakeVisible(envDisplay);

    // ADSR knobs (LFO assignable)
    {
        const juce::String adsrIds[] = { "ENV3_A", "ENV3_D", "ENV3_S", "ENV3_R" };
        const juce::String adsrNamesList[] = { "A", "D", "S", "R" };
        bb::LFODest envDests[] = { bb::LFODest::Env3A, bb::LFODest::Env3D, bb::LFODest::Env3S, bb::LFODest::Env3R };
        for (int i = 0; i < 4; ++i)
        {
            adsrKnobs[i].initMod(apvts, envDests[i]);
            setupKnob(adsrKnobs[i], adsrLabels[i], adsrNamesList[i]);
            adsrAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, adsrIds[i], adsrKnobs[i]);
        }
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

    // --- Design mode (harmonic editor) ---
    designBtn.onClick = [this] {
        if (!designMode)
        {
            int waveIdx = static_cast<int>(state.getRawParameterValue("CAR_WAVE")->load());
            if (waveIdx != 5) // 5 = Custom
            {
                harmonicTable.initFromWaveType(waveIdx);
                syncHarmonicsToParams();
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
        int waveIdx = static_cast<int>(state.getRawParameterValue("CAR_WAVE")->load());
        if (waveIdx != 5)
        {
            auto* param = state.getParameter("CAR_WAVE");
            param->setValueNotifyingHost(param->convertTo0to1(5.0f));
        }
        lastDesignWave = 5;
    };

    // Route carrier harmonic edits through CAR_H## params for Cmd+Z undo
    harmonicEditor.onSetHarmonic = [this](int idx, float amp) {
        auto pid = "CAR_H" + juce::String(idx).paddedLeft('0', 2);
        if (auto* param = state.getParameter(pid))
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, amp));
    };

    addChildComponent(harmonicEditor);

    // Timer to refresh labels on preset change
    startTimerHz(5);
}

void CarrierSection::timerCallback()
{
    // Sync Fixed toggle with KB param
    bool kbOn = state.getRawParameterValue(kbParamId)->load() > 0.5f;
    if (fixedToggle.getToggleState() == kbOn)
        fixedToggle.setToggleState(!kbOn, juce::dontSendNotification);

    // In design mode: if wave combo changed to a standard wave, update harmonics
    if (designMode)
    {
        int waveIdx = static_cast<int>(state.getRawParameterValue("CAR_WAVE")->load());
        if (waveIdx != 5 && waveIdx != lastDesignWave)
        {
            harmonicTable.initFromWaveType(waveIdx);
            syncHarmonicsToParams();
            lastDesignWave = waveIdx;
        }
    }

    bool isFixed = fixedToggle.getToggleState();
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



void CarrierSection::syncHarmonicsToParams()
{
    // Per-tick beginNewTransaction in the editor groups these 32 writes into
    // one undo step. Listener round-trip is idempotent (~32 rebakes total).
    for (int i = 0; i < 32; ++i)
    {
        auto pid = "CAR_H" + juce::String(i).paddedLeft('0', 2);
        if (auto* p = state.getParameter(pid))
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, harmonicTable.getHarmonic(i)));
    }
}

void CarrierSection::setDesignMode(bool on)
{
    designMode = on;
    designBtn.setButtonText(on ? "Back" : "Harmo");

    bool isFixed = fixedToggle.getToggleState();

    // Hide Row 2 knobs in design mode
    coarseKnob.setVisible(!on && !isFixed);
    fixedFreqKnob.setVisible(!on && isFixed);
    mainKnobLabel.setVisible(!on);
    fineKnob.setVisible(!on && !isFixed);
    multiKnob.setVisible(!on && isFixed);
    fineLabel.setVisible(!on);
    driftKnob.setVisible(!on);
    driftLabel.setVisible(!on);
    noiseKnob.setVisible(!on);
    noiseLabel.setVisible(!on);
    spreadKnob.setVisible(!on);
    spreadLabel.setVisible(!on);
    harmonicEditor.setVisible(on);
    envDisplay.setVisible(!on);

    resized();
    repaint();
}

void CarrierSection::setupKnob(juce::Slider& knob, juce::Label& label,
                                 const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setSliderSnapsToMousePosition(false);
    knob.setMouseDragSensitivity(200);
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
    knob.setSliderSnapsToMousePosition(false);
    knob.setMouseDragSensitivity(200);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
}

void CarrierSection::resized()
{
    int labelH = 12;
    int knobH = 36;

    if (designMode)
    {
        auto area = getLocalBounds().reduced(4);

        // Row 1: Wave combo + Harmo + Fixed + XOR + Sync
        auto topBar = area.removeFromTop(26);
        waveCombo.setBounds(topBar.removeFromLeft(72).reduced(1));
        topBar.removeFromLeft(1);
        designBtn.setBounds(topBar.removeFromLeft(38).reduced(1));
        topBar.removeFromLeft(1);
        int toggleW = topBar.getWidth() / 3;
        fixedToggle.setBounds(topBar.removeFromLeft(toggleW).reduced(1));
        xorToggle.setBounds(topBar.removeFromLeft(toggleW).reduced(1));
        syncToggle.setBounds(topBar.reduced(1));

        area.removeFromTop(2);

        // Bottom: ADSR knobs (no env display)
        auto knobRow2 = area.removeFromBottom(knobH + labelH);
        int adsrColW = knobRow2.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto col = knobRow2.removeFromLeft(adsrColW);
            adsrLabels[i].setBounds(col.removeFromBottom(labelH));
            adsrKnobs[i].setBounds(col.reduced(2, 0));
        }

        area.removeFromBottom(2);

        // Remaining: harmonic editor
        harmonicEditor.setBounds(area);
    }
    else
    {
        // Single area from reduced(2) — aligns ADSR bottom with PitchEnv/Global
        auto area = getLocalBounds().reduced(2);
        area.removeFromTop(2);

        // Row 1: Wave combo + Harmo + Fixed + XOR + Sync
        auto topBar = area.removeFromTop(26).reduced(2, 0);
        waveCombo.setBounds(topBar.removeFromLeft(72).reduced(1));
        topBar.removeFromLeft(1);
        designBtn.setBounds(topBar.removeFromLeft(38).reduced(1));
        topBar.removeFromLeft(1);
        int toggleW = topBar.getWidth() / 3;
        fixedToggle.setBounds(topBar.removeFromLeft(toggleW).reduced(1));
        xorToggle.setBounds(topBar.removeFromLeft(toggleW).reduced(1));
        syncToggle.setBounds(topBar.reduced(1));

        area.removeFromTop(6);

        // Knob row: Coarse/Fine/Drift/Noise/Spread (right below top bar)
        auto knobRow1 = area.removeFromTop(knobH + labelH);
        int colW = knobRow1.getWidth() / 5;

        auto coarseArea = knobRow1.removeFromLeft(colW);
        mainKnobLabel.setBounds(coarseArea.removeFromBottom(labelH));
        coarseKnob.setBounds(coarseArea.reduced(2, 0));
        fixedFreqKnob.setBounds(coarseArea.reduced(2, 0));

        auto fineArea = knobRow1.removeFromLeft(colW);
        fineLabel.setBounds(fineArea.removeFromBottom(labelH));
        fineKnob.setBounds(fineArea.reduced(2, 0));
        multiKnob.setBounds(fineArea.reduced(2, 0));

        auto driftArea = knobRow1.removeFromLeft(colW);
        driftLabel.setBounds(driftArea.removeFromBottom(labelH));
        driftKnob.setBounds(driftArea.reduced(2, 0));

        auto noiseArea = knobRow1.removeFromLeft(colW);
        noiseLabel.setBounds(noiseArea.removeFromBottom(labelH));
        noiseKnob.setBounds(noiseArea.reduced(2, 0));

        auto spreadArea = knobRow1;
        spreadLabel.setBounds(spreadArea.removeFromBottom(labelH));
        spreadKnob.setBounds(spreadArea.reduced(2, 0));

        area.removeFromTop(3);

        // Bottom: ADSR knobs (aligned with PitchEnv/Global)
        area.removeFromBottom(5);
        auto knobRow2 = area.removeFromBottom(knobH + labelH);
        int adsrColW = knobRow2.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto col = knobRow2.removeFromLeft(adsrColW);
            adsrLabels[i].setBounds(col.removeFromBottom(labelH));
            adsrKnobs[i].setBounds(col);
        }

        // ENV3 display fills remaining space between knob row and ADSR
        area.removeFromBottom(2);
        area.removeFromTop(2);
        envDisplay.setBounds(area);
    }
}
