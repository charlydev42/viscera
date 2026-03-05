// test_Processor.cpp — Tests for VisceraProcessor (full APVTS integration)
#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 512;

TEST_CASE("Processor - Construction succeeds", "[processor]")
{
    VisceraProcessor proc;
    REQUIRE(proc.getName() == "Viscera");
    REQUIRE(proc.acceptsMidi());
    REQUIRE_FALSE(proc.producesMidi());
}

TEST_CASE("Processor - prepareToPlay and processBlock with silence", "[processor]")
{
    VisceraProcessor proc;
    proc.prepareToPlay(kSR, kBlock);

    juce::AudioBuffer<float> buffer(2, kBlock);
    buffer.clear();
    juce::MidiBuffer midi;

    proc.processBlock(buffer, midi);

    REQUIRE_FALSE(test::hasNaN(buffer));
    // No MIDI = silence
    REQUIRE(test::isSilent(buffer, 1e-5f));
}

TEST_CASE("Processor - processBlock with note produces audio", "[processor]")
{
    VisceraProcessor proc;
    proc.prepareToPlay(kSR, kBlock);

    juce::AudioBuffer<float> buffer(2, kBlock);
    buffer.clear();
    auto midi = test::createNoteOnBuffer(60, 0.8f, 0);

    proc.processBlock(buffer, midi);

    REQUIRE_FALSE(test::hasNaN(buffer));
    REQUIRE_FALSE(test::isSilent(buffer));
}

TEST_CASE("Processor - Parameter layout is complete", "[processor]")
{
    VisceraProcessor proc;

    // Spot-check critical parameters exist
    REQUIRE(proc.apvts.getParameter("MOD1_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("MOD2_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("CAR_WAVE") != nullptr);
    REQUIRE(proc.apvts.getParameter("FILT_CUTOFF") != nullptr);
    REQUIRE(proc.apvts.getParameter("VOLUME") != nullptr);
    REQUIRE(proc.apvts.getParameter("FM_ALGO") != nullptr);
    REQUIRE(proc.apvts.getParameter("DLY_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("REV_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("LIQ_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("RUB_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("SHAPER_ON") != nullptr);
    REQUIRE(proc.apvts.getParameter("LFO1_RATE") != nullptr);
    REQUIRE(proc.apvts.getParameter("LFO2_RATE") != nullptr);
    REQUIRE(proc.apvts.getParameter("LFO3_RATE") != nullptr);
    REQUIRE(proc.apvts.getParameter("PLASMA") != nullptr);
    REQUIRE(proc.apvts.getParameter("CORTEX") != nullptr);
    REQUIRE(proc.apvts.getParameter("ICHOR") != nullptr);
}
