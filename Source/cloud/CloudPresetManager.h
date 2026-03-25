// CloudPresetManager.h — Bidirectional cloud preset sync
//
// Owns cloud lifecycle:
//   1. JWT token management (from license activation or /auth/token-from-license)
//   2. Upload on save, delete on delete
//   3. Full bidirectional sync on launch / refresh / manual trigger
//   4. Offline-first: local ops always succeed, cloud is best-effort
//
#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>

namespace bb { class LicenseManager; }
class ParasiteProcessor;

class CloudPresetManager
{
public:
    CloudPresetManager(ParasiteProcessor& proc, bb::LicenseManager& license);
    ~CloudPresetManager()
    {
        alive_->store(false);
        shuttingDown_.store(true);
        for (int i = 0; i < 120 && pendingThreads_.load() > 0; ++i)
            juce::Thread::sleep(100);
    }

    // ── Sync triggers ──────────────────────────────────────────────
    void syncAll();                               // Full bidirectional sync (async)
    void uploadPreset(const juce::String& uuid);  // Fire-and-forget after save
    void deletePreset(const juce::String& uuid);  // Fire-and-forget after delete

    // ── JWT ────────────────────────────────────────────────────────
    bool hasValidToken() const;

    // ── Status ─────────────────────────────────────────────────────
    bool isSyncing() const { return syncing_.load(); }

    // ── Observer ───────────────────────────────────────────────────
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void cloudSyncCompleted(bool success) = 0;
    };
    void addListener(Listener* l)    { listeners_.add(l); }
    void removeListener(Listener* l) { listeners_.remove(l); }

private:
    ParasiteProcessor&  proc_;
    bb::LicenseManager& license_;

    // ── HTTP ───────────────────────────────────────────────────────
    struct HttpResponse { int statusCode = 0; juce::String body; };
    HttpResponse httpRequest(const juce::String& method,
                             const juce::String& endpoint,
                             const juce::String& jsonBody = {}) const;

    // ── JWT management ─────────────────────────────────────────────
    mutable juce::CriticalSection tokenLock_;
    juce::String jwtToken_;
    int64_t tokenExpiresAt_ = 0;

    void   storeToken(const juce::String& token);
    juce::String getToken() const;
    bool   isTokenExpired() const;
    bool   ensureToken();           // Blocking refresh if needed (call from worker thread)
    void   refreshTokenSync();      // Blocking POST /auth/token-from-license

    void   saveTokenToCache() const;
    void   loadTokenFromCache();
    static juce::File getTokenCacheFile();

    // ── Sync state ─────────────────────────────────────────────────
    std::atomic<bool> syncing_ { false };

    void performSync();                                // Worker thread body
    void applyServerPresets(const juce::var& serverPresets);  // On message thread

    // ── Helpers ────────────────────────────────────────────────────
    juce::var buildLocalPresetArray() const;
    void writePresetToDisk(const juce::String& uuid, const juce::String& name,
                           const juce::String& category, const juce::String& pack,
                           const juce::String& data, int version,
                           const juce::String& updatedAt);
    // Deletion log (tracks locally deleted presets between syncs)
    void   logDeletion(const juce::String& uuid);
    juce::var loadDeletionLog() const;
    void   clearDeletionLog();
    static juce::File getDeletionLogFile();

    juce::ListenerList<Listener> listeners_;

    // ── Thread safety ────────────────────────────────────────────────
    std::atomic<bool> shuttingDown_{false};
    std::atomic<int>  pendingThreads_{0};
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CloudPresetManager)
};
