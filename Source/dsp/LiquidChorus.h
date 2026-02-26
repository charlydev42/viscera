// LiquidChorus.h — Liquid / water texture
// Stochastic resonant droplet bank: 8 narrow SVF bandpass filters at random
// frequencies with random activity envelopes create water-drop/bubble textures.
// Input signal feeds the resonant bank — no noise generator, no white noise floor.
// All texture comes from resonant filtering of the actual input signal.
// Rate: droplet speed, Depth: droplet density, Tone: freq tilt, Feed: resonance, Mix: dry/wet
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace bb {

class LiquidChorus
{
public:
    void prepare(double sampleRate, int /*samplesPerBlock*/) noexcept
    {
        sr = sampleRate;
        float srf = static_cast<float>(sr);

        envAttCoeff = std::exp(-1.0f / (srf * 0.0008f)); // 0.8ms attack
        envRelCoeff = std::exp(-1.0f / (srf * 0.040f));  // 40ms release

        actFadeUp   = 1.0f - std::exp(-1.0f / (srf * 0.008f));  // 8ms fade in
        actFadeDown = 1.0f - std::exp(-1.0f / (srf * 0.030f));  // 30ms fade out

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

        rngState = 77777u;
        for (int d = 0; d < kNum; ++d)
        {
            float r = rng();
            freq[d]       = kFLo[d] * std::pow(kFHi[d] / kFLo[d], r);
            freqTarget[d] = freq[d];
            freqCD[d]     = jitter(0.06f, 0.25f);

            act[d]       = 0.0f;
            actTarget[d] = (rng() > 0.5f) ? 1.0f : 0.0f;
            actCD[d]     = jitter(0.04f, 0.18f);
        }
    }

    // tone: 0=low/deep drops, 0.5=neutral, 1=high/sparkle
    void setParameters(float rate, float depth, float tone, float feedback, float mix) noexcept
    {
        float rN = std::clamp(rate, 0.05f, 3.0f) / 3.0f;
        speed    = rN;
        density  = std::clamp(depth, 0.0f, 1.0f);

        // Tone: shift all droplet frequency ranges
        // 0 → ×0.3 (deep/bassy), 0.5 → ×1.0, 1.0 → ×2.5 (bright/sparkly)
        float t = std::clamp(tone, 0.0f, 1.0f);
        toneShift = 0.3f * std::pow(2.5f / 0.3f, t); // 0.3–2.5 exponential

        float fb = std::clamp(feedback, 0.0f, 0.8f);
        // Restored full range — feedback is envelope-gated to prevent infinite ringing
        Q        = 8.0f + fb * 30.0f;          // 8–38
        fbAmt    = fb * 0.45f;                  // 0–0.36
        noiseAmt = density * 0.18f;             // 0–0.18

        wet = std::clamp(mix, 0.0f, 1.0f);

        float srf = static_cast<float>(sr);
        envRelCoeff = std::exp(-1.0f / (srf * (0.020f + 0.060f * (1.0f - rN))));
        freqSmooth  = 1.0f - std::exp(-6.2832f * (3.0f + rN * 20.0f) / srf);
    }

    void process(float* left, float* right, int numSamples) noexcept
    {
        if (wet < 0.0001f) return;
        float* chan[2] = { left, right };
        float srf = static_cast<float>(sr);
        constexpr float pi = 3.14159265f;

        for (int i = 0; i < numSamples; ++i)
        {
            // --- Shared droplet random walks ---
            for (int d = 0; d < kNum; ++d)
            {
                // Frequency walk (with tone shift applied)
                if (--freqCD[d] <= 0)
                {
                    float lo = kFLo[d] * toneShift;
                    float hi = kFHi[d] * toneShift;
                    // Clamp to valid audio range
                    lo = std::clamp(lo, 40.0f, srf * 0.4f);
                    hi = std::clamp(hi, lo + 20.0f, srf * 0.45f);
                    freqTarget[d] = lo * std::pow(hi / lo, rng());
                    freqCD[d] = jitter(0.04f, 0.30f, speed);
                }
                freq[d] += (freqTarget[d] - freq[d]) * freqSmooth;

                // Activity walk
                if (--actCD[d] <= 0)
                {
                    float prob = 0.15f + density * 0.55f;
                    actTarget[d] = (rng() < prob) ? 1.0f : 0.0f;
                    actCD[d] = jitter(0.03f, 0.22f, speed);
                }
                float aS = (actTarget[d] > act[d]) ? actFadeUp : actFadeDown;
                act[d] += (actTarget[d] - act[d]) * aS;
            }

            // --- Per channel ---
            for (int ch = 0; ch < 2; ++ch)
            {
                float dry = chan[ch][i];

                // Envelope follower
                float absIn = std::fabs(dry);
                float ec = (absIn > envState[ch]) ? envAttCoeff : envRelCoeff;
                envState[ch] = ec * envState[ch] + (1.0f - ec) * absIn;

                // Filter bank input: signal + tiny gated noise + feedback
                float noise = (rng() * 2.0f - 1.0f) * envState[ch] * noiseAmt;
                float in = dry + noise + fbState[ch] * fbAmt;

                // Accumulate droplet outputs
                float sum = 0.0f;
                for (int d = 0; d < kNum; ++d)
                {
                    if (act[d] < 0.001f)
                    {
                        // Bleed state toward zero to avoid clicks on reactivation
                        ic1[ch][d] *= 0.999f;
                        ic2[ch][d] *= 0.999f;
                        continue;
                    }

                    // Stereo detuning: R channel slightly offset
                    float f = freq[d];
                    if (ch == 1) f *= (1.0f + ((d & 1) ? 0.015f : -0.015f));
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

                    sum += v1 * act[d]; // v1 = bandpass output
                }

                // Gain + envelope gate + gentle soft limit (ASMR: softer output)
                float gain = 0.8f + density * 1.2f;
                sum *= gain / static_cast<float>(kNum);
                sum = std::tanh(sum);

                // Gate feedback by input envelope — dies when input stops
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
    // Base frequency ranges per droplet (before tone shift)
    static constexpr float kFLo[kNum] = {  200,  400,  700, 1200, 2000, 3500, 5500,  8000 };
    static constexpr float kFHi[kNum] = {  800, 1500, 2500, 4000, 6500, 9000, 13000, 16000 };

    double sr = 44100.0;

    // Envelope follower (per channel)
    float envState[2] = {};
    float envAttCoeff = 0.99f;
    float envRelCoeff = 0.999f;

    // SVF states (per channel x per droplet)
    float ic1[2][kNum] = {};
    float ic2[2][kNum] = {};

    // Droplet frequency walks (shared between channels)
    float freq[kNum] = {};
    float freqTarget[kNum] = {};
    int   freqCD[kNum] = {};

    // Droplet activity walks (shared between channels)
    float act[kNum] = {};
    float actTarget[kNum] = {};
    int   actCD[kNum] = {};

    float freqSmooth = 0.01f;
    float actFadeUp  = 0.01f;
    float actFadeDown = 0.005f;

    // Feedback
    float fbState[2] = {};
    float fbAmt    = 0.0f;
    float noiseAmt = 0.0f;

    // Parameters
    float Q         = 10.0f;
    float speed     = 0.5f;
    float density   = 0.5f;
    float toneShift = 1.0f;
    float wet       = 0.0f;

    // Fast xorshift PRNG
    uint32_t rngState = 77777u;
    float rng() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return static_cast<float>(rngState & 0x7FFFFFFFu) / 2147483647.0f;
    }

    int jitter(float lo, float hi, float spd = 0.5f) noexcept
    {
        float sec = (lo + (hi - lo) * rng()) / (0.3f + spd * 3.0f);
        return std::max(1, static_cast<int>(sr * sec));
    }
};

} // namespace bb
