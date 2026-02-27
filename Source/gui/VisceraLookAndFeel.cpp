// VisceraLookAndFeel.cpp — Dark warm theme with filmstrip knobs
#include "VisceraLookAndFeel.h"
#include "ModSlider.h"
#include <BinaryData.h>

VisceraLookAndFeel::VisceraLookAndFeel()
{
    // Load filmstrip knob images from binary data
    knobVirgin = juce::ImageCache::getFromMemory(BinaryData::Knob_virgin_png,
                                                  BinaryData::Knob_virgin_pngSize);
    knobCircle = juce::ImageCache::getFromMemory(BinaryData::Knob_empty_circle_png,
                                                  BinaryData::Knob_empty_circle_pngSize);
    knobBlue   = juce::ImageCache::getFromMemory(BinaryData::Knob_Blue_png,
                                                  BinaryData::Knob_Blue_pngSize);
    knobCircleGreen = juce::ImageCache::getFromMemory(BinaryData::Knob_circle_green_png,
                                                       BinaryData::Knob_circle_green_pngSize);

    // Couleurs globales JUCE — light theme
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(kBgColor));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextColor));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour(kTextColor));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanelColor));
    setColour(juce::ComboBox::textColourId, juce::Colour(kTextColor));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(kToggleOff));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colours::white);
    setColour(juce::PopupMenu::textColourId, juce::Colour(kTextColor));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(kAccentColor));
    setColour(juce::TextButton::textColourOffId, juce::Colour(kTextColor));
    setColour(juce::TextButton::textColourOnId, juce::Colour(kTextColor));
    setColour(juce::TextButton::buttonColourId, juce::Colours::transparentWhite);
}

// Filmstrip knob — pick image based on slider type
void VisceraLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float /*rotaryStartAngle*/, float /*rotaryEndAngle*/,
    juce::Slider& slider)
{
    // Mapped ModSliders use circle (white ring), others use circle green
    auto* mod = dynamic_cast<ModSlider*>(&slider);
    const auto& img = (mod && mod->isMapped) ? knobCircle : knobCircleGreen;
    if (! img.isValid()) return;

    int frameH = img.getHeight() / kNumFrames;
    int frameW = img.getWidth();
    int frameIdx = juce::jlimit(0, kNumFrames - 1, juce::roundToInt(sliderPos * (kNumFrames - 1)));

    // Destination: square centered in bounds
    int side = juce::jmin(width, height);
    int dx = x + (width  - side) / 2;
    int dy = y + (height - side) / 2;

    g.drawImage(img, dx, dy, side, side,
                0, frameIdx * frameH, frameW, frameH);
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
