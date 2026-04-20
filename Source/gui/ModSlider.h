// ModSlider.h — Slider subclass with Serum-style modulation ring, drag-to-set-amount, LFO D&D + learn mode
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/FMVoice.h" // for bb::LFODest, bb::VoiceParams
#include "ParasiteLookAndFeel.h"
#include <array>
#include <functional>

// Per-editor state shared between every ModSlider and the LFOSection. Owned
// by ParasiteEditor, discovered by its descendants via the provider below.
// Keeps each plugin instance's state independent in multi-instance hosts.
struct ModSliderContext
{
    const bb::VoiceParams* voiceParams = nullptr;   // read-only, for arc peaks & ghost tick
    std::function<void(bb::LFODest)> onLearnClick;  // next ModSlider click routes here
    bool showDropTargets = false;                    // true while LFO D&D is in progress
};

// Interface implemented by ParasiteEditor. ModSlider and LFOSection look this
// up via Component::findParentComponentOfClass so ModSlider.h has no compile
// dependency on ParasiteEditor.
class ModSliderContextProvider
{
public:
    virtual ~ModSliderContextProvider() = default;
    virtual ModSliderContext& getModSliderContext() noexcept = 0;
};

// Precomputed LFO slot parameter IDs ("LFO1_DEST3", "LFO2_AMT7", ...). Built
// once via Meyers singleton; every ModSlider / LFOSection read is a zero-alloc
// const-ref lookup instead of a per-frame juce::String concat.
namespace detail {
    struct LfoSlotIds
    {
        static constexpr int kLfos = 3;
        static constexpr int kSlots = 8;
        std::array<std::array<juce::String, kSlots>, kLfos> destIds;
        std::array<std::array<juce::String, kSlots>, kLfos> amtIds;

        LfoSlotIds()
        {
            for (int l = 0; l < kLfos; ++l)
            {
                auto pfx = "LFO" + juce::String(l + 1) + "_";
                for (int s = 0; s < kSlots; ++s)
                {
                    destIds[l][s] = pfx + "DEST" + juce::String(s + 1);
                    amtIds[l][s]  = pfx + "AMT"  + juce::String(s + 1);
                }
            }
        }
    };
    inline const LfoSlotIds& lfoSlotIds() noexcept
    {
        static const LfoSlotIds instance;
        return instance;
    }
}

class ModSlider : public juce::Slider,
                  public juce::DragAndDropTarget,
                  private juce::Timer
{
    static constexpr int kSlotsPerLFO = detail::LfoSlotIds::kSlots;
public:
    ModSlider()
    {
        setSliderSnapsToMousePosition(false);
        setMouseDragSensitivity(200);
        setWantsKeyboardFocus(false);
    }
    ~ModSlider() override { stopTimer(); }

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

    // Resolve the per-editor context when this component is attached / reattached.
    // Handles addToDesktop → back, page switches, and editor recreation.
    void parentHierarchyChanged() override
    {
        ctx = nullptr;
        if (auto* provider = findParentComponentOfClass<ModSliderContextProvider>())
            ctx = &provider->getModSliderContext();
    }

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
        if (ctx) ctx->showDropTargets = false;
        if (!statePtr) return;
        int lfoIdx = details.description.toString().getTrailingIntValue();
        if (lfoIdx < 0 || lfoIdx > 2) return;

        const auto& ids = detail::lfoSlotIds();

        // Check if this exact LFO is already assigned to this knob
        for (int s = 0; s < kSlotsPerLFO; ++s)
        {
            int curDest = static_cast<int>(statePtr->getRawParameterValue(ids.destIds[lfoIdx][s])->load());
            if (curDest == static_cast<int>(myDest))
                return; // already assigned, nothing to do
        }

        // Policy: max 1 LFO per knob — remove any existing assignment from ANY LFO
        for (int l = 0; l < 3; ++l)
        {
            for (int s = 0; s < kSlotsPerLFO; ++s)
            {
                const auto& destId = ids.destIds[l][s];
                const auto& amtId  = ids.amtIds[l][s];
                int curDest = static_cast<int>(statePtr->getRawParameterValue(destId)->load());
                if (curDest == static_cast<int>(myDest))
                {
                    statePtr->getParameter(destId)->setValueNotifyingHost(
                        statePtr->getParameter(destId)->convertTo0to1(0.0f));
                    statePtr->getParameter(amtId)->setValueNotifyingHost(
                        statePtr->getParameter(amtId)->convertTo0to1(0.0f));
                }
            }
        }

        // Assign to first free slot on the dropped LFO
        for (int s = 0; s < kSlotsPerLFO; ++s)
        {
            const auto& destId = ids.destIds[lfoIdx][s];
            const auto& amtId  = ids.amtIds[lfoIdx][s];
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
            g.setColour(juce::Colour(ParasiteLookAndFeel::kShadowLight).withAlpha(0.15f));
            g.fillEllipse(getLocalBounds().toFloat().reduced(2));
        }

        static const juce::Colour lfoColors[] = {
            juce::Colour(0xFF8BC34A), // LFO1 — green
            juce::Colour(0xFF8BC34A), // LFO2 — green
            juce::Colour(0xFF8BC34A)  // LFO3 — green
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

        const auto& ids = detail::lfoSlotIds();
        const bb::VoiceParams* vp = ctx ? ctx->voiceParams : nullptr;

        for (int l = 0; l < 3; ++l)
        {
            for (int s = 0; s < kSlotsPerLFO; ++s)
            {
                auto* destRaw = statePtr->getRawParameterValue(ids.destIds[l][s]);
                if (!destRaw) continue;
                int dest = static_cast<int>(destRaw->load());
                if (dest != static_cast<int>(myDest)) continue;

                float amt = statePtr->getRawParameterValue(ids.amtIds[l][s])->load();
                auto col = lfoColors[l];

                // Scale arc by actual LFO peak (custom curves may not reach 1.0)
                float peak = vp
                    ? vp->lfoPeak[l].load(std::memory_order_relaxed)
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
                    g.setColour(col.withAlpha(0.06f));
                    g.strokePath(arc, juce::PathStrokeType(5.0f));
                    // Core
                    g.setColour(col.withAlpha(0.85f));
                    g.strokePath(arc, juce::PathStrokeType(2.8f));
                    // Hot center
                    g.setColour(col.brighter(0.5f).withAlpha(0.3f));
                    g.strokePath(arc, juce::PathStrokeType(1.0f));
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
            auto tickCol = juce::Colour(ParasiteLookAndFeel::kAccentColor);
            // Core
            g.setColour(tickCol.withAlpha(0.7f));
            g.drawLine(centre.x + cosA * innerR, centre.y + sinA * innerR,
                       centre.x + cosA * outerR, centre.y + sinA * outerR, 2.2f);
        }
    }

    // --- Mouse handling: ring drag + learn mode + context menu ---
    void mouseDown(const juce::MouseEvent& e) override
    {
        // Learn mode intercept
        if (statePtr && ctx && ctx->onLearnClick)
        {
            ctx->onLearnClick(myDest);
            return;
        }

        // Context menu on right-click (always available on ModSliders)
        if (e.mods.isPopupMenu())
        {
            if (statePtr)
                showContextMenu();
            return;
        }

        // Check for ring drag (click near an arc endpoint)
        if (statePtr && hitTestRingDrag(e.position))
            return;

        customDrag = true;
        customDragValue = getValue();
        lastDragPos = e.position;
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

            const auto& amtId = detail::lfoSlotIds().amtIds[ringDragLfo][ringDragSlot - 1];
            if (auto* p = statePtr->getParameter(amtId))
                p->setValueNotifyingHost(p->convertTo0to1(newAmt));
            repaint();
            return;
        }
        if (customDrag)
        {
            float dy = -(e.position.y - lastDragPos.y);
            float dx = e.position.x - lastDragPos.x;
            float delta = (std::abs(dx) > std::abs(dy)) ? dx : dy;
            lastDragPos = e.position;

            // Work in normalised (0-1) space so NormalisableRange skew is respected
            double sens = e.mods.isShiftDown() ? 4000.0 : 500.0;
            double proportion = valueToProportionOfLength(customDragValue);
            proportion += delta / sens;
            proportion = juce::jlimit(0.0, 1.0, proportion);
            customDragValue = proportionOfLengthToValue(proportion);
            setValue(customDragValue, juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseDrag(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        customDrag = false;
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

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (isDoubleClickReturnEnabled())
            juce::Slider::mouseDoubleClick(e);
        else
            showTextBox();
    }


private:
    juce::AudioProcessorValueTreeState* statePtr = nullptr;
    // Resolved in parentHierarchyChanged — null when detached. All reads
    // must null-check; writes use `if (ctx) ctx->...`.
    ModSliderContext* ctx = nullptr;
    bb::LFODest myDest = bb::LFODest::None;
    bool dragHover = false;

    // Custom drag state (frame-by-frame for seamless shift toggle)
    bool customDrag = false;
    double customDragValue = 0.0;
    juce::Point<float> lastDragPos;

    // Ring drag state
    bool isRingDrag = false;
    int ringDragLfo = -1;   // 0-2
    int ringDragSlot = -1;  // 0-3
    float ringDragStartAmt = 0.0f;
    bool wasShowingDropTargets = false; // track transition to clear residual glow

    void timerCallback() override
    {
        // Update mapped flag
        bool mapped = false;
        if (statePtr && myDest != bb::LFODest::None)
        {
            const auto& ids = detail::lfoSlotIds();
            for (int l = 0; l < 3 && !mapped; ++l)
            {
                for (int s = 0; s < kSlotsPerLFO && !mapped; ++s)
                {
                    auto* raw = statePtr->getRawParameterValue(ids.destIds[l][s]);
                    if (raw && static_cast<int>(raw->load()) == static_cast<int>(myDest))
                        mapped = true;
                }
            }
        }

        bool changed = (mapped != isMapped);
        isMapped = mapped;

        const bool dropTargets = (ctx && ctx->showDropTargets);
        // Detect drop-targets turning off — need one final repaint to clear the glow
        bool dropTargetsJustEnded = (wasShowingDropTargets && !dropTargets);
        wasShowingDropTargets = dropTargets;

        // Repaint if mapped, state changed, drag targets showing, or targets just ended
        if (mapped || changed || dropTargets || dropTargetsJustEnded)
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
        Assignment assignments[24];
        int numAssignments = 0;

        auto rp = getRotaryParameters();
        constexpr float hPi = juce::MathConstants<float>::halfPi;
        float rotStart  = rp.startAngleRadians;
        float rotEnd    = rp.endAngleRadians;
        float rotRange  = rotEnd - rotStart;

        float baseRotAngle = rotStart + static_cast<float>(valueToProportionOfLength(getValue())) * rotRange;

        const auto& ids = detail::lfoSlotIds();
        for (int l = 0; l < 3; ++l)
        {
            for (int s = 0; s < kSlotsPerLFO; ++s)
            {
                auto* destRaw = statePtr->getRawParameterValue(ids.destIds[l][s]);
                if (!destRaw) continue;
                int dest = static_cast<int>(destRaw->load());
                if (dest != static_cast<int>(myDest)) continue;

                float amt = statePtr->getRawParameterValue(ids.amtIds[l][s])->load();
                float arcEnd = juce::jlimit(rotStart, rotEnd,
                                             baseRotAngle + amt * rotRange);
                assignments[numAssignments++] = { l, s + 1, arcEnd }; // slot stays 1-indexed downstream
            }
        }

        if (numAssignments == 0) return false;

        // Single assignment: grab it directly from anywhere in the ring
        if (numAssignments == 1)
        {
            isRingDrag = true;
            ringDragLfo = assignments[0].lfo;
            ringDragSlot = assignments[0].slot;
            ringDragStartAmt = statePtr->getRawParameterValue(
                detail::lfoSlotIds().amtIds[ringDragLfo][ringDragSlot - 1])->load();
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
        ringDragStartAmt = statePtr->getRawParameterValue(
            detail::lfoSlotIds().amtIds[ringDragLfo][ringDragSlot - 1])->load();
        return true;
    }

    // Get current live LFO modulation value for this destination
    float getModValue() const
    {
        const bb::VoiceParams* vp = ctx ? ctx->voiceParams : nullptr;
        if (!vp || myDest == bb::LFODest::None) return 0.0f;
        switch (myDest)
        {
            case bb::LFODest::Pitch:        return vp->lfoModPitch.load(std::memory_order_relaxed);
            case bb::LFODest::FilterCutoff:  return vp->lfoModCutoff.load(std::memory_order_relaxed);
            case bb::LFODest::FilterRes:     return vp->lfoModRes.load(std::memory_order_relaxed);
            case bb::LFODest::Mod1Level:     return vp->lfoModMod1Lvl.load(std::memory_order_relaxed);
            case bb::LFODest::Mod2Level:     return vp->lfoModMod2Lvl.load(std::memory_order_relaxed);
            case bb::LFODest::Volume:        return vp->lfoModVolume.load(std::memory_order_relaxed);
            case bb::LFODest::Drive:         return vp->lfoModDrive.load(std::memory_order_relaxed);
            case bb::LFODest::CarNoise:      return vp->lfoModNoise.load(std::memory_order_relaxed);
            case bb::LFODest::CarSpread:     return vp->lfoModSpread.load(std::memory_order_relaxed);
            case bb::LFODest::FoldAmt:       return vp->lfoModFold.load(std::memory_order_relaxed);
            case bb::LFODest::Mod1Fine:      return vp->lfoModMod1Fine.load(std::memory_order_relaxed);
            case bb::LFODest::Mod2Fine:      return vp->lfoModMod2Fine.load(std::memory_order_relaxed);
            case bb::LFODest::CarDrift:      return vp->lfoModCarDrift.load(std::memory_order_relaxed);
            case bb::LFODest::CarFine:       return vp->lfoModCarFine.load(std::memory_order_relaxed);
            case bb::LFODest::DlyTime:       return vp->lfoModDlyTime.load(std::memory_order_relaxed);
            case bb::LFODest::DlyFeed:       return vp->lfoModDlyFeed.load(std::memory_order_relaxed);
            case bb::LFODest::DlyMix:        return vp->lfoModDlyMix.load(std::memory_order_relaxed);
            case bb::LFODest::RevSize:       return vp->lfoModRevSize.load(std::memory_order_relaxed);
            case bb::LFODest::RevMix:        return vp->lfoModRevMix.load(std::memory_order_relaxed);
            case bb::LFODest::LiqDepth:      return vp->lfoModLiqDepth.load(std::memory_order_relaxed);
            case bb::LFODest::LiqMix:        return vp->lfoModLiqMix.load(std::memory_order_relaxed);
            case bb::LFODest::RubWarp:       return vp->lfoModRubWarp.load(std::memory_order_relaxed);
            case bb::LFODest::RubMix:        return vp->lfoModRubMix.load(std::memory_order_relaxed);
            case bb::LFODest::PEnvAmt:       return vp->lfoModPEnvAmt.load(std::memory_order_relaxed);
            case bb::LFODest::RevDamp:       return vp->lfoModRevDamp.load(std::memory_order_relaxed);
            case bb::LFODest::RevWidth:      return vp->lfoModRevWidth.load(std::memory_order_relaxed);
            case bb::LFODest::RevPdly:       return vp->lfoModRevPdly.load(std::memory_order_relaxed);
            case bb::LFODest::DlyDamp:       return vp->lfoModDlyDamp.load(std::memory_order_relaxed);
            case bb::LFODest::DlySpread:     return vp->lfoModDlySpread.load(std::memory_order_relaxed);
            case bb::LFODest::LiqRate:       return vp->lfoModLiqRate.load(std::memory_order_relaxed);
            case bb::LFODest::LiqTone:       return vp->lfoModLiqTone.load(std::memory_order_relaxed);
            case bb::LFODest::LiqFeed:       return vp->lfoModLiqFeed.load(std::memory_order_relaxed);
            case bb::LFODest::RubTone:       return vp->lfoModRubTone.load(std::memory_order_relaxed);
            case bb::LFODest::RubStretch:    return vp->lfoModRubStretch.load(std::memory_order_relaxed);
            case bb::LFODest::RubFeed:       return vp->lfoModRubFeed.load(std::memory_order_relaxed);
            case bb::LFODest::Porta:         return vp->lfoModPorta.load(std::memory_order_relaxed);
            case bb::LFODest::Env1A:         return vp->lfoModEnv1A.load(std::memory_order_relaxed);
            case bb::LFODest::Env1D:         return vp->lfoModEnv1D.load(std::memory_order_relaxed);
            case bb::LFODest::Env1S:         return vp->lfoModEnv1S.load(std::memory_order_relaxed);
            case bb::LFODest::Env1R:         return vp->lfoModEnv1R.load(std::memory_order_relaxed);
            case bb::LFODest::Env2A:         return vp->lfoModEnv2A.load(std::memory_order_relaxed);
            case bb::LFODest::Env2D:         return vp->lfoModEnv2D.load(std::memory_order_relaxed);
            case bb::LFODest::Env2S:         return vp->lfoModEnv2S.load(std::memory_order_relaxed);
            case bb::LFODest::Env2R:         return vp->lfoModEnv2R.load(std::memory_order_relaxed);
            case bb::LFODest::Env3A:         return vp->lfoModEnv3A.load(std::memory_order_relaxed);
            case bb::LFODest::Env3D:         return vp->lfoModEnv3D.load(std::memory_order_relaxed);
            case bb::LFODest::Env3S:         return vp->lfoModEnv3S.load(std::memory_order_relaxed);
            case bb::LFODest::Env3R:         return vp->lfoModEnv3R.load(std::memory_order_relaxed);
            case bb::LFODest::PEnvA:         return vp->lfoModPEnvA.load(std::memory_order_relaxed);
            case bb::LFODest::PEnvD:         return vp->lfoModPEnvD.load(std::memory_order_relaxed);
            case bb::LFODest::PEnvS:         return vp->lfoModPEnvS.load(std::memory_order_relaxed);
            case bb::LFODest::PEnvR:         return vp->lfoModPEnvR.load(std::memory_order_relaxed);
            case bb::LFODest::ShaperRate:    return vp->lfoModShaperRate.load(std::memory_order_relaxed);
            case bb::LFODest::ShaperDepth:   return vp->lfoModShaperDepth.load(std::memory_order_relaxed);
            case bb::LFODest::Mod1Coarse:    return vp->lfoModMod1Coarse.load(std::memory_order_relaxed);
            case bb::LFODest::Mod2Coarse:    return vp->lfoModMod2Coarse.load(std::memory_order_relaxed);
            case bb::LFODest::CarCoarse:     return vp->lfoModCarCoarse.load(std::memory_order_relaxed);
            case bb::LFODest::Tremor:        return vp->lfoModTremor.load(std::memory_order_relaxed);
            case bb::LFODest::Vein:          return vp->lfoModVein.load(std::memory_order_relaxed);
            case bb::LFODest::Flux:          return vp->lfoModFlux.load(std::memory_order_relaxed);
            case bb::LFODest::Cortex:        return vp->lfoModCortex.load(std::memory_order_relaxed);
            case bb::LFODest::Ichor:         return vp->lfoModIchor.load(std::memory_order_relaxed);
            case bb::LFODest::Plasma:        return vp->lfoModPlasma.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void showContextMenu()
    {
        if (!statePtr) return;
        juce::PopupMenu menu;

        // Collect LFO assignments targeting this knob
        struct Hit { int lfo; int slot; float amt; };
        Hit hits[24];
        int numHits = 0;

        const auto& ids = detail::lfoSlotIds();
        for (int l = 0; l < 3; ++l)
        {
            for (int s = 0; s < kSlotsPerLFO; ++s)
            {
                int dest = static_cast<int>(statePtr->getRawParameterValue(ids.destIds[l][s])->load());
                if (dest == static_cast<int>(myDest))
                {
                    float amt = statePtr->getRawParameterValue(ids.amtIds[l][s])->load();
                    hits[numHits] = { l, s + 1, amt }; // keep slot 1-indexed for downstream code
                    auto amtStr = juce::String(static_cast<int>(amt * 100.0f));
                    menu.addItem(numHits + 1,
                        juce::String::charToString(0x2716) + "  LFO" + juce::String(l + 1)
                        + "  " + amtStr + "%");
                    ++numHits;
                }
            }
        }

        // Separator + Reset to Default (always available)
        if (numHits > 0)
            menu.addSeparator();
        menu.addItem(100, juce::String::charToString(0x21BA) + "  Reset to Default");

        auto safeThis = juce::Component::SafePointer<ModSlider>(this);
        menu.setLookAndFeel(&getLookAndFeel());
        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(this)
            .withStandardItemHeight(28),
            [safeThis, hits, numHits](int result) {
                if (safeThis == nullptr || !safeThis->statePtr || result <= 0) return;
                auto* statePtr = safeThis->statePtr;

                if (result == 100)
                {
                    // Reset slider to default value
                    auto paramId = safeThis->getComponentID();
                    if (auto* param = statePtr->getParameter(paramId))
                        param->setValueNotifyingHost(param->getDefaultValue());
                    else
                        safeThis->setValue(safeThis->getDoubleClickReturnValue(), juce::sendNotificationSync);
                    safeThis->repaint();
                    return;
                }

                if (result > numHits) return;
                auto h = hits[result - 1];
                const auto& ids2 = detail::lfoSlotIds();
                const auto& destId = ids2.destIds[h.lfo][h.slot - 1];
                const auto& amtId  = ids2.amtIds[h.lfo][h.slot - 1];
                statePtr->getParameter(destId)->setValueNotifyingHost(
                    statePtr->getParameter(destId)->convertTo0to1(0.0f));
                statePtr->getParameter(amtId)->setValueNotifyingHost(
                    statePtr->getParameter(amtId)->convertTo0to1(0.0f));
                safeThis->repaint();
            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModSlider)
};
