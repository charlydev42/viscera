// test_FMVoice.cpp — Tests for bb::FMVoice (isolated with local atomics)
#include <catch2/catch_test_macros.hpp>
#include "dsp/FMVoice.h"
#include "dsp/FMSound.h"
#include "TestHelpers.h"

using namespace bb;

static constexpr double kSR = 44100.0;
static constexpr int kBlock = 4096;

// Helper: set up VoiceParams with local atomics (no APVTS needed)
struct TestVoiceParams
{
    // Owned atomic storage
    std::atomic<float> mod1On{1.0f}, mod1Wave{0.0f}, mod1KB{1.0f}, mod1Level{0.5f};
    std::atomic<float> mod1Coarse{1.0f}, mod1Fine{0.0f}, mod1FixedFreq{440.0f}, mod1Multi{4.0f};
    std::atomic<float> env1A{0.01f}, env1D{0.3f}, env1S{0.7f}, env1R{0.3f};

    std::atomic<float> mod2On{1.0f}, mod2Wave{0.0f}, mod2KB{1.0f}, mod2Level{0.5f};
    std::atomic<float> mod2Coarse{1.0f}, mod2Fine{0.0f}, mod2FixedFreq{440.0f}, mod2Multi{4.0f};
    std::atomic<float> env2A{0.01f}, env2D{0.3f}, env2S{0.7f}, env2R{0.3f};

    std::atomic<float> carWave{0.0f}, carCoarse{1.0f}, carFine{0.0f};
    std::atomic<float> carFixedFreq{440.0f}, carMulti{4.0f}, carKB{1.0f};
    std::atomic<float> carNoise{0.0f}, carSpread{0.0f};
    std::atomic<float> env3A{0.01f}, env3D{0.3f}, env3S{1.0f}, env3R{0.3f};

    std::atomic<float> tremor{0.0f}, vein{0.0f}, flux{0.0f};
    std::atomic<float> xorOn{0.0f}, syncOn{0.0f}, fmAlgo{0.0f};

    std::atomic<float> pitchEnvOn{0.0f}, pitchEnvAmt{0.0f};
    std::atomic<float> pitchEnvA{0.001f}, pitchEnvD{0.15f}, pitchEnvS{0.0f}, pitchEnvR{0.1f};

    std::atomic<float> filtOn{0.0f}, filtCutoff{20000.0f}, filtRes{0.0f}, filtType{0.0f};

    std::atomic<float> volume{0.8f}, drive{0.0f}, mono{0.0f}, retrig{0.0f};
    std::atomic<float> porta{0.0f}, dispAmt{0.0f}, carDrift{0.0f};
    std::atomic<float> cortex{0.5f}, ichor{0.0f}, plasma{0.5f}, macroTime{0.5f};

    HarmonicTable mod1Harmonics, mod2Harmonics, carHarmonics;
    VoiceParams params;

    TestVoiceParams()
    {
        params.mod1On = &mod1On; params.mod1Wave = &mod1Wave; params.mod1KB = &mod1KB;
        params.mod1Level = &mod1Level; params.mod1Coarse = &mod1Coarse; params.mod1Fine = &mod1Fine;
        params.mod1FixedFreq = &mod1FixedFreq; params.mod1Multi = &mod1Multi;
        params.env1A = &env1A; params.env1D = &env1D; params.env1S = &env1S; params.env1R = &env1R;

        params.mod2On = &mod2On; params.mod2Wave = &mod2Wave; params.mod2KB = &mod2KB;
        params.mod2Level = &mod2Level; params.mod2Coarse = &mod2Coarse; params.mod2Fine = &mod2Fine;
        params.mod2FixedFreq = &mod2FixedFreq; params.mod2Multi = &mod2Multi;
        params.env2A = &env2A; params.env2D = &env2D; params.env2S = &env2S; params.env2R = &env2R;

        params.carWave = &carWave; params.carCoarse = &carCoarse; params.carFine = &carFine;
        params.carFixedFreq = &carFixedFreq; params.carMulti = &carMulti; params.carKB = &carKB;
        params.carNoise = &carNoise; params.carSpread = &carSpread;
        params.env3A = &env3A; params.env3D = &env3D; params.env3S = &env3S; params.env3R = &env3R;

        params.tremor = &tremor; params.vein = &vein; params.flux = &flux;
        params.xorOn = &xorOn; params.syncOn = &syncOn; params.fmAlgo = &fmAlgo;

        params.pitchEnvOn = &pitchEnvOn; params.pitchEnvAmt = &pitchEnvAmt;
        params.pitchEnvA = &pitchEnvA; params.pitchEnvD = &pitchEnvD;
        params.pitchEnvS = &pitchEnvS; params.pitchEnvR = &pitchEnvR;

        params.filtOn = &filtOn; params.filtCutoff = &filtCutoff;
        params.filtRes = &filtRes; params.filtType = &filtType;

        params.volume = &volume; params.drive = &drive; params.mono = &mono;
        params.retrig = &retrig; params.porta = &porta; params.dispAmt = &dispAmt;
        params.carDrift = &carDrift; params.cortex = &cortex; params.ichor = &ichor;
        params.plasma = &plasma; params.macroTime = &macroTime;

        params.mod1Harmonics = &mod1Harmonics;
        params.mod2Harmonics = &mod2Harmonics;
        params.carHarmonics = &carHarmonics;
    }
};

// Render a note through FMVoice and return the buffer
static juce::AudioBuffer<float> renderNote(VoiceParams& params, int note = 60,
                                           float velocity = 0.8f, int numSamples = kBlock)
{
    FMVoice voice(params);
    voice.prepareToPlay(kSR, 512);

    juce::AudioBuffer<float> buffer(2, numSamples);
    buffer.clear();

    FMSound sound;
    voice.startNote(note, velocity, &sound, 8192);
    voice.renderNextBlock(buffer, 0, numSamples);

    return buffer;
}

TEST_CASE("FMVoice - Basic note rendering produces audio", "[voice]")
{
    TestVoiceParams tvp;
    auto buf = renderNote(tvp.params);

    REQUIRE_FALSE(test::hasNaN(buf));
    REQUIRE_FALSE(test::isSilent(buf));
    REQUIRE(test::peakAmplitude(buf) < 3.0f); // reasonable headroom
}

TEST_CASE("FMVoice - All 5 FM algorithms", "[voice]")
{
    for (int algo = 0; algo < 5; ++algo)
    {
        TestVoiceParams tvp;
        tvp.fmAlgo.store(static_cast<float>(algo));
        tvp.mod1Level.store(0.7f);
        tvp.mod2Level.store(0.5f);

        auto buf = renderNote(tvp.params);

        REQUIRE_FALSE(test::hasNaN(buf));
        REQUIRE_FALSE(test::isSilent(buf));
    }
}

TEST_CASE("FMVoice - XOR + Filter + Fold paths", "[voice]")
{
    TestVoiceParams tvp;
    tvp.xorOn.store(1.0f);
    tvp.filtOn.store(1.0f);
    tvp.filtCutoff.store(2000.0f);
    tvp.filtRes.store(0.5f);
    tvp.drive.store(0.5f); // HemoFold drive

    auto buf = renderNote(tvp.params);

    REQUIRE_FALSE(test::hasNaN(buf));
    REQUIRE_FALSE(test::isSilent(buf));
}

TEST_CASE("FMVoice - Pitch wheel", "[voice]")
{
    TestVoiceParams tvp;
    FMVoice voice(tvp.params);
    voice.prepareToPlay(kSR, 512);

    juce::AudioBuffer<float> buf1(2, kBlock), buf2(2, kBlock);
    buf1.clear();
    buf2.clear();

    FMSound sound;

    // Note at center pitch wheel
    voice.startNote(60, 0.8f, &sound, 8192);
    voice.renderNextBlock(buf1, 0, kBlock);

    // Same note with pitch wheel up
    voice.stopNote(0.0f, false);
    voice.startNote(60, 0.8f, &sound, 16383); // pitch wheel max
    voice.renderNextBlock(buf2, 0, kBlock);

    // Both should produce audio
    REQUIRE_FALSE(test::hasNaN(buf1));
    REQUIRE_FALSE(test::hasNaN(buf2));
    REQUIRE_FALSE(test::isSilent(buf1));
    REQUIRE_FALSE(test::isSilent(buf2));
}

TEST_CASE("FMVoice - Voice stealing fade-out", "[voice]")
{
    TestVoiceParams tvp;
    FMVoice voice(tvp.params);
    voice.prepareToPlay(kSR, 512);

    FMSound sound;
    juce::AudioBuffer<float> buf(2, kBlock);
    buf.clear();

    voice.startNote(60, 0.8f, &sound, 8192);
    voice.renderNextBlock(buf, 0, kBlock / 2);

    // Force voice steal (stopNote with allowTailOff=false)
    voice.stopNote(0.0f, false);
    voice.renderNextBlock(buf, kBlock / 2, kBlock / 2);

    REQUIRE_FALSE(test::hasNaN(buf));
    // End of buffer should be near-silent (fade-out)
    float endRms = test::rms(buf.getReadPointer(0) + kBlock - 100, 100);
    REQUIRE(endRms < 0.1f);
}
