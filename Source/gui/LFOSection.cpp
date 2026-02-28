// LFOSection.cpp — 3 tabbed assignable LFOs with Serum-style curve editor + learn mode
#include "LFOSection.h"
#include "ModSlider.h"
#include "../PluginProcessor.h"
#include "VisceraLookAndFeel.h"

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
            juce::String dragDesc = "LFO_" + juce::String(lfoIdx);
            container->startDragging(dragDesc, this);
        }
    }
}

void LFOWaveDisplay::mouseUp(const juce::MouseEvent&)
{
    isDraggingPoint = false;
    dragPointIndex = -1;
    repaint();
}

void LFOWaveDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
    bool isCustom = (waveType == static_cast<int>(bb::LFOWaveType::Custom));
    if (!isCustom || lfoPtr == nullptr) return;

    auto inner = getLocalBounds().toFloat().reduced(4.0f);
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
    "Porta"
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
        tabButtons[i].onClick = [this, i] { switchTab(i); };
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

    // Reset custom curve button
    resetCurveBtn.onClick = [this] {
        auto& lfo = processor.getGlobalLFO(activeTab);
        lfo.setCurvePoints({ {0.0f, 0.5f}, {1.0f, 0.5f} });
        waveDisplay.repaint();
    };
    addAndMakeVisible(resetCurveBtn);

    // Slot buttons: show dest name or "+" for learn, click enters learn mode
    for (int i = 0; i < 4; ++i)
    {
        slotButtons[i].setClickingTogglesState(false);
        slotButtons[i].setName("lfoSlot");
        slotButtons[i].onClick = [this, i] {
            if (learnSlotIndex == i)
            {
                cancelLearnMode();
                return;
            }
            auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
            int dest = static_cast<int>(state.getRawParameterValue(pfx + "DEST" + juce::String(i + 1))->load());
            if (dest == 0)
                enterLearnMode(i);
        };
        addAndMakeVisible(slotButtons[i]);

        // Clear button — overlaid inside slot pill, no background
        slotClearBtns[i].setButtonText("x");
        slotClearBtns[i].setColour(juce::TextButton::buttonColourId,
                                    juce::Colours::transparentBlack);
        slotClearBtns[i].onClick = [this, i] {
            auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
            auto destId = pfx + "DEST" + juce::String(i + 1);
            auto amtId  = pfx + "AMT"  + juce::String(i + 1);
            if (auto* dp = state.getParameter(destId))
                dp->setValueNotifyingHost(dp->convertTo0to1(0.0f));
            if (auto* ap = state.getParameter(amtId))
                ap->setValueNotifyingHost(ap->convertTo0to1(0.0f));
        };
        addAndMakeVisible(slotClearBtns[i]);

        // Listen for right-click on slot button
        slotButtons[i].addMouseListener(this, false);
    }

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
    return static_cast<int>(state.getRawParameterValue(pfx + "SYNC")->load());
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
    int waveType = static_cast<int>(state.getRawParameterValue(pfx + "WAVE")->load());
    waveDisplay.setWaveType(waveType);

    // Show reset button only in Custom mode
    resetCurveBtn.setVisible(waveType == static_cast<int>(bb::LFOWaveType::Custom));

    updateSyncDisplay();
    updateAssignmentLabels();

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
        float rate = state.getRawParameterValue(pfx + "RATE")->load();
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

    for (int s = 0; s < 4; ++s)
    {
        int dest = static_cast<int>(state.getRawParameterValue(pfx + "DEST" + juce::String(s + 1))->load());

        if (dest > 0 && dest < kDestNames.size())
        {
            slotButtons[s].setButtonText(kDestNames[dest]);
            slotClearBtns[s].setVisible(true);
        }
        else if (learnSlotIndex == s)
        {
            slotButtons[s].setButtonText("...");
            slotClearBtns[s].setVisible(false);
        }
        else
        {
            slotButtons[s].setButtonText("+");
            slotClearBtns[s].setVisible(false);
        }
    }
}

void LFOSection::showSlotPopup(int /*slotIdx*/)
{
    // No longer used — slots now show just name + x to clear
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
    if (!e.mods.isPopupMenu()) return;

    // Right-click on a slot button → clear that slot
    for (int i = 0; i < 4; ++i)
    {
        if (e.eventComponent == &slotButtons[i])
        {
            auto pfx = "LFO" + juce::String(activeTab + 1) + "_";
            auto destId = pfx + "DEST" + juce::String(i + 1);
            auto amtId  = pfx + "AMT"  + juce::String(i + 1);
            if (auto* dp = state.getParameter(destId))
                dp->setValueNotifyingHost(dp->convertTo0to1(0.0f));
            if (auto* ap = state.getParameter(amtId))
                ap->setValueNotifyingHost(ap->convertTo0to1(0.0f));
            return;
        }
    }
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

    // Bottom row: 4 slot cells — "x" sits inside the pill
    auto bottomRow = area.removeFromBottom(18);
    int cellW = bottomRow.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = bottomRow.removeFromLeft(cellW).reduced(1, 0);
        slotButtons[i].setBounds(cell);
        // "x" overlaid flush-right inside the pill
        slotClearBtns[i].setBounds(cell.withLeft(cell.getRight() - 16));
        slotClearBtns[i].toFront(false);
    }

    // Remaining: wave display
    waveDisplay.setBounds(area);
}
