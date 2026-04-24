// PluginProcessor.h — AudioProcessor principal de Parasite
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
#include "license/LicenseManager.h"
#include "cloud/CloudPresetManager.h"

class ParasiteProcessor : public juce::AudioProcessor,
                         private bb::LicenseManager::Listener
{
public:
    ParasiteProcessor();
    ~ParasiteProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }

    // Reverb pre-delay can reach 200ms + the reverb tail itself runs several
    // seconds; delay feedback can sustain longer still. Reporting a realistic
    // tail prevents hosts (Ableton/Logic) from cutting offline renders
    // prematurely and stopping the tail mid-frame in loop playback.
    double getTailLengthSeconds() const override { return 12.0; }

    // Bypass: output silence but keep DSP state live so unbypass resumes
    // cleanly (reverb/delay buffers retain their content). We also skip
    // MIDI processing while bypassed so new notes aren't accidentally
    // triggered on resume.
    void processBlockBypassed(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midi) override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Undo/Redo manager (wired to APVTS — all param changes are undoable)
    juce::UndoManager undoManager;

    juce::UndoManager& getUndoManager() { return undoManager; }

    // APVTS publique pour que l'éditeur puisse s'y connecter
    juce::AudioProcessorValueTreeState apvts;

    // --- Preset system ---
    struct PresetEntry {
        juce::String name;
        juce::String category;
        juce::String pack;           // "Factory", "User", or pack name (e.g. "Dua Lipa")
        juce::String uuid;           // Cloud sync UUID (empty for factory presets)
        bool isFactory = true;
        juce::String resourceName;   // BinaryData resource name (factory)
        juce::String userFileName;   // filename without extension (user)
    };

    const std::vector<PresetEntry>& getPresetRegistry() const { return presetRegistry; }
    void buildPresetRegistry();
    juce::StringArray getAvailablePacks() const;
    void loadPresetAt(int index);
    int getCurrentPresetIndex() const { return currentPreset; }
    int getPresetCount() const { return static_cast<int>(presetRegistry.size()); }

    // User presets
    static juce::File getUserPresetsDir();
    void saveUserPreset(const juce::String& name, const juce::String& category = "User");
    void loadUserPreset(const juce::String& name);
    bool deleteUserPreset(const juce::String& name);
    bool isUserPreset() const { return isUserPresetLoaded; }
    const juce::String& getUserPresetName() const { return currentUserPresetName; }

    // Favorites
    bool isFavorite(const juce::String& presetName) const;
    void toggleFavorite(const juce::String& presetName);
    void saveFavorites();
    void loadFavorites();

    // Display name override (e.g. "Random", "Custom") — empty = use preset name
    void setDisplayName(const juce::String& name)
    {
        const juce::SpinLock::ScopedLockType lock(displayNameLock);
        displayName = name;
    }
    juce::String getDisplayName() const
    {
        const juce::SpinLock::ScopedLockType lock(displayNameLock);
        return displayName;
    }

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

    // Curve↔Param sync (harmonic tables, shaper steps, LFO tables)
    void setupCurveParamListeners();
    void syncInternalToCurveParams();           // Push internal state → params (e.g. after preset load)
    void applyCurveParamToInternal(const juce::String& paramId, float value01);

    // Forward declare the APVTS listener that owns curve-param callbacks
    struct CurveListener;
    std::unique_ptr<CurveListener> curveListener;

    juce::Synthesiser synth;
    int currentPreset = -1;  // -1 = uninitialised; set by loadPresetAt or setStateInformation
    bool isUserPresetLoaded = false;
    juce::String currentUserPresetName;
    juce::String displayName; // override for preset display (e.g. "Random")
    mutable juce::SpinLock displayNameLock;
    std::vector<PresetEntry> presetRegistry;
    juce::StringArray favorites;

    // Shared logic for loading preset XML
    void loadPresetFromXml(const juce::String& xmlStr);
    void loadFactoryPreset(const juce::String& resourceName);

    // DRY helpers for state serialization
    void serializeCustomData(juce::ValueTree& state) const;
    void deserializeCustomData(const juce::ValueTree& tree);

    // 3 global assignable LFOs
    bb::LFO globalLFO[3];

    static constexpr int kSlotsPerLFO = 8;

    // Cached APVTS pointers for global LFOs (rate+wave+8×dest+8×amt per LFO)
    struct LFOParamCache {
        std::atomic<float>* rate    = nullptr;
        std::atomic<float>* wave    = nullptr;
        std::atomic<float>* sync    = nullptr;
        std::atomic<float>* retrig  = nullptr;
        std::atomic<float>* vel     = nullptr;
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
    std::atomic<float>* dlySyncParam  = nullptr;
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

    // Harmonic tables for Custom waveform (owned by processor, shared with voices + GUI)
    bb::HarmonicTable mod1Harmonics, mod2Harmonics, carHarmonics;

public:
    bb::LicenseManager& getLicenseManager() { return licenseManager; }
    CloudPresetManager& getCloudPresetManager() { return cloudPresetManager; }

    bb::HarmonicTable& getHarmonicTable(int idx)
    {
        if (idx == 0) return mod1Harmonics;
        if (idx == 1) return mod2Harmonics;
        return carHarmonics;
    }
    bb::VolumeShaper& getVolumeShaper() { return volumeShaper; }
    bb::LFO& getGlobalLFO(int index) { return globalLFO[juce::jlimit(0, 2, index)]; }
    const bb::VoiceParams& getVoiceParams() const { return voiceParams; }
    bb::AudioVisualBuffer& getVisualBuffer()  { return visualBuffer; }
    bb::AudioVisualBuffer& getVisualBufferR() { return visualBufferR; }


    // Inject a single MIDI note for preset preview (works in all formats)
    void sendPreviewNoteOn()  { previewNoteOn.store(true, std::memory_order_relaxed); }
    void sendPreviewNoteOff() { previewNoteOff.store(true, std::memory_order_relaxed); }
    std::atomic<bool> previewNoteOn  { false };
    std::atomic<bool> previewNoteOff { false };

    // "Panic" — silence every active voice immediately and clear the
    // effects chain's delay/reverb buffers. Called before any preset change
    // (setStateInformation, loadUserPreset, loadPresetFromXml, randomize)
    // so the incoming patch starts from silence instead of morphing a live
    // tail with whatever new params just landed.
    //
    // Scheduled via an atomic flag so we stay message-thread-friendly and
    // let the audio thread service it at the top of its next processBlock
    // (where it already holds the internal Synthesiser lock).
    void requestVoicePanic() { voicePanicPending.store(true, std::memory_order_release); }
    std::atomic<bool> voicePanicPending { false };

    // Bidirectional sync between APVTS H##/S## params and internal curve
    // state so every bar edit is undoable via Cmd+Z. GUI writes go through
    // AudioProcessorParameter::setValueNotifyingHost directly (see
    // HarmonicEditor::onSetHarmonic / ShaperDisplay::onSetStep); the
    // listener below propagates the change to the HarmonicTable /
    // VolumeShaper. DAW undo/redo reverts the param → listener reapplies.
    //
    // Raised during bulk operations (preset load, syncInternalToCurveParams,
    // apvts.replaceState) so the listener doesn't stomp internal state that
    // was just restored from serialized strings.
    std::atomic<bool> suppressCurveListener { false };

    // Incremented every time a preset / full state is applied. GUI widgets
    // poll this to discard transient UI state that only makes sense for the
    // previous preset (e.g. LFOSection's learn-click armed on a now-stale
    // slot). Monotonic, 32 bits is plenty — wraps every 4 billion loads.
    std::atomic<uint32_t> stateGeneration { 0 };

    // Last user-visible load error (empty when the most recent load was
    // clean). GUI polls in its timer and shows a toast when this is set.
    // SpinLock-guarded — set from the message thread only but still want
    // coherent reads from the editor's timerCallback.
    juce::String getAndClearLastLoadError()
    {
        const juce::SpinLock::ScopedLockType lock(lastLoadErrorLock);
        auto out = lastLoadError;
        lastLoadError.clear();
        return out;
    }
    void setLastLoadError(const juce::String& msg)
    {
        const juce::SpinLock::ScopedLockType lock(lastLoadErrorLock);
        lastLoadError = msg;
    }

private:

    // Visual buffers for GUI oscilloscope/FFT (L + R)
    bb::AudioVisualBuffer visualBuffer;
    bb::AudioVisualBuffer visualBufferR;

    // License + cloud sync
    bb::LicenseManager licenseManager;
    CloudPresetManager cloudPresetManager;

    // LicenseManager::Listener
    void licenseStateChanged(bool licensed) override;

    // Audio-thread gate (magic token, not a simple bool — harder to find/patch)
    std::atomic<uint32_t> dspGainToken { 0 };
    static constexpr uint32_t kDspActive = 0x8A3C5F21;

    // ── Stage shaping (periodic attenuation when unlicensed) ──
    // Sample-accurate cycle; values computed in processBlock from a rolling
    // sample counter. Intentionally distributed across multiple sites (voice,
    // reverb, delay, master) so a single patched multiply does not defeat it.
    int64_t stageSamplePos     = 0;
    int64_t stageCycleSamples  = 0;
    int64_t stageQuietSamples  = 0;
    int     stageFadeSamples   = 0;
    std::atomic<float> stageMaster { 1.0f };  // cached for master-out site

    float computeStageEnvelope(int numSamples);

    // Set by setStateInformation / loadUserPreset / loadPresetFromXml on any
    // load error. GUI pulls + clears in its timer to surface a toast. Always
    // accessed via getAndClearLastLoadError().
    juce::String lastLoadError;
    juce::SpinLock lastLoadErrorLock;

    // ── State schema versioning ──
    // Bump kCurrentSchemaVersion whenever params are renamed, reshuffled, or
    // a new default behaviour needs to be backfilled for old presets. Every
    // migration is additive: applyStateMigrations walks v1 → v2 → ... → current
    // so a preset saved on any prior plugin version loads correctly.
    static constexpr int kCurrentSchemaVersion = 3;
    static int getSchemaVersion(const juce::ValueTree& tree) noexcept;
    static void applyStateMigrations(juce::ValueTree& tree);

    // v1 → v2: add COARSE+FINE from legacy MOD_PITCH, inject LFO defaults.
    // Kept as named helpers so each migration step is easy to review.
    static void migrateOldPitchParams(juce::ValueTree& tree);
    static void injectMissingLFODefaults(juce::ValueTree& tree);

    // v2 → v3: harmonic macros renamed — CORTEX/ICHOR → VORTEX/HELIX.
    // Rewrites the `id` property of any PARAM child referencing the old IDs
    // so pre-rename presets and DAW sessions keep their values.
    static void renameMacroParamIds(juce::ValueTree& tree);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParasiteProcessor)
};
