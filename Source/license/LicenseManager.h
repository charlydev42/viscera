// LicenseManager.h — License activation & periodic verification
//
// Owns the full lifecycle:
//   1. Load cached license from disk on construction
//   2. Expose isLicensed() for GUI gating
//   3. activate() — async POST to /auth/activate
//   4. verify()   — async POST to /auth/verify (auto-scheduled every 48 h)
//   5. Encrypted local cache with 7-day offline grace period
//
#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace bb {

class LicenseManager : private juce::Timer
{
public:
    LicenseManager();
    ~LicenseManager() override;

    // ── Query ────────────────────────────────────────────────────────
    bool isLicensed() const;
    juce::String getLicenseKey() const;

    // ── Actions ──────────────────────────────────────────────────────
    using Callback = std::function<void(bool success, const juce::String& message)>;
    void activate(const juce::String& licenseKey, Callback callback);
    void verify(Callback callback);
    void deactivate();

    // ── Observer ─────────────────────────────────────────────────────
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void licenseStateChanged(bool licensed) = 0;
    };
    void addListener(Listener* l)    { listeners_.add(l); }
    void removeListener(Listener* l) { listeners_.remove(l); }

    // ── JWT token from last activation ─────────────────────────────
    juce::String getActivationToken() const;

    // ── Public static helpers (shared with CloudPresetManager) ─────
    static juce::String canonicalJson(const juce::var& value);
    static juce::String hmacSha256(const juce::String& key, const juce::String& data);
    static juce::String getMachineId();

private:
    void timerCallback() override;

    // HTTP
    struct HttpResponse { int statusCode = 0; juce::String body; };
    HttpResponse httpPost(const juce::String& endpoint, const juce::var& jsonBody) const;

    // Request signing (X-TD-Signature)
    juce::String buildSignature(const juce::String& canonicalBody) const;

    // Machine fingerprint
    static juce::String getMachineName();

    // Encrypted cache
    void saveCache() const;
    void loadCache();
    void clearCache();
    static juce::File getCacheFile();
    static juce::MemoryBlock xorCipher(const void* data, size_t size,
                                       const juce::String& key);

    // State (guarded by stateLock)
    mutable juce::CriticalSection stateLock;
    bool        licensed_     = false;
    juce::String licenseKey_;
    juce::String machineId_;
    juce::String userId_;
    juce::String activationToken_;  // JWT from last activation
    int64_t     activatedAt_  = 0;
    int64_t     lastVerified_ = 0;

    juce::ListenerList<Listener> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseManager)
};

} // namespace bb
