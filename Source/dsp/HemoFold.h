// HemoFold.h — Multi-stage wavefolder with asymmetry and feedback
// Inspired by Buchla 259 / Serge wavefolders, tuned for FM synthesis
// Low amount: subtle harmonic enrichment (even + odd harmonics)
// Mid amount: rich complex timbres with asymmetric folding
// High amount: chaotic metallic textures with internal feedback
#pragma once
#include <cmath>
#include <algorithm>

namespace bb {

class HemoFold
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        // DC blocker at ~5Hz: R = 1 - (2*pi*5/sr)
        dcCoeff = static_cast<float>(1.0 - (2.0 * 3.14159265358979 * 5.0 / sr));
        reset();
    }

    void reset()
    {
        prevOutput = 0.0f;
        dcX1 = 0.0f;
        dcY1 = 0.0f;
    }

    // amount: 0-1
    void setAmount(float a)
    {
        amount = std::max(0.0f, std::min(a, 1.0f));
    }

    float tick(float input)
    {
        if (amount < 0.001f)
            return input;

        // Input gain drives the signal into folding territory
        // Exponential scaling for musical response: 1x → 16x
        float gain = 1.0f + amount * amount * 15.0f;

        // Feedback: quadratic onset for gentle start, max ~35%
        float fb = amount * amount * 0.35f;

        // Drive signal with feedback
        float signal = input * gain + prevOutput * fb;

        // Asymmetric bias: adds even harmonics (organ-like richness)
        // Scales with amount so it's subtle at low settings
        float bias = amount * 0.15f;
        signal += bias;

        // === Stage 1: Sine fold (always active) ===
        // The fundamental wavefolder: sin(x) wraps the waveform musically
        // pi/2 scaling means ±1 input maps to full sine cycle
        signal = std::sin(signal * kPi * 0.5f);

        // === Stage 2: Secondary fold (kicks in at 0.3+) ===
        // Re-folds the already-folded signal for more complex harmonics
        if (amount > 0.3f)
        {
            float blend = (amount - 0.3f) * (1.0f / 0.7f); // 0→1 over 0.3→1.0
            float folded = std::sin(signal * kPi); // second fold pass
            signal += (folded - signal) * blend * 0.5f;
        }

        // === Stage 3: Tanh saturation fold (kicks in at 0.6+) ===
        // Adds density and prevents the signal from getting too spiky
        if (amount > 0.6f)
        {
            float blend = (amount - 0.6f) * (1.0f / 0.4f); // 0→1 over 0.6→1.0
            float saturated = std::tanh(signal * 2.5f);
            signal += (saturated - signal) * blend;
        }

        // Store for feedback path
        prevOutput = signal;

        // Remove bias-induced DC offset
        signal -= bias;

        // DC blocker (1-pole highpass at ~5Hz)
        float dcOut = signal - dcX1 + dcCoeff * dcY1;
        dcX1 = signal;
        dcY1 = dcOut;
        signal = dcOut;

        // Wet/dry mix proportional to amount
        return input + (signal - input) * amount;
    }

private:
    static constexpr float kPi = 3.14159265358979f;

    double sampleRate = 44100.0;
    // DC blocker coefficient: R = 1 - (2*pi*5/sr), computed in prepare()
    float dcCoeff = 0.9993f;
    float amount = 0.0f;
    float prevOutput = 0.0f;

    // DC blocker state
    float dcX1 = 0.0f;
    float dcY1 = 0.0f;
};

} // namespace bb
