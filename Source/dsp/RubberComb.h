// RubberComb.h — Plastic / rubber / latex texture
// Dense stochastic formant bank: 8 SVF bandpass resonators, ALL always active,
// with pre-saturation (harmonic generation) and fast random frequency walks.
// Creates plastic, rubber, squeaky latex textures from any input signal.
// vs Liquid: dense (all active), saturated, faster freq changes, wider formants
// Tone: freq shift, Stretch: resonance/ring, Warp: saturation + speed, Mix: dry/wet
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace bb {

class RubberComb
{
public:
    void prepare(double sampleRate, int /*samplesPerBlock*/) noexcept
    {
        sr = sampleRate;
        float srf = static_cast<float>(sr);

        envAttCoeff = std::exp(-1.0f / (srf * 0.001f));  // 1ms attack
        envRelCoeff = std::exp(-1.0f / (srf * 0.030f));  // 30ms release

        for (int ch = 0; ch < 2; ++ch)
        {
            envState[ch] = 0.0f;
            fbState[ch]  = 0.0f;
            for (int d = 0; d < kNum; ++d)
            {
                ic1[ch][d] = 0.0f;
                ic2[ch][d] = 0.0f;
            }
        }

        rngState = 12345u;
        for (int d = 0; d < kNum; ++d)
        {
            float r = rng();
            freq[d]       = kFLo[d] * std::pow(kFHi[d] / kFLo[d], r);
            freqTarget[d] = freq[d];
            freqCD[d]     = jitter(0.02f, 0.10f);
        }
    }

    void setParameters(float tone, float stretch, float warp, float mix, float feed = 0.0f) noexcept
    {
        float t = std::clamp(tone, 0.0f, 1.0f);
        float s = std::clamp(stretch, 0.0f, 1.0f);
        float w = std::clamp(warp, 0.0f, 1.0f);
        float f = std::clamp(feed, 0.0f, 1.0f);

        // Tone → frequency shift
        toneShift = 0.3f * std::pow(2.5f / 0.3f, t);

        // Stretch → Q/resonance
        Q = 4.0f + s * 22.0f;    // 4–26

        // Feed → independent feedback amount
        fbAmt = f * 0.40f;       // 0–0.40

        // Warp → saturation drive + frequency walk speed
        satDrive     = 1.0f + w * 4.0f;    // 1–5
        freqSpeedMul = 0.3f + w * 3.0f;    // 0.3–3.3

        wet = std::clamp(mix, 0.0f, 1.0f);

        float srf = static_cast<float>(sr);
        envRelCoeff = std::exp(-1.0f / (srf * (0.015f + 0.040f * (1.0f - w))));
        freqSmooth  = 1.0f - std::exp(-6.2832f * (5.0f + w * 30.0f) / srf);
    }

    void process(float* left, float* right, int numSamples) noexcept
    {
        if (wet < 0.0001f) return;
        float* chan[2] = { left, right };
        float srf = static_cast<float>(sr);
        constexpr float pi = 3.14159265f;

        for (int i = 0; i < numSamples; ++i)
        {
            // --- Shared frequency random walks (faster than Liquid) ---
            for (int d = 0; d < kNum; ++d)
            {
                if (--freqCD[d] <= 0)
                {
                    float lo = kFLo[d] * toneShift;
                    float hi = kFHi[d] * toneShift;
                    lo = std::clamp(lo, 40.0f, srf * 0.4f);
                    hi = std::clamp(hi, lo + 20.0f, srf * 0.45f);
                    freqTarget[d] = lo * std::pow(hi / lo, rng());
                    // Shorter intervals than Liquid → squeaky character
                    freqCD[d] = jitter(0.015f, 0.10f, freqSpeedMul);
                }
                freq[d] += (freqTarget[d] - freq[d]) * freqSmooth;
            }

            // --- Per channel ---
            for (int ch = 0; ch < 2; ++ch)
            {
                float dry = chan[ch][i];

                // Envelope follower
                float absIn = std::fabs(dry);
                float ec = (absIn > envState[ch]) ? envAttCoeff : envRelCoeff;
                envState[ch] = ec * envState[ch] + (1.0f - ec) * absIn;

                // Pre-saturate input → dense harmonics (the "plastic" base character)
                float sat = std::tanh(dry * satDrive);
                float in = sat + fbState[ch] * fbAmt;

                // ALL 8 SVF bandpass resonators (always active, unlike Liquid)
                float sum = 0.0f;
                for (int d = 0; d < kNum; ++d)
                {
                    // More aggressive stereo detuning than Liquid (2.5% vs 1.5%)
                    float f = freq[d];
                    if (ch == 1) f *= (1.0f + ((d & 1) ? 0.025f : -0.025f));
                    f = std::clamp(f, 30.0f, srf * 0.45f);

                    // Cytomic TPT SVF bandpass
                    float g  = std::tan(pi * f / srf);
                    float k  = 1.0f / Q;
                    float a1 = 1.0f / (1.0f + g * (g + k));
                    float a2 = g * a1;
                    float a3 = g * a2;

                    float v3 = in - ic2[ch][d];
                    float v1 = a1 * ic1[ch][d] + a2 * v3;
                    float v2 = ic2[ch][d] + a2 * ic1[ch][d] + a3 * v3;
                    ic1[ch][d] = 2.0f * v1 - ic1[ch][d];
                    ic2[ch][d] = 2.0f * v2 - ic2[ch][d];

                    sum += v1; // All always active (no activity gating)
                }

                // Gain + gentle soft limit (ASMR: less crushed)
                sum /= static_cast<float>(kNum);
                sum = std::tanh(sum * 1.2f);

                // Envelope-gated feedback (dies when input stops)
                float envGate = std::min(envState[ch] * 20.0f, 1.0f);
                fbState[ch] = sum * envGate;

                chan[ch][i] = dry * (1.0f - wet) + sum * wet;
            }
        }
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            envState[ch] = 0.0f;
            fbState[ch]  = 0.0f;
            for (int d = 0; d < kNum; ++d)
            {
                ic1[ch][d] = 0.0f;
                ic2[ch][d] = 0.0f;
            }
        }
    }

private:
    static constexpr int kNum = 8;
    // Frequency ranges: slightly more mid-focused than Liquid for formant character
    static constexpr float kFLo[kNum] = {  120,  280,  500,  900, 1600, 2800, 4500,  7500 };
    static constexpr float kFHi[kNum] = {  500, 1000, 1800, 3200, 5000, 7500, 11000, 15000 };

    double sr = 44100.0;

    // Envelope follower (per channel)
    float envState[2] = {};
    float envAttCoeff = 0.99f;
    float envRelCoeff = 0.999f;

    // SVF states (per channel x per resonator)
    float ic1[2][kNum] = {};
    float ic2[2][kNum] = {};

    // Frequency random walks (shared between channels)
    float freq[kNum] = {};
    float freqTarget[kNum] = {};
    int   freqCD[kNum] = {};
    float freqSmooth = 0.01f;

    // Feedback
    float fbState[2] = {};
    float fbAmt = 0.0f;

    // Parameters
    float Q            = 10.0f;
    float toneShift    = 1.0f;
    float satDrive     = 1.0f;
    float freqSpeedMul = 1.0f;
    float wet          = 0.0f;

    // Fast xorshift PRNG
    uint32_t rngState = 12345u;
    float rng() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return static_cast<float>(rngState & 0x7FFFFFFFu) / 2147483647.0f;
    }

    int jitter(float lo, float hi, float spd = 1.0f) noexcept
    {
        float sec = (lo + (hi - lo) * rng()) / (0.3f + spd * 3.0f);
        return std::max(1, static_cast<int>(sr * sec));
    }
};

} // namespace bb
