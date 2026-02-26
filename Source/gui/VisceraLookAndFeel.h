// VisceraLookAndFeel.h — Style visuel fidèle au SynthEdit original
// Fond sombre, knobs carrés blancs, labels monospace
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class VisceraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VisceraLookAndFeel();

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

    // Couleurs du thème — dark warm
    static constexpr uint32_t kBgColor       = 0xFF1A1A1F;
    static constexpr uint32_t kPanelColor    = 0xFF252529;
    static constexpr uint32_t kKnobColor     = 0xFF707078;
    static constexpr uint32_t kKnobMarker    = 0xFFE8E8E8;
    static constexpr uint32_t kTextColor     = 0xFFB0B0B4;
    static constexpr uint32_t kAccentColor   = 0xFF8BC34A;
    static constexpr uint32_t kToggleOn      = 0xFF8BC34A;
    static constexpr uint32_t kToggleOff     = 0xFF4A4A50;
    static constexpr uint32_t kHeaderBg      = 0xFF303036;
    static constexpr uint32_t kDisplayBg     = 0xFF1F1F24;
};
