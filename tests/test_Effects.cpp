// test_Effects.cpp — Tests for LiquidChorus, RubberComb, PlateReverb, StereoDelay
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/LiquidChorus.h"
#include "dsp/RubberComb.h"
#include "dsp/PlateReverb.h"
#include "dsp/StereoDelay.h"
#include "dsp/Oscillator.h"
#include "TestHelpers.h"

using namespace bb;

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 4096;

// Fill stereo buffers with a sine tone
static void fillStereoSine(float* left, float* right, int n, double freq = 440.0)
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(freq);

    for (int i = 0; i < n; ++i)
    {
        float v = osc.tick();
        left[i] = v;
        right[i] = v;
    }
}

// ------ LiquidChorus ------

TEST_CASE("LiquidChorus - Bypass at mix=0", "[fx][liquid]")
{
    LiquidChorus liq;
    liq.prepare(kSR, kBlock);
    liq.setParameters(0.5f, 0.5f, 0.5f, 0.2f, 0.0f); // mix=0

    float left[kBlock], right[kBlock];
    fillStereoSine(left, right, kBlock);
    float origL[kBlock];
    std::copy(left, left + kBlock, origL);

    liq.process(left, right, kBlock);

    // With mix=0, process returns early (wet < 0.0001)
    for (int i = 0; i < kBlock; ++i)
        REQUIRE(left[i] == origL[i]);
}

TEST_CASE("LiquidChorus - Wet signal is finite", "[fx][liquid]")
{
    LiquidChorus liq;
    liq.prepare(kSR, kBlock);
    liq.setParameters(0.5f, 0.5f, 0.5f, 0.2f, 0.6f);

    float left[kBlock], right[kBlock];
    fillStereoSine(left, right, kBlock);

    liq.process(left, right, kBlock);

    REQUIRE_FALSE(test::hasNaN(left, kBlock));
    REQUIRE_FALSE(test::hasNaN(right, kBlock));
}

TEST_CASE("LiquidChorus - Stability and reset", "[fx][liquid]")
{
    LiquidChorus liq;
    liq.prepare(kSR, kBlock);
    liq.setParameters(3.0f, 1.0f, 1.0f, 0.8f, 1.0f); // extreme params

    float left[kBlock], right[kBlock];
    fillStereoSine(left, right, kBlock);

    // Process multiple blocks
    for (int b = 0; b < 10; ++b)
    {
        fillStereoSine(left, right, kBlock);
        liq.process(left, right, kBlock);
        REQUIRE_FALSE(test::hasNaN(left, kBlock));
    }

    liq.reset();
}

// ------ RubberComb ------

TEST_CASE("RubberComb - Bypass at mix=0", "[fx][rubber]")
{
    RubberComb rub;
    rub.prepare(kSR, kBlock);
    rub.setParameters(0.5f, 0.5f, 0.5f, 0.0f); // mix=0

    float left[kBlock], right[kBlock];
    fillStereoSine(left, right, kBlock);
    float origL[kBlock];
    std::copy(left, left + kBlock, origL);

    rub.process(left, right, kBlock);

    for (int i = 0; i < kBlock; ++i)
        REQUIRE(left[i] == origL[i]);
}

TEST_CASE("RubberComb - Wet signal and stability", "[fx][rubber]")
{
    RubberComb rub;
    rub.prepare(kSR, kBlock);
    rub.setParameters(0.5f, 0.5f, 1.0f, 0.6f, 1.0f); // warp max, feed max

    float left[kBlock], right[kBlock];

    for (int b = 0; b < 10; ++b)
    {
        fillStereoSine(left, right, kBlock);
        rub.process(left, right, kBlock);
        REQUIRE_FALSE(test::hasNaN(left, kBlock));
        REQUIRE_FALSE(test::hasNaN(right, kBlock));
    }
}

// ------ PlateReverb ------

TEST_CASE("PlateReverb - Bypass at mix=0", "[fx][reverb]")
{
    PlateReverb rev;
    rev.prepare(kSR, kBlock);
    rev.setParameters(0.5f, 0.5f, 0.0f); // mix=0

    float left[kBlock], right[kBlock];
    fillStereoSine(left, right, kBlock);
    float origPeak = test::peakAmplitude(left, kBlock);

    rev.process(left, right, kBlock);

    // At mix=0, dry signal should dominate — output close to original
    float outPeak = test::peakAmplitude(left, kBlock);
    REQUIRE_THAT(static_cast<double>(outPeak), Catch::Matchers::WithinAbs(origPeak, 0.01));
}

TEST_CASE("PlateReverb - Wet signal produces reverb tail", "[fx][reverb]")
{
    PlateReverb rev;
    rev.prepare(kSR, kBlock);
    rev.setParameters(0.8f, 0.3f, 0.8f, 1.0f, 20.0f); // large, wet

    // Feed a short impulse
    float left[kBlock] = {}, right[kBlock] = {};
    left[0] = 1.0f;
    right[0] = 1.0f;

    rev.process(left, right, kBlock);

    REQUIRE_FALSE(test::hasNaN(left, kBlock));
    // Reverb tail should have energy after the impulse
    float tailRms = test::rms(left + 100, kBlock - 100);
    REQUIRE(tailRms > 0.001f);
}

TEST_CASE("PlateReverb - Stability with extreme params and reset", "[fx][reverb]")
{
    PlateReverb rev;
    rev.prepare(kSR, kBlock);
    rev.setParameters(1.0f, 1.0f, 1.0f, 1.0f, 200.0f);

    float left[kBlock], right[kBlock];
    for (int b = 0; b < 20; ++b)
    {
        fillStereoSine(left, right, kBlock);
        rev.process(left, right, kBlock);
        REQUIRE_FALSE(test::hasNaN(left, kBlock));
        REQUIRE(test::peakAmplitude(left, kBlock) < 10.0f);
    }

    rev.reset();
}

// ------ StereoDelay ------

TEST_CASE("StereoDelay - Bypass at mix=0", "[fx][delay]")
{
    StereoDelay dly;
    dly.prepare(kSR, kBlock);
    dly.setParameters(0.3f, 0.3f, 0.3f, 0.0f, false); // mix=0

    float left[kBlock], right[kBlock];
    fillStereoSine(left, right, kBlock);
    float origL[kBlock];
    std::copy(left, left + kBlock, origL);

    dly.process(left, right, kBlock);

    // mix=0 means output = dry × 1.0 + delayed × 0.0
    for (int i = 0; i < kBlock; ++i)
        REQUIRE_THAT(static_cast<double>(left[i]),
                     Catch::Matchers::WithinAbs(origL[i], 0.001));
}

TEST_CASE("StereoDelay - Wet delayed signal", "[fx][delay]")
{
    StereoDelay dly;
    dly.prepare(kSR, kBlock);
    dly.setParameters(0.01f, 0.0f, 0.0f, 1.0f, false); // short delay, full wet, no feedback

    // Impulse
    float left[kBlock] = {}, right[kBlock] = {};
    left[0] = 1.0f;
    right[0] = 1.0f;

    dly.process(left, right, kBlock);

    REQUIRE_FALSE(test::hasNaN(left, kBlock));
    // Delayed impulse should appear around sample 441 (10ms at 44.1kHz)
    int delaySamples = static_cast<int>(0.01 * kSR);
    float delayedPeak = test::peakAmplitude(left + delaySamples - 5, 10);
    REQUIRE(delayedPeak > 0.5f);
}

TEST_CASE("StereoDelay - Stability and reset", "[fx][delay]")
{
    StereoDelay dly;
    dly.prepare(kSR, kBlock);
    dly.setParameters(0.5f, 0.9f, 0.5f, 0.5f, true, 1.0f); // extreme feedback, pingpong, spread

    float left[kBlock], right[kBlock];
    for (int b = 0; b < 20; ++b)
    {
        fillStereoSine(left, right, kBlock);
        dly.process(left, right, kBlock);
        REQUIRE_FALSE(test::hasNaN(left, kBlock));
        REQUIRE(test::peakAmplitude(left, kBlock) < 10.0f);
    }

    dly.reset();
}
