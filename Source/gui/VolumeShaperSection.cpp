// VolumeShaperSection.cpp — Drawable volume shaper display + presets + Fixed/Rate/Depth
#include "VolumeShaperSection.h"
#include "VisceraLookAndFeel.h"
#include <cmath>

// ============================================================
// ShaperDisplay
// ============================================================

ShaperDisplay::ShaperDisplay(bb::VolumeShaper& shaper)
    : volumeShaper(shaper)
{
    startTimerHz(30);
}

void ShaperDisplay::timerCallback() { repaint(); }

void ShaperDisplay::applyMouse(const juce::MouseEvent& e)
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    int numBars = coarseMode ? 8 : bb::VolumeShaper::kNumSteps;
    float stepW = b.getWidth() / static_cast<float>(numBars);

    int barIdx = static_cast<int>((e.position.x - b.getX()) / stepW);
    barIdx = juce::jlimit(0, numBars - 1, barIdx);

    float val = 1.0f - (e.position.y - b.getY()) / b.getHeight();
    val = juce::jlimit(0.0f, 1.0f, val);

    if (coarseMode)
    {
        // Each coarse bar covers 4 fine steps
        int startStep = barIdx * 4;
        for (int i = 0; i < 4; ++i)
            volumeShaper.setStep(startStep + i, val);
    }
    else
    {
        volumeShaper.setStep(barIdx, val);
    }
}

void ShaperDisplay::mouseDown(const juce::MouseEvent& e) { applyMouse(e); }
void ShaperDisplay::mouseDrag(const juce::MouseEvent& e) { applyMouse(e); }

void ShaperDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(juce::Colour(VisceraLookAndFeel::kDisplayBg));
    g.fillRoundedRectangle(b, 3.0f);

    auto inner = b.reduced(1.0f);
    int numBars = coarseMode ? 8 : bb::VolumeShaper::kNumSteps;
    float stepW = inner.getWidth() / static_cast<float>(numBars);
    float h = inner.getHeight();
    float y0 = inner.getY();
    float x0 = inner.getX();

    g.setColour(juce::Colour(VisceraLookAndFeel::kKnobColor).withAlpha(0.7f));
    for (int i = 0; i < numBars; ++i)
    {
        float val;
        if (coarseMode)
        {
            // Average of 4 underlying steps
            int s = i * 4;
            val = (volumeShaper.getStep(s) + volumeShaper.getStep(s + 1)
                 + volumeShaper.getStep(s + 2) + volumeShaper.getStep(s + 3)) * 0.25f;
        }
        else
        {
            val = volumeShaper.getStep(i);
        }
        float barH = val * h;
        float bx = x0 + i * stepW + 1.0f;
        float bw = stepW - 2.0f;
        if (bw < 1.0f) bw = 1.0f;
        g.fillRect(bx, y0 + h - barH, bw, barH);
    }

    float phase = volumeShaper.getPhase();
    if (phase > 0.001f)
    {
        float px = x0 + phase * inner.getWidth();
        g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor));
        g.drawVerticalLine(static_cast<int>(px), y0, y0 + h);
    }
}

// ============================================================
// Shape presets — 32 float values per preset
// ============================================================

// Presets work at any sync speed — smooth curves for clean sound even at 1/16
static const float kShapePresets[][32] = {
    // 0: Sidechain — smooth pump curve (classic EDM)
    { 0.0f, 0.1f, 0.25f, 0.4f, 0.55f, 0.7f, 0.8f, 0.88f,
      0.94f, 0.97f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.4f },
    // 1: Pump — tight sidechain, fast recovery
    { 0.0f, 0.2f, 0.5f, 0.75f, 0.9f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.9f, 0.6f,
      0.0f, 0.2f, 0.5f, 0.75f, 0.9f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.9f, 0.6f },
    // 2: Trance Gate — smooth on/off, half cycle
    { 0.0f, 0.3f, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 0.3f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // 3: Choppy — 3/4 on, 1/4 off with soft edges
    { 0.2f, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 0.2f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // 4: Sine — smooth sine wave shape
    { 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 0.95f, 1.0f, 1.0f,
      1.0f, 1.0f, 0.95f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f,
      0.4f, 0.3f, 0.2f, 0.1f, 0.05f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.05f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f },
    // 5: Saw Down — linear ramp down
    { 1.0f, 0.97f, 0.94f, 0.9f, 0.87f, 0.84f, 0.8f, 0.77f,
      0.74f, 0.7f, 0.67f, 0.64f, 0.6f, 0.57f, 0.54f, 0.5f,
      0.47f, 0.44f, 0.4f, 0.37f, 0.34f, 0.3f, 0.27f, 0.23f,
      0.2f, 0.16f, 0.13f, 0.1f, 0.07f, 0.04f, 0.01f, 0.0f },
    // 6: Saw Up — linear ramp up
    { 0.0f, 0.03f, 0.06f, 0.1f, 0.13f, 0.16f, 0.2f, 0.23f,
      0.26f, 0.3f, 0.33f, 0.36f, 0.4f, 0.43f, 0.46f, 0.5f,
      0.53f, 0.56f, 0.6f, 0.63f, 0.66f, 0.7f, 0.73f, 0.77f,
      0.8f, 0.84f, 0.87f, 0.9f, 0.93f, 0.96f, 0.99f, 1.0f },
    // 7: Bounce — double pump (two hits per cycle)
    { 0.0f, 0.3f, 0.7f, 1.0f, 1.0f, 1.0f, 0.8f, 0.5f,
      0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.3f, 0.7f, 1.0f, 1.0f, 1.0f, 0.8f, 0.5f,
      0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // 8: Wobble — asymmetric sine, bass wobble feel
    { 1.0f, 1.0f, 1.0f, 1.0f, 0.95f, 0.85f, 0.7f, 0.5f,
      0.3f, 0.15f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.05f, 0.15f, 0.3f, 0.5f, 0.7f,
      0.85f, 0.95f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    // 9: Stutter — quick repeating hits
    { 1.0f, 1.0f, 0.8f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 1.0f, 0.8f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 1.0f, 0.8f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f,
      1.0f, 1.0f, 0.8f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f },
    // 10: Breathe — slow fade in, slow fade out
    { 0.0f, 0.0f, 0.02f, 0.05f, 0.1f, 0.18f, 0.28f, 0.4f,
      0.52f, 0.65f, 0.76f, 0.85f, 0.92f, 0.97f, 1.0f, 1.0f,
      1.0f, 1.0f, 0.97f, 0.92f, 0.85f, 0.76f, 0.65f, 0.52f,
      0.4f, 0.28f, 0.18f, 0.1f, 0.05f, 0.02f, 0.0f, 0.0f },
    // 11: SC Hard — reverse bass, ramp mid-cycle then duck before end
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.05f, 0.12f, 0.25f, 0.4f, 0.6f, 0.75f,
      0.88f, 0.95f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.9f,
      0.7f, 0.45f, 0.2f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f },
    // 12: Swirl — alternating long/short bursts, triplet-ish
    { 0.0f, 0.4f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 0.9f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.5f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    // 13: Glitch — irregular hits, broken rhythm
    { 1.0f, 1.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 1.0f,
      0.8f, 0.0f, 0.0f, 0.4f, 1.0f, 1.0f, 0.4f, 0.0f },
    // 14: Flat — bypass
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
};

static constexpr int kNumShapePresets = 15;

// ============================================================
// VolumeShaperSection
// ============================================================

VolumeShaperSection::VolumeShaperSection(juce::AudioProcessorValueTreeState& apvts,
                                         bb::VolumeShaper& shaper)
    : state(apvts), volumeShaper(shaper), shaperDisplay(shaper)
{
    syncNames = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                  "1/4T", "1/8T", "1/16T" };

    // On toggle
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "SHAPER_ON", onToggle);

    // Shape preset selector
    shapePresetBox.addItemList({ "Sidechain", "Pump", "Trance",
                                  "Choppy", "Sine", "Saw Down",
                                  "Saw Up", "Bounce", "Wobble",
                                  "Stutter", "Breathe", "SC Hard",
                                  "Swirl", "Glitch", "Flat" }, 1);
    shapePresetBox.onChange = [this] {
        int idx = shapePresetBox.getSelectedItemIndex();
        if (idx >= 0) loadShapePreset(idx);
    };
    addAndMakeVisible(shapePresetBox);

    // Fixed toggle
    fixedToggle.setButtonText("Fixed");
    fixedToggle.onClick = [this] {
        if (fixedToggle.getToggleState())
            setSyncParam(lastSyncIdx);
        else
        {
            int cur = getSyncParam();
            if (cur > 0) lastSyncIdx = cur;
            setSyncParam(0);
        }
        updateDisplay();
        resized();
    };
    addAndMakeVisible(fixedToggle);

    // Rate knob (free mode)
    rateKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    rateKnob.setSliderSnapsToMousePosition(false);
    rateKnob.setMouseDragSensitivity(200);
    rateKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(rateKnob);
    rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "SHAPER_RATE", rateKnob);

    rateValueLabel.setJustificationType(juce::Justification::centred);
    rateValueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(rateValueLabel);

    // Sync knob (fixed mode)
    syncKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    syncKnob.setSliderSnapsToMousePosition(false);
    syncKnob.setMouseDragSensitivity(200);
    syncKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    syncKnob.setRange(1.0, 9.0, 1.0);
    syncKnob.onValueChange = [this] {
        int idx = static_cast<int>(syncKnob.getValue());
        setSyncParam(idx);
        lastSyncIdx = idx;
        updateDisplay();
    };
    addAndMakeVisible(syncKnob);

    syncValueLabel.setJustificationType(juce::Justification::centred);
    syncValueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(syncValueLabel);

    // Depth knob
    depthKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    depthKnob.setSliderSnapsToMousePosition(false);
    depthKnob.setMouseDragSensitivity(200);
    depthKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(depthKnob);
    depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "SHAPER_DEPTH", depthKnob);
    depthLabel.setText("Depth", juce::dontSendNotification);
    depthLabel.setJustificationType(juce::Justification::centred);
    depthLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(depthLabel);

    // Subdivision toggle (32/8 bars)
    subdivBtn.setButtonText("x4");
    subdivBtn.setClickingTogglesState(true);
    subdivBtn.onClick = [this] {
        shaperDisplay.setCoarseMode(subdivBtn.getToggleState());
        subdivBtn.setButtonText(subdivBtn.getToggleState() ? "x1" : "x4");
    };
    addAndMakeVisible(subdivBtn);

    addAndMakeVisible(shaperDisplay);

    // Load default shape preset (Sidechain)
    shapePresetBox.setSelectedItemIndex(0, juce::dontSendNotification);
    loadShapePreset(0);

    // Init from parameter
    int syncIdx = getSyncParam();
    fixedToggle.setToggleState(syncIdx > 0, juce::dontSendNotification);
    if (syncIdx > 0)
    {
        lastSyncIdx = syncIdx;
        syncKnob.setValue(syncIdx, juce::dontSendNotification);
    }
    updateDisplay();

    startTimerHz(8);
}

void VolumeShaperSection::timerCallback()
{
    updateDisplay();
    int syncIdx = getSyncParam();
    if (syncIdx > 0 && static_cast<int>(syncKnob.getValue()) != syncIdx)
        syncKnob.setValue(syncIdx, juce::dontSendNotification);

    if (depthKnob.isMouseOverOrDragging())
        depthLabel.setText(juce::String(static_cast<int>(depthKnob.getValue() * 100)) + "%", juce::dontSendNotification);
    else
        depthLabel.setText("Depth", juce::dontSendNotification);
}

int VolumeShaperSection::getSyncParam() const
{
    return static_cast<int>(state.getRawParameterValue("SHAPER_SYNC")->load());
}

void VolumeShaperSection::setSyncParam(int idx)
{
    if (auto* p = state.getParameter("SHAPER_SYNC"))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
}

void VolumeShaperSection::loadShapePreset(int presetIdx)
{
    if (presetIdx < 0 || presetIdx >= kNumShapePresets) return;
    for (int i = 0; i < bb::VolumeShaper::kNumSteps; ++i)
        volumeShaper.setStep(i, kShapePresets[presetIdx][i]);
}

void VolumeShaperSection::updateDisplay()
{
    int syncIdx = getSyncParam();
    bool isFixed = syncIdx > 0;

    rateKnob.setVisible(!isFixed);
    rateValueLabel.setVisible(!isFixed);
    syncKnob.setVisible(isFixed);
    syncValueLabel.setVisible(isFixed);

    if (isFixed)
    {
        int nameIdx = syncIdx - 1;
        if (nameIdx >= 0 && nameIdx < syncNames.size())
            syncValueLabel.setText(syncNames[nameIdx], juce::dontSendNotification);
    }
    else
    {
        float rate = state.getRawParameterValue("SHAPER_RATE")->load();
        rateValueLabel.setText(juce::String(rate, 1) + " Hz", juce::dontSendNotification);
    }
}

void VolumeShaperSection::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(2);

    // Top row: On toggle + shape preset + subdivision toggle
    auto topRow = area.removeFromTop(18);
    onToggle.setBounds(topRow.removeFromLeft(36));
    topRow.removeFromLeft(4);
    subdivBtn.setBounds(topRow.removeFromRight(28));
    topRow.removeFromRight(4);
    shapePresetBox.setBounds(topRow.reduced(0, 1));

    area.removeFromTop(2);

    // Bottom row: [Fixed] [Rate/Sync] [Depth]
    int knobH = 28;
    int labelH = 12;
    auto knobRow = area.removeFromBottom(knobH + labelH);
    int colW = knobRow.getWidth() / 3;

    // Fixed toggle (text inside, aligned like On toggle)
    auto fixedCol = knobRow.removeFromLeft(colW);
    fixedToggle.setBounds(fixedCol.withSizeKeepingCentre(fixedCol.getWidth(), 18));

    // Rate / Sync knob + label
    auto rateCol = knobRow.removeFromLeft(colW);
    auto rateLbl = rateCol.removeFromBottom(labelH);
    rateValueLabel.setBounds(rateLbl);
    syncValueLabel.setBounds(rateLbl);
    rateKnob.setBounds(rateCol);
    syncKnob.setBounds(rateCol);

    // Depth
    depthLabel.setBounds(knobRow.removeFromBottom(labelH));
    depthKnob.setBounds(knobRow);

    // Display
    area.removeFromBottom(2);
    shaperDisplay.setBounds(area);
}
