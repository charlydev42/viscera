// LicenseOverlay.cpp — License key entry overlay
#include "LicenseOverlay.h"
#include "ParasiteLookAndFeel.h"
#include "BinaryData.h"

LicenseOverlay::LicenseOverlay(bb::LicenseManager& mgr)
    : manager(mgr)
{
    manager.addListener(this);

    // Logo (picks light/dark variant)
    {
        auto img = ParasiteLookAndFeel::darkMode
            ? juce::ImageCache::getFromMemory(BinaryData::parasite_logo_dark_nodolph_png,
                                               BinaryData::parasite_logo_dark_nodolph_pngSize)
            : juce::ImageCache::getFromMemory(BinaryData::parasite_logo_light_nodolph_png,
                                               BinaryData::parasite_logo_light_nodolph_pngSize);
        logoImage.setImage(img, juce::RectanglePlacement::centred);
    }
    addAndMakeVisible(logoImage);

    subtitleLabel.setText("Enter your license key to activate",
                          juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                                     9.0f, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(subtitleLabel);

    keyInput.setMultiLine(false);
    keyInput.setReturnKeyStartsNewLine(false);
    keyInput.setTextToShowWhenEmpty("XXXX-XXXX-XXXX-XXXX-XXXX-XXXX",
                                    juce::Colours::grey);
    keyInput.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                                14.0f, juce::Font::plain));
    keyInput.setJustification(juce::Justification::centred);
    keyInput.setInputRestrictions(29, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
    keyInput.onReturnKey = [this] { activateBtn.triggerClick(); };
    // Auto-format: uppercase + insert dashes every 4 chars (PARA-XXXX-XXXX-...)
    keyInput.onTextChange = [this]
    {
        auto raw = keyInput.getText().toUpperCase()
                       .retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
                       .substring(0, 24); // 6 groups × 4 = 24 max

        juce::String formatted;
        for (int i = 0; i < raw.length(); ++i)
        {
            if (i > 0 && i % 4 == 0)
                formatted += "-";
            formatted += raw[i];
        }

        if (formatted != keyInput.getText())
        {
            keyInput.setText(formatted, false);
            keyInput.moveCaretToEnd();
        }
    };
    addAndMakeVisible(keyInput);

    activateBtn.setButtonText("Activate");
    activateBtn.onClick = [this]
    {
        auto key = keyInput.getText().trim().toUpperCase();
        if (key.isEmpty()) return;

        if (key.length() != 29)
        {
            statusLabel.setText("Please enter a complete license key.", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFCD5C5C));
            return;
        }

        if (activating) return;

        activating = true;
        statusLabel.setText("Activating...", juce::dontSendNotification);
        activateBtn.setEnabled(false);

        auto safeThis = juce::Component::SafePointer<LicenseOverlay>(this);
        manager.activate(key, [safeThis](bool ok, const juce::String& msg)
        {
            if (safeThis == nullptr) return;

            safeThis->activating = false;
            safeThis->activateBtn.setEnabled(true);

            auto col = ok ? juce::Colours::lightgreen : juce::Colours::indianred;
            safeThis->statusLabel.setColour(juce::Label::textColourId, col);
            safeThis->statusLabel.setText(msg, juce::dontSendNotification);

            if (ok && safeThis->onLicensed)
                safeThis->onLicensed();
        });
    };
    addAndMakeVisible(activateBtn);

    demoBtn.setButtonText("Use demo mode");
    demoBtn.onClick = [this]
    {
        writeDemoAcknowledged(true);
        if (onDemoRequested) onDemoRequested();
    };
    addAndMakeVisible(demoBtn);

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                                    10.0f, juce::Font::plain));
    addAndMakeVisible(statusLabel);

    refreshColors();
}

LicenseOverlay::~LicenseOverlay()
{
    manager.removeListener(this);
}

void LicenseOverlay::refreshColors()
{
    const bool dark = ParasiteLookAndFeel::darkMode;

    // Input field — softer palette in both modes so the overlay doesn't
    // feel like a Windows 95 dialog dropped in the middle of the plugin.
    // Dark input bg sits noticeably deeper than the card bg (0xFF2E3440) so
    // the text field reads as a sunken well, not a brighter rectangle.
    const juce::uint32 inBg      = dark ? 0xFF252B35 : 0xFFF0F0F0;
    const juce::uint32 inText    = dark ? 0xFFD8DEE9 : 0xFF444444;
    const juce::uint32 inOutline = dark ? 0xFF4C566A : 0xFFD0D0D0;
    const juce::uint32 inFocus   = dark ? 0xFF7B8494 : 0xFFBBBBBB;
    const juce::uint32 caret     = dark ? 0xFFD8DEE9 : 0xFF888888;
    const juce::uint32 hint      = dark ? 0xFF6E7686 : 0xFF9E9E9E;
    const juce::uint32 subtitle  = dark ? 0xFF8B92A0 : 0xFF7A7A7A;
    const juce::uint32 btnText   = dark ? 0xFFE0E4EC : 0xFF444444;
    const juce::uint32 btnBg     = dark ? 0xFF3B4252 : 0xFFE8E8E8;

    keyInput.setColour(juce::TextEditor::backgroundColourId,      juce::Colour(inBg));
    keyInput.setColour(juce::TextEditor::textColourId,            juce::Colour(inText));
    keyInput.setColour(juce::TextEditor::outlineColourId,         juce::Colour(inOutline));
    keyInput.setColour(juce::TextEditor::focusedOutlineColourId,  juce::Colour(inFocus));
    keyInput.setColour(juce::CaretComponent::caretColourId,       juce::Colour(caret));
    keyInput.setTextToShowWhenEmpty("XXXX-XXXX-XXXX-XXXX-XXXX-XXXX",
                                    juce::Colour(hint));

    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(subtitle));

    // Status label keeps its context-specific colour (green/red on action).
    // Reset its default base so the next success/error message starts from a
    // neutral adaptive shade instead of a stale one from the other theme.
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(subtitle));

    activateBtn.setColour(juce::TextButton::buttonColourId,   juce::Colour(btnBg));
    activateBtn.setColour(juce::TextButton::textColourOffId,  juce::Colour(btnText));
    activateBtn.setColour(juce::TextButton::textColourOnId,   juce::Colour(btnText));
    demoBtn.setColour(juce::TextButton::buttonColourId,       juce::Colour(btnBg));
    demoBtn.setColour(juce::TextButton::textColourOffId,      juce::Colour(btnText));
    demoBtn.setColour(juce::TextButton::textColourOnId,       juce::Colour(btnText));

    repaint();
}

static juce::File getDemoAckFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("Voidscan").getChildFile("Parasite")
           .getChildFile("demo_ack");
}

bool LicenseOverlay::readDemoAcknowledged()
{
    return getDemoAckFile().existsAsFile();
}

void LicenseOverlay::writeDemoAcknowledged(bool ack)
{
    auto f = getDemoAckFile();
    f.getParentDirectory().createDirectory();
    if (ack)
    {
        // Atomic write — concurrent instances can't corrupt each other
        juce::TemporaryFile tmp(f);
        tmp.getFile().replaceWithText("ack");
        tmp.overwriteTargetFileWithTemporary();
    }
    else if (f.existsAsFile())
    {
        f.deleteFile();
    }
}

void LicenseOverlay::reset()
{
    keyInput.clear();
    statusLabel.setText("", juce::dontSendNotification);
    activateBtn.setEnabled(true);
    activating = false;
}

void LicenseOverlay::licenseStateChanged(bool licensed)
{
    if (licensed && onLicensed)
        onLicensed();
}

void LicenseOverlay::paint(juce::Graphics& g)
{
    // Semi-transparent dark background
    g.fillAll(juce::Colour(ParasiteLookAndFeel::kBgColor));

    // Central card
    auto card = getLocalBounds().reduced(60, 80);
    float cr = 10.0f;

    // Neumorphic shadows
    juce::Path cardPath;
    cardPath.addRoundedRectangle(card.toFloat(), cr);
    juce::DropShadow(juce::Colour(ParasiteLookAndFeel::kShadowLight).withAlpha(0.6f),
                     6, { -3, -3 }).drawForPath(g, cardPath);
    juce::DropShadow(juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(0.5f),
                     8, { 4, 4 }).drawForPath(g, cardPath);

    g.setColour(juce::Colour(ParasiteLookAndFeel::kBgColor).brighter(0.06f));
    g.fillRoundedRectangle(card.toFloat(), cr);

    g.setColour(juce::Colour(ParasiteLookAndFeel::kHeaderBg));
    g.drawRoundedRectangle(card.toFloat(), cr, 1.0f);
}

void LicenseOverlay::resized()
{
    auto card = getLocalBounds().reduced(60, 80);

    // Main content: logo + input + button + status (centered)
    int logoH     = 50;
    int inputH    = 26;
    int btnH      = 28;
    int statusH   = 20;
    int gap       = 10;
    int totalH    = logoH + gap*2 + inputH + gap*2 + btnH + gap + statusH;

    // Center vertically within card
    auto area = card.withSizeKeepingCentre(card.getWidth() - 60, totalH)
                    .translated(0, -20);

    logoImage.setBounds(area.removeFromTop(logoH).reduced(60, 0));
    area.removeFromTop(gap * 2);

    int inputW = juce::jmin(320, area.getWidth());
    keyInput.setBounds(area.removeFromTop(inputH).withSizeKeepingCentre(inputW, inputH + 4).translated(0, -2));

    area.removeFromTop(gap);
    {
        auto btnRow = area.removeFromTop(btnH);
        int rowW = juce::jmin(260, btnRow.getWidth());
        auto row = btnRow.withSizeKeepingCentre(rowW, btnH);
        activateBtn.setBounds(row.removeFromLeft(120));
        row.removeFromLeft(20);
        demoBtn.setBounds(row.removeFromLeft(120));
    }

    area.removeFromTop(gap);
    statusLabel.setBounds(area.removeFromTop(statusH));

    // Subtitle as discrete hint at bottom of card
    auto cardInner = card.reduced(20, 0);
    subtitleLabel.setBounds(cardInner.removeFromBottom(28).reduced(0, 4));
}
