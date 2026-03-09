// LicenseOverlay.cpp — License key entry overlay
#include "LicenseOverlay.h"
#include "VisceraLookAndFeel.h"
#include "BinaryData.h"

LicenseOverlay::LicenseOverlay(bb::LicenseManager& mgr)
    : manager(mgr)
{
    manager.addListener(this);

    // Logo (picks light/dark variant)
    {
        auto img = VisceraLookAndFeel::darkMode
            ? juce::ImageCache::getFromMemory(BinaryData::viscera_logo_dark_nodolph_png,
                                               BinaryData::viscera_logo_dark_nodolph_pngSize)
            : juce::ImageCache::getFromMemory(BinaryData::viscera_logo_light_nodolph_png,
                                               BinaryData::viscera_logo_light_nodolph_pngSize);
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
    // White-ish input field for contrast
    keyInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFFF0F0F0));
    keyInput.setColour(juce::TextEditor::textColourId,       juce::Colours::black);
    keyInput.setColour(juce::TextEditor::outlineColourId,    juce::Colour(0xFFD0D0D0));
    keyInput.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFFBBBBBB));
    keyInput.setColour(juce::CaretComponent::caretColourId,     juce::Colour(0xFF888888));
    keyInput.onReturnKey = [this] { activateBtn.triggerClick(); };
    // Auto-format: uppercase + insert dashes every 4 chars (VISC-XXXX-XXXX-...)
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
        if (activating) return;

        activating = true;
        statusLabel.setText("Activating...", juce::dontSendNotification);
        activateBtn.setEnabled(false);

        manager.activate(key, [this](bool ok, const juce::String& msg)
        {
            activating = false;
            activateBtn.setEnabled(true);

            auto col = ok ? juce::Colours::lightgreen : juce::Colours::indianred;
            statusLabel.setColour(juce::Label::textColourId, col);
            statusLabel.setText(msg, juce::dontSendNotification);

            if (ok && onLicensed)
                onLicensed();
        });
    };
    addAndMakeVisible(activateBtn);

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                                    10.0f, juce::Font::plain));
    addAndMakeVisible(statusLabel);
}

LicenseOverlay::~LicenseOverlay()
{
    manager.removeListener(this);
}

void LicenseOverlay::licenseStateChanged(bool licensed)
{
    if (licensed && onLicensed)
        onLicensed();
}

void LicenseOverlay::paint(juce::Graphics& g)
{
    // Semi-transparent dark background
    g.fillAll(juce::Colour(VisceraLookAndFeel::kBgColor));

    // Central card
    auto card = getLocalBounds().reduced(60, 80);
    float cr = 10.0f;

    // Neumorphic shadows
    juce::Path cardPath;
    cardPath.addRoundedRectangle(card.toFloat(), cr);
    juce::DropShadow(juce::Colour(VisceraLookAndFeel::kShadowLight).withAlpha(0.6f),
                     6, { -3, -3 }).drawForPath(g, cardPath);
    juce::DropShadow(juce::Colour(VisceraLookAndFeel::kShadowDark).withAlpha(0.5f),
                     8, { 4, 4 }).drawForPath(g, cardPath);

    g.setColour(juce::Colour(VisceraLookAndFeel::kBgColor).brighter(0.06f));
    g.fillRoundedRectangle(card.toFloat(), cr);

    g.setColour(juce::Colour(VisceraLookAndFeel::kHeaderBg));
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
    activateBtn.setBounds(area.removeFromTop(btnH).withSizeKeepingCentre(120, btnH));

    area.removeFromTop(gap);
    statusLabel.setBounds(area.removeFromTop(statusH));

    // Subtitle as discrete hint at bottom of card
    auto cardInner = card.reduced(20, 0);
    subtitleLabel.setBounds(cardInner.removeFromBottom(28).reduced(0, 4));
}
