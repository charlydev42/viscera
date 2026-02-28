// PitchEnvSection.cpp — Pitch Envelope: visual display + knobs
#include "PitchEnvSection.h"
#include "../gui/VisceraLookAndFeel.h"

// ============================================================
// PitchEnvDisplay — visual ADSR curve (no drag)
// ============================================================

PitchEnvDisplay::PitchEnvDisplay(juce::AudioProcessorValueTreeState& apvts)
    : state(apvts)
{
    startTimerHz(15);
}

void PitchEnvDisplay::timerCallback() { repaint(); }

void PitchEnvDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(juce::Colour(VisceraLookAndFeel::kDisplayBg));
    g.fillRoundedRectangle(b, 3.0f);

    bool enabled = *state.getRawParameterValue("PENV_ON") > 0.5f;

    auto inner = b.reduced(3.0f);
    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float y0 = inner.getY();

    float baseline = y0 + h * 0.5f;

    // Baseline
    g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff));
    g.drawHorizontalLine(static_cast<int>(baseline), inner.getX() + 2, inner.getRight() - 2);

    if (!enabled)
    {
        g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.drawText("OFF", b.toNearestInt(), juce::Justification::centred);
        return;
    }

    float attack  = *state.getRawParameterValue("PENV_A");
    float decay   = *state.getRawParameterValue("PENV_D");
    float sustain = *state.getRawParameterValue("PENV_S");
    float release = *state.getRawParameterValue("PENV_R");
    float amount  = *state.getRawParameterValue("PENV_AMT");

    float sustainHold = 0.3f;
    float totalTime = attack + decay + sustainHold + release;
    if (totalTime < 0.01f) totalTime = 0.01f;
    float pps = w / totalTime;
    float ampScale = (h * 0.45f) / 96.0f;

    float peakY = baseline - amount * ampScale;
    float sustainY = baseline - amount * sustain * ampScale;

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

    // Fill
    juce::Path fill(path);
    fill.lineTo(pRelEnd.x, baseline);
    fill.lineTo(pStart.x, baseline);
    fill.closeSubPath();
    g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor).withAlpha(0.06f));
    g.fillPath(fill);

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

    static const char* adsrNames[] = { "A", "D", "S", "R" };
    static const char* paramIds[]  = { "PENV_A", "PENV_D", "PENV_S", "PENV_R" };
    for (int i = 0; i < 4; ++i)
    {
        setupKnob(adsrKnobs[i], adsrLabels[i], adsrNames[i]);
        adsrAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramIds[i], adsrKnobs[i]);
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
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void PitchEnvSection::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(2);

    auto toggleRow = area.removeFromTop(18);
    onToggle.setBounds(toggleRow.removeFromLeft(50));

    area.removeFromTop(2);

    int knobSize = 36;
    int labelH = 12;
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
