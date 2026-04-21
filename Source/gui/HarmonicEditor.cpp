// HarmonicEditor.cpp — 32-bar harmonic editor with shape presets
#include "HarmonicEditor.h"
#include "ParasiteLookAndFeel.h"
#include <array>

HarmonicEditor::HarmonicEditor(bb::HarmonicTable& table)
    : harmonicTable(table)
{
    startTimerHz(15);
}

void HarmonicEditor::timerCallback()
{
    // Flush pending wavetable rebake — the APVTS CurveListener only flags
    // the table dirty; we batch the rebake here so a fast drag gets one
    // rebake per tick instead of one per pixel.
    harmonicTable.flushIfDirty();

    // 32 bars stay static until the user draws or a preset loads — no reason
    // to repaint 15×/sec when nothing changed. Digest is "sum of (amp × idx)"
    // which catches any single-bar edit without a full array compare.
    float digest = 0.0f;
    for (int i = 0; i < bb::kHarmonicCount; ++i)
        digest += harmonicTable.getHarmonic(i) * static_cast<float>(i + 1);
    if (std::abs(digest - lastHarmonicsDigest) > 1e-4f)
    {
        lastHarmonicsDigest = digest;
        repaint();
    }
}

void HarmonicEditor::resized()
{
    barArea = getLocalBounds().reduced(1, 2);
}

void HarmonicEditor::paint(juce::Graphics& g)
{
    auto area = barArea.toFloat();

    // Background
    g.setColour(juce::Colour(ParasiteLookAndFeel::kDisplayBg));
    g.fillRoundedRectangle(area, 3.0f);

    if (area.getWidth() < 1 || area.getHeight() < 1) return;

    float barW = area.getWidth() / static_cast<float>(bb::kHarmonicCount);
    float maxH = area.getHeight();

    // Pre-baked bar colours (one per harmonic index, darkening with index).
    // withMultipliedBrightness does HSV conversion — caching avoids 32 of
    // those per paint. Rebuilt lazily when the accent colour flips via
    // dark mode (single uint32 compare tells us when).
    static std::array<juce::Colour, bb::kHarmonicCount> barColourCache;
    static uint32_t                                     cachedAccent = 0;
    const uint32_t currentAccent = ParasiteLookAndFeel::kAccentColor;
    if (currentAccent != cachedAccent)
    {
        cachedAccent = currentAccent;
        const juce::Colour accent(currentAccent);
        for (int h = 0; h < bb::kHarmonicCount; ++h)
        {
            float brightness = 1.0f - static_cast<float>(h) * 0.015f;
            barColourCache[h] = accent.withMultipliedBrightness(brightness).withAlpha(0.85f);
        }
    }

    for (int h = 0; h < bb::kHarmonicCount; ++h)
    {
        float amp = harmonicTable.getHarmonic(h);
        float bh = std::fabs(amp) * maxH;
        float x = area.getX() + static_cast<float>(h) * barW;

        // Filled bar
        if (bh > 0.5f)
        {
            auto barRect = juce::Rectangle<float>(x + 1.0f, area.getBottom() - bh,
                                                   barW - 2.0f, bh);
            g.setColour(barColourCache[h]);
            g.fillRect(barRect);
        }

        // Thin separator line
        if (h > 0)
        {
            g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(0.08f));
            g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        }
    }

    // Outline
    g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(0.15f));
    g.drawRoundedRectangle(area, 3.0f, 1.0f);
}

void HarmonicEditor::mouseDown(const juce::MouseEvent& e)
{
    drawBar(e);
}

void HarmonicEditor::mouseDrag(const juce::MouseEvent& e)
{
    drawBar(e);
}

void HarmonicEditor::drawBar(const juce::MouseEvent& e)
{
    auto area = barArea.toFloat();
    if (area.getWidth() < 1 || area.getHeight() < 1) return;

    float barW = area.getWidth() / static_cast<float>(bb::kHarmonicCount);
    int idx = static_cast<int>((static_cast<float>(e.x) - area.getX()) / barW);

    if (idx < 0 || idx >= bb::kHarmonicCount) return;

    // Map Y to amplitude [0, 1] — top = 1, bottom = 0
    float amp = 1.0f - (static_cast<float>(e.y) - area.getY()) / area.getHeight();
    amp = juce::jlimit(0.0f, 1.0f, amp);

    if (onSetHarmonic)
    {
        // Route through APVTS so the edit is undoable (Cmd+Z)
        onSetHarmonic(idx, amp);
    }
    else
    {
        harmonicTable.setHarmonic(idx, amp);
        harmonicTable.rebake();
    }
    if (onUserDraw) onUserDraw();
    repaint();
}
