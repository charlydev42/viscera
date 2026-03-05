// test_VolumeShaper.cpp — Tests for bb::VolumeShaper
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/VolumeShaper.h"

using namespace bb;
using Catch::Matchers::WithinAbs;

static constexpr double kSR = 44100.0;

TEST_CASE("VolumeShaper - Bypass at depth=0", "[shaper]")
{
    VolumeShaper vs;
    vs.prepare(kSR);
    vs.setRate(4.0f);
    vs.setDepth(0.0f);

    // At depth 0, gain should always be 1.0
    for (int i = 0; i < 1000; ++i)
    {
        float gain = vs.tick();
        REQUIRE_THAT(static_cast<double>(gain), WithinAbs(1.0, 0.001));
    }
}

TEST_CASE("VolumeShaper - Depth=1 follows table", "[shaper]")
{
    VolumeShaper vs;
    vs.prepare(kSR);
    vs.setRate(4.0f);
    vs.setDepth(1.0f);
    vs.resetTable(); // default sidechain curve

    // First step of sidechain is 0.0, so gain should be 0.0
    float firstGain = vs.tick();
    REQUIRE_THAT(static_cast<double>(firstGain), WithinAbs(0.0, 0.01));

    // After advancing half a cycle, gain should be near 1.0 (sustain region)
    int halfCycle = static_cast<int>(kSR / 4.0 / 2.0);
    float gain = 0.0f;
    for (int i = 0; i < halfCycle; ++i)
        gain = vs.tick();

    REQUIRE(gain > 0.8f);
}

TEST_CASE("VolumeShaper - Phase advances correctly", "[shaper]")
{
    VolumeShaper vs;
    vs.prepare(kSR);
    vs.setRate(1.0f); // 1 Hz
    vs.setDepth(0.5f);

    REQUIRE_THAT(static_cast<double>(vs.getPhase()), WithinAbs(0.0, 0.001));

    // After 44100 ticks at 1 Hz, phase should be back to 0 (one full cycle)
    for (int i = 0; i < 44100; ++i)
        vs.tick();

    REQUIRE_THAT(static_cast<double>(vs.getPhase()), WithinAbs(0.0, 0.01));
}

TEST_CASE("VolumeShaper - Serialization round-trip", "[shaper]")
{
    VolumeShaper vs;
    vs.prepare(kSR);
    vs.resetTable();

    auto csv = vs.serializeTable();
    REQUIRE(csv.isNotEmpty());

    VolumeShaper vs2;
    vs2.prepare(kSR);
    vs2.deserializeTable(csv);

    for (int i = 0; i < VolumeShaper::kNumSteps; ++i)
        REQUIRE_THAT(static_cast<double>(vs2.getStep(i)),
                     WithinAbs(vs.getStep(i), 0.001));
}
