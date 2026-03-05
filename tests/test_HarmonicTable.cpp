// test_HarmonicTable.cpp — Tests for bb::HarmonicTable
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h> // needed for juce::String used by HarmonicTable
#include "dsp/HarmonicTable.h"

using namespace bb;
using Catch::Matchers::WithinAbs;

TEST_CASE("HarmonicTable - Default is sine", "[harmonic]")
{
    HarmonicTable ht;

    // Only H1 should be set
    REQUIRE(ht.getHarmonic(0) == 1.0f);
    for (int h = 1; h < kHarmonicCount; ++h)
        REQUIRE(ht.getHarmonic(h) == 0.0f);

    // Lookup at phase=0.25 should be ~1.0 (sine peak)
    float val = ht.lookup(0.25);
    REQUIRE_THAT(static_cast<double>(val), WithinAbs(1.0, 0.01));
}

TEST_CASE("HarmonicTable - setHarmonic/getHarmonic round-trip", "[harmonic]")
{
    HarmonicTable ht;

    ht.setHarmonic(3, 0.75f);
    REQUIRE(ht.getHarmonic(3) == 0.75f);

    ht.setHarmonic(0, 0.5f);
    REQUIRE(ht.getHarmonic(0) == 0.5f);

    // Out of bounds returns 0
    REQUIRE(ht.getHarmonic(-1) == 0.0f);
    REQUIRE(ht.getHarmonic(32) == 0.0f);
}

TEST_CASE("HarmonicTable - initFromWaveType presets", "[harmonic]")
{
    HarmonicTable ht;

    SECTION("Sine")
    {
        ht.initFromWaveType(0);
        REQUIRE(ht.getHarmonic(0) == 1.0f);
        REQUIRE(ht.getHarmonic(1) == 0.0f);
    }

    SECTION("Saw (1/n)")
    {
        ht.initFromWaveType(1);
        REQUIRE_THAT(static_cast<double>(ht.getHarmonic(0)), WithinAbs(1.0, 0.01));
        REQUIRE_THAT(static_cast<double>(ht.getHarmonic(1)), WithinAbs(0.5, 0.01));
        REQUIRE_THAT(static_cast<double>(ht.getHarmonic(2)), WithinAbs(1.0/3.0, 0.01));
    }

    SECTION("Square (odd harmonics only)")
    {
        ht.initFromWaveType(2);
        REQUIRE(ht.getHarmonic(0) > 0.0f); // H1 (odd)
        REQUIRE(ht.getHarmonic(1) == 0.0f); // H2 (even)
        REQUIRE(ht.getHarmonic(2) > 0.0f); // H3 (odd)
        REQUIRE(ht.getHarmonic(3) == 0.0f); // H4 (even)
    }

    SECTION("Triangle (odd harmonics, 1/n^2)")
    {
        ht.initFromWaveType(3);
        REQUIRE(std::fabs(ht.getHarmonic(0)) > 0.0f);
        REQUIRE(ht.getHarmonic(1) == 0.0f); // even
    }
}

TEST_CASE("HarmonicTable - Rebake normalizes to peak 1.0", "[harmonic]")
{
    HarmonicTable ht;

    // Set multiple harmonics at full
    for (int h = 0; h < 8; ++h)
        ht.setHarmonic(h, 1.0f);

    ht.rebake();

    // After rebake, peak amplitude in table should be ~1.0
    float peak = 0.0f;
    for (int i = 0; i < kWavetableSize; ++i)
    {
        float v = std::fabs(ht.lookup(static_cast<double>(i) / kWavetableSize));
        peak = std::max(peak, v);
    }

    REQUIRE_THAT(static_cast<double>(peak), WithinAbs(1.0, 0.05));
}

TEST_CASE("HarmonicTable - Serialization round-trip", "[harmonic]")
{
    HarmonicTable ht;
    ht.initFromWaveType(1); // Saw

    auto csv = ht.serializeHarmonics();
    REQUIRE(csv.isNotEmpty());

    HarmonicTable ht2;
    ht2.deserializeHarmonics(csv);

    for (int h = 0; h < kHarmonicCount; ++h)
        REQUIRE_THAT(static_cast<double>(ht2.getHarmonic(h)),
                     WithinAbs(ht.getHarmonic(h), 0.001));
}
