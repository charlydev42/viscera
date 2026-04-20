// CloudPresetManager.cpp — Cloud preset sync implementation
#include "CloudPresetManager.h"
#include "../license/LicenseManager.h"
#include "../license/LicenseConfig.h"
#include "../PluginProcessor.h"
#include "../util/Logger.h"

using namespace bb::license;

static constexpr int kTokenMarginMs = 60 * 1000;  // refresh 60s before expiry
static constexpr int kMaxRetries    = 5;          // 500ms, 1s, 2s, 4s, 8s

// =====================================================================
// Construction / destruction
// =====================================================================

CloudPresetManager::CloudPresetManager(ParasiteProcessor& proc,
                                       bb::LicenseManager& license)
    : proc_(proc), license_(license)
{
    loadTokenFromCache();

    // Seed from activation token if we don't have one cached
    if (jwtToken_.isEmpty())
    {
        auto seed = license_.getActivationToken();
        if (seed.isNotEmpty())
        {
            storeToken(seed);
            saveTokenToCache();
        }
    }

}

// =====================================================================
// JWT management
// =====================================================================

void CloudPresetManager::storeToken(const juce::String& token)
{
    const juce::ScopedLock sl(tokenLock_);
    jwtToken_ = token;
    tokenExpiresAt_ = 0;

    // Parse expiry from JWT payload (base64url middle segment)
    auto parts = juce::StringArray::fromTokens(token, ".", "");
    if (parts.size() >= 2)
    {
        auto payload = parts[1];
        payload = payload.replace("-", "+").replace("_", "/");
        while (payload.length() % 4 != 0) payload += "=";

        juce::MemoryOutputStream decoded;
        if (juce::Base64::convertFromBase64(decoded, payload))
        {
            auto json = juce::JSON::parse(
                juce::String::fromUTF8(static_cast<const char*>(decoded.getData()),
                                       static_cast<int>(decoded.getDataSize())));
            auto exp = static_cast<int64_t>((double)json.getProperty("exp", 0));
            tokenExpiresAt_ = exp * 1000; // seconds → ms
        }
    }
}

juce::String CloudPresetManager::getToken() const
{
    const juce::ScopedLock sl(tokenLock_);
    return jwtToken_;
}

bool CloudPresetManager::isTokenExpired() const
{
    const juce::ScopedLock sl(tokenLock_);
    if (jwtToken_.isEmpty()) return true;
    if (tokenExpiresAt_ == 0) return false; // couldn't parse, assume valid
    return juce::Time::currentTimeMillis() > (tokenExpiresAt_ - kTokenMarginMs);
}

bool CloudPresetManager::hasValidToken() const
{
    return !isTokenExpired();
}

bool CloudPresetManager::ensureToken()
{
    if (!isTokenExpired()) return true;

    // Try activation token first
    auto seed = license_.getActivationToken();
    if (seed.isNotEmpty())
    {
        storeToken(seed);
        if (!isTokenExpired())
        {
            saveTokenToCache();
            return true;
        }
    }

    // Refresh via /auth/token-from-license
    refreshTokenSync();
    return !isTokenExpired();
}

void CloudPresetManager::refreshTokenSync()
{
    auto licenseKey = license_.getLicenseKey();
    auto machineId  = bb::LicenseManager::getMachineId();
    if (licenseKey.isEmpty()) return;

    auto* body = new juce::DynamicObject();
    body->setProperty("licenseKey", licenseKey);
    body->setProperty("machineId",  machineId);
    juce::var jsonVar(body);

    auto canonical = bb::LicenseManager::canonicalJson(jsonVar);
    auto ts     = juce::String(juce::Time::currentTimeMillis());
    auto secret = decodeSecret();
    auto hmac   = bb::LicenseManager::hmacSha256(secret, ts + ":" + canonical);
    auto sigHeader = ts + "." + hmac;

    juce::String headers = "Content-Type: application/json\r\n"
                           "X-TD-Signature: " + sigHeader + "\r\n";

    auto url = juce::URL(juce::String(kApiBaseUrl) + "/auth/token-from-license")
                   .withPOSTData(canonical);
    int statusCode = 0;
    auto stream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
            .withExtraHeaders(headers)
            .withConnectionTimeoutMs(kHttpTimeoutMs)
            .withStatusCode(&statusCode));

    if (statusCode == 200 && stream)
    {
        auto respBody = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(respBody);
        auto token = json.getProperty("token", "").toString();
        if (token.isNotEmpty())
        {
            storeToken(token);
            saveTokenToCache();
        }
    }
}

// ── Token cache ────────────────────────────────────────────────────

juce::File CloudPresetManager::getTokenCacheFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("Voidscan").getChildFile("Parasite")
           .getChildFile("cloud_token.dat");
}

void CloudPresetManager::saveTokenToCache() const
{
    auto token = getToken();
    if (token.isEmpty()) return;

    auto machineId = bb::LicenseManager::getMachineId();
    auto keyBytes  = machineId.toRawUTF8();
    auto keyLen    = static_cast<size_t>(machineId.getNumBytesAsUTF8());
    if (keyLen == 0) return;

    auto data    = token.toRawUTF8();
    auto dataLen = static_cast<size_t>(token.getNumBytesAsUTF8());

    juce::MemoryBlock encrypted(dataLen);
    auto* src = reinterpret_cast<const uint8_t*>(data);
    auto* dst = static_cast<uint8_t*>(encrypted.getData());
    for (size_t i = 0; i < dataLen; ++i)
        dst[i] = src[i] ^ static_cast<uint8_t>(keyBytes[i % keyLen]);

    auto file = getTokenCacheFile();
    file.getParentDirectory().createDirectory();
    if (!file.replaceWithData(encrypted.getData(), encrypted.getSize()))
        DBG("CloudPresetManager: failed to save token cache to " + file.getFullPathName());
}

void CloudPresetManager::loadTokenFromCache()
{
    auto file = getTokenCacheFile();
    if (!file.existsAsFile()) return;

    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() == 0) return;

    auto machineId = bb::LicenseManager::getMachineId();
    auto keyBytes  = machineId.toRawUTF8();
    auto keyLen    = static_cast<size_t>(machineId.getNumBytesAsUTF8());
    if (keyLen == 0) return;

    juce::MemoryBlock decrypted(data.getSize());
    auto* src = static_cast<const uint8_t*>(data.getData());
    auto* dst = static_cast<uint8_t*>(decrypted.getData());
    for (size_t i = 0; i < data.getSize(); ++i)
        dst[i] = src[i] ^ static_cast<uint8_t>(keyBytes[i % keyLen]);

    auto token = juce::String::fromUTF8(
        static_cast<const char*>(decrypted.getData()),
        static_cast<int>(decrypted.getSize()));

    if (token.isNotEmpty())
        storeToken(token);
}

// =====================================================================
// HTTP layer — JWT-authenticated requests
// =====================================================================

CloudPresetManager::HttpResponse CloudPresetManager::httpRequest(
    const juce::String& method,
    const juce::String& endpoint,
    const juce::String& jsonBody) const
{
    auto token = getToken();
    juce::String headers = "Content-Type: application/json\r\n";
    if (token.isNotEmpty())
        headers += "Authorization: Bearer " + token + "\r\n";

    auto url = juce::URL(juce::String(kApiBaseUrl) + endpoint);
    if (jsonBody.isNotEmpty())
        url = url.withPOSTData(jsonBody);

    int statusCode = 0;
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
        .withExtraHeaders(headers)
        .withConnectionTimeoutMs(kHttpTimeoutMs)
        .withStatusCode(&statusCode)
        .withHttpRequestCmd(method);

    auto stream = url.createInputStream(opts);

    HttpResponse resp;
    resp.statusCode = statusCode;
    if (stream)
        resp.body = stream->readEntireStreamAsString();
    return resp;
}

// Transient-failure codes worth retrying. 4xx other than 408/429 are user
// errors (malformed request, auth, not-found) and won't fix themselves.
bool CloudPresetManager::isRetryableStatus(int code) noexcept
{
    if (code == 0) return true;               // network error / DNS / timeout
    if (code == 408 || code == 429) return true; // request timeout / rate limit
    if (code >= 500 && code < 600) return true;  // server error
    return false;
}

CloudPresetManager::HttpResponse
CloudPresetManager::httpRequestWithRetries(const juce::String& method,
                                            const juce::String& endpoint,
                                            const juce::String& jsonBody) const
{
    HttpResponse resp;
    int delayMs = 500;
    for (int attempt = 0; attempt < kMaxRetries; ++attempt)
    {
        resp = httpRequest(method, endpoint, jsonBody);
        if (!isRetryableStatus(resp.statusCode))
            return resp; // success or non-retryable failure (401/403/404/…)

        // Last attempt — return whatever we got, caller decides what to do
        if (attempt == kMaxRetries - 1)
            return resp;

        // Wait with ±20% jitter in 100ms slices so we can bail early on shutdown.
        const int jitter = juce::Random::getSystemRandom().nextInt(delayMs / 5 + 1) - delayMs / 10;
        int remaining = juce::jmax(0, delayMs + jitter);
        while (remaining > 0)
        {
            if (shuttingDown_.load(std::memory_order_relaxed))
                return resp;
            const int slice = juce::jmin(100, remaining);
            juce::Thread::sleep(slice);
            remaining -= slice;
        }
        delayMs = juce::jmin(delayMs * 2, 8000); // cap at 8 s
    }
    return resp;
}

// ─ Persistent upload retry queue ──────────────────────────────────────

juce::File CloudPresetManager::getPendingUploadsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Voidscan").getChildFile("Parasite");
    dir.createDirectory();
    return dir.getChildFile("cloud_pending.json");
}

juce::StringArray CloudPresetManager::loadPendingUploads() const
{
    juce::StringArray out;
    auto file = getPendingUploadsFile();
    if (!file.existsAsFile()) return out;
    auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (auto* arr = parsed.getArray())
        for (const auto& v : *arr)
            if (v.isString())
                out.add(v.toString());
    return out;
}

void CloudPresetManager::enqueuePendingUpload(const juce::String& uuid)
{
    if (uuid.isEmpty()) return;
    auto pending = loadPendingUploads();
    if (pending.contains(uuid)) return; // dedup
    pending.add(uuid);

    juce::Array<juce::var> arr;
    for (const auto& u : pending) arr.add(juce::var(u));

    auto file = getPendingUploadsFile();
    juce::TemporaryFile tmp(file);
    tmp.getFile().replaceWithText(juce::JSON::toString(juce::var(arr), true));
    tmp.overwriteTargetFileWithTemporary();
    BB_LOG_INFO("Queued preset upload for later retry: " + uuid);
}

void CloudPresetManager::removePendingUpload(const juce::String& uuid)
{
    auto pending = loadPendingUploads();
    if (!pending.contains(uuid)) return;
    pending.removeString(uuid);

    juce::Array<juce::var> arr;
    for (const auto& u : pending) arr.add(juce::var(u));

    auto file = getPendingUploadsFile();
    juce::TemporaryFile tmp(file);
    tmp.getFile().replaceWithText(juce::JSON::toString(juce::var(arr), true));
    tmp.overwriteTargetFileWithTemporary();
}

void CloudPresetManager::drainPendingUploads()
{
    auto pending = loadPendingUploads();
    if (pending.isEmpty()) return;
    BB_LOG_INFO("Draining " + juce::String(pending.size()) + " pending cloud uploads");
    for (const auto& uuid : pending)
        uploadPreset(uuid); // uploadPreset removes on success / re-queues on failure
}

// =====================================================================
// Upload preset (after save)
// =====================================================================

void CloudPresetManager::uploadPreset(const juce::String& uuid)
{
    if (uuid.isEmpty() || !license_.isLicensed()) return;

    ++pendingThreads_;
    auto alive = alive_;

    juce::Thread::launch([this, uuid, alive]
    {
        if (!ensureToken()) { --pendingThreads_; return; }
        if (shuttingDown_.load()) { --pendingThreads_; return; }

        // Read the preset file to get its current data
        auto dir = ParasiteProcessor::getUserPresetsDir();
        juce::Array<juce::File> files;
        files.addArray(dir.findChildFiles(juce::File::findFiles, false, "*.prst"));

        for (auto& f : files)
        {
            auto xml = juce::parseXML(f);
            if (!xml) continue;
            if (xml->getStringAttribute("uuid") != uuid) continue;

            auto* body = new juce::DynamicObject();
            body->setProperty("uuid",     uuid);
            body->setProperty("name",     xml->getStringAttribute("name", f.getFileNameWithoutExtension()));
            body->setProperty("category", xml->getStringAttribute("category", "User"));
            body->setProperty("pack",     xml->getStringAttribute("pack", "User"));
            body->setProperty("data",     xml->toString());

            auto json = juce::JSON::toString(juce::var(body), true);

            if (shuttingDown_.load()) { --pendingThreads_; return; }

            // Try create first — POST /presets with retries on transient
            // failures (5xx / timeout / rate limit).
            auto resp = httpRequestWithRetries("POST", "/presets", json);
            if (resp.statusCode == 409)
                resp = httpRequestWithRetries("PATCH", "/presets/" + uuid, json);

            if (resp.statusCode == 401)
            {
                if (shuttingDown_.load()) { --pendingThreads_; return; }
                // Token expired — refresh and retry once with new token.
                refreshTokenSync();
                resp = httpRequestWithRetries("POST", "/presets", json);
                if (resp.statusCode == 409)
                    resp = httpRequestWithRetries("PATCH", "/presets/" + uuid, json);
            }

            const bool success = (resp.statusCode == 200 || resp.statusCode == 201);
            if (success)
            {
                removePendingUpload(uuid);
                BB_LOG_INFO("Uploaded preset " + uuid);
            }
            else if (isRetryableStatus(resp.statusCode))
            {
                // Still transient after backoff — queue for next session.
                BB_LOG_WARN("Cloud upload transient failure for " + uuid
                            + " (HTTP " + juce::String(resp.statusCode) + ") — queued");
                enqueuePendingUpload(uuid);
            }
            else
            {
                // Hard error (auth / bad request / not found) — don't retry.
                BB_LOG_ERROR("Cloud upload failed for " + uuid
                             + " (HTTP " + juce::String(resp.statusCode) + ")");
                removePendingUpload(uuid);
            }

            break;
        }

        --pendingThreads_;
    });
}

// =====================================================================
// Delete preset (after local delete)
// =====================================================================

void CloudPresetManager::deletePreset(const juce::String& uuid)
{
    if (uuid.isEmpty() || !license_.isLicensed()) return;

    logDeletion(uuid);

    ++pendingThreads_;
    auto alive = alive_;

    juce::Thread::launch([this, uuid, alive]
    {
        if (!ensureToken()) { --pendingThreads_; return; }
        if (shuttingDown_.load()) { --pendingThreads_; return; }

        auto resp = httpRequestWithRetries("DELETE", "/presets/" + uuid, {});

        if (resp.statusCode == 401)
        {
            if (shuttingDown_.load()) { --pendingThreads_; return; }
            refreshTokenSync();
            resp = httpRequestWithRetries("DELETE", "/presets/" + uuid, {});
        }

        if (resp.statusCode != 204 && resp.statusCode != 404)
            BB_LOG_WARN("Cloud delete failed for " + uuid
                        + " (HTTP " + juce::String(resp.statusCode) + ")");

        --pendingThreads_;
    });
}

// =====================================================================
// Full bidirectional sync
// =====================================================================

void CloudPresetManager::syncAll()
{
    if (!license_.isLicensed()) return;
    if (syncing_.exchange(true)) return; // already syncing

    ++pendingThreads_;
    auto alive = alive_;

    juce::Thread::launch([this, alive]
    {
        performSync();
        syncing_.store(false);
        --pendingThreads_;
    });
}

void CloudPresetManager::performSync()
{
    auto alive = alive_;

    if (!ensureToken())
    {
        juce::MessageManager::callAsync([this, alive] {
            if (!alive->load()) return;
            listeners_.call(&Listener::cloudSyncCompleted, false);
        });
        return;
    }

    if (shuttingDown_.load()) return;

    // Build local preset array
    auto localPresets = buildLocalPresetArray();

    // Include deletion log
    auto deletions = loadDeletionLog();
    if (deletions.isArray())
    {
        for (int i = 0; i < deletions.getArray()->size(); ++i)
        {
            const auto& entry = (*deletions.getArray())[i];
            auto* delObj = new juce::DynamicObject();
            delObj->setProperty("uuid",      entry.getProperty("uuid", ""));
            delObj->setProperty("name",      "deleted");
            delObj->setProperty("category",  "User");
            delObj->setProperty("pack",      "User");
            delObj->setProperty("data",      "");
            delObj->setProperty("version",   1);
            delObj->setProperty("updatedAt", entry.getProperty("updatedAt", ""));
            delObj->setProperty("deleted",   true);
            localPresets.getArray()->add(juce::var(delObj));
        }
    }

    // Build request — use standard JSON (no canonical needed, JWT auth not plugin sig)
    auto* reqBody = new juce::DynamicObject();
    reqBody->setProperty("presets", localPresets);
    auto json = juce::JSON::toString(juce::var(reqBody), true);

    auto resp = httpRequestWithRetries("POST", "/presets/sync", json);

    if (resp.statusCode == 401)
    {
        if (shuttingDown_.load()) return;
        refreshTokenSync();
        resp = httpRequestWithRetries("POST", "/presets/sync", json);
    }

    if (shuttingDown_.load()) return;

    if (resp.statusCode != 200)
    {
        BB_LOG_WARN("Cloud sync failed (HTTP " + juce::String(resp.statusCode) + ")");
        juce::MessageManager::callAsync([this, alive] {
            if (!alive->load()) return;
            listeners_.call(&Listener::cloudSyncCompleted, false);
        });
        return;
    }

    auto responseJson = juce::JSON::parse(resp.body);
    auto serverPresets = responseJson.getProperty("presets", juce::var());

    if (!serverPresets.isArray())
    {
        juce::MessageManager::callAsync([this, alive] {
            if (!alive->load()) return;
            listeners_.call(&Listener::cloudSyncCompleted, false);
        });
        return;
    }

    // Apply on message thread
    juce::MessageManager::callAsync([this, alive, serverPresets] {
        if (!alive->load()) return;
        applyServerPresets(serverPresets);
        clearDeletionLog();
        proc_.buildPresetRegistry();
        // Sync succeeded → we have connectivity, a good moment to drain
        // any uploads that failed in a previous session.
        drainPendingUploads();
        listeners_.call(&Listener::cloudSyncCompleted, true);
    });
}

void CloudPresetManager::applyServerPresets(const juce::var& serverPresets)
{
    if (!serverPresets.isArray()) return;

    // Build map of local UUIDs → files
    auto dir = ParasiteProcessor::getUserPresetsDir();
    juce::Array<juce::File> localFiles;
    localFiles.addArray(dir.findChildFiles(juce::File::findFiles, false, "*.prst"));

    std::map<juce::String, juce::File> localUuidMap;
    std::map<juce::String, juce::String> localUpdatedAtMap;

    for (auto& f : localFiles)
    {
        auto xml = juce::parseXML(f);
        if (!xml) continue;
        auto uuid = xml->getStringAttribute("uuid");
        if (uuid.isNotEmpty())
        {
            localUuidMap[uuid] = f;
            localUpdatedAtMap[uuid] = xml->getStringAttribute("updatedAt", "");
        }
    }

    for (int i = 0; i < serverPresets.getArray()->size(); ++i)
    {
        const auto& preset = (*serverPresets.getArray())[i];
        auto uuid      = preset.getProperty("uuid", "").toString();
        auto deleted   = (bool)preset.getProperty("deleted", false);
        auto name      = preset.getProperty("name", "").toString();
        auto category  = preset.getProperty("category", "User").toString();
        auto pack      = preset.getProperty("pack", "User").toString();
        auto data      = preset.getProperty("data", "").toString();
        auto version   = (int)preset.getProperty("version", 1);
        auto updatedAt = preset.getProperty("updatedAt", "").toString();

        if (uuid.isEmpty()) continue;

        if (deleted)
        {
            // Delete local if exists
            if (localUuidMap.count(uuid) > 0)
                localUuidMap[uuid].deleteFile();
            continue;
        }

        bool localExists = localUuidMap.count(uuid) > 0;

        if (!localExists)
        {
            // New from server → write to disk
            writePresetToDisk(uuid, name, category, pack, data, version, updatedAt);
        }
        else
        {
            // Compare timestamps — server newer wins
            auto localTime  = localUpdatedAtMap[uuid];
            if (updatedAt > localTime)
                writePresetToDisk(uuid, name, category, pack, data, version, updatedAt);
        }
    }
}


// =====================================================================
// Helpers
// =====================================================================

juce::var CloudPresetManager::buildLocalPresetArray() const
{
    juce::Array<juce::var> arr;
    auto dir = ParasiteProcessor::getUserPresetsDir();
    juce::Array<juce::File> files;
    files.addArray(dir.findChildFiles(juce::File::findFiles, false, "*.prst"));

    for (auto& f : files)
    {
        auto xml = juce::parseXML(f);
        if (!xml) continue;

        auto uuid = xml->getStringAttribute("uuid");

        // Migrate legacy presets without UUID
        if (uuid.isEmpty())
        {
            uuid = juce::Uuid().toString();
            xml->setAttribute("uuid", uuid);
            xml->setAttribute("version", 1);
            xml->setAttribute("updatedAt",
                f.getLastModificationTime().toISO8601(true));
            juce::TemporaryFile tmp(f);
            if (xml->writeTo(tmp.getFile()))
                tmp.overwriteTargetFileWithTemporary();
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty("uuid",      uuid);
        obj->setProperty("name",      xml->getStringAttribute("name", f.getFileNameWithoutExtension()));
        obj->setProperty("category",  xml->getStringAttribute("category", "User"));
        obj->setProperty("pack",      xml->getStringAttribute("pack", "User"));
        obj->setProperty("data",      xml->toString());
        obj->setProperty("version",   xml->getIntAttribute("version", 1));
        obj->setProperty("updatedAt", xml->getStringAttribute("updatedAt",
            juce::Time::getCurrentTime().toISO8601(true)));
        obj->setProperty("deleted",   false);

        arr.add(juce::var(obj));
    }

    return juce::var(arr);
}

void CloudPresetManager::writePresetToDisk(const juce::String& uuid,
                                            const juce::String& name,
                                            const juce::String& category,
                                            const juce::String& pack,
                                            const juce::String& data,
                                            int version,
                                            const juce::String& updatedAt)
{
    if (data.isEmpty()) return;

    // Parse the preset XML data from server
    auto xml = juce::parseXML(data);
    if (!xml) return;

    // Ensure cloud metadata is set
    xml->setAttribute("uuid", uuid);
    xml->setAttribute("name", name);
    xml->setAttribute("category", category);
    xml->setAttribute("pack", pack);
    xml->setAttribute("version", version);
    xml->setAttribute("updatedAt", updatedAt);

    auto safeName = juce::File::createLegalFileName(name);
    if (safeName.isEmpty()) safeName = uuid;

    auto dir  = ParasiteProcessor::getUserPresetsDir();
    auto file = dir.getChildFile(safeName + ".prst");

    // Avoid filename collisions with different UUIDs
    if (file.existsAsFile())
    {
        auto existingXml = juce::parseXML(file);
        if (existingXml)
        {
            auto existingUuid = existingXml->getStringAttribute("uuid");
            if (existingUuid.isNotEmpty() && existingUuid != uuid)
            {
                // Different preset — append uuid fragment to avoid collision
                safeName += "_" + uuid.substring(0, 8);
                file = dir.getChildFile(safeName + ".prst");
            }
        }
    }

    juce::TemporaryFile tmp(file);
    if (!xml->writeTo(tmp.getFile()) || !tmp.overwriteTargetFileWithTemporary())
        DBG("CloudPresetManager: failed to write preset to " + file.getFullPathName());
}

// ── Deletion log ────────────────────────────────────────────────────

juce::File CloudPresetManager::getDeletionLogFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("Voidscan").getChildFile("Parasite")
           .getChildFile("cloud_deletions.json");
}

void CloudPresetManager::logDeletion(const juce::String& uuid)
{
    auto log = loadDeletionLog();
    juce::Array<juce::var>* arr = log.getArray();

    juce::Array<juce::var> entries;
    if (arr) entries = *arr;

    auto* entry = new juce::DynamicObject();
    entry->setProperty("uuid", uuid);
    entry->setProperty("updatedAt", juce::Time::getCurrentTime().toISO8601(true));
    entries.add(juce::var(entry));

    auto file = getDeletionLogFile();
    file.getParentDirectory().createDirectory();
    juce::TemporaryFile tmp(file);
    tmp.getFile().replaceWithText(juce::JSON::toString(juce::var(entries), true));
    tmp.overwriteTargetFileWithTemporary();
}

juce::var CloudPresetManager::loadDeletionLog() const
{
    auto file = getDeletionLogFile();
    if (!file.existsAsFile()) return juce::var(juce::Array<juce::var>());
    auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (parsed.isArray()) return parsed;
    return juce::var(juce::Array<juce::var>());
}

void CloudPresetManager::clearDeletionLog()
{
    getDeletionLogFile().deleteFile();
}
