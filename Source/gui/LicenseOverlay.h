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

    // Called when the user chooses "Use demo mode" — editor hides overlay,
    // persists the choice so it doesn't reappear next session, and shows a
    // small activate banner instead.
    std::function<void()> onDemoRequested;

    // Reset the overlay to its initial state (clear input, status, etc.)
    void reset();

    // Re-apply colors based on the current ParasiteLookAndFeel::darkMode state.
    // Called on construction and whenever dark mode toggles.
    void refreshColors();

    // Persistent dismissal flag — written to an atomic file in the shared
    // prefs dir so all instances (and future sessions) see the user's choice.
    static bool readDemoAcknowledged();
    static void writeDemoAcknowledged(bool ack);

private:
    void licenseStateChanged(bool licensed) override;

    bb::LicenseManager& manager;

    juce::ImageComponent logoImage;
    juce::Label          subtitleLabel;
    juce::TextEditor     keyInput;
    juce::TextButton     activateBtn;
    juce::TextButton     demoBtn;
    juce::Label          statusLabel;

    bool activating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseOverlay)
};
