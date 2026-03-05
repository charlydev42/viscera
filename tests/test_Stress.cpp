// test_Stress.cpp — Stress tests for stability under extreme conditions
#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

static constexpr double kSR = 44100.0;

TEST_CASE("Stress - Various buffer sizes", "[stress]")
{
    VisceraProcessor proc;
    proc.prepareToPlay(kSR, 512);

    for (int blockSize : { 1, 16, 64, 128, 256, 512, 1024, 2048, 4096 })
    {
        INFO("Buffer size: " << blockSize);
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        auto midi = test::createNoteOnBuffer(60, 0.8f, 0);

        proc.processBlock(buffer, midi);

        REQUIRE_FALSE(test::hasNaN(buffer));
        REQUIRE(test::peakAmplitude(buffer) < 10.0f);
    }
}

TEST_CASE("Stress - Various sample rates", "[stress]")
{
    for (double sr : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        INFO("Sample rate: " << sr);
        VisceraProcessor proc;
        proc.prepareToPlay(sr, 512);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        auto midi = test::createNoteOnBuffer(60, 0.8f, 0);

        proc.processBlock(buffer, midi);

        REQUIRE_FALSE(test::hasNaN(buffer));
    }
}

TEST_CASE("Stress - Note spam (rapid note-on/off)", "[stress]")
{
    VisceraProcessor proc;
    proc.prepareToPlay(kSR, 256);

    for (int round = 0; round < 50; ++round)
    {
        juce::AudioBuffer<float> buffer(2, 256);
        buffer.clear();
        juce::MidiBuffer midi;

        // Multiple notes in same block
        for (int n = 0; n < 10; ++n)
        {
            int note = 36 + (round * 3 + n * 7) % 72;
            midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.9f), n * 10);
            if (n > 0) // turn off previous notes
                midi.addEvent(juce::MidiMessage::noteOff(1, 36 + ((round * 3 + (n-1) * 7) % 72), 0.0f), n * 10 + 1);
        }

        proc.processBlock(buffer, midi);
        REQUIRE_FALSE(test::hasNaN(buffer));
    }
}

TEST_CASE("Stress - Rapid preset changes", "[stress]")
{
    VisceraProcessor proc;
    proc.prepareToPlay(kSR, 512);

    const auto& registry = proc.getPresetRegistry();
    int numPresets = static_cast<int>(registry.size());

    for (int i = 0; i < std::min(numPresets, 30); ++i)
    {
        proc.loadPresetAt(i);

        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        auto midi = test::createNoteOnBuffer(60, 0.8f, 0);

        proc.processBlock(buffer, midi);
        REQUIRE_FALSE(test::hasNaN(buffer));
    }
}

TEST_CASE("Stress - Extreme parameter values", "[stress]")
{
    VisceraProcessor proc;
    proc.prepareToPlay(kSR, 512);

    // Set everything to max
    auto setMax = [&](const juce::String& id) {
        if (auto* p = proc.apvts.getParameter(id))
            p->setValueNotifyingHost(1.0f);
    };

    setMax("MOD1_LEVEL"); setMax("MOD2_LEVEL");
    setMax("FILT_RES"); setMax("DRIVE");
    setMax("CAR_NOISE"); setMax("CAR_SPREAD");
    setMax("PLASMA"); setMax("CORTEX"); setMax("ICHOR");
    setMax("DISP_AMT");

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    auto midi = test::createNoteOnBuffer(60, 1.0f, 0);

    proc.processBlock(buffer, midi);

    REQUIRE_FALSE(test::hasNaN(buffer));
    REQUIRE(test::peakAmplitude(buffer) < 20.0f); // extreme but bounded
}

TEST_CASE("Stress - All effects enabled simultaneously", "[stress]")
{
    VisceraProcessor proc;
    proc.prepareToPlay(kSR, 512);

    // Enable all FX
    auto enable = [&](const juce::String& id) {
        if (auto* p = proc.apvts.getParameter(id))
            p->setValueNotifyingHost(1.0f);
    };

    enable("DLY_ON"); enable("REV_ON"); enable("LIQ_ON"); enable("RUB_ON"); enable("SHAPER_ON");
    enable("FILT_ON"); enable("XOR_ON"); enable("PENV_ON");

    // Set FX to moderate wet
    auto setParam = [&](const juce::String& id, float val) {
        if (auto* p = proc.apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(val));
    };

    setParam("DLY_MIX", 0.5f); setParam("REV_MIX", 0.5f);
    setParam("LIQ_MIX", 0.5f); setParam("RUB_MIX", 0.5f);
    setParam("SHAPER_DEPTH", 0.8f);

    // Render multiple blocks with notes
    for (int round = 0; round < 20; ++round)
    {
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        juce::MidiBuffer midi;
        if (round == 0)
            midi = test::createNoteOnBuffer(60, 0.8f, 0);

        proc.processBlock(buffer, midi);

        REQUIRE_FALSE(test::hasNaN(buffer));
        REQUIRE(test::peakAmplitude(buffer) < 20.0f);
    }
}
