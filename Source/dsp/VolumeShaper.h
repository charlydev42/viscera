// VolumeShaper.h — 32-step volume shaper with drawable table
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

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
        phase.store(0.0, std::memory_order_relaxed);
        // Per-sample smoother so step-to-step jumps in the drawable table
        // don't fire audible clicks. 3 ms keeps the rhythmic character of
        // sharp shapes while killing the zero-crossing discontinuities.
        smoothedGain.reset(sr, 0.003);
        smoothedGain.setCurrentAndTargetValue(1.0f);
    }

    void setRate(float hz)  { rate.store(hz, std::memory_order_relaxed); }
    void setDepth(float d)  { depth.store(d, std::memory_order_relaxed); }

    void reset() { phase.store(0.0, std::memory_order_relaxed); }

    // Returns the gain value for the current sample (call once per sample)
    float tick()
    {
        // Audio-thread-only mutation of phase is still atomic so that a GUI
        // reset() can't interleave with our read-modify-write and tear the
        // double across threads. Relaxed ordering is enough — we don't
        // synchronise any other state on it.
        double p = phase.load(std::memory_order_relaxed);
        int idx = static_cast<int>(static_cast<float>(p) * kNumSteps);
        if (idx >= kNumSteps) idx = kNumSteps - 1;

        float tableVal = table[idx].load(std::memory_order_relaxed);

        p += rate.load(std::memory_order_relaxed) / sampleRate;
        if (p >= 1.0) p -= 1.0;
        phase.store(p, std::memory_order_relaxed);

        const float d = depth.load(std::memory_order_relaxed);
        // depth=0 → gain=1 (bypass), depth=1 → gain follows table
        const float target = 1.0f - d * (1.0f - tableVal);
        smoothedGain.setTargetValue(target);
        return smoothedGain.getNextValue();
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

    float getPhase() const { return static_cast<float>(phase.load(std::memory_order_relaxed)); }

    // Reset table to default sidechain curve
    void resetTable()
    {
        static constexpr float kSidechain[kNumSteps] = {
            0.0f, 0.1f, 0.25f, 0.4f, 0.55f, 0.7f, 0.8f, 0.88f,
            0.94f, 0.97f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.4f
        };
        for (int i = 0; i < kNumSteps; ++i)
            table[i].store(kSidechain[i], std::memory_order_relaxed);
    }

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

    // Deserialize from comma-separated string (with bounds validation)
    void deserializeTable(const juce::String& s)
    {
        if (s.isEmpty()) { resetTable(); return; }
        auto tokens = juce::StringArray::fromTokens(s, ",", "");
        if (tokens.size() < kNumSteps) { resetTable(); return; }
        for (int i = 0; i < kNumSteps && i < tokens.size(); ++i)
        {
            float val = tokens[i].getFloatValue();
            table[i].store(juce::jlimit(0.0f, 1.0f, val), std::memory_order_relaxed);
        }
    }

private:
    std::array<std::atomic<float>, kNumSteps> table;
    // sampleRate is written only in prepare() (message thread, before audio starts)
    // and read by the audio thread — no concurrent access so plain double is fine.
    double sampleRate = 44100.0;
    // phase is written by the audio thread in tick() AND by the GUI thread via
    // reset()/prepare(). rate/depth are written by the GUI thread and read by
    // the audio thread every sample. All atomic with relaxed ordering — we don't
    // synchronise any other state on these values, and floating-point tearing is
    // otherwise observable on some 32-bit platforms.
    std::atomic<double> phase { 0.0 };
    std::atomic<float>  rate  { 4.0f };
    std::atomic<float>  depth { 0.0f };
    juce::SmoothedValue<float> smoothedGain;
};

} // namespace bb
