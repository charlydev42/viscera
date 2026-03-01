// PluginProcessor.h — AudioProcessor principal de Viscera
// Gère le Synthesiser, l'APVTS (AudioProcessorValueTreeState) et les presets
// APVTS = système de JUCE qui synchronise paramètres ↔ GUI ↔ automation ↔ state
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/FMVoice.h"
#include "dsp/LFO.h"
#include "dsp/StereoDelay.h"
#include "dsp/PlateReverb.h"
#include "dsp/LiquidChorus.h"
#include "dsp/RubberComb.h"
#include "dsp/VolumeShaper.h"
#include "dsp/AudioVisualBuffer.h"

class VisceraProcessor : public juce::AudioProcessor
{
public:
    VisceraProcessor();
    ~VisceraProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS publique pour que l'éditeur puisse s'y connecter
    juce::AudioProcessorValueTreeState apvts;

    // --- Preset system ---
    struct PresetEntry {
        juce::String name;
        juce::String category;
        bool isFactory = true;
        juce::String resourceName;   // BinaryData resource name (factory)
        juce::String userFileName;   // filename without extension (user)
    };

    const std::vector<PresetEntry>& getPresetRegistry() const { return presetRegistry; }
    void buildPresetRegistry();
    void loadPresetAt(int index);
    int getCurrentPresetIndex() const { return currentPreset; }
    int getPresetCount() const { return static_cast<int>(presetRegistry.size()); }

    // User presets
    static juce::File getUserPresetsDir();
    void saveUserPreset(const juce::String& name, const juce::String& category = "User");
    void loadUserPreset(const juce::String& name);
    bool isUserPreset() const { return isUserPresetLoaded; }
    const juce::String& getUserPresetName() const { return currentUserPresetName; }

    // Global LFO phase getter (for GUI display)
    float getGlobalLFOPhase(int index) const
    {
        if (index >= 0 && index < 3)
            return globalLFO[index].getPhase();
        return 0.0f;
    }

private:
    // Création des paramètres (appelée dans le constructeur)
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Pointeurs atomiques cachés vers les paramètres (pour accès rapide dans processBlock)
    bb::VoiceParams voiceParams;
    void cacheParameterPointers();

    juce::Synthesiser synth;
    int currentPreset = 0;
    bool isUserPresetLoaded = false;
    juce::String currentUserPresetName;
    std::vector<PresetEntry> presetRegistry;

    // Shared logic for loading preset XML
    void loadPresetFromXml(const juce::String& xmlStr);
    void loadFactoryPreset(const juce::String& resourceName);

    // 3 global assignable LFOs
    bb::LFO globalLFO[3];

    static constexpr int kSlotsPerLFO = 8;

    // Cached APVTS pointers for global LFOs (rate+wave+8×dest+8×amt per LFO)
    struct LFOParamCache {
        std::atomic<float>* rate  = nullptr;
        std::atomic<float>* wave  = nullptr;
        std::atomic<float>* sync  = nullptr;
        std::atomic<float>* dest[kSlotsPerLFO] = {};
        std::atomic<float>* amt[kSlotsPerLFO]  = {};
    } lfoCache[3];

    // Post-synth FX on/off
    std::atomic<float>* dlyOnParam    = nullptr;
    std::atomic<float>* revOnParam    = nullptr;

    bb::StereoDelay stereoDelay;
    bb::PlateReverb plateReverb;
    bool revWasOn = false;
    bool dlyWasOn = false;
    std::atomic<float>* dlyTimeParam  = nullptr;
    std::atomic<float>* dlyFeedParam  = nullptr;
    std::atomic<float>* dlyDampParam  = nullptr;
    std::atomic<float>* dlyMixParam    = nullptr;
    std::atomic<float>* dlyPingParam   = nullptr;
    std::atomic<float>* dlySpreadParam = nullptr;
    std::atomic<float>* revSizeParam  = nullptr;
    std::atomic<float>* revDampParam  = nullptr;
    std::atomic<float>* revMixParam   = nullptr;
    std::atomic<float>* revWidthParam  = nullptr;
    std::atomic<float>* revPdlyParam   = nullptr;

    // Liquid Chorus
    bb::LiquidChorus liquidChorus;
    std::atomic<float>* liqOnParam    = nullptr;
    std::atomic<float>* liqRateParam  = nullptr;
    std::atomic<float>* liqDepthParam = nullptr;
    std::atomic<float>* liqToneParam  = nullptr;
    std::atomic<float>* liqFeedParam  = nullptr;
    std::atomic<float>* liqMixParam   = nullptr;

    // Rubber Comb
    bb::RubberComb rubberComb;
    std::atomic<float>* rubOnParam      = nullptr;
    std::atomic<float>* rubToneParam    = nullptr;
    std::atomic<float>* rubStretchParam = nullptr;
    std::atomic<float>* rubWarpParam    = nullptr;
    std::atomic<float>* rubMixParam     = nullptr;
    std::atomic<float>* rubFeedParam    = nullptr;

    // Volume Shaper
    bb::VolumeShaper volumeShaper;
    std::atomic<float>* shaperOnParam    = nullptr;
    std::atomic<float>* shaperSyncParam  = nullptr;
    std::atomic<float>* shaperRateParam  = nullptr;
    std::atomic<float>* shaperDepthParam = nullptr;

public:
    bb::VolumeShaper& getVolumeShaper() { return volumeShaper; }
    bb::LFO& getGlobalLFO(int index) { return globalLFO[juce::jlimit(0, 2, index)]; }
    const bb::VoiceParams& getVoiceParams() const { return voiceParams; }
    bb::AudioVisualBuffer& getVisualBuffer()  { return visualBuffer; }
    bb::AudioVisualBuffer& getVisualBufferR() { return visualBufferR; }

    // Le MidiKeyboardState est public pour que l'éditeur puisse y connecter son clavier
    juce::MidiKeyboardState keyboardState;
private:

    // Visual buffers for GUI oscilloscope/FFT (L + R)
    bb::AudioVisualBuffer visualBuffer;
    bb::AudioVisualBuffer visualBufferR;

    // Migration des anciens presets (MOD_PITCH → COARSE+FINE ou FIXED_FREQ)
    static void migrateOldPitchParams(juce::ValueTree& tree);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisceraProcessor)
};
