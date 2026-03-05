// test_SVFilter.cpp — Tests for bb::SVFilter (Cytomic TPT)
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/SVFilter.h"
#include "dsp/Oscillator.h"
#include "TestHelpers.h"

using namespace bb;
using Catch::Matchers::WithinAbs;

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 8192;

// Generate white noise
static void fillWhiteNoise(float* buf, int n, uint32_t seed = 0xDEADBEEF)
{
    for (int i = 0; i < n; ++i)
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        buf[i] = static_cast<float>(static_cast<int32_t>(seed)) / static_cast<float>(INT32_MAX);
    }
}

TEST_CASE("SVFilter - LP attenuates high frequencies", "[filter]")
{
    SVFilter filt;
    filt.prepare(kSR);
    filt.setParameters(500.0f, 0.0f); // LP at 500 Hz, no resonance

    // Generate impulse
    float input[kBlock] = {};
    input[0] = 1.0f;

    float output[kBlock];
    for (int i = 0; i < kBlock; ++i)
        output[i] = filt.tick(input[i], FilterMode::LP);

    REQUIRE_FALSE(test::hasNaN(output, kBlock));
    // LP output should be non-silent (passes low freqs from impulse)
    REQUIRE(test::peakAmplitude(output, kBlock) > 0.001f);
    // Energy should be lower than input (filtered)
    REQUIRE(test::rms(output, kBlock) < test::rms(input, kBlock));
}

TEST_CASE("SVFilter - HP attenuates low frequencies", "[filter]")
{
    SVFilter filt;
    filt.prepare(kSR);
    filt.setParameters(5000.0f, 0.0f); // HP at 5kHz

    // Feed a 100 Hz sine (should be mostly attenuated)
    float output[kBlock];
    bb::Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(100.0);

    for (int i = 0; i < kBlock; ++i)
        output[i] = filt.tick(osc.tick(), FilterMode::HP);

    REQUIRE_FALSE(test::hasNaN(output, kBlock));
    // 100 Hz through HP@5kHz should be very quiet
    REQUIRE(test::rms(output, kBlock) < 0.1f);
}

TEST_CASE("SVFilter - BP passes center frequency", "[filter]")
{
    SVFilter filt;
    filt.prepare(kSR);
    filt.setParameters(1000.0f, 0.7f); // BP at 1kHz with resonance

    // Feed 1kHz sine (should pass)
    float output[kBlock];
    bb::Oscillator osc;
    osc.prepare(kSR);
    osc.setWaveType(WaveType::Sine);
    osc.setFrequency(1000.0);

    for (int i = 0; i < kBlock; ++i)
        output[i] = filt.tick(osc.tick(), FilterMode::BP);

    REQUIRE_FALSE(test::hasNaN(output, kBlock));
    REQUIRE(test::rms(output, kBlock) > 0.1f);
}

TEST_CASE("SVFilter - Resonance boosts without self-oscillation blowup", "[filter]")
{
    SVFilter filt;
    filt.prepare(kSR);
    filt.setParameters(1000.0f, 0.99f); // Near-max resonance

    float noise[kBlock];
    fillWhiteNoise(noise, kBlock);

    float output[kBlock];
    for (int i = 0; i < kBlock; ++i)
        output[i] = filt.tick(noise[i], FilterMode::LP);

    REQUIRE_FALSE(test::hasNaN(output, kBlock));
    // Filter with high res should still be bounded
    REQUIRE(test::peakAmplitude(output, kBlock) < 50.0f);
}

TEST_CASE("SVFilter - Reset clears state, stability after extreme input", "[filter]")
{
    SVFilter filt;
    filt.prepare(kSR);
    filt.setParameters(1000.0f, 0.5f);

    // Feed some signal
    for (int i = 0; i < 1000; ++i)
        filt.tick(0.5f, FilterMode::LP);

    // Reset
    filt.reset();

    // After reset, feeding zero should produce zero
    float out = filt.tick(0.0f, FilterMode::LP);
    REQUIRE_THAT(static_cast<double>(out), WithinAbs(0.0, 1e-10));

    // Stability: extreme cutoff values
    filt.setParameters(20.0f, 0.0f); // minimum cutoff
    for (int i = 0; i < 1000; ++i)
        filt.tick(1.0f, FilterMode::LP);
    REQUIRE_FALSE(std::isnan(filt.tick(0.0f, FilterMode::LP)));

    filt.setParameters(20000.0f, 0.0f); // maximum cutoff
    for (int i = 0; i < 1000; ++i)
        filt.tick(1.0f, FilterMode::LP);
    REQUIRE_FALSE(std::isnan(filt.tick(0.0f, FilterMode::LP)));
}
