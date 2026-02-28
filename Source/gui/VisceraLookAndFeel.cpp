// VisceraLookAndFeel.cpp — Neumorphic theme with programmatic knobs
#include "VisceraLookAndFeel.h"
#include "ModSlider.h"
#include <BinaryData.h>

// --- Neumorphic helper ---
void VisceraLookAndFeel::drawNeumorphicRect(juce::Graphics& g,
    juce::Rectangle<float> bounds, float cornerRadius, bool inset)
{
    if (inset)
    {
        g.setColour(juce::Colour(kBgColor));
        g.fillRoundedRectangle(bounds, cornerRadius);

        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(bounds, cornerRadius);
        g.reduceClipRegion(clip);

        juce::DropShadow darkInner(juce::Colour(kShadowDark).withAlpha(0.55f), 6, { 3, 3 });
        darkInner.drawForRectangle(g, bounds.toNearestInt());
        juce::DropShadow lightInner(juce::Colour(kShadowLight).withAlpha(0.55f), 6, { -3, -3 });
        lightInner.drawForRectangle(g, bounds.toNearestInt());

        g.restoreState();
    }
    else
    {
        juce::DropShadow lightShadow(juce::Colour(kShadowLight).withAlpha(0.65f), 5, { -3, -3 });
        lightShadow.drawForRectangle(g, bounds.toNearestInt());
        juce::DropShadow darkShadow(juce::Colour(kShadowDark).withAlpha(0.4f), 5, { 3, 3 });
        darkShadow.drawForRectangle(g, bounds.toNearestInt());

        g.setColour(juce::Colour(kBgColor));
        g.fillRoundedRectangle(bounds, cornerRadius);
    }
}

void VisceraLookAndFeel::setDarkMode(bool dark)
{
    darkMode = dark;
    if (dark)
    {
        kBgColor     = 0xFF2E3440;
        kPanelColor  = 0xFF2E3440;
        kKnobColor   = 0xFF4C566A;
        kKnobMarker  = 0xFFD8DEE9;
        kTextColor   = 0xFFD8DEE9;
        kAccentColor = 0xFF8BC34A;
        kToggleOn    = 0xFF8BC34A;
        kToggleOff   = 0xFF4C566A;
        kHeaderBg    = 0xFF3B4252;
        kDisplayBg   = 0xFF353C4A;
        kShadowDark  = 0xFF1A1E26;
        kShadowLight = 0xFF434C5E;
    }
    else
    {
        kBgColor     = 0xFFE0E5EC;
        kPanelColor  = 0xFFE0E5EC;
        kKnobColor   = 0xFFB0B0B8;
        kKnobMarker  = 0xFF3A3A40;
        kTextColor   = 0xFF404048;
        kAccentColor = 0xFF8BC34A;
        kToggleOn    = 0xFF8BC34A;
        kToggleOff   = 0xFFD0D5DC;
        kHeaderBg    = 0xFFD8DDE4;
        kDisplayBg   = 0xFFE4E9F0;
        kShadowDark  = 0xFFA3B1C6;
        kShadowLight = 0xFFFFFFFF;
    }
}

void VisceraLookAndFeel::refreshJuceColours()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(kBgColor));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(kTextColor));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour(kTextColor));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(kBgColor));
    setColour(juce::ComboBox::textColourId, juce::Colour(kTextColor));
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(kBgColor));
    setColour(juce::PopupMenu::textColourId, juce::Colour(kTextColor));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(kAccentColor));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::TextButton::textColourOffId, juce::Colour(kTextColor));
    setColour(juce::TextButton::textColourOnId, juce::Colour(kTextColor));
    setColour(juce::TextButton::buttonColourId, juce::Colour(kBgColor));
}

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

    refreshJuceColours();
}

// Neumorphic rotary knob with value arc
void VisceraLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    auto* mod = dynamic_cast<ModSlider*>(&slider);

    int side = juce::jmin(width, height);
    int dx = x + (width  - side) / 2;
    int dy = y + (height - side) / 2;
    auto knobBounds = juce::Rectangle<int>(dx, dy, side, side).toFloat();
    auto outerBounds = knobBounds.reduced(3.0f);

    float cx = knobBounds.getCentreX();
    float cy = knobBounds.getCentreY();
    float radius = outerBounds.getWidth() * 0.5f;

    // 1) Neumorphic double shadow — deep
    juce::Path circle;
    circle.addEllipse(outerBounds);
    juce::DropShadow lightSh(juce::Colour(kShadowLight).withAlpha(0.9f), 8, { -4, -4 });
    lightSh.drawForPath(g, circle);
    juce::DropShadow darkSh(juce::Colour(kShadowDark).withAlpha(0.65f), 8, { 4, 4 });
    darkSh.drawForPath(g, circle);

    // 2) Outer knob face with directional gradient
    {
        juce::ColourGradient face(juce::Colour(kBgColor).brighter(0.05f),
                                   outerBounds.getX(), outerBounds.getY(),
                                   juce::Colour(kBgColor).darker(0.04f),
                                   outerBounds.getRight(), outerBounds.getBottom(),
                                   true);
        g.setGradientFill(face);
        g.fillEllipse(outerBounds);
    }

    // 3) Inner circle — clean inset groove
    auto innerBounds = outerBounds.reduced(radius * 0.22f);
    {
        // Fill inner face
        g.setColour(juce::Colour(kBgColor));
        g.fillEllipse(innerBounds);

        // Inset shadow using ellipse path (no rectangular artifacts)
        juce::Path innerPath;
        innerPath.addEllipse(innerBounds);

        g.saveState();
        g.reduceClipRegion(innerPath);

        juce::DropShadow darkIn(juce::Colour(kShadowDark).withAlpha(0.3f), 4, { 2, 2 });
        darkIn.drawForPath(g, innerPath);
        juce::DropShadow lightIn(juce::Colour(kShadowLight).withAlpha(0.3f), 4, { -2, -2 });
        lightIn.drawForPath(g, innerPath);

        g.restoreState();

        // Subtle top-left highlight
        juce::ColourGradient innerGrad(juce::Colour(kShadowLight).withAlpha(0.06f),
                                        innerBounds.getX(), innerBounds.getY(),
                                        juce::Colours::transparentBlack,
                                        innerBounds.getCentreX(), innerBounds.getBottom(), true);
        g.setGradientFill(innerGrad);
        g.fillEllipse(innerBounds);
    }

    // 4) Value arc — glowing light arc from start to current position
    float arcRadius = radius - 4.0f;
    float curAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    if (curAngle - rotaryStartAngle > 0.05f)
    {
        auto accentCol = juce::Colour(kAccentColor);
        juce::Path arcPath;
        arcPath.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f,
                              rotaryStartAngle, curAngle, true);

        // Soft bloom halo
        g.setColour(accentCol.withAlpha(0.08f));
        g.strokePath(arcPath, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        // Core — sharp bright line
        g.setColour(accentCol.withAlpha(0.85f));
        g.strokePath(arcPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        // Hot center — white highlight
        g.setColour(accentCol.brighter(0.6f).withAlpha(0.4f));
        g.strokePath(arcPath, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // 5) Track arc background (unfilled portion)
    if (rotaryEndAngle - curAngle > 0.05f)
    {
        juce::Path trackPath;
        trackPath.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f,
                                curAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(kShadowDark).withAlpha(0.18f));
        g.strokePath(trackPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // 6) Clean indicator line
    float screenAngle = curAngle - juce::MathConstants<float>::halfPi;
    float notchInner = radius * 0.30f;
    float notchOuter = radius * 0.62f;
    float cosA = std::cos(screenAngle);
    float sinA = std::sin(screenAngle);

    g.setColour(juce::Colour(kShadowDark).withAlpha(0.55f));
    g.drawLine(cx + cosA * notchInner, cy + sinA * notchInner,
               cx + cosA * notchOuter, cy + sinA * notchOuter, 2.0f);
}

// Toggle button — round neumorphic button with inner groove + animated LED
void VisceraLookAndFeel::drawToggleButton(juce::Graphics& g,
    juce::ToggleButton& button, bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    float btnSize = 15.0f;
    auto btnRect = juce::Rectangle<float>(bounds.getX(), bounds.getCentreY() - btnSize * 0.5f,
                                           btnSize, btnSize);
    bool on = button.getToggleState();
    float cx = btnRect.getCentreX();
    float cy = btnRect.getCentreY();

    // --- Smooth LED animation ---
    float target = on ? 1.0f : 0.0f;
    float& anim = toggleAnimValues[&button];
    constexpr float kLerpSpeed = 0.55f; // snappy
    anim += (target - anim) * kLerpSpeed;
    if (std::abs(target - anim) < 0.005f)
        anim = target;
    else
    {
        auto safeBtn = juce::Component::SafePointer<juce::ToggleButton>(&button);
        juce::MessageManager::callAsync([safeBtn] { if (safeBtn != nullptr) safeBtn->repaint(); });
    }

    // Subtle intensity wobble when ON — mimics real LED current ripple
    float wobble = 1.0f;
    if (anim > 0.5f)
    {
        double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        wobble = 1.0f - 0.04f * static_cast<float>(std::sin(now * 3.7))
                      - 0.02f * static_cast<float>(std::sin(now * 7.3));
    }
    float glow = anim * wobble;

    // Raised neumorphic circle — deep shadows
    juce::Path circle;
    circle.addEllipse(btnRect);
    juce::DropShadow lightSh(juce::Colour(kShadowLight).withAlpha(0.85f), 5, { -2, -2 });
    lightSh.drawForPath(g, circle);
    juce::DropShadow darkSh(juce::Colour(kShadowDark).withAlpha(0.55f), 5, { 2, 2 });
    darkSh.drawForPath(g, circle);

    // Button face with gradient
    {
        juce::ColourGradient face(juce::Colour(kBgColor).brighter(0.04f),
                                   btnRect.getX(), btnRect.getY(),
                                   juce::Colour(kBgColor).darker(0.03f),
                                   btnRect.getRight(), btnRect.getBottom(), true);
        g.setGradientFill(face);
        g.fillEllipse(btnRect);
    }

    // Inner groove circle
    auto innerRect = btnRect.reduced(btnSize * 0.22f);
    {
        g.saveState();
        juce::Path innerClip;
        innerClip.addEllipse(innerRect);
        g.reduceClipRegion(innerClip);

        g.setColour(juce::Colour(kBgColor));
        g.fillEllipse(innerRect);

        juce::DropShadow darkIn(juce::Colour(kShadowDark).withAlpha(0.35f), 3, { 1, 1 });
        darkIn.drawForRectangle(g, innerRect.toNearestInt());
        juce::DropShadow lightIn(juce::Colour(kShadowLight).withAlpha(0.35f), 3, { -1, -1 });
        lightIn.drawForRectangle(g, innerRect.toNearestInt());

        g.restoreState();
    }

    // Animated LED glow (uses glow value for smooth fade + wobble)
    if (glow > 0.005f)
    {
        auto accentCol = juce::Colour(kAccentColor);
        float ledR = btnSize * 0.18f;
        auto ledRect = juce::Rectangle<float>(cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);

        // Wide outer bloom — spills onto button face
        juce::ColourGradient bloom(accentCol.withAlpha(0.30f * glow), cx, cy,
                                    accentCol.withAlpha(0.0f), cx, cy + btnSize * 0.55f, true);
        g.setGradientFill(bloom);
        g.fillEllipse(btnRect.expanded(2.0f));

        // Inner glow — tighter radial
        juce::ColourGradient innerGlow(accentCol.withAlpha(0.55f * glow), cx, cy,
                                        accentCol.withAlpha(0.0f), cx, cy + btnSize * 0.35f, true);
        g.setGradientFill(innerGlow);
        g.fillEllipse(innerRect);

        // LED body — saturated, fades in
        auto ledColBright = accentCol.brighter(0.3f);
        auto ledColDark   = accentCol.darker(0.05f);
        auto bgCol        = juce::Colour(kBgColor);
        auto ledTop    = bgCol.interpolatedWith(ledColBright, glow);
        auto ledBottom = bgCol.interpolatedWith(ledColDark, glow);
        juce::ColourGradient ledGrad(ledTop, cx, cy - ledR * 0.4f,
                                      ledBottom, cx, cy + ledR * 0.6f, false);
        g.setGradientFill(ledGrad);
        g.fillEllipse(ledRect);

        // Specular highlight — crisp white spot
        float specW = ledR * 0.7f;
        float specH = ledR * 0.45f;
        juce::ColourGradient spec(juce::Colours::white.withAlpha(0.7f * glow),
                                   cx - ledR * 0.15f, cy - ledR * 0.35f,
                                   juce::Colours::white.withAlpha(0.0f),
                                   cx - ledR * 0.15f, cy, false);
        g.setGradientFill(spec);
        g.fillEllipse(cx - specW * 0.5f, cy - ledR * 0.55f, specW, specH);
    }

    // No continuous repaint for wobble — saves CPU

    // Text — colour fades between text and accent
    auto textCol = juce::Colour(kTextColor).interpolatedWith(juce::Colour(kAccentColor), glow);
    g.setColour(textCol);
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    g.drawText(button.getButtonText(),
               btnRect.getRight() + 4.0f, bounds.getY(),
               bounds.getWidth() - btnSize - 6.0f, bounds.getHeight(),
               juce::Justification::centredLeft);
}

// ComboBox — soft pill inset
void VisceraLookAndFeel::drawComboBox(juce::Graphics& g,
    int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    float cr = bounds.getHeight() * 0.5f; // full pill

    // Flat inset — very subtle
    g.setColour(juce::Colour(kBgColor).darker(0.02f));
    g.fillRoundedRectangle(bounds, cr);

    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle(bounds, cr);
    g.reduceClipRegion(clip);
    juce::DropShadow darkIn(juce::Colour(kShadowDark).withAlpha(0.2f), 2, { 1, 1 });
    darkIn.drawForRectangle(g, bounds.toNearestInt());
    juce::DropShadow lightIn(juce::Colour(kShadowLight).withAlpha(0.2f), 2, { -1, -1 });
    lightIn.drawForRectangle(g, bounds.toNearestInt());
    g.restoreState();

    // Dropdown arrow
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

// TextButton text
void VisceraLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                         bool, bool)
{
    g.setColour(button.findColour(juce::TextButton::textColourOffId));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));

    if (button.getName() == "lfoSlot")
    {
        // LFO slot: text left-justified with padding
        auto area = button.getLocalBounds().reduced(6, 0);
        g.drawText(button.getButtonText(), area, juce::Justification::centredLeft);
    }
    else
    {
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred);
    }
}

// TextButton background — soft pill, lighter shadows
void VisceraLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool highlighted, bool down)
{
    // Skip drawing for transparent-background buttons (e.g. clear "x")
    if (backgroundColour.isTransparent())
        return;

    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    float cr = bounds.getHeight() * 0.5f; // pill shape

    if (down)
    {
        // Pressed: soft inset
        g.setColour(juce::Colour(kBgColor));
        g.fillRoundedRectangle(bounds, cr);

        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(bounds, cr);
        g.reduceClipRegion(clip);
        juce::DropShadow darkIn(juce::Colour(kShadowDark).withAlpha(0.35f), 4, { 2, 2 });
        darkIn.drawForRectangle(g, bounds.toNearestInt());
        juce::DropShadow lightIn(juce::Colour(kShadowLight).withAlpha(0.35f), 4, { -2, -2 });
        lightIn.drawForRectangle(g, bounds.toNearestInt());
        g.restoreState();
    }
    else
    {
        // Raised pill — shadow follows pill shape
        juce::Path pillPath;
        pillPath.addRoundedRectangle(bounds, cr);
        juce::DropShadow lightSh(juce::Colour(kShadowLight).withAlpha(0.6f), 4, { -2, -2 });
        lightSh.drawForPath(g, pillPath);
        juce::DropShadow darkSh(juce::Colour(kShadowDark).withAlpha(0.35f), 4, { 2, 2 });
        darkSh.drawForPath(g, pillPath);

        g.setColour(juce::Colour(kBgColor).brighter(highlighted ? 0.03f : 0.0f));
        g.fillRoundedRectangle(bounds, cr);
    }
}

// PopupMenu — neumorphic rounded panel
void VisceraLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    float cr = 14.0f;

    // Clear to transparent first (window is non-opaque)
    g.fillAll(juce::Colours::transparentBlack);

    // Drop shadow behind popup
    juce::DropShadow shadow(juce::Colour(kShadowDark).withAlpha(0.45f), 14, { 0, 4 });
    shadow.drawForRectangle(g, bounds.reduced(4).toNearestInt());

    // Slightly darker than panel background so the pill stands out
    auto bgCol = juce::Colour(kBgColor);
    auto pillCol = darkMode ? bgCol.brighter(0.06f) : bgCol.darker(0.03f);
    g.setColour(pillCol);
    g.fillRoundedRectangle(bounds.reduced(4), cr);

    // Visible border
    g.setColour(juce::Colour(kShadowDark).withAlpha(darkMode ? 0.5f : 0.22f));
    g.drawRoundedRectangle(bounds.reduced(4.5f), cr, 1.0f);
}

void VisceraLookAndFeel::preparePopupMenuWindow(juce::Component& newWindow)
{
    newWindow.setOpaque(false);
    newWindow.setRepaintsOnMouseActivity(true);
}

void VisceraLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                            bool isSeparator, bool isActive, bool isHighlighted,
                                            bool isTicked, bool /*hasSubMenu*/,
                                            const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                            const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        auto sepArea = area.reduced(12, 0);
        g.setColour(juce::Colour(kShadowDark).withAlpha(darkMode ? 0.4f : 0.15f));
        g.fillRect(sepArea.getX(), sepArea.getCentreY(), sepArea.getWidth(), 1);
        return;
    }

    auto r = area.reduced(6, 1);

    if (isHighlighted && isActive)
    {
        float pillCr = 6.0f;
        g.setColour(juce::Colour(kAccentColor).withAlpha(0.18f));
        g.fillRoundedRectangle(r.toFloat(), pillCr);
    }

    // Destructive items (starting with cross symbol) get a red tint
    bool isDestructive = text.startsWithChar(0x2716);

    auto textCol = isActive ? juce::Colour(kTextColor) : juce::Colour(kTextColor).withAlpha(0.4f);
    if (isHighlighted && isActive)
        textCol = isDestructive ? juce::Colour(0xFFE57373) : juce::Colour(kAccentColor);
    else if (isDestructive && isActive)
        textCol = juce::Colour(kTextColor).withAlpha(0.7f);

    g.setColour(textCol);
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    auto textArea = r.reduced(10, 0);
    g.drawText(text, textArea, juce::Justification::centredLeft);

    if (isTicked)
    {
        g.setColour(juce::Colour(kAccentColor));
        auto tickArea = r.removeFromRight(20);
        g.setFont(juce::Font(12.0f));
        g.drawText(juce::String::charToString(0x2713), tickArea, juce::Justification::centred);
    }
}

void VisceraLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                                     int /*standardMenuItemHeight*/,
                                                     int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 50;
        idealHeight = 8;
    }
    else
    {
        auto font = juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain);
        idealWidth = static_cast<int>(font.getStringWidthFloat(text)) + 40;
        idealHeight = 28;
    }
}

int VisceraLookAndFeel::getPopupMenuBorderSize()
{
    return 10; // 4px transparent margin + 6px inner padding
}

int VisceraLookAndFeel::getMenuWindowFlags()
{
    // Remove default window border/shadow — we draw our own
    return juce::ComponentPeer::windowIsTemporary;
}
