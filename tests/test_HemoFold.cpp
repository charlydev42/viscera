// test_HemoFold.cpp — Tests for bb::HemoFold (multi-stage wavefolder)
#include <catch2/catch_test_macros.hpp>
#include "dsp/HemoFold.h"
#include "dsp/Oscillator.h"
#include "TestHelpers.h"

using namespace bb;

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 4096;

TEST_CASE("HemoFold - Bypass when amount < 0.001", "[fold]")
{
    HemoFold fold;
    fold.prepare(kSR);
    fold.setAmount(0.0f);

    float input = 0.7f;
    float output = fold.tick(input);

    REQUIRE(output == input);
}

TEST_CASE("HemoFold - Progressive stages activate", "[fold]")
{
    HemoFold fold;
    fold.prepare(kSR);

    // Low amount: only stage 1
    fold.setAmount(0.2f);
    float out_low = fold.tick(0.8f);
    fold.reset();

    // High amount: all 3 stages
    fold.setAmount(0.9f);
    float out_high = fold.tick(0.8f);
    fold.reset();

    REQUIRE(std::isfinite(out_low));
    REQUIRE(std::isfinite(out_high));
    // Different amounts should produce different outputs
    REQUIRE(out_low != out_high);
}

TEST_CASE("HemoFold - DC blocker removes offset", "[fold]")
{
    HemoFold fold;
    fold.prepare(kSR);
    fold.setAmount(0.5f);

    // Feed a sine signal and check DC offset is small
    bb::Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(440.0);

    float buf[kBlock];
    for (int i = 0; i < kBlock; ++i)
        buf[i] = fold.tick(osc.tick());

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    // DC should be small (DC blocker active)
    REQUIRE(std::fabs(test::dcOffset(buf, kBlock)) < 0.15f);
}

TEST_CASE("HemoFold - Stability at max amount with loud input", "[fold]")
{
    HemoFold fold;
    fold.prepare(kSR);
    fold.setAmount(1.0f);

    // Feed loud signal
    float buf[kBlock];
    for (int i = 0; i < kBlock; ++i)
    {
        float input = (i % 2 == 0) ? 1.0f : -1.0f; // harsh square
        buf[i] = fold.tick(input);
    }

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    // Should not blow up (bounded by tanh/sin)
    REQUIRE(test::peakAmplitude(buf, kBlock) < 3.0f);
}
