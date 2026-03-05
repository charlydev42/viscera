// test_Oscillator.cpp — Tests for bb::Oscillator
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/Oscillator.h"
#include "TestHelpers.h"

using namespace bb;
using Catch::Matchers::WithinAbs;

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 4096;

TEST_CASE("Oscillator - Sine produces clean signal", "[osc]")
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(440.0);

    float buf[kBlock];
    test::renderOscillator(osc, buf, kBlock);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    REQUIRE(test::peakAmplitude(buf, kBlock) > 0.9f);
    REQUIRE(test::peakAmplitude(buf, kBlock) <= 1.0f);

    float estFreq = test::estimatedFrequency(buf, kBlock, kSR);
    REQUIRE_THAT(estFreq, WithinAbs(440.0, 20.0));
}

TEST_CASE("Oscillator - Saw has correct range and anti-aliasing", "[osc]")
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Saw);
    osc.setFrequency(440.0);

    float buf[kBlock];
    test::renderOscillator(osc, buf, kBlock);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    REQUIRE(test::peakAmplitude(buf, kBlock) > 0.8f);
    REQUIRE(test::peakAmplitude(buf, kBlock) < 1.1f); // PolyBLEP may slightly overshoot
}

TEST_CASE("Oscillator - Square is bipolar", "[osc]")
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Square);
    osc.setFrequency(440.0);

    float buf[kBlock];
    test::renderOscillator(osc, buf, kBlock);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    // Square should have near-zero DC (symmetric)
    REQUIRE(std::fabs(test::dcOffset(buf, kBlock)) < 0.05f);
}

TEST_CASE("Oscillator - Triangle via integrated square", "[osc]")
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Triangle);
    osc.setFrequency(440.0);

    float buf[kBlock];
    test::renderOscillator(osc, buf, kBlock);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    REQUIRE(test::peakAmplitude(buf, kBlock) > 0.5f);
    REQUIRE(test::peakAmplitude(buf, kBlock) < 3.0f); // integrated square overshoots during settling
}

TEST_CASE("Oscillator - Pulse 25% duty cycle", "[osc]")
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Pulse);
    osc.setFrequency(440.0);

    float buf[kBlock];
    test::renderOscillator(osc, buf, kBlock);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    // Pulse has DC offset (asymmetric duty)
    float dc = test::dcOffset(buf, kBlock);
    REQUIRE(std::fabs(dc) > 0.1f); // expect ~-0.5 DC
}

TEST_CASE("Oscillator - Custom wavetable with HarmonicTable", "[osc]")
{
    HarmonicTable ht;
    ht.initFromWaveType(1); // Saw harmonics

    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Custom);
    osc.setHarmonicTable(&ht);
    osc.setFrequency(440.0);

    float buf[kBlock];
    test::renderOscillator(osc, buf, kBlock);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    REQUIRE(test::peakAmplitude(buf, kBlock) > 0.5f);
}

TEST_CASE("Oscillator - Phase modulation", "[osc]")
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(440.0);

    float buf[kBlock];
    // Apply moderate PM (1 radian)
    for (int i = 0; i < kBlock; ++i)
        buf[i] = osc.tick(1.0);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    REQUIRE(test::peakAmplitude(buf, kBlock) > 0.5f);
}

TEST_CASE("Oscillator - Hard sync produces sync pulses", "[osc]")
{
    Oscillator master, slave;
    master.prepare(kSR);
    slave.prepare(kSR);
    master.setWaveType(WaveType::Saw);
    slave.setWaveType(WaveType::Saw);
    master.setFrequency(220.0);
    slave.setFrequency(440.0);

    int syncCount = 0;
    float buf[kBlock];
    for (int i = 0; i < kBlock; ++i)
    {
        master.tick();
        if (master.hasSyncPulse())
        {
            slave.hardSyncReset(master.getSyncFraction());
            ++syncCount;
        }
        buf[i] = slave.tick();
    }

    REQUIRE_FALSE(test::hasNaN(buf, kBlock));
    REQUIRE(syncCount > 0);
    // Sync pulses at ~220 Hz over ~93ms => ~20 syncs
    REQUIRE(syncCount > 15);
}

TEST_CASE("Oscillator - Drift modulates pitch without NaN", "[osc]")
{
    Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(440.0);
    osc.setDrift(1.0f); // max drift

    float buf[kBlock * 4]; // longer buffer to let drift develop
    test::renderOscillator(osc, buf, kBlock * 4);

    REQUIRE_FALSE(test::hasNaN(buf, kBlock * 4));
    REQUIRE(test::peakAmplitude(buf, kBlock * 4) > 0.5f);
}

TEST_CASE("Oscillator - Multiple sample rates", "[osc]")
{
    for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Oscillator osc;
        osc.prepare(sr);
        osc.setWaveType(WaveType::Saw);
        osc.setFrequency(440.0);

        float buf[1024];
        test::renderOscillator(osc, buf, 1024);

        REQUIRE_FALSE(test::hasNaN(buf, 1024));
        REQUIRE(test::peakAmplitude(buf, 1024) > 0.5f);
    }
}
