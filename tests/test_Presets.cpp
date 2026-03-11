// test_Presets.cpp — Tests for factory preset loading + rendering
#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"
#include <BinaryData.h>

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 2048;

TEST_CASE("Presets - All factory presets load and render NaN-free", "[preset]")
{
    ParasiteProcessor proc;
    proc.prepareToPlay(kSR, kBlock);

    const auto& registry = proc.getPresetRegistry();
    REQUIRE(registry.size() > 50); // expect 90+ factory presets

    for (int i = 0; i < static_cast<int>(registry.size()); ++i)
    {
        if (!registry[static_cast<size_t>(i)].isFactory) continue;

        INFO("Loading preset " << i << ": " << registry[static_cast<size_t>(i)].name);

        proc.loadPresetAt(i);
        proc.prepareToPlay(kSR, kBlock);

        // Render with a note
        juce::AudioBuffer<float> buffer(2, kBlock);
        buffer.clear();
        auto midi = test::createNoteOnBuffer(60, 0.8f, 0);

        proc.processBlock(buffer, midi);

        REQUIRE_FALSE(test::hasNaN(buffer));
        REQUIRE(test::peakAmplitude(buffer) < 10.0f); // no blowup
    }
}

TEST_CASE("Presets - Factory presets are valid XML", "[preset]")
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        juce::String resName = BinaryData::namedResourceList[i];
        juce::String origName = BinaryData::originalFilenames[i];
        if (!origName.endsWith(".prst")) continue;

        int size = 0;
        auto* data = BinaryData::getNamedResource(resName.toRawUTF8(), size);
        REQUIRE(data != nullptr);
        REQUIRE(size > 0);

        auto xmlStr = juce::String::fromUTF8(data, size);
        auto xml = juce::parseXML(xmlStr);

        INFO("Validating XML for: " << origName);
        REQUIRE(xml != nullptr);
    }
}

TEST_CASE("Presets - Registry contains expected count", "[preset]")
{
    ParasiteProcessor proc;
    const auto& registry = proc.getPresetRegistry();

    // Count factory presets
    int factoryCount = 0;
    for (const auto& p : registry)
        if (p.isFactory) ++factoryCount;

    // We have 90+ presets in BinaryData
    REQUIRE(factoryCount > 80);
}
