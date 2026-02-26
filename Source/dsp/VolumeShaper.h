// VolumeShaper.h — 32-step volume shaper with drawable table
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <juce_core/juce_core.h>

namespace bb {

class VolumeShaper
{
public:
    static constexpr int kNumSteps = 32;

    VolumeShaper()
    {
        for (int i = 0; i < kNumSteps; ++i)
            table[i].store(1.0f, std::memory_order_relaxed);
    }

    void prepare(double sr)
    {
        sampleRate = sr;
        phase = 0.0;
    }

    void setRate(float hz) { rate = hz; }
    void setDepth(float d) { depth = d; }

    void reset() { phase = 0.0; }

    // Returns the gain value for the current sample (call once per sample)
    float tick()
    {
        int idx = static_cast<int>(static_cast<float>(phase) * kNumSteps);
        if (idx >= kNumSteps) idx = kNumSteps - 1;

        float tableVal = table[idx].load(std::memory_order_relaxed);

        // Advance phase
        phase += rate / sampleRate;
        if (phase >= 1.0)
            phase -= 1.0;

        // depth=0 → gain=1 (bypass), depth=1 → gain follows table
        return 1.0f - depth * (1.0f - tableVal);
    }

    // GUI writes steps here (lock-free via atomics)
    void setStep(int index, float value)
    {
        if (index >= 0 && index < kNumSteps)
            table[index].store(value, std::memory_order_relaxed);
    }

    float getStep(int index) const
    {
        if (index >= 0 && index < kNumSteps)
            return table[index].load(std::memory_order_relaxed);
        return 1.0f;
    }

    float getPhase() const { return static_cast<float>(phase); }

    // Serialize table to comma-separated string
    juce::String serializeTable() const
    {
        juce::String s;
        for (int i = 0; i < kNumSteps; ++i)
        {
            if (i > 0) s += ",";
            s += juce::String(table[i].load(std::memory_order_relaxed), 3);
        }
        return s;
    }

    // Deserialize from comma-separated string
    void deserializeTable(const juce::String& s)
    {
        auto tokens = juce::StringArray::fromTokens(s, ",", "");
        for (int i = 0; i < kNumSteps && i < tokens.size(); ++i)
            table[i].store(tokens[i].getFloatValue(), std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, kNumSteps> table;
    double sampleRate = 44100.0;
    double phase = 0.0;
    float rate = 4.0f;
    float depth = 0.0f;
};

} // namespace bb
