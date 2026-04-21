// ParasiteLookAndFeel.h — Neumorphic UI theme
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_map>
#include <atomic>

class ParasiteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ParasiteLookAndFeel();

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
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
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

    void drawPopupMenuSectionHeader(juce::Graphics& g,
                                     const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override;

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight, int& idealWidth,
                                    int& idealHeight) override;

    int getPopupMenuBorderSize() override;
    int getMenuWindowFlags() override;

    // Make popup window transparent so no white rect behind rounded corners
    void preparePopupMenuWindow(juce::Component& newWindow) override;

    // Thread-safe colour cell. The theme palette is shared across all plugin
    // instances in the process: another instance's dark-mode toggle writes
    // these while our GUI / GL threads are reading them. Implicit conversion
    // to uint32_t keeps every juce::Colour(kBgColor) call site unchanged.
    // Relaxed ordering is sufficient — we don't synchronise other data on it.
    class AtomicColor
    {
    public:
        AtomicColor(uint32_t v) noexcept : value(v) {}
        AtomicColor(const AtomicColor&) = delete;
        AtomicColor& operator=(const AtomicColor&) = delete;

        operator uint32_t() const noexcept
        {
            return value.load(std::memory_order_relaxed);
        }
        AtomicColor& operator=(uint32_t v) noexcept
        {
            value.store(v, std::memory_order_relaxed);
            return *this;
        }

    private:
        std::atomic<uint32_t> value;
    };

    // Couleurs du theme — neumorphism (mutable for dark mode toggle)
    static inline AtomicColor kBgColor       { 0xFFE0E5EC };
    static inline AtomicColor kPanelColor    { 0xFFE0E5EC };
    static inline AtomicColor kKnobColor     { 0xFF9696A0 };
    static inline AtomicColor kKnobMarker    { 0xFF222230 };
    static inline AtomicColor kTextColor     { 0xFF24242E };
    static inline AtomicColor kAccentColor   { 0xFF8BC34A };
    static inline AtomicColor kToggleOn      { 0xFF8BC34A };
    static inline AtomicColor kToggleOff     { 0xFFB6BCC6 };
    static inline AtomicColor kHeaderBg      { 0xFFC4CAD4 };
    static inline AtomicColor kDisplayBg     { 0xFFD0D6E0 };

    // Neumorphic shadow colours
    static inline AtomicColor kShadowDark    { 0xFF8896AC };
    static inline AtomicColor kShadowLight   { 0xFFFFFFFF };

    // Dark mode toggle (atomic: read by GL thread, written by GUI thread)
    static inline std::atomic<bool> darkMode { false };
    static void setDarkMode(bool dark);
    static void loadDarkModePreference();  // Load persisted choice from disk
    void refreshJuceColours();

    void fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                   juce::TextEditor& editor) override;
    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& editor) override;

    // Draw a raised or inset neumorphic rectangle
    static void drawNeumorphicRect(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float cornerRadius, bool inset = false);

    // Shared DropShadow cache — reused by every draw* method instead of
    // constructing a fresh juce::DropShadow on every paint call. Rebuilt in
    // refreshShadowCache() which refreshJuceColours() calls whenever dark
    // mode flips (the only time the shadow colours change).
    struct ShadowCache
    {
        // drawNeumorphicRect — raised
        juce::DropShadow neuRaisedLight, neuRaisedDark;
        // drawNeumorphicRect — inset
        juce::DropShadow neuInsetDark, neuInsetLight;
        // drawRotarySlider outer + inner dish
        juce::DropShadow knobOuterLight, knobOuterDark;
        juce::DropShadow knobInnerDark,  knobInnerLight;
        // drawToggleButton outer + inset (LED well)
        juce::DropShadow toggleOuterLight, toggleOuterDark;
        juce::DropShadow toggleInsetDark,  toggleInsetLight;
        // drawComboBox / drawTextEditorOutline (2px inset)
        juce::DropShadow tinyInsetDark, tinyInsetLight;
        // drawButtonBackground (4px inset + outer)
        juce::DropShadow btnInsetDark, btnInsetLight;
        juce::DropShadow btnOuterLight, btnOuterDark;
    };
    // Single shared cache — rebuilt whenever dark mode changes. Safe to
    // share: DropShadow is a POD (colour/radius/offset) and only the GUI
    // thread mutates it via refreshShadowCache → setDarkMode.
    static inline ShadowCache shadowCache {};
    static const ShadowCache& getShadowCache() noexcept { return shadowCache; }

    // Refresh the shadow cache to match the current kShadow* colour atoms.
    // Called by refreshJuceColours() and from setDarkMode().
    static void refreshShadowCache();

private:
    // Per-toggle smooth animation state (0.0 = off, 1.0 = fully on)
    std::unordered_map<juce::Component*, float> toggleAnimValues;
};
