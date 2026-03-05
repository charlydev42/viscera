// test_DCBlocker.cpp — Tests for bb::DCBlocker
#include <catch2/catch_test_macros.hpp>
#include "dsp/DCBlocker.h"
#include "dsp/Oscillator.h"
#include "TestHelpers.h"

using namespace bb;

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 44100; // 1 second

TEST_CASE("DCBlocker - Removes DC offset", "[dc]")
{
    DCBlocker dc;
    dc.prepare(kSR);

    // Feed constant DC (= pure offset)
    float buf[kBlock];
    for (int i = 0; i < kBlock; ++i)
        buf[i] = dc.tick(0.5f);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    // After settling, DC should be removed — output near zero at end
    float endRms = test::rms(buf + kBlock - 1000, 1000);
    REQUIRE(endRms < 0.01f);
}

TEST_CASE("DCBlocker - Preserves AC signal", "[dc]")
{
    DCBlocker dc;
    dc.prepare(kSR);

    // Feed a 440 Hz sine
    bb::Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(440.0);

    float input[kBlock], output[kBlock];
    for (int i = 0; i < kBlock; ++i)
    {
        input[i] = osc.tick();
        output[i] = dc.tick(input[i]);
    }

    REQUIRE_FALSE(test::hasNaN(output, kBlock));
    // RMS of output should be close to input RMS (after settling)
    float inputRms = test::rms(input + 1000, kBlock - 1000);
    float outputRms = test::rms(output + 1000, kBlock - 1000);
    REQUIRE(outputRms > inputRms * 0.95f);
}

TEST_CASE("DCBlocker - Stability after reset", "[dc]")
{
    DCBlocker dc;
    dc.prepare(kSR);

    for (int i = 0; i < 1000; ++i)
        dc.tick(1.0f);

    dc.reset();

    // After reset, should be clean
    float out = dc.tick(0.0f);
    REQUIRE(std::fabs(out) < 0.001f);
}
