// StereoDelay.h — Delay stéréo avec buffer circulaire et interpolation linéaire
// Post-synth effect, appliqué dans processBlock()
// Features: ping-pong mode, LP filter in feedback loop (tape delay damp)
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace bb {

class StereoDelay
{
public:
    void prepare(double sampleRate, int /*samplesPerBlock*/) noexcept
    {
        sr = sampleRate;
        maxSamples = static_cast<int>(sr * 2.0); // 2 secondes max
        bufferL.assign(static_cast<size_t>(maxSamples), 0.0f);
        bufferR.assign(static_cast<size_t>(maxSamples), 0.0f);
        writePos = 0;
        lpStateL = 0.0f;
        lpStateR = 0.0f;
    }

    void setParameters(float timeSec, float feedback, float damp, float mix, bool pingpong, float spreadParam = 0.0f) noexcept
    {
        delaySamples = std::clamp(static_cast<double>(timeSec) * sr, 1.0, static_cast<double>(maxSamples - 1));
        fb = std::clamp(feedback, 0.0f, 0.9f);
        wet = std::clamp(mix, 0.0f, 1.0f);
        pingPong = pingpong;
        dampCoeff = std::clamp(damp, 0.0f, 1.0f);

        // Spread: offset R delay time relative to L (0 = same, 1 = +50%)
        float sp = std::clamp(spreadParam, 0.0f, 1.0f);
        delaySamplesR = std::clamp(delaySamples * (1.0 + sp * 0.5), 1.0, static_cast<double>(maxSamples - 1));
    }

    void process(float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // Interpolation linéaire pour le read position (L)
            double readPosL = static_cast<double>(writePos) - delaySamples;
            if (readPosL < 0.0) readPosL += maxSamples;
            int idxL0 = static_cast<int>(readPosL);
            int idxL1 = (idxL0 + 1) % maxSamples;
            float fracL = static_cast<float>(readPosL - idxL0);
            float delayedL = bufferL[static_cast<size_t>(idxL0)] * (1.0f - fracL)
                           + bufferL[static_cast<size_t>(idxL1)] * fracL;

            // R channel with spread offset
            double readPosR = static_cast<double>(writePos) - delaySamplesR;
            if (readPosR < 0.0) readPosR += maxSamples;
            int idxR0 = static_cast<int>(readPosR);
            int idxR1 = (idxR0 + 1) % maxSamples;
            float fracR = static_cast<float>(readPosR - idxR0);
            float delayedR = bufferR[static_cast<size_t>(idxR0)] * (1.0f - fracR)
                           + bufferR[static_cast<size_t>(idxR1)] * fracR;

            // 1-pole LP filter in feedback path (tape delay damping)
            // Each repeat loses high frequencies
            lpStateL = lpStateL + (1.0f - dampCoeff) * (delayedL - lpStateL);
            lpStateR = lpStateR + (1.0f - dampCoeff) * (delayedR - lpStateR);
            float filteredL = lpStateL;
            float filteredR = lpStateR;

            // Écrire dans le buffer : input + feedback
            if (pingPong)
            {
                // Ping-pong: L feedback goes to R, R feedback goes to L
                bufferL[static_cast<size_t>(writePos)] = left[i]  + filteredR * fb;
                bufferR[static_cast<size_t>(writePos)] = right[i] + filteredL * fb;
            }
            else
            {
                // Normal stereo delay
                bufferL[static_cast<size_t>(writePos)] = left[i]  + filteredL * fb;
                bufferR[static_cast<size_t>(writePos)] = right[i] + filteredR * fb;
            }

            // Mix dry/wet
            left[i]  = left[i]  * (1.0f - wet) + delayedL * wet;
            right[i] = right[i] * (1.0f - wet) + delayedR * wet;

            writePos = (writePos + 1) % maxSamples;
        }
    }

    void reset() noexcept
    {
        std::fill(bufferL.begin(), bufferL.end(), 0.0f);
        std::fill(bufferR.begin(), bufferR.end(), 0.0f);
        writePos = 0;
        lpStateL = 0.0f;
        lpStateR = 0.0f;
    }

private:
    double sr = 44100.0;
    int maxSamples = 88200;
    std::vector<float> bufferL, bufferR;
    int writePos = 0;
    double delaySamples = 4410.0;
    double delaySamplesR = 4410.0;
    float fb = 0.3f;
    float wet = 0.0f;
    float dampCoeff = 0.3f;
    bool pingPong = false;

    // 1-pole LP filter states for feedback damping
    float lpStateL = 0.0f;
    float lpStateR = 0.0f;
};

} // namespace bb
