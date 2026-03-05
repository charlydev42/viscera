// test_LFO.cpp — Tests for bb::LFO
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/LFO.h"
#include "TestHelpers.h"

using namespace bb;
using Catch::Matchers::WithinAbs;

static constexpr double kSR = 44100.0;

TEST_CASE("LFO - Sine waveform range [-1, +1]", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);
    lfo.setRate(1.0f);
    lfo.setWaveType(LFOWaveType::Sine);

    float minVal = 2.0f, maxVal = -2.0f;
    for (int i = 0; i < 44100; ++i) // 1 full cycle at 1Hz
    {
        float v = lfo.tick();
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
        REQUIRE(std::isfinite(v));
    }

    REQUIRE(maxVal > 0.9f);
    REQUIRE(minVal < -0.9f);
    REQUIRE(maxVal <= 1.001f);
    REQUIRE(minVal >= -1.001f);
}

TEST_CASE("LFO - Triangle waveform", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);
    lfo.setRate(2.0f);
    lfo.setWaveType(LFOWaveType::Triangle);

    float minVal = 2.0f, maxVal = -2.0f;
    for (int i = 0; i < 44100; ++i)
    {
        float v = lfo.tick();
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
    }

    REQUIRE(maxVal > 0.9f);
    REQUIRE(minVal < -0.9f);
}

TEST_CASE("LFO - Saw waveform", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);
    lfo.setRate(2.0f);
    lfo.setWaveType(LFOWaveType::Saw);

    float minVal = 2.0f, maxVal = -2.0f;
    for (int i = 0; i < 44100; ++i)
    {
        float v = lfo.tick();
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
    }

    REQUIRE(maxVal > 0.9f);
    REQUIRE(minVal < -0.9f);
}

TEST_CASE("LFO - Square waveform", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);
    lfo.setRate(2.0f);
    lfo.setWaveType(LFOWaveType::Square);

    bool hasPos = false, hasNeg = false;
    for (int i = 0; i < 44100; ++i)
    {
        float v = lfo.tick();
        if (v > 0.5f) hasPos = true;
        if (v < -0.5f) hasNeg = true;
    }

    REQUIRE(hasPos);
    REQUIRE(hasNeg);
}

TEST_CASE("LFO - Sample & Hold produces random values", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);
    lfo.setRate(10.0f); // Fast S&H
    lfo.setWaveType(LFOWaveType::SandH);

    float prev = lfo.tick();
    int changes = 0;
    for (int i = 0; i < 44100; ++i)
    {
        float v = lfo.tick();
        if (v != prev) ++changes;
        prev = v;
        REQUIRE(v >= -1.0f);
        REQUIRE(v <= 1.0f);
    }

    // S&H at 10Hz should change ~10 times per second
    REQUIRE(changes > 5);
    REQUIRE(changes < 50);
}

TEST_CASE("LFO - Custom curve with Catmull-Rom", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);
    lfo.setRate(2.0f);
    lfo.setWaveType(LFOWaveType::Custom);

    // Set a ramp curve: 0→1 across the cycle
    std::vector<CurvePoint> pts = { {0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f} };
    lfo.setCurvePoints(pts);

    float buf[4096];
    for (int i = 0; i < 4096; ++i)
        buf[i] = lfo.tick();

    REQUIRE_FALSE(test::hasNaN(buf, 4096));
    // Custom output in [-1, +1] (remapped from [0,1] table)
    for (int i = 0; i < 4096; ++i)
    {
        REQUIRE(buf[i] >= -1.01f);
        REQUIRE(buf[i] <= 1.01f);
    }
}

TEST_CASE("LFO - tickBlock advances phase correctly", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);
    lfo.setRate(1.0f);
    lfo.setWaveType(LFOWaveType::Sine);

    // tickBlock(441) = advance 441 samples at 1Hz = phase advance of 441/44100 = 0.01
    float phase0 = lfo.getPhase();
    lfo.tickBlock(441);
    float phase1 = lfo.getPhase();

    float expected = static_cast<float>(441.0 / 44100.0);
    REQUIRE_THAT(static_cast<double>(phase1 - phase0), WithinAbs(expected, 0.001));
}

TEST_CASE("LFO - Serialization round-trip", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);

    // Set custom table
    for (int i = 0; i < LFO::kNumSteps; ++i)
        lfo.setStep(i, static_cast<float>(i) / 31.0f);

    // Serialize + deserialize table
    auto tableStr = lfo.serializeTable();
    LFO lfo2;
    lfo2.prepare(kSR);
    lfo2.deserializeTable(tableStr);

    for (int i = 0; i < LFO::kNumSteps; ++i)
        REQUIRE_THAT(static_cast<double>(lfo2.getStep(i)),
                     WithinAbs(static_cast<double>(lfo.getStep(i)), 0.01));

    // Serialize + deserialize curve
    std::vector<CurvePoint> pts = { {0.0f, 0.2f}, {0.3f, 0.8f}, {0.7f, 0.1f}, {1.0f, 0.5f} };
    lfo.setCurvePoints(pts);
    auto curveStr = lfo.serializeCurve();

    LFO lfo3;
    lfo3.prepare(kSR);
    lfo3.deserializeCurve(curveStr);

    auto pts2 = lfo3.getCurvePoints();
    REQUIRE(pts2.size() == pts.size());
    for (size_t i = 0; i < pts.size(); ++i)
    {
        REQUIRE_THAT(static_cast<double>(pts2[i].x), WithinAbs(pts[i].x, 0.01));
        REQUIRE_THAT(static_cast<double>(pts2[i].y), WithinAbs(pts[i].y, 0.01));
    }
}

TEST_CASE("LFO - getUniPeak for standard vs custom", "[lfo]")
{
    LFO lfo;
    lfo.prepare(kSR);

    // Standard waveforms always return 1.0
    lfo.setWaveType(LFOWaveType::Sine);
    REQUIRE(lfo.getUniPeak() == 1.0f);

    // Custom with all 0.5 returns 0.5
    lfo.setWaveType(LFOWaveType::Custom);
    lfo.resetCurve();
    REQUIRE_THAT(static_cast<double>(lfo.getUniPeak()), WithinAbs(0.5, 0.01));
}
