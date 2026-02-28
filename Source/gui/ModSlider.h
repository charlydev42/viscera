// ModSlider.h — Slider subclass with Serum-style modulation ring, drag-to-set-amount, LFO D&D + learn mode
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/FMVoice.h" // for bb::LFODest, bb::VoiceParams
#include "VisceraLookAndFeel.h"
#include <functional>

class ModSlider : public juce::Slider,
                  public juce::DragAndDropTarget,
                  private juce::Timer
{
public:
    ModSlider() = default;

    // Static learn mode callback — when non-null, next click on a ModSlider calls it
    static inline std::function<void(bb::LFODest)> onLearnClick = nullptr;

    // Static pointer to VoiceParams for reading live LFO modulation values
    static inline const bb::VoiceParams* voiceParamsPtr = nullptr;

    bb::LFODest getDest() const { return myDest; }
    bool isMapped = false;  // true when at least one LFO targets this knob

    // Call after construction to enable LFO modulation overlay
    void initMod(juce::AudioProcessorValueTreeState& apvts, bb::LFODest dest)
    {
        statePtr = &apvts;
        myDest = dest;
        setPaintingIsUnclipped(true); // allow arcs to extend slightly beyond bounds
        startTimerHz(20);
    }

    bool hasModInit() const { return statePtr != nullptr; }

    // --- DragAndDropTarget ---
    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        if (!statePtr) return false;
        return details.description.toString().startsWith("LFO_");
    }

    void itemDragEnter(const SourceDetails&) override { dragHover = true; repaint(); }
    void itemDragExit(const SourceDetails&) override  { dragHover = false; repaint(); }

    void itemDropped(const SourceDetails& details) override
    {
        dragHover = false;
        if (!statePtr) return;
        int lfoIdx = details.description.toString().getTrailingIntValue();
        if (lfoIdx < 0 || lfoIdx > 2) return;

        auto pfx = "LFO" + juce::String(lfoIdx + 1) + "_";

        // Check if already assigned
        for (int s = 1; s <= 4; ++s)
        {
            auto destId = pfx + "DEST" + juce::String(s);
            int curDest = static_cast<int>(statePtr->getRawParameterValue(destId)->load());
            if (curDest == static_cast<int>(myDest))
                return;
        }

        for (int s = 1; s <= 4; ++s)
        {
            auto destId = pfx + "DEST" + juce::String(s);
            auto amtId  = pfx + "AMT"  + juce::String(s);
            int curDest = static_cast<int>(statePtr->getRawParameterValue(destId)->load());

            if (curDest == 0)
            {
                auto* destParam = statePtr->getParameter(destId);
                destParam->setValueNotifyingHost(
                    destParam->convertTo0to1(static_cast<float>(static_cast<int>(myDest))));
                auto* amtParam = statePtr->getParameter(amtId);
                amtParam->setValueNotifyingHost(amtParam->convertTo0to1(0.5f));
                repaint();
                return;
            }
        }
    }

    // --- Paint overlay (Serum-style) ---
    void paint(juce::Graphics& g) override
    {
        juce::Slider::paint(g);
        if (!statePtr) return;

        // Drag hover highlight
        if (dragHover)
        {
            g.setColour(juce::Colour(VisceraLookAndFeel::kShadowLight).withAlpha(0.15f));
            g.fillEllipse(getLocalBounds().toFloat().reduced(2));
        }

        // Learn mode: subtle outline on mappable knobs
        if (onLearnClick)
        {
            g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor).withAlpha(0.4f));
            g.drawEllipse(getLocalBounds().toFloat().reduced(3.0f), 1.0f);
        }

        static const juce::Colour lfoColors[] = {
            juce::Colour(0xFF8BC34A), // LFO1 — #8BC34A
            juce::Colour(0xFF8BC34A), // LFO2 — #8BC34A
            juce::Colour(0xFF8BC34A)  // LFO3 — #8BC34A
        };

        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto centre = bounds.getCentre();
        float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;

        // JUCE uses two different angle conventions:
        // - addCentredArc() uses raw rotary angles (0 = top, clockwise)
        // - cos/sin need screen angles: screenAngle = rotaryAngle - halfPi
        auto rp = getRotaryParameters();
        constexpr float hPi = juce::MathConstants<float>::halfPi;
        float rotStart  = rp.startAngleRadians;          // for addCentredArc
        float rotEnd    = rp.endAngleRadians;             // for addCentredArc
        float rotRange  = rotEnd - rotStart;

        // Knob normalized position [0,1] — skew-aware
        float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
        float baseRotAngle = rotStart + proportion * rotRange;  // rotary-space

        // --- Couche 1: Range arcs (Serum-style, anchored to knob position) ---
        float arcR = radius - 3.0f;

        // Collect arc bounds for ghost tick clamping (in rotary space)
        float ghostClampMin = baseRotAngle;
        float ghostClampMax = baseRotAngle;

        for (int l = 0; l < 3; ++l)
        {
            auto pfx = "LFO" + juce::String(l + 1) + "_";
            for (int s = 1; s <= 4; ++s)
            {
                auto destId = pfx + "DEST" + juce::String(s);
                auto amtId  = pfx + "AMT"  + juce::String(s);
                auto* destRaw = statePtr->getRawParameterValue(destId);
                if (!destRaw) continue;
                int dest = static_cast<int>(destRaw->load());
                if (dest != static_cast<int>(myDest)) continue;

                float amt = statePtr->getRawParameterValue(amtId)->load();
                auto col = lfoColors[l];

                // Scale arc by actual LFO peak (custom curves may not reach 1.0)
                float peak = voiceParamsPtr
                    ? voiceParamsPtr->lfoPeak[l].load(std::memory_order_relaxed)
                    : 1.0f;
                // Arc endpoint in rotary space, clamped to knob range
                float arcEnd = juce::jlimit(rotStart, rotEnd,
                                             baseRotAngle + amt * peak * rotRange);

                // Track total arc extent for ghost clamping
                ghostClampMin = std::min(ghostClampMin, arcEnd);
                ghostClampMax = std::max(ghostClampMax, arcEnd);

                // addCentredArc uses raw rotary angles (same convention as JUCE LookAndFeel)
                juce::Path arc;
                float a1 = std::min(baseRotAngle, arcEnd);
                float a2 = std::max(baseRotAngle, arcEnd);
                if (a2 - a1 > 0.01f)
                {
                    arc.addCentredArc(centre.x, centre.y, arcR, arcR, 0,
                                      a1, a2, true);
                    // Soft bloom halo
                    g.setColour(col.withAlpha(0.08f));
                    g.strokePath(arc, juce::PathStrokeType(5.0f));
                    // Core — sharp
                    g.setColour(col.withAlpha(0.85f));
                    g.strokePath(arc, juce::PathStrokeType(2.0f));
                    // Hot center
                    g.setColour(col.brighter(0.5f).withAlpha(0.35f));
                    g.strokePath(arc, juce::PathStrokeType(0.8f));
                }
            }
        }

        // --- Couche 2: Ghost tick (live modulated position, clamped to arc extent) ---
        float modVal = getModValue();
        if (std::abs(modVal) > 0.001f && ghostClampMin != ghostClampMax)
        {
            // Clamp ghost to the combined arc extent (rotary space)
            float modRotAngle = juce::jlimit(ghostClampMin, ghostClampMax,
                                              baseRotAngle + modVal * rotRange);

            // Convert to screen space for cos/sin (subtract halfPi)
            float screenAngle = modRotAngle - hPi;
            float innerR = radius * 0.7f;
            float outerR = arcR + 2.0f;
            float cosA = std::cos(screenAngle);
            float sinA = std::sin(screenAngle);
            auto tickCol = juce::Colour(VisceraLookAndFeel::kAccentColor);
            // Bloom halo
            g.setColour(tickCol.withAlpha(0.08f));
            g.drawLine(centre.x + cosA * innerR, centre.y + sinA * innerR,
                       centre.x + cosA * outerR, centre.y + sinA * outerR, 4.0f);
            // Core — sharp
            g.setColour(tickCol.withAlpha(0.75f));
            g.drawLine(centre.x + cosA * innerR, centre.y + sinA * innerR,
                       centre.x + cosA * outerR, centre.y + sinA * outerR, 1.6f);
            // Hot
            g.setColour(tickCol.brighter(0.5f).withAlpha(0.35f));
            g.drawLine(centre.x + cosA * innerR, centre.y + sinA * innerR,
                       centre.x + cosA * outerR, centre.y + sinA * outerR, 0.7f);
        }
    }

    // --- Mouse handling: ring drag + learn mode + context menu ---
    void mouseDown(const juce::MouseEvent& e) override
    {
        // Learn mode intercept
        if (onLearnClick && statePtr)
        {
            onLearnClick(myDest);
            return;
        }

        // Context menu on right-click
        if (statePtr && e.mods.isPopupMenu())
        {
            showContextMenu();
            return;
        }

        // Check for ring drag (click near an arc endpoint)
        if (statePtr && hitTestRingDrag(e.position))
            return;

        juce::Slider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isRingDrag)
        {
            // Combined X+Y drag: right/up = increase, left/down = decrease
            float dx = static_cast<float>(e.getDistanceFromDragStartX());
            float dy = static_cast<float>(-e.getDistanceFromDragStartY()); // invert Y (up = positive)
            float delta = (std::abs(dx) > std::abs(dy)) ? dx : dy;
            float sensitivity = 1.0f / static_cast<float>(getWidth() * 2);
            float newAmt = ringDragStartAmt + delta * sensitivity * 2.0f;
            newAmt = juce::jlimit(-1.0f, 1.0f, newAmt);

            auto amtId = "LFO" + juce::String(ringDragLfo + 1) + "_AMT" + juce::String(ringDragSlot);
            if (auto* p = statePtr->getParameter(amtId))
                p->setValueNotifyingHost(p->convertTo0to1(newAmt));
            repaint();
            return;
        }
        juce::Slider::mouseDrag(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (isRingDrag)
        {
            isRingDrag = false;
            ringDragLfo = -1;
            ringDragSlot = -1;
            repaint();
            return;
        }
        juce::Slider::mouseUp(e);
    }

private:
    juce::AudioProcessorValueTreeState* statePtr = nullptr;
    bb::LFODest myDest = bb::LFODest::None;
    bool dragHover = false;

    // Ring drag state
    bool isRingDrag = false;
    int ringDragLfo = -1;   // 0-2
    int ringDragSlot = -1;  // 0-3
    float ringDragStartAmt = 0.0f;

    void timerCallback() override
    {
        // Update mapped flag
        bool mapped = false;
        if (statePtr && myDest != bb::LFODest::None)
        {
            for (int l = 0; l < 3 && !mapped; ++l)
            {
                auto pfx = "LFO" + juce::String(l + 1) + "_";
                for (int s = 1; s <= 4 && !mapped; ++s)
                {
                    auto* raw = statePtr->getRawParameterValue(pfx + "DEST" + juce::String(s));
                    if (raw && static_cast<int>(raw->load()) == static_cast<int>(myDest))
                        mapped = true;
                }
            }
        }
        isMapped = mapped;
        repaint();
    }

    // --- Ring drag helpers ---
    bool hitTestRingDrag(juce::Point<float> pos)
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto centre = bounds.getCentre();
        float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
        float dist = pos.getDistanceFrom(centre);

        // Must be in outer ring zone (55%-120% of radius)
        if (dist < radius * 0.55f || dist > radius * 1.20f)
            return false;

        // Collect all LFO assignments targeting this knob
        struct Assignment { int lfo; int slot; float arcEnd; };
        Assignment assignments[12];
        int numAssignments = 0;

        auto rp = getRotaryParameters();
        constexpr float hPi = juce::MathConstants<float>::halfPi;
        float rotStart  = rp.startAngleRadians;
        float rotEnd    = rp.endAngleRadians;
        float rotRange  = rotEnd - rotStart;

        float baseRotAngle = rotStart + static_cast<float>(valueToProportionOfLength(getValue())) * rotRange;

        for (int l = 0; l < 3; ++l)
        {
            auto pfx = "LFO" + juce::String(l + 1) + "_";
            for (int s = 1; s <= 4; ++s)
            {
                auto destId = pfx + "DEST" + juce::String(s);
                auto* destRaw = statePtr->getRawParameterValue(destId);
                if (!destRaw) continue;
                int dest = static_cast<int>(destRaw->load());
                if (dest != static_cast<int>(myDest)) continue;

                float amt = statePtr->getRawParameterValue(pfx + "AMT" + juce::String(s))->load();
                float arcEnd = juce::jlimit(rotStart, rotEnd,
                                             baseRotAngle + amt * rotRange);
                assignments[numAssignments++] = { l, s, arcEnd };
            }
        }

        if (numAssignments == 0) return false;

        // Single assignment: grab it directly from anywhere in the ring
        if (numAssignments == 1)
        {
            isRingDrag = true;
            ringDragLfo = assignments[0].lfo;
            ringDragSlot = assignments[0].slot;
            auto amtId = "LFO" + juce::String(ringDragLfo + 1) + "_AMT" + juce::String(ringDragSlot);
            ringDragStartAmt = statePtr->getRawParameterValue(amtId)->load();
            return true;
        }

        // Multiple assignments: pick the one whose arc endpoint is closest to mouse angle
        // Convert mouse screen angle to rotary space (add halfPi)
        float mouseAngle = std::atan2(pos.y - centre.y, pos.x - centre.x) + hPi;
        if (mouseAngle < rotStart - 0.3f)
            mouseAngle += juce::MathConstants<float>::twoPi;

        float bestDist = 999.0f;
        int bestIdx = 0;
        for (int i = 0; i < numAssignments; ++i)
        {
            float d = std::abs(mouseAngle - assignments[i].arcEnd);
            if (d < bestDist) { bestDist = d; bestIdx = i; }
        }

        isRingDrag = true;
        ringDragLfo = assignments[bestIdx].lfo;
        ringDragSlot = assignments[bestIdx].slot;
        {
            auto amtId = "LFO" + juce::String(ringDragLfo + 1) + "_AMT" + juce::String(ringDragSlot);
            ringDragStartAmt = statePtr->getRawParameterValue(amtId)->load();
        }
        return true;
    }

    // Get current live LFO modulation value for this destination
    float getModValue() const
    {
        if (!voiceParamsPtr || myDest == bb::LFODest::None) return 0.0f;
        switch (myDest)
        {
            case bb::LFODest::Pitch:        return voiceParamsPtr->lfoModPitch.load(std::memory_order_relaxed);
            case bb::LFODest::FilterCutoff:  return voiceParamsPtr->lfoModCutoff.load(std::memory_order_relaxed);
            case bb::LFODest::FilterRes:     return voiceParamsPtr->lfoModRes.load(std::memory_order_relaxed);
            case bb::LFODest::Mod1Level:     return voiceParamsPtr->lfoModMod1Lvl.load(std::memory_order_relaxed);
            case bb::LFODest::Mod2Level:     return voiceParamsPtr->lfoModMod2Lvl.load(std::memory_order_relaxed);
            case bb::LFODest::Volume:        return voiceParamsPtr->lfoModVolume.load(std::memory_order_relaxed);
            case bb::LFODest::Drive:         return voiceParamsPtr->lfoModDrive.load(std::memory_order_relaxed);
            case bb::LFODest::CarNoise:      return voiceParamsPtr->lfoModNoise.load(std::memory_order_relaxed);
            case bb::LFODest::CarSpread:     return voiceParamsPtr->lfoModSpread.load(std::memory_order_relaxed);
            case bb::LFODest::FoldAmt:       return voiceParamsPtr->lfoModFold.load(std::memory_order_relaxed);
            case bb::LFODest::Mod1Fine:      return voiceParamsPtr->lfoModMod1Fine.load(std::memory_order_relaxed);
            case bb::LFODest::Mod2Fine:      return voiceParamsPtr->lfoModMod2Fine.load(std::memory_order_relaxed);
            case bb::LFODest::CarDrift:      return voiceParamsPtr->lfoModCarDrift.load(std::memory_order_relaxed);
            case bb::LFODest::CarFine:       return voiceParamsPtr->lfoModCarFine.load(std::memory_order_relaxed);
            case bb::LFODest::DlyTime:       return voiceParamsPtr->lfoModDlyTime.load(std::memory_order_relaxed);
            case bb::LFODest::DlyFeed:       return voiceParamsPtr->lfoModDlyFeed.load(std::memory_order_relaxed);
            case bb::LFODest::DlyMix:        return voiceParamsPtr->lfoModDlyMix.load(std::memory_order_relaxed);
            case bb::LFODest::RevSize:       return voiceParamsPtr->lfoModRevSize.load(std::memory_order_relaxed);
            case bb::LFODest::RevMix:        return voiceParamsPtr->lfoModRevMix.load(std::memory_order_relaxed);
            case bb::LFODest::LiqDepth:      return voiceParamsPtr->lfoModLiqDepth.load(std::memory_order_relaxed);
            case bb::LFODest::LiqMix:        return voiceParamsPtr->lfoModLiqMix.load(std::memory_order_relaxed);
            case bb::LFODest::RubWarp:       return voiceParamsPtr->lfoModRubWarp.load(std::memory_order_relaxed);
            case bb::LFODest::RubMix:        return voiceParamsPtr->lfoModRubMix.load(std::memory_order_relaxed);
            case bb::LFODest::PEnvAmt:       return voiceParamsPtr->lfoModPEnvAmt.load(std::memory_order_relaxed);
            case bb::LFODest::RevDamp:       return voiceParamsPtr->lfoModRevDamp.load(std::memory_order_relaxed);
            case bb::LFODest::RevWidth:      return voiceParamsPtr->lfoModRevWidth.load(std::memory_order_relaxed);
            case bb::LFODest::RevPdly:       return voiceParamsPtr->lfoModRevPdly.load(std::memory_order_relaxed);
            case bb::LFODest::DlyDamp:       return voiceParamsPtr->lfoModDlyDamp.load(std::memory_order_relaxed);
            case bb::LFODest::DlySpread:     return voiceParamsPtr->lfoModDlySpread.load(std::memory_order_relaxed);
            case bb::LFODest::LiqRate:       return voiceParamsPtr->lfoModLiqRate.load(std::memory_order_relaxed);
            case bb::LFODest::LiqTone:       return voiceParamsPtr->lfoModLiqTone.load(std::memory_order_relaxed);
            case bb::LFODest::LiqFeed:       return voiceParamsPtr->lfoModLiqFeed.load(std::memory_order_relaxed);
            case bb::LFODest::RubTone:       return voiceParamsPtr->lfoModRubTone.load(std::memory_order_relaxed);
            case bb::LFODest::RubStretch:    return voiceParamsPtr->lfoModRubStretch.load(std::memory_order_relaxed);
            case bb::LFODest::RubFeed:       return voiceParamsPtr->lfoModRubFeed.load(std::memory_order_relaxed);
            case bb::LFODest::Porta:         return voiceParamsPtr->lfoModPorta.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void showContextMenu()
    {
        if (!statePtr) return;
        juce::PopupMenu menu;

        // Collect assignments targeting this knob
        struct Hit { int lfo; int slot; };
        Hit hits[12];
        int numHits = 0;

        for (int l = 0; l < 3; ++l)
        {
            auto pfx = "LFO" + juce::String(l + 1) + "_";
            for (int s = 1; s <= 4; ++s)
            {
                auto destId = pfx + "DEST" + juce::String(s);
                int dest = static_cast<int>(statePtr->getRawParameterValue(destId)->load());
                if (dest == static_cast<int>(myDest))
                {
                    hits[numHits] = { l, s };
                    menu.addItem(numHits + 1, "x  Remove LFO" + juce::String(l + 1));
                    ++numHits;
                }
            }
        }

        if (numHits == 0) return;

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this, hits, numHits](int result) {
                if (!statePtr || result <= 0 || result > numHits) return;
                auto h = hits[result - 1];
                auto pfx = "LFO" + juce::String(h.lfo + 1) + "_";
                auto destId = pfx + "DEST" + juce::String(h.slot);
                auto amtId  = pfx + "AMT"  + juce::String(h.slot);
                statePtr->getParameter(destId)->setValueNotifyingHost(
                    statePtr->getParameter(destId)->convertTo0to1(0.0f));
                statePtr->getParameter(amtId)->setValueNotifyingHost(
                    statePtr->getParameter(amtId)->convertTo0to1(0.0f));
                repaint();
            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModSlider)
};
