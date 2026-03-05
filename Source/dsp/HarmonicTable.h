// HarmonicTable.h — 32-harmonic additive wavetable with double-buffered atomic swap
// GUI thread writes harmonics + rebakes wavetable; audio thread reads via lookup()
// Same zero-overhead pattern as SineTable: linear interpolation on a 4096-sample cycle
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace bb {

static constexpr int kHarmonicCount = 32;
static constexpr int kWavetableSize = 4096;
static constexpr double kTwoPiWT = 2.0 * 3.14159265358979323846;

class HarmonicTable
{
public:
    HarmonicTable()
    {
        harmonics[0].store(1.0f, std::memory_order_relaxed); // default: pure sine
        for (int i = 1; i < kHarmonicCount; ++i)
            harmonics[i].store(0.0f, std::memory_order_relaxed);

        rebake();
    }

    // --- GUI writes a single harmonic amplitude [0,1] ---
    void setHarmonic(int idx, float amp)
    {
        if (idx >= 0 && idx < kHarmonicCount)
            harmonics[idx].store(amp, std::memory_order_relaxed);
    }

    // --- GUI/DSP reads a harmonic amplitude ---
    float getHarmonic(int idx) const
    {
        if (idx >= 0 && idx < kHarmonicCount)
            return harmonics[idx].load(std::memory_order_relaxed);
        return 0.0f;
    }

    // --- Audio thread: wavetable lookup with linear interpolation ---
    float lookup(double phase) const noexcept
    {
        const float* table = readTable.load(std::memory_order_acquire);
        double idx = (phase - std::floor(phase)) * kWavetableSize;
        int i0 = static_cast<int>(idx) & (kWavetableSize - 1);
        float frac = static_cast<float>(idx - std::floor(idx));
        return table[i0] + frac * (table[i0 + 1] - table[i0]);
    }

    // --- GUI thread: recalculate wavetable from harmonics, then swap ---
    void rebake()
    {
        // Determine which buffer is the write buffer
        float* wBuf = (readTable.load(std::memory_order_relaxed) == tableA) ? tableB : tableA;

        // Additive synthesis: sum 32 harmonics
        float peak = 0.0f;
        for (int s = 0; s < kWavetableSize; ++s)
        {
            double sum = 0.0;
            double ph = static_cast<double>(s) / static_cast<double>(kWavetableSize);
            for (int h = 0; h < kHarmonicCount; ++h)
            {
                float amp = harmonics[h].load(std::memory_order_relaxed);
                if (amp > 0.0001f)
                    sum += static_cast<double>(amp) * std::sin(kTwoPiWT * ph * (h + 1));
            }
            wBuf[s] = static_cast<float>(sum);
            float abs = std::fabs(wBuf[s]);
            if (abs > peak) peak = abs;
        }

        // Normalize peak to [-1, 1]
        if (peak > 0.0001f)
        {
            float inv = 1.0f / peak;
            for (int s = 0; s < kWavetableSize; ++s)
                wBuf[s] *= inv;
        }

        // Guard sample for interpolation
        wBuf[kWavetableSize] = wBuf[0];

        // Atomic swap
        readTable.store(wBuf, std::memory_order_release);
    }

    // --- Pre-fill harmonics from standard waveform types ---
    void initFromWaveType(int waveIdx)
    {
        for (int h = 0; h < kHarmonicCount; ++h)
            harmonics[h].store(0.0f, std::memory_order_relaxed);

        switch (waveIdx)
        {
            case 0: // Sine
                harmonics[0].store(1.0f, std::memory_order_relaxed);
                break;

            case 1: // Saw: H(n) = 1/n
                for (int h = 0; h < kHarmonicCount; ++h)
                    harmonics[h].store(1.0f / static_cast<float>(h + 1), std::memory_order_relaxed);
                break;

            case 2: // Square: H(n) = 1/n for odd n
                for (int h = 0; h < kHarmonicCount; ++h)
                {
                    int n = h + 1;
                    if (n % 2 == 1)
                        harmonics[h].store(1.0f / static_cast<float>(n), std::memory_order_relaxed);
                }
                break;

            case 3: // Triangle: H(n) = 1/n² for odd n, alternating ±
            {
                int sign = 1;
                for (int h = 0; h < kHarmonicCount; ++h)
                {
                    int n = h + 1;
                    if (n % 2 == 1)
                    {
                        float amp = 1.0f / static_cast<float>(n * n);
                        harmonics[h].store(amp * sign, std::memory_order_relaxed);
                        sign = -sign;
                    }
                }
                break;
            }

            case 4: // Pulse: H(n) = sin(n*pi/4) * 2/(n*pi)
            {
                constexpr float pi = 3.14159265358979323846f;
                for (int h = 0; h < kHarmonicCount; ++h)
                {
                    int n = h + 1;
                    float amp = std::sin(static_cast<float>(n) * pi / 4.0f)
                                * 2.0f / (static_cast<float>(n) * pi);
                    harmonics[h].store(std::fabs(amp), std::memory_order_relaxed);
                }
                break;
            }

            default: // Fallback to sine
                harmonics[0].store(1.0f, std::memory_order_relaxed);
                break;
        }

        rebake();
    }

    // --- Serialization: CSV string of 32 values ---
    juce::String serializeHarmonics() const
    {
        juce::String s;
        for (int h = 0; h < kHarmonicCount; ++h)
        {
            if (h > 0) s += ",";
            s += juce::String(harmonics[h].load(std::memory_order_relaxed), 4);
        }
        return s;
    }

    // Reset to default sine (H1=1, rest=0)
    void resetHarmonics()
    {
        for (int h = 0; h < kHarmonicCount; ++h)
            harmonics[h].store(h == 0 ? 1.0f : 0.0f, std::memory_order_relaxed);
        rebake();
    }

    void deserializeHarmonics(const juce::String& csv)
    {
        auto tokens = juce::StringArray::fromTokens(csv, ",", "");
        for (int h = 0; h < kHarmonicCount; ++h)
        {
            if (h < tokens.size())
                harmonics[h].store(tokens[h].getFloatValue(), std::memory_order_relaxed);
            else
                harmonics[h].store(0.0f, std::memory_order_relaxed);
        }
        rebake();
    }

private:
    std::array<std::atomic<float>, kHarmonicCount> harmonics;
    float tableA[kWavetableSize + 1] = {};
    float tableB[kWavetableSize + 1] = {};
    std::atomic<float*> readTable { tableA };
};

} // namespace bb
