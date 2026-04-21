// PitchEnvSection.cpp — Pitch Envelope: visual display + knobs
#include "PitchEnvSection.h"
#include "../gui/ParasiteLookAndFeel.h"

// ============================================================
// PitchEnvDisplay — visual ADSR curve (no drag)
// ============================================================

PitchEnvDisplay::PitchEnvDisplay(juce::AudioProcessorValueTreeState& apvts)
    : state(apvts)
{
    startTimerHz(15);
}

void PitchEnvDisplay::timerCallback()
{
    // Pitch env display is static until a param changes. Digest the values
    // to skip 15 repaints/sec when the user isn't editing.
    const float amt = state.getRawParameterValue("PENV_AMT")->load();
    const float a   = state.getRawParameterValue("PENV_A")->load();
    const float d   = state.getRawParameterValue("PENV_D")->load();
    const float s   = state.getRawParameterValue("PENV_S")->load();
    const float r   = state.getRawParameterValue("PENV_R")->load();
    const bool  on  = state.getRawParameterValue("PENV_ON")->load() > 0.5f;
    const float digest = amt * 0.01f + a + d * 3.17f + s * 5.41f + r * 7.83f
                       + (on ? 1000.0f : 0.0f);
    if (std::abs(digest - lastPitchEnvDigest) > 1e-4f)
    {
        lastPitchEnvDigest = digest;
        repaint();
    }
}

// Same cumulative layout as CarrierEnvDisplay
static constexpr float kPMaxA   = 5.0f;
static constexpr float kPMaxD   = 5.0f;
static constexpr float kPSusHold = 0.14f;
static constexpr float kPMaxR   = 8.0f;

static float penvParamToFrac(float val, float maxV)
{
    if (maxV <= 0.0f) return 0.0f;
    return std::sqrt(juce::jlimit(0.0f, 1.0f, val / maxV));
}

void PitchEnvDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(juce::Colour(ParasiteLookAndFeel::kDisplayBg));
    g.fillRoundedRectangle(b, 3.0f);

    bool enabled = *state.getRawParameterValue("PENV_ON") > 0.5f;

    auto inner = b.reduced(6.0f, 4.0f);
    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float y0 = inner.getY();

    float baseline = y0 + h * 0.5f;

    // Baseline
    g.setColour(juce::Colour(ParasiteLookAndFeel::kToggleOff));
    g.drawHorizontalLine(static_cast<int>(baseline), inner.getX() + 2, inner.getRight() - 2);

    if (!enabled)
    {
        g.setColour(juce::Colour(ParasiteLookAndFeel::kToggleOff));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.drawText("OFF", b.toNearestInt(), juce::Justification::centred);
        return;
    }

    float attack  = *state.getRawParameterValue("PENV_A");
    float decay   = *state.getRawParameterValue("PENV_D");
    float sustain = *state.getRawParameterValue("PENV_S");
    float release = *state.getRawParameterValue("PENV_R");
    float amount  = *state.getRawParameterValue("PENV_AMT");

    float ampScale = (h * 0.45f) / 96.0f;
    float peakY = baseline - amount * ampScale;
    float sustainY = baseline - amount * sustain * ampScale;

    // Cumulative positions — points overlap when params are 0
    float maxADR  = w * (1.0f - kPSusHold);
    float budgetA = maxADR * (kPMaxA / (kPMaxA + kPMaxD + kPMaxR));
    float budgetD = maxADR * (kPMaxD / (kPMaxA + kPMaxD + kPMaxR));
    float budgetR = maxADR * (kPMaxR / (kPMaxA + kPMaxD + kPMaxR));
    float susW    = w * kPSusHold;

    float peakX     = x0 + penvParamToFrac(attack, kPMaxA) * budgetA;
    float susStartX = peakX + penvParamToFrac(decay, kPMaxD) * budgetD;
    float susEndX   = susStartX + susW;
    float relEndX   = susEndX + penvParamToFrac(release, kPMaxR) * budgetR;

    juce::Point<float> pStart  = { x0, baseline };
    juce::Point<float> pPeak   = { peakX, peakY };
    juce::Point<float> pSusS   = { susStartX, sustainY };
    juce::Point<float> pSusE   = { susEndX, sustainY };
    juce::Point<float> pRelEnd = { relEndX, baseline };

    // Fill
    juce::Path fill;
    fill.startNewSubPath(pStart);
    fill.lineTo(pPeak);
    fill.lineTo(pSusS);
    fill.lineTo(pSusE);
    fill.lineTo(pRelEnd);
    fill.lineTo(pRelEnd.x, baseline);
    fill.lineTo(pStart.x, baseline);
    fill.closeSubPath();
    g.setColour(juce::Colour(ParasiteLookAndFeel::kAccentColor).withAlpha(0.06f));
    g.fillPath(fill);

    // Curve
    juce::Path path;
    path.startNewSubPath(pStart);
    path.lineTo(pPeak);
    path.lineTo(pSusS);
    path.lineTo(pSusE);
    path.lineTo(pRelEnd);
    g.setColour(juce::Colour(ParasiteLookAndFeel::kAccentColor));
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

// ============================================================
// PitchEnvSection — toggle + display + knobs
// ============================================================

PitchEnvSection::PitchEnvSection(juce::AudioProcessorValueTreeState& apvts)
    : envDisplay(apvts)
{
    onToggle.setButtonText("On");
    addAndMakeVisible(onToggle);
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "PENV_ON", onToggle);

    setupKnob(amtKnob, amtLabel, "Amt");
    amtKnob.initMod(apvts, bb::LFODest::PEnvAmt);
    amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "PENV_AMT", amtKnob);

    {
        static const char* adsrNames[] = { "A", "D", "S", "R" };
        static const char* paramIds[]  = { "PENV_A", "PENV_D", "PENV_S", "PENV_R" };
        bb::LFODest envDests[] = { bb::LFODest::PEnvA, bb::LFODest::PEnvD, bb::LFODest::PEnvS, bb::LFODest::PEnvR };
        for (int i = 0; i < 4; ++i)
        {
            adsrKnobs[i].initMod(apvts, envDests[i]);
            setupKnob(adsrKnobs[i], adsrLabels[i], adsrNames[i]);
            adsrAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, paramIds[i], adsrKnobs[i]);
        }
    }

    addAndMakeVisible(envDisplay);

    startTimerHz(5);
}

void PitchEnvSection::timerCallback()
{
    // Amt label
    if (amtKnob.isMouseOverOrDragging())
    {
        int st = static_cast<int>(amtKnob.getValue());
        amtLabel.setText((st > 0 ? "+" : "") + juce::String(st) + "st", juce::dontSendNotification);
    }
    else
        amtLabel.setText("Amt", juce::dontSendNotification);

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

void PitchEnvSection::setupKnob(juce::Slider& knob, juce::Label& label,
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

void PitchEnvSection::resized()
{
    auto area = getLocalBounds().reduced(2);
    area.removeFromTop(2);

    auto toggleRow = area.removeFromTop(18);
    toggleRow.removeFromLeft(2);  // align with effect toggles
    onToggle.setBounds(toggleRow.removeFromLeft(48));

    area.removeFromTop(2);

    int knobSize = 36;
    int labelH = 12;
    area.removeFromBottom(5);
    auto knobRow = area.removeFromBottom(knobSize + labelH);
    int colW = knobRow.getWidth() / 5;

    auto amtArea = knobRow.removeFromLeft(colW);
    amtLabel.setBounds(amtArea.removeFromBottom(labelH));
    amtKnob.setBounds(amtArea);

    for (int i = 0; i < 4; ++i)
    {
        auto col = knobRow.removeFromLeft(colW);
        adsrLabels[i].setBounds(col.removeFromBottom(labelH));
        adsrKnobs[i].setBounds(col);
    }

    area.removeFromBottom(2);
    envDisplay.setBounds(area);
}
