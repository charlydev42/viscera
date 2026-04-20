// LicenseConfig.h — API URL, obfuscated secret, timing constants
//
// The PLUGIN_SECRET is XOR'd with kXorKey so it doesn't appear in
// plaintext in the binary.  To generate the obfuscated bytes from
// your secret, run this snippet (C++ or Python):
//
//   const char* secret = "your-plugin-secret-here";
//   const uint8_t xor[] = {0x4B,0x72,0x1F,0xA8,0x5C,0xE3,0x91,0x07,
//                           0xD6,0x3A,0x8E,0x54,0xC1,0x6F,0xB2,0x29};
//   for (size_t i = 0; i < strlen(secret); ++i)
//       printf("0x%02X, ", (uint8_t)(secret[i] ^ xor[i % 16]));
//
#pragma once
#include <cstdint>
#include <juce_core/juce_core.h>

namespace bb::license {

// ── Voidscan API ────────────────────────────────────────────────────
static constexpr const char* kApiBaseUrl = "https://api.voidscan-audio.com";

// ── Timing ──────────────────────────────────────────────────────────
static constexpr int64_t kVerifyIntervalMs  = 48LL * 60 * 60 * 1000;   // 48 h
static constexpr int     kTimerIntervalMs   = 30 * 60 * 1000;           // 30 min
static constexpr int     kHttpTimeoutMs     = 10000;                     // 10 s

// Maximum time the plugin stays licensed without a successful server verify.
// Users working offline keep the license alive; users blocking the domain
// indefinitely have their license revoked after this window.
static constexpr int64_t kOfflineGraceMs    = 7LL * 24 * 60 * 60 * 1000; // 7 days

// ── Obfuscated secret ───────────────────────────────────────────────
static constexpr uint8_t kXorKey[] = {
    0x4B, 0x72, 0x1F, 0xA8, 0x5C, 0xE3, 0x91, 0x07,
    0xD6, 0x3A, 0x8E, 0x54, 0xC1, 0x6F, 0xB2, 0x29
};

static constexpr uint8_t kObfuscatedSecret[] = {
    0x2A, 0x43, 0x2B, 0xCD, 0x6D, 0x80, 0xA1, 0x3F,
    0xB5, 0x59, 0xE8, 0x30, 0xA4, 0x57, 0x87, 0x10,
    0x79, 0x4A, 0x7D, 0xCC, 0x69, 0xDB, 0xF4, 0x64,
    0xE7, 0x0A, 0xBB, 0x67, 0xA5, 0x09, 0x82, 0x1B,
    0x2D, 0x45, 0x7A, 0xCA, 0x3F, 0x80, 0xF7, 0x37,
    0xB5, 0x59, 0xBA, 0x6D, 0xF1, 0x59, 0xD7, 0x4A,
    0x73, 0x4A, 0x27, 0xCD, 0x6E, 0xD4, 0xA4, 0x30,   
    0xE5, 0x58, 0xB6, 0x66, 0xF6, 0x5D, 0x8B, 0x48, 
};

inline juce::String decodeSecret()
{
    juce::String result;
    for (size_t i = 0; i < sizeof(kObfuscatedSecret); ++i)
        result += static_cast<char>(kObfuscatedSecret[i] ^ kXorKey[i % sizeof(kXorKey)]);
    return result;
}

} // namespace bb::license
