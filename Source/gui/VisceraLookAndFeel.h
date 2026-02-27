// VisceraLookAndFeel.h — Style visuel fidèle au SynthEdit original
// Fond sombre, knobs carrés blancs, labels monospace
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class VisceraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VisceraLookAndFeel();

    static constexpr int kNumFrames = 32;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getLabelFont(juce::Label& label) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    // Filmstrip knob images
    juce::Image knobVirgin, knobCircle, knobBlue, knobCircleGreen;

    // Couleurs du thème — dark warm
    static constexpr uint32_t kBgColor       = 0xFFFFFFFF;
    static constexpr uint32_t kPanelColor    = 0xFFFAFAFC;
    static constexpr uint32_t kKnobColor     = 0xFFB0B0B8;
    static constexpr uint32_t kKnobMarker    = 0xFF3A3A40;
    static constexpr uint32_t kTextColor     = 0xFF404048;
    static constexpr uint32_t kAccentColor   = 0xFF8BC34A;
    static constexpr uint32_t kToggleOn      = 0xFF8BC34A;
    static constexpr uint32_t kToggleOff     = 0xFFCCCCD0;
    static constexpr uint32_t kHeaderBg      = 0xFFE8E8EC;
    static constexpr uint32_t kDisplayBg     = 0xFFF8F8FA;
};
