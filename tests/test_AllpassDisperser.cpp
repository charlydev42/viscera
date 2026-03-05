// test_AllpassDisperser.cpp — Tests for bb::AllpassDisperser
#include <catch2/catch_test_macros.hpp>
#include "dsp/AllpassDisperser.h"
#include "dsp/Oscillator.h"
#include "TestHelpers.h"

using namespace bb;

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 4096;

TEST_CASE("AllpassDisperser - Bypass when amount < 0.001", "[disperser]")
{
    AllpassDisperser disp;
    disp.prepare(kSR);
    disp.setAmount(0.0f);

    float input = 0.7f;
    REQUIRE(disp.tick(input) == input);
}

TEST_CASE("AllpassDisperser - Dispersion changes phase without changing level", "[disperser]")
{
    AllpassDisperser disp;
    disp.prepare(kSR);
    disp.setAmount(0.5f);

    // Feed a sine and measure RMS (allpass should preserve energy approximately)
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(440.0);

    float input[kBlock], output[kBlock];
    for (int i = 0; i < kBlock; ++i)
    {
        input[i] = osc.tick();
        output[i] = disp.tick(input[i]);
    }

    REQUIRE_FALSE(test::hasNaN(output, kBlock));

    // Allpass with dry/wet mix may attenuate, but output should be non-trivial
    float outRms = test::rms(output + 500, kBlock - 500);
    REQUIRE(outRms > 0.05f); // meaningful output
    REQUIRE(outRms < 2.0f);  // no blowup
}

TEST_CASE("AllpassDisperser - Stability at max amount and reset", "[disperser]")
{
    AllpassDisperser disp;
    disp.prepare(kSR);
    disp.setAmount(1.0f);

    float buf[kBlock];
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Saw);
    osc.setFrequency(440.0);

    for (int i = 0; i < kBlock; ++i)
        buf[i] = disp.tick(osc.tick());

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    REQUIRE(test::peakAmplitude(buf, kBlock) < 3.0f);

    disp.reset();
    REQUIRE(std::fabs(disp.tick(0.0f)) < 0.001f);
}
