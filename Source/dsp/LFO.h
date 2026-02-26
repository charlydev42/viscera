// LFO.h — Oscillateur basse fréquence (Low Frequency Oscillator)
// Free-running : la phase ne se reset pas à chaque note
// Formes d'onde : Sine, Triangle, Saw, Square, Sample & Hold (S&H), Custom (Catmull-Rom curve)
#pragma once
#include "Oscillator.h"
#include <array>
#include <atomic>
#include <random>
#include <vector>
#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

namespace bb {

enum class LFOWaveType : int { Sine = 0, Triangle, Saw, Square, SandH, Custom, Count };

struct CurvePoint { float x, y; }; // x,y in [0,1]

class LFO
{
public:
    static constexpr int kNumSteps = 32;

    LFO()
    {
        for (int i = 0; i < kNumSteps; ++i)
            customTable[i].store(0.5f, std::memory_order_relaxed);
        curvePoints = { {0.0f, 0.5f}, {1.0f, 0.5f} };
    }

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        phase = 0.0;
        sAndHValue = 0.0f;
        prevPhaseWasHigh = false;
    }

    void setRate(float rateHz) noexcept
    {
        rate = static_cast<double>(rateHz);
    }

    void setWaveType(LFOWaveType type) noexcept { waveType = type; }

    void resetPhase() noexcept { phase = 0.0; }

    // Per-sample tick (for per-voice LFOs called every sample)
    float tick() noexcept { return tickBlock(1); }

    // Returns a value in [-1, +1], called once per audio block
    float tickBlock(int numSamples) noexcept
    {
        float out = 0.0f;

        switch (waveType)
        {
        case LFOWaveType::Sine:
            out = Oscillator::lookupSinePublic(phase);
            break;

        case LFOWaveType::Triangle:
            out = static_cast<float>(2.0 * std::abs(2.0 * phase - 1.0) - 1.0);
            break;

        case LFOWaveType::Saw:
            out = static_cast<float>(2.0 * phase - 1.0);
            break;

        case LFOWaveType::Square:
            out = (phase < 0.5) ? 1.0f : -1.0f;
            break;

        case LFOWaveType::SandH:
        {
            bool high = (phase >= 0.5);
            if (high && !prevPhaseWasHigh)
                sAndHValue = dist(rng);
            prevPhaseWasHigh = high;
            out = sAndHValue;
            break;
        }

        case LFOWaveType::Custom:
        {
            // Evaluate Catmull-Rom directly for smooth output
            float v = evalCatmullRom(static_cast<float>(phase));
            out = v * 2.0f - 1.0f; // [0,1] -> [-1,+1]
            break;
        }

        default:
            break;
        }

        // Advance phase by the full block duration
        phase += (rate * numSamples) / sr;
        phase -= std::floor(phase);

        return out;
    }

    float getPhase() const noexcept { return static_cast<float>(phase); }

    // Peak of current waveform in unipolar [0,1] space
    float getUniPeak() const noexcept
    {
        if (waveType != LFOWaveType::Custom)
            return 1.0f; // standard waveforms always reach full range

        // Custom curve: peak = max curve y value (which equals unipolar value)
        float peak = 0.0f;
        for (int i = 0; i < kNumSteps; ++i)
            peak = std::max(peak, customTable[i].load(std::memory_order_relaxed));
        return peak;
    }

    // --- Custom table step access (lock-free via atomics) ---
    void setStep(int index, float value)
    {
        if (index >= 0 && index < kNumSteps)
            customTable[index].store(value, std::memory_order_relaxed);
    }

    float getStep(int index) const
    {
        if (index >= 0 && index < kNumSteps)
            return customTable[index].load(std::memory_order_relaxed);
        return 0.5f;
    }

    // --- Curve point system (GUI thread only) ---
    void setCurvePoints(const std::vector<CurvePoint>& pts)
    {
        curvePoints = pts;
        // Sort by x
        std::sort(curvePoints.begin(), curvePoints.end(),
                  [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
        // Force endpoints
        if (curvePoints.empty())
            curvePoints = { {0.0f, 0.5f}, {1.0f, 0.5f} };
        curvePoints.front().x = 0.0f;
        curvePoints.back().x = 1.0f;
        // Clamp y values
        for (auto& p : curvePoints)
            p.y = std::clamp(p.y, 0.0f, 1.0f);
        bakeToTable();
    }

    const std::vector<CurvePoint>& getCurvePoints() const { return curvePoints; }

    // Evaluate Catmull-Rom curve at position t in [0,1], returns y in [0,1]
    float evalCatmullRom(float t) const
    {
        if (curvePoints.size() < 2) return 0.5f;
        t = std::clamp(t, 0.0f, 1.0f);

        // Find segment: curvePoints[i].x <= t < curvePoints[i+1].x
        int n = static_cast<int>(curvePoints.size());
        int seg = 0;
        for (int i = 0; i < n - 1; ++i)
        {
            if (t >= curvePoints[i].x && t <= curvePoints[i + 1].x)
            { seg = i; break; }
            seg = i;
        }

        // 4 control points for Catmull-Rom: p0, p1, p2, p3
        int i0 = std::max(seg - 1, 0);
        int i1 = seg;
        int i2 = std::min(seg + 1, n - 1);
        int i3 = std::min(seg + 2, n - 1);

        float x1 = curvePoints[i1].x;
        float x2 = curvePoints[i2].x;
        float span = x2 - x1;
        float localT = (span > 1e-6f) ? (t - x1) / span : 0.0f;
        localT = std::clamp(localT, 0.0f, 1.0f);

        float p0 = curvePoints[i0].y;
        float p1 = curvePoints[i1].y;
        float p2 = curvePoints[i2].y;
        float p3 = curvePoints[i3].y;

        // Catmull-Rom spline formula
        float tt = localT * localT;
        float ttt = tt * localT;
        float v = 0.5f * ((2.0f * p1) +
                          (-p0 + p2) * localT +
                          (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * tt +
                          (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * ttt);
        return std::clamp(v, 0.0f, 1.0f);
    }

    // Bake curve points into the 32-step atomic table
    void bakeToTable()
    {
        for (int i = 0; i < kNumSteps; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(kNumSteps - 1);
            float v = evalCatmullRom(t);
            customTable[i].store(v, std::memory_order_relaxed);
        }
    }

    // --- Serialization ---
    juce::String serializeTable() const
    {
        juce::String s;
        for (int i = 0; i < kNumSteps; ++i)
        {
            if (i > 0) s += ",";
            s += juce::String(customTable[i].load(std::memory_order_relaxed), 3);
        }
        return s;
    }

    void deserializeTable(const juce::String& s)
    {
        auto tokens = juce::StringArray::fromTokens(s, ",", "");
        for (int i = 0; i < kNumSteps && i < tokens.size(); ++i)
            customTable[i].store(tokens[i].getFloatValue(), std::memory_order_relaxed);
    }

    // Curve serialization: "x,y;x,y;x,y"
    juce::String serializeCurve() const
    {
        juce::String s;
        for (size_t i = 0; i < curvePoints.size(); ++i)
        {
            if (i > 0) s += ";";
            s += juce::String(curvePoints[i].x, 3) + "," + juce::String(curvePoints[i].y, 3);
        }
        return s;
    }

    void deserializeCurve(const juce::String& s)
    {
        auto pointTokens = juce::StringArray::fromTokens(s, ";", "");
        std::vector<CurvePoint> pts;
        for (const auto& tok : pointTokens)
        {
            auto xy = juce::StringArray::fromTokens(tok, ",", "");
            if (xy.size() >= 2)
                pts.push_back({ xy[0].getFloatValue(), xy[1].getFloatValue() });
        }
        if (pts.size() >= 2)
            setCurvePoints(pts); // sorts, clamps, bakes
    }

private:
    double sr = 44100.0;
    double rate = 1.0;   // Hz
    double phase = 0.0;
    LFOWaveType waveType = LFOWaveType::Sine;

    // Sample & Hold
    float sAndHValue = 0.0f;
    bool prevPhaseWasHigh = false;
    std::mt19937 rng { 42 };
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };

    // Custom drawable table (32 steps, [0,1])
    std::array<std::atomic<float>, kNumSteps> customTable;

    // Curve control points (GUI thread only)
    std::vector<CurvePoint> curvePoints;
};

} // namespace bb
