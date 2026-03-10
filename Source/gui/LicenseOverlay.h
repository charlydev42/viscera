// LicenseOverlay.h — Modal overlay for license key entry
//
// Shown when the plugin is not licensed.  Covers the editor's
// content area and blocks interaction until a valid key is entered.
//
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../license/LicenseManager.h"

class LicenseOverlay : public juce::Component,
                       private bb::LicenseManager::Listener
{
public:
    explicit LicenseOverlay(bb::LicenseManager& mgr);
    ~LicenseOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Called when activation succeeds (editor hides overlay)
    std::function<void()> onLicensed;

    // Reset the overlay to its initial state (clear input, status, etc.)
    void reset();

private:
    void licenseStateChanged(bool licensed) override;

    bb::LicenseManager& manager;

    juce::ImageComponent logoImage;
    juce::Label          subtitleLabel;
    juce::TextEditor     keyInput;
    juce::TextButton     activateBtn;
    juce::Label          statusLabel;

    bool activating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseOverlay)
};
