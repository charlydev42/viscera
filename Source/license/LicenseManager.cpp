// LicenseManager.cpp — License activation, verification, cache, HMAC signing
#include "LicenseManager.h"
#include "LicenseConfig.h"
#include <juce_cryptography/juce_cryptography.h>

#if JUCE_MAC
 #include <IOKit/IOKitLib.h>
 #include <CoreFoundation/CoreFoundation.h>
 #include <pwd.h>
 #include <unistd.h>
#endif

namespace bb {

using namespace bb::license;

// =====================================================================
// Construction / destruction
// =====================================================================

LicenseManager::LicenseManager()
{
    machineId_ = getMachineId();
    loadCache();
    startTimer(kTimerIntervalMs);

    // Verify on launch if last check was >1h ago — avoids flooding
    // the server when a DAW opens many plugin instances at once.
    // If offline, silently keeps using cache.
    if (isLicensed())
    {
        const juce::ScopedLock sl(stateLock);
        auto elapsed = juce::Time::currentTimeMillis() - lastVerified_;
        if (elapsed > 60 * 60 * 1000) // 1 hour
            verify(nullptr);
    }
}

LicenseManager::~LicenseManager()
{
    stopTimer();
    alive_->store(false);
    shuttingDown_.store(true);

    // Wait up to 12 s for in-flight HTTP threads to finish
    for (int i = 0; i < 120 && pendingThreads_.load() > 0; ++i)
        juce::Thread::sleep(100);
}

// =====================================================================
// Public query
// =====================================================================

bool LicenseManager::isLicensed() const
{
    const juce::ScopedLock sl(stateLock);
    return licensed_;
}

juce::String LicenseManager::getLicenseKey() const
{
    const juce::ScopedLock sl(stateLock);
    return licenseKey_;
}

juce::String LicenseManager::getActivationToken() const
{
    const juce::ScopedLock sl(stateLock);
    return activationToken_;
}

// =====================================================================
// Activate — first launch license entry
// =====================================================================

void LicenseManager::activate(const juce::String& key, Callback callback)
{
    auto machineId   = machineId_;
    auto machineName = getMachineName();

    ++pendingThreads_;
    auto alive = alive_;

    juce::Thread::launch([this, alive, key, machineId, machineName, callback]
    {
        auto* body = new juce::DynamicObject();
        body->setProperty("licenseKey",  key);
        body->setProperty("machineId",   machineId);
        body->setProperty("machineName", machineName);

        auto resp = httpPost("/auth/activate", juce::var(body));

        if (shuttingDown_.load()) { --pendingThreads_; return; }

        bool ok = false;
        juce::String msg;

        if (resp.statusCode == 200)
        {
            auto json = juce::JSON::parse(resp.body);
            juce::String uid = json.getProperty("userId", "").toString();
            juce::String token = json.getProperty("token", "").toString();

            {
                const juce::ScopedLock sl(stateLock);
                licensed_          = true;
                licenseKey_        = key;
                userId_            = uid;
                activationToken_   = token;
                activatedAt_       = juce::Time::currentTimeMillis();
                lastVerified_      = activatedAt_;
            }
            saveCache();
            ok  = true;
            msg = "License activated!";
        }
        else if (resp.statusCode == 0)
        {
            msg = "Could not reach server. Check your internet connection.";
        }
        else
        {
            auto json = juce::JSON::parse(resp.body);
            msg = json.getProperty("error", "Activation failed (HTTP "
                        + juce::String(resp.statusCode) + ")").toString();
        }

        --pendingThreads_;

        juce::MessageManager::callAsync([this, alive, callback, ok, msg]
        {
            if (!alive->load()) return;
            if (callback) callback(ok, msg);
            if (ok) listeners_.call(&Listener::licenseStateChanged, true);
        });
    });
}

// =====================================================================
// Verify — periodic heartbeat
// =====================================================================

void LicenseManager::verify(Callback callback)
{
    juce::String key;
    juce::String mid;
    {
        const juce::ScopedLock sl(stateLock);
        key = licenseKey_;
        mid = machineId_;
    }

    if (key.isEmpty()) return;

    ++pendingThreads_;
    auto alive = alive_;

    juce::Thread::launch([this, alive, key, mid, callback]
    {
        auto* body = new juce::DynamicObject();
        body->setProperty("licenseKey", key);
        body->setProperty("machineId",  mid);

        auto resp = httpPost("/auth/verify", juce::var(body));

        if (shuttingDown_.load()) { --pendingThreads_; return; }

        bool ok = false;
        juce::String msg;

        if (resp.statusCode == 200)
        {
            auto json = juce::JSON::parse(resp.body);
            bool valid = json.getProperty("valid", false);

            if (valid)
            {
                {
                    const juce::ScopedLock sl(stateLock);
                    lastVerified_ = juce::Time::currentTimeMillis();
                }
                saveCache();
                ok  = true;
                msg = "License verified.";
            }
            else
            {
                // Server says license is no longer valid
                {
                    const juce::ScopedLock sl(stateLock);
                    licensed_ = false;
                }
                clearCache();
                msg = "License is no longer valid.";
            }
        }
        else if (resp.statusCode == 0)
        {
            // Network error — keep the cached license only if we've verified
            // within the grace window. A user who's been offline (or has
            // blocked the domain) beyond the window must reconnect to keep
            // working.
            //
            // Clock-skew protection: if the system clock has been set BACK
            // (lastVerified_ is in the "future"), we can't trust any time
            // comparison. Revoke until a successful server verify re-anchors
            // us to real time. Small positive tolerance absorbs NTP jitter.
            int64_t lastOk = 0;
            {
                const juce::ScopedLock sl(stateLock);
                lastOk = lastVerified_;
            }
            const auto now = juce::Time::currentTimeMillis();
            const auto elapsed = now - lastOk;
            constexpr int64_t kClockSkewTolerance = 60 * 60 * 1000; // 1 hour

            if (lastOk > 0 && elapsed >= -kClockSkewTolerance && elapsed <= kOfflineGraceMs)
            {
                ok  = true;
                msg = "Offline — using cached license (grace period).";
            }
            else if (lastOk > 0 && elapsed < -kClockSkewTolerance)
            {
                {
                    const juce::ScopedLock sl(stateLock);
                    licensed_ = false;
                }
                ok  = false;
                msg = "Clock appears to have moved backward — please reconnect to verify.";
            }
            else
            {
                {
                    const juce::ScopedLock sl(stateLock);
                    licensed_ = false;
                }
                ok  = false;
                msg = "Offline for too long — please reconnect to verify your license.";
            }
        }
        else
        {
            msg = "Verification failed (HTTP " + juce::String(resp.statusCode) + ")";
        }

        --pendingThreads_;

        juce::MessageManager::callAsync([this, alive, callback, ok, msg]
        {
            if (!alive->load()) return;
            if (callback) callback(ok, msg);
            if (!ok) listeners_.call(&Listener::licenseStateChanged, false);
        });
    });
}

// =====================================================================
// Deactivate — clear local cache
// =====================================================================

void LicenseManager::deactivate()
{
    juce::String key, machine;
    {
        const juce::ScopedLock sl(stateLock);
        key     = licenseKey_;
        machine = machineId_;
        licensed_        = false;
        licenseKey_      = {};
        userId_          = {};
        activationToken_ = {};
        activatedAt_     = 0;
        lastVerified_    = 0;
    }
    clearCache();
    listeners_.call(&Listener::licenseStateChanged, false);

    // Tell server to deactivate this machine (fire and forget)
    // Captures only value types — no reference to `this`, so no use-after-free
    if (key.isNotEmpty() && machine.isNotEmpty())
    {
        auto secret = bb::license::decodeSecret();
        std::thread([key, machine, secret] {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("licenseKey", key);
            obj->setProperty("machineId",  machine);
            juce::var jsonVar(obj);
            auto canon = LicenseManager::canonicalJson(jsonVar);

            // Sign the request
            auto timestamp = juce::String(juce::Time::currentTimeMillis());
            auto sig = LicenseManager::hmacSha256(secret, timestamp + ":" + canon);
            auto sigHeader = timestamp + "." + sig;

            auto extraHeaders = "Content-Type: application/json\r\nX-TD-Signature: " + sigHeader;

            int statusCode = 0;
            auto url = juce::URL(juce::String(bb::license::kApiBaseUrl) + "/auth/deactivate")
                           .withPOSTData(canon);
            url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                .withExtraHeaders(extraHeaders)
                .withConnectionTimeoutMs(bb::license::kHttpTimeoutMs)
                .withStatusCode(&statusCode));
        }).detach();
    }
}

// =====================================================================
// Timer — auto-verify every 48 h
// =====================================================================

void LicenseManager::timerCallback()
{
    bool needsVerify = false;

    {
        const juce::ScopedLock sl(stateLock);
        if (!licensed_ || licenseKey_.isEmpty()) return;

        auto now = juce::Time::currentTimeMillis();
        if (now - lastVerified_ > kVerifyIntervalMs)
            needsVerify = true;
    }

    // Try to re-verify; if offline, silently skip (license stays valid)
    if (needsVerify)
        verify(nullptr);
}

// =====================================================================
// HTTP POST helper
// =====================================================================

LicenseManager::HttpResponse LicenseManager::httpPost(
    const juce::String& endpoint,
    const juce::var& jsonBody) const
{
    auto canonical = canonicalJson(jsonBody);
    auto signature = buildSignature(canonical);

    auto url = juce::URL(juce::String(kApiBaseUrl) + endpoint)
                   .withPOSTData(canonical);

    int statusCode = 0;
    auto headers = juce::String("Content-Type: application/json\r\n")
                 + "X-TD-Signature: " + signature;

    auto stream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
            .withExtraHeaders(headers)
            .withConnectionTimeoutMs(kHttpTimeoutMs)
            .withStatusCode(&statusCode));

    HttpResponse resp;
    resp.statusCode = statusCode;
    if (stream)
        resp.body = stream->readEntireStreamAsString();

    return resp;
}

// =====================================================================
// Request signing — X-TD-Signature: <timestamp>.<hmac_hex>
// =====================================================================

juce::String LicenseManager::buildSignature(const juce::String& canonicalBody) const
{
    auto ts  = juce::String(juce::Time::currentTimeMillis());
    auto payload = ts + ":" + canonicalBody;
    auto secret  = decodeSecret();
    auto hmac    = hmacSha256(secret, payload);
    return ts + "." + hmac;
}

// =====================================================================
// Canonical JSON — sorted keys, no whitespace
// =====================================================================

juce::String LicenseManager::canonicalJson(const juce::var& value)
{
    if (value.isVoid() || value.isUndefined())
        return {};

    // Check array BEFORE object — juce::var arrays can satisfy isObject() in some cases
    if (value.isArray())
    {
        auto* arr = value.getArray();
        if (!arr) return "[]";
        juce::String result = "[";
        for (int i = 0; i < arr->size(); ++i)
        {
            if (i > 0) result += ",";
            result += canonicalJson((*arr)[i]);
        }
        result += "]";
        return result;
    }

    if (value.isObject())
    {
        auto* obj = value.getDynamicObject();
        if (!obj) return "{}";

        // Collect and sort keys
        juce::StringArray keys;
        for (auto& prop : obj->getProperties())
            keys.add(prop.name.toString());
        keys.sort(false);

        juce::String result = "{";
        for (int i = 0; i < keys.size(); ++i)
        {
            if (i > 0) result += ",";
            result += "\"" + keys[i] + "\":" + canonicalJson(obj->getProperty(keys[i]));
        }
        result += "}";
        return result;
    }

    if (value.isString())
    {
        // Escape special characters
        auto s = value.toString();
        s = s.replace("\\", "\\\\")
             .replace("\"", "\\\"")
             .replace("\n", "\\n")
             .replace("\r", "\\r")
             .replace("\t", "\\t");
        return "\"" + s + "\"";
    }

    if (value.isBool())
        return value ? "true" : "false";

    // Numbers
    return value.toString();
}

// =====================================================================
// HMAC-SHA256 — cross-platform impl using JUCE's SHA256
// Follows RFC 2104: H(K xor opad, H(K xor ipad, data))
// =====================================================================

static constexpr int kSha256BlockSize  = 64;
static constexpr int kSha256DigestSize = 32;

static juce::MemoryBlock sha256Raw(const void* data, size_t size)
{
    juce::MemoryBlock mb(data, size);
    auto hash = juce::SHA256(mb.getData(), mb.getSize()).getRawData();
    return hash;
}

juce::String LicenseManager::hmacSha256(const juce::String& key,
                                         const juce::String& data)
{
    auto keyUtf8  = key.toRawUTF8();
    size_t keyLen = static_cast<size_t>(key.getNumBytesAsUTF8());

    // Prepare key: hash if too long, zero-pad if too short
    juce::uint8 kBlock[kSha256BlockSize] = {};
    if (keyLen > kSha256BlockSize)
    {
        auto hashed = sha256Raw(keyUtf8, keyLen);
        std::memcpy(kBlock, hashed.getData(), kSha256DigestSize);
    }
    else
    {
        std::memcpy(kBlock, keyUtf8, keyLen);
    }

    juce::uint8 ipad[kSha256BlockSize], opad[kSha256BlockSize];
    for (int i = 0; i < kSha256BlockSize; ++i)
    {
        ipad[i] = kBlock[i] ^ 0x36;
        opad[i] = kBlock[i] ^ 0x5c;
    }

    auto dataUtf8  = data.toRawUTF8();
    size_t dataLen = static_cast<size_t>(data.getNumBytesAsUTF8());

    // Inner hash: H(ipad || data)
    juce::MemoryBlock inner;
    inner.append(ipad, kSha256BlockSize);
    inner.append(dataUtf8, dataLen);
    auto innerHash = sha256Raw(inner.getData(), inner.getSize());

    // Outer hash: H(opad || innerHash)
    juce::MemoryBlock outer;
    outer.append(opad, kSha256BlockSize);
    outer.append(innerHash.getData(), innerHash.getSize());
    auto finalHash = sha256Raw(outer.getData(), outer.getSize());

    // Hex encode
    static const char hex[] = "0123456789abcdef";
    juce::String result;
    result.preallocateBytes(kSha256DigestSize * 2 + 1);
    auto* bytes = static_cast<const juce::uint8*>(finalHash.getData());
    for (int i = 0; i < kSha256DigestSize; ++i)
    {
        result += hex[(bytes[i] >> 4) & 0x0F];
        result += hex[bytes[i] & 0x0F];
    }
    return result;
}

// =====================================================================
// Machine fingerprint
// =====================================================================

juce::String LicenseManager::getMachineId()
{
    juce::String raw;

#if JUCE_MAC
    io_service_t platformExpert = IOServiceGetMatchingService(
        MACH_PORT_NULL, IOServiceMatching("IOPlatformExpertDevice"));

    if (platformExpert)
    {
        if (auto uuid = (CFStringRef)IORegistryEntryCreateCFProperty(
                platformExpert, CFSTR("IOPlatformUUID"),
                kCFAllocatorDefault, 0))
        {
            char buf[128] = {};
            CFStringGetCString(uuid, buf, sizeof(buf), kCFStringEncodingUTF8);
            raw = buf;
            CFRelease(uuid);
        }
        IOObjectRelease(platformExpert);
    }
#endif

    // Windows / Linux / fallback: use JUCE's unique device ID
    if (raw.isEmpty())
    {
        raw = juce::SystemStats::getUniqueDeviceID();
    }

    if (raw.isEmpty())
        raw = juce::SystemStats::getComputerName() + "-fallback";

    // Hash for consistent length + privacy (SHA256 hex — 64 chars)
    auto hashed = sha256Raw(raw.toRawUTF8(),
                            static_cast<size_t>(raw.getNumBytesAsUTF8()));
    static const char hexChars[] = "0123456789abcdef";
    juce::String hash;
    hash.preallocateBytes(kSha256DigestSize * 2 + 1);
    auto* bytes = static_cast<const juce::uint8*>(hashed.getData());
    for (int i = 0; i < kSha256DigestSize; ++i)
    {
        hash += hexChars[(bytes[i] >> 4) & 0x0F];
        hash += hexChars[bytes[i] & 0x0F];
    }
    return hash;
}

juce::String LicenseManager::getMachineName()
{
    return juce::SystemStats::getComputerName();
}

// =====================================================================
// Encrypted cache — XOR with machine-derived key
// =====================================================================

juce::File LicenseManager::getCacheFile()
{
#if JUCE_MAC
    // Use the real home directory (bypasses sandbox container remapping)
    // so standalone, VST3 and AU all share the same license cache.
    if (auto* pw = getpwuid(getuid()))
    {
        return juce::File(juce::String(pw->pw_dir))
            .getChildFile("Library/Voidscan/Parasite/license.dat");
    }
#endif
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
           .getChildFile("Voidscan").getChildFile("Parasite")
           .getChildFile("license.dat");
}

juce::MemoryBlock LicenseManager::xorCipher(const void* data, size_t size,
                                             const juce::String& key)
{
    auto keyBytes = key.toRawUTF8();
    auto keyLen   = static_cast<size_t>(key.getNumBytesAsUTF8());
    if (keyLen == 0) return {};

    juce::MemoryBlock out(size);
    auto* src = static_cast<const uint8_t*>(data);
    auto* dst = static_cast<uint8_t*>(out.getData());

    for (size_t i = 0; i < size; ++i)
        dst[i] = src[i] ^ static_cast<uint8_t>(keyBytes[i % keyLen]);

    return out;
}

void LicenseManager::saveCache() const
{
    const juce::ScopedLock sl(stateLock);

    // Build object WITHOUT seal first
    auto* obj = new juce::DynamicObject();
    obj->setProperty("key",          licenseKey_);
    obj->setProperty("machineId",    machineId_);
    obj->setProperty("userId",       userId_);
    obj->setProperty("activatedAt",  activatedAt_);
    obj->setProperty("lastVerified", lastVerified_);
    obj->setProperty("token",        activationToken_);

    juce::var jsonVar(obj);
    auto canonical = canonicalJson(jsonVar);

    // HMAC integrity seal — prevents forging a cache without the secret
    auto seal = hmacSha256(decodeSecret(), canonical + machineId_);
    obj->setProperty("seal", seal);

    // Serialize for storage (pretty-print is fine for the encrypted file)
    auto json = juce::JSON::toString(jsonVar, true);

    auto encrypted = xorCipher(json.toRawUTF8(),
                               static_cast<size_t>(json.getNumBytesAsUTF8()),
                               machineId_);

    auto file = getCacheFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithData(encrypted.getData(), encrypted.getSize());
}

void LicenseManager::loadCache()
{
    auto file = getCacheFile();
    if (!file.existsAsFile()) return;

    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() == 0) return;

    auto decrypted = xorCipher(data.getData(), data.getSize(), machineId_);
    auto json = juce::String::fromUTF8(
        static_cast<const char*>(decrypted.getData()),
        static_cast<int>(decrypted.getSize()));

    auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject()) return;

    auto storedMachine = parsed.getProperty("machineId", "").toString();
    if (storedMachine != machineId_) return; // wrong machine

    // Verify HMAC seal — reject tampered cache
    auto storedSeal = parsed.getProperty("seal", "").toString();
    if (storedSeal.isNotEmpty())
    {
        // Rebuild the object without the seal to verify
        auto* verify = new juce::DynamicObject();
        verify->setProperty("key",          parsed.getProperty("key", ""));
        verify->setProperty("machineId",    parsed.getProperty("machineId", ""));
        verify->setProperty("userId",       parsed.getProperty("userId", ""));
        verify->setProperty("activatedAt",  parsed.getProperty("activatedAt", 0));
        verify->setProperty("lastVerified", parsed.getProperty("lastVerified", 0));
        verify->setProperty("token",        parsed.getProperty("token", ""));
        auto expectedSeal = hmacSha256(decodeSecret(), canonicalJson(juce::var(verify)) + machineId_);

        if (storedSeal != expectedSeal)
        {
            // Fallback: try legacy seal format (JSON::toString)
            auto* legacyObj = new juce::DynamicObject();
            legacyObj->setProperty("key",          parsed.getProperty("key", ""));
            legacyObj->setProperty("machineId",    parsed.getProperty("machineId", ""));
            legacyObj->setProperty("userId",       parsed.getProperty("userId", ""));
            legacyObj->setProperty("activatedAt",  parsed.getProperty("activatedAt", 0));
            legacyObj->setProperty("lastVerified", parsed.getProperty("lastVerified", 0));
            legacyObj->setProperty("token",        parsed.getProperty("token", ""));
            auto legacyJson = juce::JSON::toString(juce::var(legacyObj), true);
            auto legacySeal = hmacSha256(decodeSecret(), legacyJson + machineId_);
            if (storedSeal != legacySeal) return; // tampered
        }
    }
    else
    {
        return; // no seal = old or forged cache
    }

    {
        const juce::ScopedLock sl(stateLock);
        licenseKey_   = parsed.getProperty("key", "").toString();
        userId_       = parsed.getProperty("userId", "").toString();
        activatedAt_  = static_cast<int64_t>(parsed.getProperty("activatedAt", 0));
        lastVerified_ = static_cast<int64_t>(parsed.getProperty("lastVerified", 0));

        activationToken_ = parsed.getProperty("token", "").toString();
        licensed_ = licenseKey_.isNotEmpty();
    }
}

void LicenseManager::clearCache()
{
    getCacheFile().deleteFile();
}

} // namespace bb
