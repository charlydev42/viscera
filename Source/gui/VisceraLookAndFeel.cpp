// VisceraLookAndFeel.cpp — Dark warm theme with beveled knobs
#include "VisceraLookAndFeel.h"

VisceraLookAndFeel::VisceraLookAndFeel()
{
    // Couleurs globales JUCE — dark theme
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(kBgColor));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextColor));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour(kTextColor));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanelColor));
    setColour(juce::ComboBox::textColourId, juce::Colour(kTextColor));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(kToggleOff));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(kPanelColor));
    setColour(juce::PopupMenu::textColourId, juce::Colour(kTextColor));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(kAccentColor));
}

// Dual-ring beveled knob with depth
void VisceraLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider&)
{
    float size = static_cast<float>(juce::jmin(width, height));
    float margin = size * 0.1f;
    float knobSize = size - margin * 2.0f;

    float cx = static_cast<float>(x) + static_cast<float>(width)  * 0.5f;
    float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

    float knobRadius = knobSize * 0.5f;

    // Outer ring (border) — darker than body
    g.setColour(juce::Colour(kKnobColor).darker(0.2f));
    g.fillEllipse(cx - knobRadius, cy - knobRadius, knobSize, knobSize);

    // Inner fill — lighter knob body
    float innerRadius = knobRadius - 2.0f;
    g.setColour(juce::Colour(kKnobColor));
    g.fillEllipse(cx - innerRadius, cy - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

    // Central concave circle — slightly darker
    float centerRadius = knobRadius * 0.30f;
    g.setColour(juce::Colour(kKnobColor).darker(0.1f));
    g.fillEllipse(cx - centerRadius, cy - centerRadius, centerRadius * 2.0f, centerRadius * 2.0f);

    // Marker line — from center circle edge to outer edge
    float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    float cosA = std::cos(angle - juce::MathConstants<float>::halfPi);
    float sinA = std::sin(angle - juce::MathConstants<float>::halfPi);
    float markerInner = centerRadius;
    float markerOuter = knobRadius - 2.0f;

    g.setColour(juce::Colour(kKnobMarker));
    g.drawLine(cx + cosA * markerInner, cy + sinA * markerInner,
               cx + cosA * markerOuter, cy + sinA * markerOuter, 3.0f);
}

// Toggle button : petit carré coloré
void VisceraLookAndFeel::drawToggleButton(juce::Graphics& g,
    juce::ToggleButton& button, bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    float boxSize = 14.0f;
    juce::Rectangle<float> box(bounds.getX(), bounds.getCentreY() - boxSize * 0.5f,
                                boxSize, boxSize);

    g.setColour(juce::Colour(button.getToggleState() ? kToggleOn : kToggleOff));
    g.fillRoundedRectangle(box, 2.0f);

    // Texte à droite du toggle
    g.setColour(juce::Colour(kTextColor));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    g.drawText(button.getButtonText(),
               box.getRight() + 4.0f, bounds.getY(),
               bounds.getWidth() - boxSize - 6.0f, bounds.getHeight(),
               juce::Justification::centredLeft);
}

// ComboBox minimaliste
void VisceraLookAndFeel::drawComboBox(juce::Graphics& g,
    int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(juce::Colour(kPanelColor));
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(juce::Colour(kToggleOff));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);

    // Flèche dropdown
    float arrowX = static_cast<float>(width) - 16.0f;
    float arrowY = static_cast<float>(height) * 0.5f - 2.0f;
    juce::Path arrow;
    arrow.addTriangle(arrowX, arrowY, arrowX + 8.0f, arrowY, arrowX + 4.0f, arrowY + 5.0f);
    g.setColour(juce::Colour(kTextColor));
    g.fillPath(arrow);
}

juce::Font VisceraLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain);
}

juce::Font VisceraLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain);
}

// TextButton — draw only the button text, nothing else
void VisceraLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                         bool, bool)
{
    g.setColour(button.findColour(juce::TextButton::textColourOffId));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred);
}
