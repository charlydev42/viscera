// LFOSection.cpp — 3 tabbed assignable LFOs with Serum-style curve editor + learn mode
#include "LFOSection.h"
#include "ModSlider.h"
#include "../PluginProcessor.h"
#include "VisceraLookAndFeel.h"

// Safe parameter read helper — returns 0 if parameter doesn't exist
static float safeParamLoad(juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    if (auto* p = s.getRawParameterValue(id))
        return p->load();
    jassertfalse; // parameter not found — bug in param naming
    return 0.0f;
}

// ============================================================================
// LFOWaveDisplay — helpers
// ============================================================================

juce::Point<float> LFOWaveDisplay::pointToPixel(const bb::CurvePoint& pt,
                                                  juce::Rectangle<float> area) const
{
    return { area.getX() + pt.x * area.getWidth(),
             area.getY() + (1.0f - pt.y) * area.getHeight() };
}

bb::CurvePoint LFOWaveDisplay::pixelToPoint(juce::Point<float> px,
                                             juce::Rectangle<float> area) const
{
    float x = (px.x - area.getX()) / area.getWidth();
    float y = 1.0f - (px.y - area.getY()) / area.getHeight();
    return { juce::jlimit(0.0f, 1.0f, x), juce::jlimit(0.0f, 1.0f, y) };
}

// ============================================================================
// LFOWaveDisplay
// ============================================================================

LFOWaveDisplay::LFOWaveDisplay(int index) : lfoIdx(index)
{
    startTimerHz(30);
}

void LFOWaveDisplay::mouseDown(const juce::MouseEvent& e)
{
    bool isCustom = (waveType == static_cast<int>(bb::LFOWaveType::Custom));
    if (isCustom && lfoPtr != nullptr)
    {
        auto inner = getLocalBounds().toFloat().reduced(4.0f);
        auto& pts = lfoPtr->getCurvePoints();

        // Hit-test control points
        for (int i = 0; i < static_cast<int>(pts.size()); ++i)
        {
            auto px = pointToPixel(pts[i], inner);
            if (e.position.getDistanceFrom(px) <= kHitRadius)
            {
                dragPointIndex = i;
                isDraggingPoint = true;
                repaint();
                return;
            }
        }
        // No point hit — don't start D&D in custom mode
        return;
    }

    // Standard modes: nothing on mouseDown (D&D starts on drag)
}

void LFOWaveDisplay::mouseDrag(const juce::MouseEvent& e)
{
    bool isCustom = (waveType == static_cast<int>(bb::LFOWaveType::Custom));

    if (isCustom && isDraggingPoint && lfoPtr != nullptr)
    {
        auto inner = getLocalBounds().toFloat().reduced(4.0f);
        auto pts = lfoPtr->getCurvePoints();
        if (dragPointIndex < 0 || dragPointIndex >= static_cast<int>(pts.size()))
            return;

        auto newPt = pixelToPoint(e.position, inner);

        // Always update y
        pts[dragPointIndex].y = newPt.y;

        // Update x only for non-endpoint points, constrained between neighbors
        bool isFirst = (dragPointIndex == 0);
        bool isLast  = (dragPointIndex == static_cast<int>(pts.size()) - 1);
        if (!isFirst && !isLast)
        {
            float minX = pts[dragPointIndex - 1].x + 0.001f;
            float maxX = pts[dragPointIndex + 1].x - 0.001f;
            pts[dragPointIndex].x = juce::jlimit(minX, maxX, newPt.x);
        }

        lfoPtr->setCurvePoints(pts);
        repaint();
        return;
    }

    // Standard mode: drag & drop source
    if (!isCustom && e.getDistanceFromDragStart() > 4)
    {
        if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
        {
            ModSlider::showDropTargets = true;
            juce::String dragDesc = "LFO_" + juce::String(lfoIdx);
            container->startDragging(dragDesc, this);
        }
    }
}

void LFOWaveDisplay::mouseUp(const juce::MouseEvent&)
{
    isDraggingPoint = false;
    dragPointIndex = -1;
    ModSlider::showDropTargets = false;
    repaint();
}

void LFOWaveDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (lfoPtr == nullptr) return;

    bool isCustom = (waveType == static_cast<int>(bb::LFOWaveType::Custom));
    auto inner = getLocalBounds().toFloat().reduced(4.0f);

    if (!isCustom)
    {
        // Sample the current waveform shape into curve points
        constexpr int kSamplePoints = 17; // enough points to capture the shape
        std::vector<bb::CurvePoint> pts;
        for (int i = 0; i < kSamplePoints; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(kSamplePoints - 1);
            float val = 0.0f;
            switch (waveType)
            {
            case 0: // Sine
                val = std::sin(t * juce::MathConstants<float>::twoPi);
                val = (val + 1.0f) * 0.5f; // [-1,1] → [0,1]
                break;
            case 1: // Triangle
                val = 2.0f * std::abs(2.0f * t - 1.0f) - 1.0f;
                val = (val + 1.0f) * 0.5f;
                break;
            case 2: // Saw
                val = (2.0f * t - 1.0f + 1.0f) * 0.5f; // → t
                break;
            case 3: // Square
                val = (t < 0.5f) ? 1.0f : 0.0f;
                break;
            case 4: // S&H — just use flat midpoint
                val = 0.5f;
                break;
            default: val = 0.5f; break;
            }
            pts.push_back({ t, juce::jlimit(0.0f, 1.0f, val) });
        }

        // Add the clicked point
        auto clickPt = pixelToPoint(e.position, inner);
        pts.push_back(clickPt);

        // Set curve points (switches to custom shape internally)
        lfoPtr->setCurvePoints(pts);

        // Switch wave type to Custom via the combo/param
        if (onWaveChange) onWaveChange(static_cast<int>(bb::LFOWaveType::Custom));

        repaint();
        return;
    }

    // Already in custom mode
    auto pts = lfoPtr->getCurvePoints();

    // Check if double-clicking an existing point (not endpoints) — delete it
    for (int i = 1; i < static_cast<int>(pts.size()) - 1; ++i)
    {
        auto px = pointToPixel(pts[i], inner);
        if (e.position.getDistanceFrom(px) <= kHitRadius)
        {
            pts.erase(pts.begin() + i);
            lfoPtr->setCurvePoints(pts);
            repaint();
            return;
        }
    }

    // Double-click on empty space — add a new point
    auto newPt = pixelToPoint(e.position, inner);
    pts.push_back(newPt);
    lfoPtr->setCurvePoints(pts); // sorts automatically
    repaint();
}

void LFOWaveDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2);
    float w = bounds.getWidth();
    float h = bounds.getHeight();
    float midY = bounds.getCentreY();

    // Background
    g.setColour(juce::Colour(VisceraLookAndFeel::kDisplayBg));
    g.fillRoundedRectangle(bounds, 3.0f);

    auto lfoColor = juce::Colour(VisceraLookAndFeel::kAccentColor);

    bool isCustom = (waveType == static_cast<int>(bb::LFOWaveType::Custom));

    if (isCustom && lfoPtr != nullptr)
    {
        // --- Custom mode: Serum-style curve editor ---
        auto inner = bounds.reduced(2.0f);

        // Midline (bipolar zero at y=0.5)
        g.setColour(juce::Colour(VisceraLookAndFeel::kShadowLight).withAlpha(0.15f));
        float midLineY = inner.getY() + inner.getHeight() * 0.5f;
        g.drawHorizontalLine(static_cast<int>(midLineY), inner.getX(), inner.getRight());

        // Evaluate curve pixel by pixel using Catmull-Rom
        juce::Path curvePath;
        int numPx = static_cast<int>(inner.getWidth());
        for (int px = 0; px <= numPx; ++px)
        {
            float t = static_cast<float>(px) / static_cast<float>(numPx);
            float val = lfoPtr->evalCatmullRom(t); // [0,1]
            float y = inner.getY() + (1.0f - val) * inner.getHeight();
            float x = inner.getX() + static_cast<float>(px);
            if (px == 0)
                curvePath.startNewSubPath(x, y);
            else
                curvePath.lineTo(x, y);
        }

        // Fill under the curve
        {
            juce::Path fillPath(curvePath);
            fillPath.lineTo(inner.getRight(), inner.getBottom());
            fillPath.lineTo(inner.getX(), inner.getBottom());
            fillPath.closeSubPath();
            g.setColour(lfoColor.withAlpha(0.15f));
            g.fillPath(fillPath);
        }

        // Stroke the curve
        g.setColour(lfoColor.withAlpha(0.85f));
        g.strokePath(curvePath, juce::PathStrokeType(1.5f));

        // Draw control points (outline only, filled when dragging)
        auto& pts = lfoPtr->getCurvePoints();
        for (int i = 0; i < static_cast<int>(pts.size()); ++i)
        {
            auto pp = pointToPixel(pts[i], inner);
            bool active = (isDraggingPoint && i == dragPointIndex);
            float r = active ? kPointRadius * 1.3f : kPointRadius;
            if (active)
            {
                g.setColour(lfoColor.withAlpha(0.4f));
                g.fillEllipse(pp.x - r, pp.y - r, r * 2.0f, r * 2.0f);
            }
            g.setColour(lfoColor.withAlpha(0.9f));
            g.drawEllipse(pp.x - r, pp.y - r, r * 2.0f, r * 2.0f, 1.2f);
        }
    }
    else
    {
        // --- Standard mode: draw waveform path ---
        juce::Path wavePath;
        for (int px = 0; px < static_cast<int>(w); ++px)
        {
            float p = static_cast<float>(px) / w;
            float val = 0.0f;

            switch (waveType)
            {
            case 0: // Sine
                val = std::sin(p * juce::MathConstants<float>::twoPi);
                break;
            case 1: // Triangle
                val = 2.0f * std::abs(2.0f * p - 1.0f) - 1.0f;
                break;
            case 2: // Saw
                val = 2.0f * p - 1.0f;
                break;
            case 3: // Square
                val = (p < 0.5f) ? 1.0f : -1.0f;
                break;
            case 4: // S&H
            {
                int steps = 8;
                int step = static_cast<int>(p * steps);
                uint32_t seed = static_cast<uint32_t>(step * 2654435761u + lfoIdx * 17u);
                seed ^= seed >> 16;
                val = static_cast<float>(static_cast<int32_t>(seed)) / static_cast<float>(INT32_MAX);
                break;
            }
            default: break;
            }

            float y = midY - val * (h * 0.4f);
            if (px == 0)
                wavePath.startNewSubPath(bounds.getX() + static_cast<float>(px), y);
            else
                wavePath.lineTo(bounds.getX() + static_cast<float>(px), y);
        }

        g.setColour(lfoColor.withAlpha(0.7f));
        g.strokePath(wavePath, juce::PathStrokeType(1.5f));
    }

    // Phase cursor
    float cursorX = bounds.getX() + phase * w;
    g.setColour(juce::Colour(VisceraLookAndFeel::kShadowLight).withAlpha(0.6f));
    g.drawLine(cursorX, bounds.getY(), cursorX, bounds.getBottom(), 1.0f);

    // Border
    g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

// ============================================================================
// LFOSection
// ============================================================================

static const juce::StringArray kDestNames {
    "None", "Pitch", "Cutoff", "Res", "Mod1Lvl", "Mod2Lvl",
    "Volume", "Drive", "Noise", "Spread", "Fold",
    "M1Fine", "M2Fine", "Drift", "CarFine",
    "DlyTime", "DlyFeed", "DlyMix",
    "RevSize", "RevMix",
    "LiqDpth", "LiqMix",
    "RubWarp", "RubMix",
    "PEnvAmt",
    "RevDamp", "RevWdth", "RevPdly",
    "DlyDamp", "DlySprd",
    "LiqRate", "LiqTone", "LiqFeed",
    "RubTone", "RubStr", "RubFeed",
    "Porta",
    "E1A", "E1D", "E1S", "E1R",
    "E2A", "E2D", "E2S", "E2R",
    "E3A", "E3D", "E3S", "E3R",
    "PEA", "PED", "PES", "PER",
    "ShpRate", "ShpDep",
    "M1Coar", "M2Coar", "CCoar",
    "Tremor", "Vein", "Flux"
};

// --- RefreshButton ---
void LFOSection::RefreshButton::paint(juce::Graphics& g)
{
    g.addTransform(juce::AffineTransform::rotation(
        juce::MathConstants<float>::halfPi,
        getWidth() * 0.5f, getHeight() * 0.5f));
    g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor));
    g.setFont(juce::Font(18.0f));
    g.drawText(juce::String::charToString(0x21BB), getLocalBounds(),
               juce::Justification::centred);
}

void LFOSection::RefreshButton::mouseUp(const juce::MouseEvent& e)
{
    if (onClick && getLocalBounds().toFloat().contains(e.position))
        onClick();
}

// --- LFOSection ---
LFOSection::LFOSection(juce::AudioProcessorValueTreeState& apvts, VisceraProcessor& proc)
    : state(apvts), processor(proc), waveDisplay(0)
{
    syncNames = { "8 bar", "4 bar", "2 bar", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32",
                  "1/4T", "1/8T", "1/16T" };

    // Tab buttons — just "1", "2", "3"
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i].setButtonText(juce::String(i + 1));
        tabButtons[i].setClickingTogglesState(false);
        tabButtons[i].setName(""); // prevent JUCE from showing component name as ghost text
        tabButtons[i].setTooltip("");
        tabButtons[i].setPaintingIsUnclipped(true);
        tabButtons[i].onClick = [this, i] { switchTab(i); };
        tabButtons[i].addMouseListener(this, false); // for drag to assign
        addAndMakeVisible(tabButtons[i]);
    }

    // Wave combo (6 options including Custom)
    waveCombo.addItemList({ "Sine", "Tri", "Saw", "Sq", "S&H", "Custom" }, 1);
    addAndMakeVisible(waveCombo);

    // Rate knob
    rateKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    rateKnob.setSliderSnapsToMousePosition(false);
    rateKnob.setMouseDragSensitivity(200);
    rateKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(rateKnob);

    rateLabel.setJustificationType(juce::Justification::centred);
    rateLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(rateLabel);

    // Fixed toggle (sync on/off)
    fixedToggle.setButtonText("");
    fixedToggle.onClick = [this] {
        if (fixedToggle.getToggleState())
            setSyncParam(lastSyncIdx);
        else
        {
            int cur = getSyncParam();
            if (cur > 0) lastSyncIdx = cur;
            setSyncParam(0);
        }
        updateSyncDisplay();
        resized();
    };
    addAndMakeVisible(fixedToggle);

    // Sync knob (fixed mode)
    syncKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    syncKnob.setSliderSnapsToMousePosition(false);
    syncKnob.setMouseDragSensitivity(200);
    syncKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    syncKnob.setRange(1.0, 12.0, 1.0);
    syncKnob.onValueChange = [this] {
        int idx = static_cast<int>(syncKnob.getValue());
        setSyncParam(idx);
        lastSyncIdx = idx;
        updateSyncDisplay();
    };
    addAndMakeVisible(syncKnob);

    syncValueLabel.setJustificationType(juce::Justification::centred);
    syncValueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    addAndMakeVisible(syncValueLabel);

    // Wave display
    addAndMakeVisible(waveDisplay);

    // Callback: double-click on non-custom waveform → switch wave param to Custom
    waveDisplay.onWaveChange = [this](int newWaveType) {
        auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
        if (auto* p = state.getParameter(pfx + "WAVE"))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(newWaveType)));
    };

    // Reset custom curve button
    resetCurveBtn.onClick = [this] {
        auto& lfo = processor.getGlobalLFO(activeTab);
        lfo.setCurvePoints({ {0.0f, 0.5f}, {1.0f, 0.5f} });
        waveDisplay.repaint();
    };
    addAndMakeVisible(resetCurveBtn);

    // Slot buttons & clear buttons — hidden, managed internally
    for (int i = 0; i < kNumSlots; ++i)
    {
        slotButtons[i].setVisible(false);
        slotClearBtns[i].setVisible(false);
    }

    // "+" button — click enters learn mode + shows drop targets, right-click shows assignments popup
    addSlotBtn.setButtonText("+");
    addSlotBtn.setName("lfoSlot");
    addSlotBtn.onClick = [this] {
        if (learnSlotIndex >= 0) { cancelLearnMode(); ModSlider::showDropTargets = false; return; }
        auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
        for (int i = 0; i < kNumSlots; ++i)
        {
            int dest = static_cast<int>(safeParamLoad(state, pfx + "DEST" + juce::String(i + 1)));
            if (dest == 0)
            {
                ModSlider::showDropTargets = true;
                enterLearnMode(i);
                return;
            }
        }
    };
    addSlotBtn.addMouseListener(this, false);
    addAndMakeVisible(addSlotBtn);

    // "-" button — remove last LFO assignment on active tab
    removeSlotBtn.setButtonText("-");
    removeSlotBtn.setName("lfoSlot");
    removeSlotBtn.onClick = [this] {
        auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
        for (int i = kNumSlots - 1; i >= 0; --i)
        {
            int dest = static_cast<int>(safeParamLoad(state, pfx + "DEST" + juce::String(i + 1)));
            if (dest > 0)
            {
                auto destId = pfx + "DEST" + juce::String(i + 1);
                auto amtId  = pfx + "AMT"  + juce::String(i + 1);
                if (auto* dp = state.getParameter(destId))
                    dp->setValueNotifyingHost(dp->convertTo0to1(0.0f));
                if (auto* ap = state.getParameter(amtId))
                    ap->setValueNotifyingHost(ap->convertTo0to1(0.0f));
                break;
            }
        }
    };
    addAndMakeVisible(removeSlotBtn);

    // Count label
    countLabel.setJustificationType(juce::Justification::centred);
    countLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    countLabel.setColour(juce::Label::textColourId, juce::Colour(VisceraLookAndFeel::kTextColor).withAlpha(0.5f));
    addAndMakeVisible(countLabel);

    // Hint label — right-justified, always visible
    hintLabel.setText("drag to knob to assign", juce::dontSendNotification);
    hintLabel.setJustificationType(juce::Justification::centredRight);
    hintLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::italic));
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(VisceraLookAndFeel::kTextColor).withAlpha(0.3f));
    addAndMakeVisible(hintLabel);

    // Initial tab
    switchTab(0);

    setWantsKeyboardFocus(true);
    startTimerHz(10);
}

LFOSection::~LFOSection()
{
    cancelLearnMode();
    waveAttach.reset();
    rateAttach.reset();
}

int LFOSection::getSyncParam() const
{
    auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
    return static_cast<int>(safeParamLoad(state, pfx + "SYNC"));
}

void LFOSection::setSyncParam(int idx)
{
    auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
    if (auto* p = state.getParameter(pfx + "SYNC"))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
}

void LFOSection::switchTab(int tab)
{
    cancelLearnMode();
    activeTab = juce::jlimit(0, 2, tab);

    auto accentCol = juce::Colour(VisceraLookAndFeel::kAccentColor);

    for (int i = 0; i < 3; ++i)
    {
        bool active = (i == activeTab);
        tabButtons[i].setColour(juce::TextButton::buttonColourId,
            active ? accentCol.withAlpha(0.6f)
                   : juce::Colour(VisceraLookAndFeel::kPanelColor));
    }

    // Detach old, reattach to new LFO params
    waveAttach.reset();
    rateAttach.reset();

    auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
    waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, pfx + "WAVE", waveCombo);
    rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, pfx + "RATE", rateKnob);

    waveDisplay.setLFOIndex(activeTab);
    waveDisplay.setLFOPointer(&processor.getGlobalLFO(activeTab));

    // Init sync state from param
    int syncIdx = getSyncParam();
    fixedToggle.setToggleState(syncIdx > 0, juce::dontSendNotification);
    if (syncIdx > 0)
    {
        lastSyncIdx = syncIdx;
        syncKnob.setValue(syncIdx, juce::dontSendNotification);
    }

    updateAssignmentLabels();
    updateSyncDisplay();
    repaint();
}

void LFOSection::timerCallback()
{
    waveDisplay.setPhase(processor.getGlobalLFOPhase(activeTab));

    auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
    int waveType = static_cast<int>(safeParamLoad(state, pfx + "WAVE"));
    waveDisplay.setWaveType(waveType);

    // Show reset button only in Custom mode
    resetCurveBtn.setVisible(waveType == static_cast<int>(bb::LFOWaveType::Custom));

    updateSyncDisplay();
    updateAssignmentLabels();

    // Pulse the "+" button when in assignment mode (learn or drag)
    if (ModSlider::showDropTargets || learnSlotIndex >= 0)
    {
        float pulse = 0.5f + 0.3f * std::sin(
            static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.004));
        addSlotBtn.setColour(juce::TextButton::buttonColourId,
            juce::Colour(VisceraLookAndFeel::kAccentColor).withAlpha(pulse));
        addSlotBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        addSlotBtn.setColour(juce::TextButton::buttonColourId,
            juce::Colour(VisceraLookAndFeel::kPanelColor));
        addSlotBtn.setColour(juce::TextButton::textColourOffId,
            juce::Colour(VisceraLookAndFeel::kTextColor));
    }

    // Auto-cancel learn mode if we lost focus
    if (learnSlotIndex >= 0 && !hasKeyboardFocus(true))
        cancelLearnMode();
}

void LFOSection::updateSyncDisplay()
{
    int syncIdx = getSyncParam();
    bool isFixed = syncIdx > 0;

    rateKnob.setVisible(!isFixed);
    rateLabel.setVisible(!isFixed);
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
        auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
        float rate = safeParamLoad(state, pfx + "RATE");
        rateLabel.setText(juce::String(rate, 1) + "Hz", juce::dontSendNotification);
    }

    // Sync the fixedToggle state if param changed externally
    if (fixedToggle.getToggleState() != isFixed)
        fixedToggle.setToggleState(isFixed, juce::dontSendNotification);
    if (isFixed && static_cast<int>(syncKnob.getValue()) != syncIdx)
        syncKnob.setValue(syncIdx, juce::dontSendNotification);
}

void LFOSection::updateAssignmentLabels()
{
    auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
    int numMapped = 0;

    for (int s = 0; s < kNumSlots; ++s)
    {
        int dest = static_cast<int>(safeParamLoad(state, pfx + "DEST" + juce::String(s + 1)));
        if (dest > 0 && dest < kDestNames.size())
            ++numMapped;
    }

    // "+" button: "..." during learn, hidden when all slots full
    addSlotBtn.setButtonText(learnSlotIndex >= 0 ? "..." : "+");
    addSlotBtn.setVisible(numMapped < kNumSlots || learnSlotIndex >= 0);

    // "-" button: visible when at least one assignment exists
    removeSlotBtn.setVisible(numMapped > 0);

    // Count + hint
    countLabel.setText(juce::String(numMapped), juce::dontSendNotification);

    layoutSlots();
}

void LFOSection::showSlotPopup(int /*slotIdx*/) {}

void LFOSection::layoutSlots()
{
    if (slotArea.isEmpty()) return;
    auto row = slotArea;

    // [+] [-] [count]                    [hint right-justified]
    if (addSlotBtn.isVisible())
        addSlotBtn.setBounds(row.removeFromLeft(20).reduced(1, 0));

    if (removeSlotBtn.isVisible())
        removeSlotBtn.setBounds(row.removeFromLeft(20).reduced(1, 0));

    countLabel.setBounds(row.removeFromLeft(14));
    hintLabel.setBounds(row);
}

// ============================================================================
// Learn Mode
// ============================================================================

void LFOSection::enterLearnMode(int slotIdx)
{
    learnSlotIndex = slotIdx;
    grabKeyboardFocus();

    int capturedTab = activeTab;
    int capturedSlot = slotIdx;

    ModSlider::onLearnClick = [this, capturedTab, capturedSlot](bb::LFODest dest)
    {
        // Policy: max 1 LFO per knob — remove any existing assignment from ANY LFO
        for (int l = 0; l < 3; ++l)
        {
            auto otherPfx = "LFO" + juce::String(l + 1) + "_";
            for (int s = 1; s <= kNumSlots; ++s)
            {
                auto dId = otherPfx + "DEST" + juce::String(s);
                auto aId = otherPfx + "AMT"  + juce::String(s);
                int curDest = static_cast<int>(safeParamLoad(state, dId));
                if (curDest == static_cast<int>(dest))
                {
                    if (auto* dp = state.getParameter(dId))
                        dp->setValueNotifyingHost(dp->convertTo0to1(0.0f));
                    if (auto* ap = state.getParameter(aId))
                        ap->setValueNotifyingHost(ap->convertTo0to1(0.0f));
                }
            }
        }

        // Assign this dest to the captured slot
        auto pfx = "LFO" + juce::String(capturedTab + 1) + "_";
        auto destId = pfx + "DEST" + juce::String(capturedSlot + 1);
        auto amtId  = pfx + "AMT"  + juce::String(capturedSlot + 1);

        if (auto* dp = state.getParameter(destId))
            dp->setValueNotifyingHost(dp->convertTo0to1(static_cast<float>(static_cast<int>(dest))));
        if (auto* ap = state.getParameter(amtId))
            ap->setValueNotifyingHost(ap->convertTo0to1(0.5f)); // default amount 0.5

        cancelLearnMode();
    };

    updateAssignmentLabels();
}

void LFOSection::cancelLearnMode()
{
    learnSlotIndex = -1;
    ModSlider::onLearnClick = nullptr;
    ModSlider::showDropTargets = false;
    updateAssignmentLabels();
}

bool LFOSection::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && learnSlotIndex >= 0)
    {
        cancelLearnMode();
        return true;
    }
    return false;
}

void LFOSection::mouseDown(const juce::MouseEvent& e)
{
    // Tab buttons: start drag on left click drag (handled in mouseDrag)
    // Right-click on "+" → show assignments popup to remove
    if (e.mods.isPopupMenu() && e.eventComponent == &addSlotBtn)
    {
        showAssignmentsPopup();
        return;
    }
}

void LFOSection::mouseDrag(const juce::MouseEvent& e)
{
    // Drag from tab buttons → start LFO drag & drop
    for (int i = 0; i < 3; ++i)
    {
        if (e.eventComponent == &tabButtons[i] && e.getDistanceFromDragStart() > 4)
        {
            if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
            {
                ModSlider::showDropTargets = true;
                juce::String dragDesc = "LFO_" + juce::String(i);
                container->startDragging(dragDesc, &tabButtons[i]);
            }
            return;
        }
    }
}

void LFOSection::mouseUp(const juce::MouseEvent& e)
{
    // Clear drop targets when tab drag ends
    for (int i = 0; i < 3; ++i)
    {
        if (e.eventComponent == &tabButtons[i])
        {
            ModSlider::showDropTargets = false;
            return;
        }
    }
}

void LFOSection::showAssignmentsPopup()
{
    auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
    juce::PopupMenu menu;
    int itemId = 1;

    for (int s = 0; s < kNumSlots; ++s)
    {
        int dest = static_cast<int>(safeParamLoad(state, pfx + "DEST" + juce::String(s + 1)));
        if (dest > 0 && dest < kDestNames.size())
        {
            float amt = safeParamLoad(state, pfx + "AMT" + juce::String(s + 1));
            auto amtStr = juce::String(static_cast<int>(amt * 100.0f));
            menu.addItem(itemId + s,
                juce::String::charToString(0x2716) + "  " + kDestNames[dest]
                + "  " + amtStr + "%");
        }
    }

    if (menu.getNumItems() == 0)
    {
        menu.addItem(-1, "No assignments", false, false);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addSlotBtn));
        return;
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addSlotBtn),
        [this, pfx](int result) {
            if (result <= 0) return;
            int slot = result - 1;
            auto destId = pfx + "DEST" + juce::String(slot + 1);
            auto amtId  = pfx + "AMT"  + juce::String(slot + 1);
            if (auto* dp = state.getParameter(destId))
                dp->setValueNotifyingHost(dp->convertTo0to1(0.0f));
            if (auto* ap = state.getParameter(amtId))
                ap->setValueNotifyingHost(ap->convertTo0to1(0.0f));
        });
}

// ============================================================================
// Paint — accent underline on active tab
// ============================================================================

void LFOSection::paint(juce::Graphics& g)
{
    auto accent = juce::Colour(VisceraLookAndFeel::kAccentColor);
    auto tabBounds = tabButtons[activeTab].getBounds().toFloat();
    // Small accent bar below the active tab
    g.setColour(accent);
    g.fillRoundedRectangle(tabBounds.getX() + 2.0f, tabBounds.getBottom(),
                           tabBounds.getWidth() - 4.0f, 2.0f, 1.0f);
}

// ============================================================================
// Layout
// ============================================================================

void LFOSection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(2); // minimal padding (header drawn outside by PluginEditor)

    // Top row: [tabs] [wave combo] [reset]  ...  [fixed] [knob] [label]
    auto topRow = area.removeFromTop(22);
    for (int i = 0; i < 3; ++i)
        tabButtons[i].setBounds(topRow.removeFromLeft(22));
    topRow.removeFromLeft(4);

    // Right-justify: rate/sync controls (knob flush against label)
    auto lblArea = topRow.removeFromRight(36);
    rateLabel.setBounds(lblArea);
    syncValueLabel.setBounds(lblArea);
    int knobSize = 22;
    auto knobArea = topRow.removeFromRight(knobSize);
    rateKnob.setBounds(knobArea);
    syncKnob.setBounds(knobArea);
    topRow.removeFromRight(2);
    auto toggleArea = topRow.removeFromRight(18);
    fixedToggle.setBounds(toggleArea.withSizeKeepingCentre(18, 18));
    topRow.removeFromRight(4);

    // Left side: wave combo + reset button
    resetCurveBtn.setBounds(topRow.removeFromRight(18));
    waveCombo.setBounds(topRow);

    area.removeFromTop(2);

    // Bottom: dynamic slot pills + "+" button
    area.removeFromBottom(1);
    slotArea = area.removeFromBottom(16);

    // Remaining: wave display
    waveDisplay.setBounds(area);

    layoutSlots();
}
