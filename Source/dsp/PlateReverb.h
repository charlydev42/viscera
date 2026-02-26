// PlateReverb.h — Algorithmic plate reverb inspired by Dattorro (1997)
// 4 input allpass diffusers, 2 crossed feedback loops with modulation,
// LP damping in each loop, multi-tap stereo output
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace bb {

class PlateReverb
{
public:
    void prepare(double sampleRate, int /*samplesPerBlock*/) noexcept
    {
        sr = sampleRate;

        // Pre-delay buffer (up to 200ms)
        int pdMax = static_cast<int>(sr * 0.2) + 1;
        predelayL.resize(pdMax);
        predelayR.resize(pdMax);

        // Scale delay lengths from reference 29761 Hz to actual sample rate
        double scale = sr / 29761.0;

        // Input diffusers (4 allpass stages per channel)
        inputDiffL[0].resize(static_cast<int>(142 * scale));
        inputDiffL[1].resize(static_cast<int>(107 * scale));
        inputDiffL[2].resize(static_cast<int>(379 * scale));
        inputDiffL[3].resize(static_cast<int>(277 * scale));

        inputDiffR[0].resize(static_cast<int>(149 * scale));
        inputDiffR[1].resize(static_cast<int>(113 * scale));
        inputDiffR[2].resize(static_cast<int>(389 * scale));
        inputDiffR[3].resize(static_cast<int>(283 * scale));

        // Tank left
        tankDiffL[0].resize(static_cast<int>(672 * scale));
        tankDiffL[1].resize(static_cast<int>(1800 * scale));
        tankDelayL[0].resize(static_cast<int>(4453 * scale));
        tankDelayL[1].resize(static_cast<int>(3720 * scale));

        // Tank right
        tankDiffR[0].resize(static_cast<int>(908 * scale));
        tankDiffR[1].resize(static_cast<int>(2656 * scale));
        tankDelayR[0].resize(static_cast<int>(4217 * scale));
        tankDelayR[1].resize(static_cast<int>(3163 * scale));

        // Modulation LFO
        modPhase = 0.0;
        modInc = 1.0 / sr; // ~1 Hz base rate

        // LP filters in tank
        lpStateL = 0.0f;
        lpStateR = 0.0f;

        // Output tap positions (scaled)
        tapL[0] = static_cast<int>(266 * scale);
        tapL[1] = static_cast<int>(2974 * scale);
        tapL[2] = static_cast<int>(1913 * scale);
        tapL[3] = static_cast<int>(1996 * scale);
        tapL[4] = static_cast<int>(1990 * scale);
        tapL[5] = static_cast<int>(187 * scale);

        tapR[0] = static_cast<int>(353 * scale);
        tapR[1] = static_cast<int>(3627 * scale);
        tapR[2] = static_cast<int>(1228 * scale);
        tapR[3] = static_cast<int>(2058 * scale);
        tapR[4] = static_cast<int>(2641 * scale);
        tapR[5] = static_cast<int>(163 * scale);

        reset();
    }

    void setParameters(float size, float damp, float mix, float widthParam = 1.0f, float predelayMs = 0.0f) noexcept
    {
        // Pre-delay in samples (0-200ms)
        pdSamples = static_cast<int>(std::clamp(predelayMs, 0.0f, 200.0f) * 0.001f * static_cast<float>(sr));

        // Size controls feedback amount (0.0 = small room, 1.0 = long tail)
        feedback = 0.3f + size * 0.55f; // range [0.3, 0.85]
        feedback = std::clamp(feedback, 0.0f, 0.85f);

        // Damp controls LP cutoff in tank (0 = bright, 1 = dark)
        dampCoeff = 0.05f + damp * 0.7f;

        // Input diffusion
        diffusion1 = 0.75f;
        diffusion2 = 0.625f;

        wet = std::clamp(mix, 0.0f, 1.0f);
        width = std::clamp(widthParam, 0.0f, 1.0f);
    }

    void process(float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // --- Pre-delay ---
            float pdInL = left[i];
            float pdInR = right[i];
            if (pdSamples > 0)
            {
                pdInL = predelayL.readAt(pdSamples);
                pdInR = predelayR.readAt(pdSamples);
                predelayL.write(left[i]);
                predelayR.write(right[i]);
            }

            // --- Input diffusion: separate L/R chains for true stereo ---
            float diffL = pdInL;
            diffL = inputDiffL[0].process(diffL, diffusion1);
            diffL = inputDiffL[1].process(diffL, diffusion1);
            diffL = inputDiffL[2].process(diffL, diffusion2);
            diffL = inputDiffL[3].process(diffL, diffusion2);

            float diffR = pdInR;
            diffR = inputDiffR[0].process(diffR, diffusion1);
            diffR = inputDiffR[1].process(diffR, diffusion1);
            diffR = inputDiffR[2].process(diffR, diffusion2);
            diffR = inputDiffR[3].process(diffR, diffusion2);

            // --- Modulation ---
            modPhase += modInc;
            if (modPhase >= 1.0) modPhase -= 1.0;
            float mod = static_cast<float>(std::sin(modPhase * 2.0 * 3.14159265358979));
            int modSamplesL = static_cast<int>(mod * 16.0f);
            int modSamplesR = static_cast<int>(-mod * 16.0f);

            // Flush denormals
            auto killDenormal = [](float& v) { if (std::fabs(v) < 1.0e-20f) v = 0.0f; };
            killDenormal(tankFeedbackL);
            killDenormal(tankFeedbackR);
            killDenormal(lpStateL);
            killDenormal(lpStateR);

            // --- Tank Left ---
            float tankInL = diffL + tankFeedbackR * feedback;
            float tl0 = tankDiffL[0].processModulated(tankInL, -diffusion1, modSamplesL);
            float tl1 = tankDelayL[0].read();
            tankDelayL[0].write(tl0);

            // LP damping
            lpStateL = lpStateL + dampCoeff * (tl1 - lpStateL);
            float tl2 = tankDiffL[1].process(lpStateL, diffusion2);
            tankFeedbackL = tankDelayL[1].read();
            tankDelayL[1].write(tl2);

            // --- Tank Right ---
            float tankInR = diffR + tankFeedbackL * feedback;
            float tr0 = tankDiffR[0].processModulated(tankInR, -diffusion1, modSamplesR);
            float tr1 = tankDelayR[0].read();
            tankDelayR[0].write(tr0);

            // LP damping
            lpStateR = lpStateR + dampCoeff * (tr1 - lpStateR);
            float tr2 = tankDiffR[1].process(lpStateR, diffusion2);
            tankFeedbackR = tankDelayR[1].read();
            tankDelayR[1].write(tr2);

            // --- Output taps ---
            float outL = tankDelayL[0].readAt(tapL[0])
                       + tankDelayL[0].readAt(tapL[1])
                       - tankDiffR[1].readAt(tapL[2])
                       + tankDelayR[1].readAt(tapL[3])
                       - tankDelayL[1].readAt(tapL[4])
                       - tankDiffL[1].readAt(tapL[5]);

            float outR = tankDelayR[0].readAt(tapR[0])
                       + tankDelayR[0].readAt(tapR[1])
                       - tankDiffL[1].readAt(tapR[2])
                       + tankDelayL[1].readAt(tapR[3])
                       - tankDelayR[1].readAt(tapR[4])
                       - tankDiffR[1].readAt(tapR[5]);

            outL *= 0.3f;
            outR *= 0.3f;

            // Width: blend between mono (mid) and full stereo
            float mid = (outL + outR) * 0.5f;
            outL = mid + width * (outL - mid);
            outR = mid + width * (outR - mid);

            // Mix dry/wet
            left[i]  = left[i]  * (1.0f - wet) + outL * wet;
            right[i] = right[i] * (1.0f - wet) + outR * wet;
        }
    }

    void reset() noexcept
    {
        predelayL.reset();
        predelayR.reset();
        for (auto& d : inputDiffL) d.reset();
        for (auto& d : inputDiffR) d.reset();
        for (auto& d : tankDiffL) d.reset();
        for (auto& d : tankDiffR) d.reset();
        for (auto& d : tankDelayL) d.reset();
        for (auto& d : tankDelayR) d.reset();
        lpStateL = 0.0f;
        lpStateR = 0.0f;
        tankFeedbackL = 0.0f;
        tankFeedbackR = 0.0f;
    }

private:
    // --- Simple delay line ---
    struct DelayLine
    {
        std::vector<float> buf;
        int writeIdx = 0;
        int len = 1;

        void resize(int length) noexcept
        {
            len = std::max(length, 1);
            buf.assign(static_cast<size_t>(len), 0.0f);
            writeIdx = 0;
        }

        void write(float sample) noexcept
        {
            buf[static_cast<size_t>(writeIdx)] = sample;
            writeIdx = (writeIdx + 1) % len;
        }

        float read() const noexcept
        {
            return buf[static_cast<size_t>(writeIdx)]; // oldest sample
        }

        float readAt(int delay) const noexcept
        {
            int idx = writeIdx - delay;
            while (idx < 0) idx += len;
            return buf[static_cast<size_t>(idx % len)];
        }

        void reset() noexcept
        {
            std::fill(buf.begin(), buf.end(), 0.0f);
            writeIdx = 0;
        }
    };

    // --- Allpass with delay line ---
    struct AllpassDelay
    {
        DelayLine delay;

        void resize(int length) noexcept { delay.resize(length); }

        float process(float input, float coeff) noexcept
        {
            float delayed = delay.read();
            float output = -input * coeff + delayed;
            delay.write(input + delayed * coeff);
            return output;
        }

        float processModulated(float input, float coeff, int modOffset) noexcept
        {
            int readDelay = delay.len + modOffset;
            readDelay = std::clamp(readDelay, 1, delay.len);
            float delayed = delay.readAt(readDelay);
            float output = -input * coeff + delayed;
            delay.write(input + delayed * coeff);
            return output;
        }

        float readAt(int delay_) const noexcept
        {
            return delay.readAt(delay_);
        }

        void reset() noexcept { delay.reset(); }
    };

    double sr = 44100.0;

    // Input diffusers (separate L/R for true stereo)
    AllpassDelay inputDiffL[4];
    AllpassDelay inputDiffR[4];

    // Tank: left and right
    AllpassDelay tankDiffL[2], tankDiffR[2];
    DelayLine tankDelayL[2], tankDelayR[2];

    // Feedback state
    float tankFeedbackL = 0.0f;
    float tankFeedbackR = 0.0f;

    // LP damping in tank
    float lpStateL = 0.0f;
    float lpStateR = 0.0f;

    // Modulation
    double modPhase = 0.0;
    double modInc = 0.0;

    // Parameters
    float feedback = 0.5f;
    float dampCoeff = 0.3f;
    float diffusion1 = 0.75f;
    float diffusion2 = 0.625f;
    float wet = 0.0f;
    float width = 1.0f;
    int pdSamples = 0;

    // Pre-delay buffers
    DelayLine predelayL, predelayR;

    // Output tap positions
    int tapL[6] = {};
    int tapR[6] = {};
};

} // namespace bb
