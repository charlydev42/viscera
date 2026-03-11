// test_StateRoundTrip.cpp — Tests for state serialization/deserialization
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "PluginProcessor.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("StateRoundTrip - APVTS params survive save/load", "[state]")
{
    ParasiteProcessor proc1;
    proc1.prepareToPlay(44100.0, 512);

    // Tweak some params
    if (auto* p = proc1.apvts.getParameter("FILT_CUTOFF"))
        p->setValueNotifyingHost(p->convertTo0to1(3000.0f));
    if (auto* p = proc1.apvts.getParameter("VOLUME"))
        p->setValueNotifyingHost(p->convertTo0to1(0.6f));
    if (auto* p = proc1.apvts.getParameter("FM_ALGO"))
        p->setValueNotifyingHost(p->convertTo0to1(2.0f)); // Stack
    if (auto* p = proc1.apvts.getParameter("DLY_ON"))
        p->setValueNotifyingHost(1.0f);

    // Save state
    juce::MemoryBlock stateData;
    proc1.getStateInformation(stateData);
    REQUIRE(stateData.getSize() > 0);

    // Load into new processor
    ParasiteProcessor proc2;
    proc2.prepareToPlay(44100.0, 512);
    proc2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    // Check params match
    auto getVal = [](ParasiteProcessor& p, const juce::String& id) {
        auto* param = p.apvts.getParameter(id);
        return param ? param->convertFrom0to1(param->getValue()) : -999.0f;
    };

    REQUIRE_THAT(static_cast<double>(getVal(proc2, "FILT_CUTOFF")),
                 WithinAbs(3000.0, 50.0));
    REQUIRE_THAT(static_cast<double>(getVal(proc2, "VOLUME")),
                 WithinAbs(0.6, 0.02));
    REQUIRE_THAT(static_cast<double>(getVal(proc2, "FM_ALGO")),
                 WithinAbs(2.0, 0.1));
}

TEST_CASE("StateRoundTrip - Custom data survives save/load", "[state]")
{
    ParasiteProcessor proc1;
    proc1.prepareToPlay(44100.0, 512);

    // Set custom shaper table
    auto& shaper = proc1.getVolumeShaper();
    shaper.resetTable();
    shaper.setStep(5, 0.42f);

    // Set custom LFO curve
    auto& lfo = proc1.getGlobalLFO(0);
    std::vector<bb::CurvePoint> pts = { {0.0f, 0.1f}, {0.5f, 0.9f}, {1.0f, 0.3f} };
    lfo.setCurvePoints(pts);

    // Set custom harmonics
    auto& ht = proc1.getHarmonicTable(0);
    ht.setHarmonic(2, 0.77f);
    ht.rebake();

    // Save
    juce::MemoryBlock stateData;
    proc1.getStateInformation(stateData);

    // Load into new processor
    ParasiteProcessor proc2;
    proc2.prepareToPlay(44100.0, 512);
    proc2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    // Check shaper table
    REQUIRE_THAT(static_cast<double>(proc2.getVolumeShaper().getStep(5)),
                 WithinAbs(0.42, 0.01));

    // Check harmonics
    REQUIRE_THAT(static_cast<double>(proc2.getHarmonicTable(0).getHarmonic(2)),
                 WithinAbs(0.77, 0.01));
}
