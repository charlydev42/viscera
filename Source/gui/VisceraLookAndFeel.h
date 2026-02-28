// VisceraLookAndFeel.h — Neumorphic UI theme
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_map>

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

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight, int& idealWidth,
                                    int& idealHeight) override;

    int getPopupMenuBorderSize() override;
    int getMenuWindowFlags() override;

    // Make popup window transparent so no white rect behind rounded corners
    void preparePopupMenuWindow(juce::Component& newWindow) override;

    // Filmstrip knob images
    juce::Image knobVirgin, knobCircle, knobBlue, knobCircleGreen;

    // Couleurs du theme — neumorphism (mutable for dark mode toggle)
    static inline uint32_t kBgColor       = 0xFFE0E5EC;
    static inline uint32_t kPanelColor    = 0xFFE0E5EC;
    static inline uint32_t kKnobColor     = 0xFFB0B0B8;
    static inline uint32_t kKnobMarker    = 0xFF3A3A40;
    static inline uint32_t kTextColor     = 0xFF404048;
    static inline uint32_t kAccentColor   = 0xFF8BC34A;
    static inline uint32_t kToggleOn      = 0xFF8BC34A;
    static inline uint32_t kToggleOff     = 0xFFD0D5DC;
    static inline uint32_t kHeaderBg      = 0xFFD8DDE4;
    static inline uint32_t kDisplayBg     = 0xFFE4E9F0;

    // Neumorphic shadow colours
    static inline uint32_t kShadowDark    = 0xFFA3B1C6;
    static inline uint32_t kShadowLight   = 0xFFFFFFFF;

    // Dark mode toggle
    static inline bool darkMode = false;
    static void setDarkMode(bool dark);
    void refreshJuceColours();

    // Draw a raised or inset neumorphic rectangle
    static void drawNeumorphicRect(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float cornerRadius, bool inset = false);

private:
    // Per-toggle smooth animation state (0.0 = off, 1.0 = fully on)
    std::unordered_map<juce::Component*, float> toggleAnimValues;
};
