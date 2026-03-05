// test_ADSREnvelope.cpp — Tests for bb::ADSREnvelope
#include <catch2/catch_test_macros.hpp>
#include "dsp/ADSREnvelope.h"

using namespace bb;

static constexpr double kSR = 44100.0;

TEST_CASE("ADSR - Full lifecycle", "[adsr]")
{
    ADSREnvelope env;
    env.prepare(kSR);
    env.setParameters(0.01f, 0.1f, 0.7f, 0.3f);

    // Before noteOn: inactive
    REQUIRE_FALSE(env.isActive());

    env.noteOn();
    REQUIRE(env.isActive());

    // Tick through attack + decay (~0.11s = ~4851 samples)
    float peak = 0.0f;
    for (int i = 0; i < 5000; ++i)
    {
        float v = env.tick();
        peak = std::max(peak, v);
        REQUIRE(v >= 0.0f);
        REQUIRE(v <= 1.001f); // small tolerance for ramp overshoot
    }

    // Should have reached peak ~1.0 during attack
    REQUIRE(peak > 0.95f);

    // After decay, should be near sustain level
    float sustainLevel = env.tick();
    REQUIRE(sustainLevel > 0.5f);
    REQUIRE(sustainLevel < 0.9f);

    // Note off
    env.noteOff();
    // Tick through release (~0.3s = ~13230 samples)
    for (int i = 0; i < 15000; ++i)
        env.tick();

    REQUIRE_FALSE(env.isActive());
}

TEST_CASE("ADSR - Output is bounded [0, 1]", "[adsr]")
{
    ADSREnvelope env;
    env.prepare(kSR);
    env.setParameters(0.001f, 0.05f, 1.0f, 0.1f);
    env.noteOn();

    for (int i = 0; i < 10000; ++i)
    {
        float v = env.tick();
        REQUIRE(v >= 0.0f);
        REQUIRE(v <= 1.001f);
    }
}

TEST_CASE("ADSR - Edge cases", "[adsr]")
{
    SECTION("Instant attack, zero sustain")
    {
        ADSREnvelope env;
        env.prepare(kSR);
        env.setParameters(0.001f, 0.01f, 0.0f, 0.001f);
        env.noteOn();

        // Should ramp up then decay to zero
        float peak = 0.0f;
        for (int i = 0; i < 2000; ++i)
        {
            float v = env.tick();
            peak = std::max(peak, v);
        }
        REQUIRE(peak > 0.5f);

        // After decay, near zero
        float final_v = env.tick();
        REQUIRE(final_v < 0.1f);
    }

    SECTION("Reset clears state")
    {
        ADSREnvelope env;
        env.prepare(kSR);
        env.setParameters(0.01f, 0.1f, 0.7f, 0.3f);
        env.noteOn();
        for (int i = 0; i < 1000; ++i)
            env.tick();

        env.reset();
        REQUIRE_FALSE(env.isActive());
    }
}
