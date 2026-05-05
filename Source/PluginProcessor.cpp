// PluginProcessor.cpp — Implémentation du processeur principal
// Contient : layout des paramètres, synthesiser, presets factory, state save/load
#include "PluginProcessor.h"
#ifndef PARASITE_HEADLESS_TESTS
#include "PluginEditor.h"
#endif
#include "dsp/FMSound.h"
#include "util/Logger.h"
#include "license/LicenseConfig.h"
#include "SnappedParameterBool.h"
#include <BinaryData.h>

// --- Constructeur ---
// APVTS listener that dispatches curve-param changes (harmonic, shaper, LFO
// table steps) to the matching internal class. Lives in the .cpp so we can
// keep the header lean.
struct ParasiteProcessor::CurveListener : public juce::AudioProcessorValueTreeState::Listener
{
    explicit CurveListener(ParasiteProcessor& p) : proc(p) {}

    void parameterChanged(const juce::String& id, float newValue) override
    {
        if (proc.suppressCurveListener.load(std::memory_order_relaxed)) return;
        proc.applyCurveParamToInternal(id, newValue);
    }

    ParasiteProcessor& proc;
};

ParasiteProcessor::ParasiteProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "ParasiteState", createParameterLayout()),
      cloudPresetManager(*this, licenseManager)
{
    // Install once per process — writes crash dumps beside parasite.log.
    bb::Logger::installCrashHandler();
    BB_LOG_INFO("ParasiteProcessor constructed");

    cacheParameterPointers();

    // Wire harmonic tables to voice params
    voiceParams.mod1Harmonics = &mod1Harmonics;
    voiceParams.mod2Harmonics = &mod2Harmonics;
    voiceParams.carHarmonics  = &carHarmonics;

    // 8 voices for polyphony — idle voices cost nothing (early return in renderNextBlock)
    synth.addSound(new bb::FMSound());
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new bb::FMVoice(voiceParams));
    synth.setNoteStealingEnabled(true);

    // Register curve listeners AFTER param layout is built and pointers cached.
    // Then push initial internal state into the params so defaults stay in sync.
    setupCurveParamListeners();
    syncInternalToCurveParams();
    // Matches the post-preset-load pattern: no undo entries from the sync.
    undoManager.clearUndoHistory();

    buildPresetRegistry();
    loadFavorites();

    // Sync audio-thread license flag
    dspGainToken.store(licenseManager.isLicensed() ? kDspActive : 0, std::memory_order_relaxed);
    licenseManager.addListener(this);

    // Initial cloud sync if licensed
    if (licenseManager.isLicensed())
        cloudPresetManager.syncAll();
}

ParasiteProcessor::~ParasiteProcessor()
{
    licenseManager.removeListener(this);
    // Explicit unregister so APVTS doesn't call into freed listener
    if (curveListener)
    {
        auto unreg = [this](const juce::String& id) {
            apvts.removeParameterListener(id, curveListener.get());
        };
        for (auto prefix : { juce::String("MOD1_H"), juce::String("MOD2_H"), juce::String("CAR_H") })
            for (int h = 0; h < 32; ++h)
                unreg(prefix + juce::String(h).paddedLeft('0', 2));
        for (int s = 0; s < 32; ++s)
            unreg("SHAPER_S" + juce::String(s).paddedLeft('0', 2));
    }
}

// Register 224 curve-param listeners
void ParasiteProcessor::setupCurveParamListeners()
{
    curveListener = std::make_unique<CurveListener>(*this);
    auto reg = [this](const juce::String& id) {
        apvts.addParameterListener(id, curveListener.get());
    };
    for (auto prefix : { juce::String("MOD1_H"), juce::String("MOD2_H"), juce::String("CAR_H") })
        for (int h = 0; h < 32; ++h)
            reg(prefix + juce::String(h).paddedLeft('0', 2));
    for (int s = 0; s < 32; ++s)
        reg("SHAPER_S" + juce::String(s).paddedLeft('0', 2));
}

// Push current internal state into APVTS params without triggering listeners
// (used after preset load or initFromWaveType).
void ParasiteProcessor::syncInternalToCurveParams()
{
    suppressCurveListener.store(true, std::memory_order_relaxed);
    auto setParam = [this](const juce::String& id, float value01) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, value01));
    };

    struct HarmRef { const char* prefix; bb::HarmonicTable& tbl; };
    HarmRef harms[] = {
        { "MOD1_H", mod1Harmonics },
        { "MOD2_H", mod2Harmonics },
        { "CAR_H",  carHarmonics  },
    };
    for (auto& h : harms)
        for (int i = 0; i < 32; ++i)
            setParam(juce::String(h.prefix) + juce::String(i).paddedLeft('0', 2),
                     h.tbl.getHarmonic(i));

    for (int s = 0; s < 32; ++s)
        setParam("SHAPER_S" + juce::String(s).paddedLeft('0', 2),
                 volumeShaper.getStep(s));

    suppressCurveListener.store(false, std::memory_order_relaxed);
}

// Called from listener — apply a single curve param change to internal state
void ParasiteProcessor::applyCurveParamToInternal(const juce::String& id, float value01)
{
    // MOD1_H## / MOD2_H## / CAR_H##
    auto applyHarm = [&](const juce::String& prefix, bb::HarmonicTable& tbl) -> bool
    {
        if (!id.startsWith(prefix)) return false;
        int idx = id.substring(prefix.length()).getIntValue();
        if (idx < 0 || idx >= 32) return true;
        // setHarmonic flags the table dirty; the editor's timer flushes the
        // rebake so a fast drag doesn't re-bake 60×/sec.
        tbl.setHarmonic(idx, value01);
        return true;
    };
    if (applyHarm("MOD1_H", mod1Harmonics)) return;
    if (applyHarm("MOD2_H", mod2Harmonics)) return;
    if (applyHarm("CAR_H",  carHarmonics))  return;

    if (id.startsWith("SHAPER_S"))
    {
        int idx = id.substring(8).getIntValue();
        if (idx >= 0 && idx < 32) volumeShaper.setStep(idx, value01);
        return;
    }

}

void ParasiteProcessor::licenseStateChanged(bool licensed)
{
    dspGainToken.store(licensed ? kDspActive : 0, std::memory_order_relaxed);
    // On any transition, reset the stage cycle so licensing changes never
    // drop the user mid-quiet-window. Audio thread reads this counter in
    // computeStageEnvelope — plain write from the message thread is an
    // intentional benign race (brief overlap is inaudible).
    stageSamplePos = 0;
    if (licensed)
        cloudPresetManager.syncAll();
}

// --- Cacher les pointeurs atomiques vers les paramètres ---
void ParasiteProcessor::cacheParameterPointers()
{
    voiceParams.mod1On        = apvts.getRawParameterValue("MOD1_ON");
    voiceParams.mod1Wave      = apvts.getRawParameterValue("MOD1_WAVE");
    voiceParams.mod1KB        = apvts.getRawParameterValue("MOD1_KB");
    voiceParams.mod1Level     = apvts.getRawParameterValue("MOD1_LEVEL");
    voiceParams.mod1Coarse    = apvts.getRawParameterValue("MOD1_COARSE");
    voiceParams.mod1Fine      = apvts.getRawParameterValue("MOD1_FINE");
    voiceParams.mod1FixedFreq = apvts.getRawParameterValue("MOD1_FIXED_FREQ");
    voiceParams.mod1Multi     = apvts.getRawParameterValue("MOD1_MULTI");
    voiceParams.env1A         = apvts.getRawParameterValue("ENV1_A");
    voiceParams.env1D         = apvts.getRawParameterValue("ENV1_D");
    voiceParams.env1S         = apvts.getRawParameterValue("ENV1_S");
    voiceParams.env1R         = apvts.getRawParameterValue("ENV1_R");

    voiceParams.mod2On        = apvts.getRawParameterValue("MOD2_ON");
    voiceParams.mod2Wave      = apvts.getRawParameterValue("MOD2_WAVE");
    voiceParams.mod2KB        = apvts.getRawParameterValue("MOD2_KB");
    voiceParams.mod2Level     = apvts.getRawParameterValue("MOD2_LEVEL");
    voiceParams.mod2Coarse    = apvts.getRawParameterValue("MOD2_COARSE");
    voiceParams.mod2Fine      = apvts.getRawParameterValue("MOD2_FINE");
    voiceParams.mod2FixedFreq = apvts.getRawParameterValue("MOD2_FIXED_FREQ");
    voiceParams.mod2Multi     = apvts.getRawParameterValue("MOD2_MULTI");
    voiceParams.env2A         = apvts.getRawParameterValue("ENV2_A");
    voiceParams.env2D         = apvts.getRawParameterValue("ENV2_D");
    voiceParams.env2S         = apvts.getRawParameterValue("ENV2_S");
    voiceParams.env2R         = apvts.getRawParameterValue("ENV2_R");

    voiceParams.carWave      = apvts.getRawParameterValue("CAR_WAVE");
    voiceParams.carCoarse    = apvts.getRawParameterValue("CAR_COARSE");
    voiceParams.carFine      = apvts.getRawParameterValue("CAR_FINE");
    voiceParams.carFixedFreq = apvts.getRawParameterValue("CAR_FIXED_FREQ");
    voiceParams.carMulti     = apvts.getRawParameterValue("CAR_MULTI");
    voiceParams.carKB        = apvts.getRawParameterValue("CAR_KB");
    voiceParams.carNoise     = apvts.getRawParameterValue("CAR_NOISE");
    voiceParams.carSpread    = apvts.getRawParameterValue("CAR_SPREAD");
    voiceParams.env3A        = apvts.getRawParameterValue("ENV3_A");
    voiceParams.env3D      = apvts.getRawParameterValue("ENV3_D");
    voiceParams.env3S      = apvts.getRawParameterValue("ENV3_S");
    voiceParams.env3R      = apvts.getRawParameterValue("ENV3_R");

    voiceParams.tremor     = apvts.getRawParameterValue("TREMOR");
    voiceParams.vein       = apvts.getRawParameterValue("VEIN");
    voiceParams.flux       = apvts.getRawParameterValue("FLUX");

    voiceParams.xorOn      = apvts.getRawParameterValue("XOR_ON");
    voiceParams.syncOn     = apvts.getRawParameterValue("SYNC");
    voiceParams.fmAlgo     = apvts.getRawParameterValue("FM_ALGO");

    voiceParams.pitchEnvOn  = apvts.getRawParameterValue("PENV_ON");
    voiceParams.pitchEnvAmt = apvts.getRawParameterValue("PENV_AMT");
    voiceParams.pitchEnvA   = apvts.getRawParameterValue("PENV_A");
    voiceParams.pitchEnvD   = apvts.getRawParameterValue("PENV_D");
    voiceParams.pitchEnvS   = apvts.getRawParameterValue("PENV_S");
    voiceParams.pitchEnvR   = apvts.getRawParameterValue("PENV_R");

    voiceParams.filtOn     = apvts.getRawParameterValue("FILT_ON");
    voiceParams.filtCutoff = apvts.getRawParameterValue("FILT_CUTOFF");
    voiceParams.filtRes    = apvts.getRawParameterValue("FILT_RES");
    voiceParams.filtType   = apvts.getRawParameterValue("FILT_TYPE");

    voiceParams.volume     = apvts.getRawParameterValue("VOLUME");
    voiceParams.drive      = apvts.getRawParameterValue("DRIVE");
    voiceParams.mono       = apvts.getRawParameterValue("MONO");
    voiceParams.retrig     = apvts.getRawParameterValue("RETRIG");
    voiceParams.porta      = apvts.getRawParameterValue("PORTA");
    voiceParams.dispAmt    = apvts.getRawParameterValue("DISP_AMT");
    voiceParams.carDrift   = apvts.getRawParameterValue("CAR_DRIFT");
    voiceParams.vortex     = apvts.getRawParameterValue("VORTEX");
    voiceParams.helix      = apvts.getRawParameterValue("HELIX");
    voiceParams.plasma     = apvts.getRawParameterValue("PLASMA");
    voiceParams.macroTime  = apvts.getRawParameterValue("MACRO_TIME");
    voiceParams.octave     = apvts.getRawParameterValue("OCTAVE");

    // FX on/off pointers
    dlyOnParam   = apvts.getRawParameterValue("DLY_ON");
    revOnParam   = apvts.getRawParameterValue("REV_ON");

    // FX param pointers
    dlyTimeParam = apvts.getRawParameterValue("DLY_TIME");
    dlySyncParam = apvts.getRawParameterValue("DLY_SYNC");
    dlyFeedParam = apvts.getRawParameterValue("DLY_FEED");
    dlyDampParam = apvts.getRawParameterValue("DLY_DAMP");
    dlyMixParam  = apvts.getRawParameterValue("DLY_MIX");
    dlyPingParam   = apvts.getRawParameterValue("DLY_PING");
    dlySpreadParam = apvts.getRawParameterValue("DLY_SPREAD");
    revSizeParam = apvts.getRawParameterValue("REV_SIZE");
    revDampParam = apvts.getRawParameterValue("REV_DAMP");
    revMixParam   = apvts.getRawParameterValue("REV_MIX");
    revWidthParam  = apvts.getRawParameterValue("REV_WIDTH");
    revPdlyParam   = apvts.getRawParameterValue("REV_PDLY");

    // Liquid param pointers
    liqOnParam    = apvts.getRawParameterValue("LIQ_ON");
    liqRateParam  = apvts.getRawParameterValue("LIQ_RATE");
    liqDepthParam = apvts.getRawParameterValue("LIQ_DEPTH");
    liqToneParam  = apvts.getRawParameterValue("LIQ_TONE");
    liqFeedParam  = apvts.getRawParameterValue("LIQ_FEED");
    liqMixParam   = apvts.getRawParameterValue("LIQ_MIX");

    // Rubber param pointers
    rubOnParam      = apvts.getRawParameterValue("RUB_ON");
    rubToneParam    = apvts.getRawParameterValue("RUB_TONE");
    rubStretchParam = apvts.getRawParameterValue("RUB_STRETCH");
    rubWarpParam    = apvts.getRawParameterValue("RUB_WARP");
    rubMixParam     = apvts.getRawParameterValue("RUB_MIX");
    rubFeedParam    = apvts.getRawParameterValue("RUB_FEED");

    // Shaper param pointers
    shaperOnParam    = apvts.getRawParameterValue("SHAPER_ON");
    shaperSyncParam  = apvts.getRawParameterValue("SHAPER_SYNC");
    shaperRateParam  = apvts.getRawParameterValue("SHAPER_RATE");
    shaperDepthParam = apvts.getRawParameterValue("SHAPER_DEPTH");

    // Global LFO param pointers
    for (int n = 0; n < 3; ++n)
    {
        auto id = [&](const juce::String& suffix) {
            return "LFO" + juce::String(n + 1) + "_" + suffix;
        };
        lfoCache[n].rate   = apvts.getRawParameterValue(id("RATE"));
        lfoCache[n].wave   = apvts.getRawParameterValue(id("WAVE"));
        lfoCache[n].sync   = apvts.getRawParameterValue(id("SYNC"));
        lfoCache[n].retrig = apvts.getRawParameterValue(id("RETRIG"));
        lfoCache[n].vel    = apvts.getRawParameterValue(id("VEL"));
        for (int s = 0; s < kSlotsPerLFO; ++s)
        {
            lfoCache[n].dest[s] = apvts.getRawParameterValue(id("DEST" + juce::String(s + 1)));
            lfoCache[n].amt[s]  = apvts.getRawParameterValue(id("AMT" + juce::String(s + 1)));
        }
    }
}

// --- Layout des paramètres ---
juce::AudioProcessorValueTreeState::ParameterLayout
ParasiteProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::AudioProcessorParameterGroup>> groups;

    juce::StringArray waveNames { "Sine", "Saw", "Square", "Triangle", "Pulse", "Custom", "Noise" };

    // --- Groupe Modulateur 1 ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("mod1", "Modulator 1", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("MOD1_ON", "Mod1 On", true));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("MOD1_WAVE", "Mod1 Wave", waveNames, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_PITCH", "Mod1 Pitch",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f)); // legacy
        g->addChild(std::make_unique<SnappedParameterBool>("MOD1_KB", "Mod1 KB", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_LEVEL", "Mod1 Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD1_COARSE", "Mod1 Coarse", 0, 48, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_FINE", "Mod1 Fine",
            juce::NormalisableRange<float>(-1000.0f, 1000.0f, 0.1f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_FIXED_FREQ", "Mod1 Fixed Freq",
            juce::NormalisableRange<float>(20.0f, 16000.0f, 0.0f, 0.3f), 440.0f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD1_MULTI", "Mod1 Multi", 0, 5, 4));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_A", "Env1 Attack",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.01f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_D", "Env1 Decay",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_S", "Env1 Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_R", "Env1 Release",
            juce::NormalisableRange<float>(0.0f, 8.0f, 0.0f, 0.3f), 0.3f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Modulateur 2 ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("mod2", "Modulator 2", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("MOD2_ON", "Mod2 On", true));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("MOD2_WAVE", "Mod2 Wave", waveNames, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_PITCH", "Mod2 Pitch",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f)); // legacy
        g->addChild(std::make_unique<SnappedParameterBool>("MOD2_KB", "Mod2 KB", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_LEVEL", "Mod2 Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD2_COARSE", "Mod2 Coarse", 0, 48, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_FINE", "Mod2 Fine",
            juce::NormalisableRange<float>(-1000.0f, 1000.0f, 0.1f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_FIXED_FREQ", "Mod2 Fixed Freq",
            juce::NormalisableRange<float>(20.0f, 16000.0f, 0.0f, 0.3f), 440.0f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD2_MULTI", "Mod2 Multi", 0, 5, 4));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_A", "Env2 Attack",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.01f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_D", "Env2 Decay",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_S", "Env2 Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_R", "Env2 Release",
            juce::NormalisableRange<float>(0.0f, 8.0f, 0.0f, 0.3f), 0.3f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Carrier ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("carrier", "Carrier", "|");
        g->addChild(std::make_unique<juce::AudioParameterChoice>("CAR_WAVE", "Carrier Wave", waveNames, 0));
        g->addChild(std::make_unique<juce::AudioParameterInt>("CAR_OCTAVE", "Carrier Octave", -2, 2, 0)); // legacy
        g->addChild(std::make_unique<juce::AudioParameterInt>("CAR_COARSE", "Carrier Coarse", 0, 48, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("CAR_FINE", "Carrier Fine",
            juce::NormalisableRange<float>(-1000.0f, 1000.0f, 0.1f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("CAR_FIXED_FREQ", "Carrier Fixed Freq",
            juce::NormalisableRange<float>(20.0f, 16000.0f, 0.0f, 0.3f), 440.0f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("CAR_MULTI", "Carrier Multi", 0, 5, 4));
        g->addChild(std::make_unique<SnappedParameterBool>("CAR_KB", "Carrier KB", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_A", "Env3 Attack",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.01f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_D", "Env3 Decay",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_S", "Env3 Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_R", "Env3 Release",
            juce::NormalisableRange<float>(0.0f, 8.0f, 0.0f, 0.3f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("CAR_DRIFT", "Carrier Drift",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("CAR_NOISE", "Carrier Noise",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("CAR_SPREAD", "Carrier Spread",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        groups.push_back(std::move(g));
    }

    // --- Groupe LFO / Modulation ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("lfo", "LFO Routing", "|");
        g->addChild(std::make_unique<juce::AudioParameterFloat>("TREMOR", "Tremor (Pitch LFO)",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("VEIN", "Vein (Filter LFO)",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("FLUX", "Flux (Index LFO)",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<SnappedParameterBool>("XOR_ON", "XOR", false));
        g->addChild(std::make_unique<SnappedParameterBool>("SYNC", "Sync", false));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("FM_ALGO", "FM Algorithm",
            juce::StringArray{ "Series", "Parallel", "Stack", "Ring", "Feedback", "Mix" }, 0));
        groups.push_back(std::move(g));
    }

    // --- Groupe Pitch Envelope ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("pitchenv", "Pitch Envelope", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("PENV_ON", "Pitch Env On", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_AMT", "Pitch Env Amount",
            juce::NormalisableRange<float>(-96.0f, 96.0f, 0.1f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_A", "Pitch Env Attack",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.001f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_D", "Pitch Env Decay",
            juce::NormalisableRange<float>(0.0f, 5.0f, 0.0f, 0.3f), 0.15f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_S", "Pitch Env Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_R", "Pitch Env Release",
            juce::NormalisableRange<float>(0.0f, 8.0f, 0.0f, 0.3f), 0.1f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Filtre ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("filter", "Filter", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("FILT_ON", "Filter On", true));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("FILT_TYPE", "Filter Type",
            juce::StringArray{ "LP", "HP", "BP", "Notch" }, 0));
        {
            juce::NormalisableRange<float> cutoffRange(20.0f, 20000.0f);
            cutoffRange.setSkewForCentre(1000.0f); // 1 kHz at knob center — standard Serum/Vital style
            g->addChild(std::make_unique<juce::AudioParameterFloat>(
                "FILT_CUTOFF", "Filter Cutoff", cutoffRange, 20000.0f));
        }
        g->addChild(std::make_unique<juce::AudioParameterFloat>("FILT_RES", "Filter Resonance",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        groups.push_back(std::move(g));
    }

    // --- Groupe FX (Delay + Reverb) ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("fx", "FX", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("DLY_ON", "Delay On", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_TIME", "Delay Time",
            juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.4f), 0.3f));
        // Sync index: 0 = free (use DLY_TIME), 1..9 = tempo-sync beat divisions
        // (1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T, 1/16T). When >0 the
        // delay time is derived from host BPM instead of the DLY_TIME knob.
        g->addChild(std::make_unique<juce::AudioParameterChoice>("DLY_SYNC", "Delay Sync",
            juce::StringArray{ "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                               "1/4T", "1/8T", "1/16T" }, 0));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_FEED", "Delay Feedback",
            juce::NormalisableRange<float>(0.0f, 0.9f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_DAMP", "Delay Damp",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_MIX", "Delay Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<SnappedParameterBool>("DLY_PING", "Delay Ping-Pong", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_SPREAD", "Delay Spread",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<SnappedParameterBool>("REV_ON", "Reverb On", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("REV_SIZE", "Reverb Size",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("REV_DAMP", "Reverb Damp",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("REV_MIX", "Reverb Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("REV_WIDTH", "Reverb Width",
            juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("REV_PDLY", "Reverb Pre-Delay",
            juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 0.0f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Liquid Chorus ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("liquid", "Liquid", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("LIQ_ON", "Liquid On", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("LIQ_RATE", "Liquid Rate",
            juce::NormalisableRange<float>(0.05f, 3.0f, 0.0f, 0.5f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("LIQ_DEPTH", "Liquid Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("LIQ_TONE", "Liquid Tone",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("LIQ_FEED", "Liquid Feed",
            juce::NormalisableRange<float>(0.0f, 0.8f), 0.2f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("LIQ_MIX", "Liquid Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.6f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Rubber Comb ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("rubber", "Rubber", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("RUB_ON", "Rubber On", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("RUB_TONE", "Rubber Tone",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("RUB_STRETCH", "Rubber Stretch",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("RUB_WARP", "Rubber Warp",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("RUB_MIX", "Rubber Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.6f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("RUB_FEED", "Rubber Feed",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Global LFOs (3 assignable LFOs) ---
    {
        juce::StringArray destNames { "None", "Pitch", "Cutoff", "Res",
                                      "Mod1Lvl", "Mod2Lvl", "Volume",
                                      "Drive", "Noise", "Spread", "Fold",
                                      "M1Fine", "M2Fine", "Drift", "CarFine",
                                      "DlyTime", "DlyFeed", "DlyMix",
                                      "RevSize", "RevMix",
                                      "LiqDpth", "LiqMix",
                                      "RubWarp", "RubMix",
                                      "PEnvAmt",
                                      "RevDamp", "RevWdth", "RevPdly",
                                      "DlyDamp", "DlySprd",
                                      "LiqRate", "LiqTone", "LiqFeed",
                                      "RubTone", "RubStr", "RubFeed",
                                      "Porta",
                                      "E1A", "E1D", "E1S", "E1R",
                                      "E2A", "E2D", "E2S", "E2R",
                                      "E3A", "E3D", "E3S", "E3R",
                                      "PEA", "PED", "PES", "PER",
                                      "ShpRate", "ShpDep",
                                      "M1Coar", "M2Coar", "CCoar",
                                      "Tremor", "Vein", "Flux",
                                      "Vortex", "Helix", "Plasma",
                                      "MacTime" };

        for (int n = 1; n <= 3; ++n)
        {
            auto id = [&](const juce::String& suffix) { return "LFO" + juce::String(n) + "_" + suffix; };
            auto nm = [&](const juce::String& suffix) { return "LFO" + juce::String(n) + " " + suffix; };
            auto g = std::make_unique<juce::AudioProcessorParameterGroup>(
                "glfo" + juce::String(n), "Global LFO " + juce::String(n), "|");

            g->addChild(std::make_unique<juce::AudioParameterFloat>(id("RATE"), nm("Rate"),
                juce::NormalisableRange<float>(0.05f, 20.0f, 0.0f, 0.3f), 1.0f));
            g->addChild(std::make_unique<juce::AudioParameterChoice>(id("WAVE"), nm("Wave"),
                juce::StringArray{ "Sine", "Tri", "Saw", "Sq", "S&H", "Custom" }, 0));
            g->addChild(std::make_unique<juce::AudioParameterChoice>(id("SYNC"), nm("Sync"),
                juce::StringArray{ "Free", "8 bar", "4 bar", "2 bar", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32",
                                   "1/4T", "1/8T", "1/16T" }, 0));
            g->addChild(std::make_unique<SnappedParameterBool>(id("RETRIG"), nm("Retrigger"), false));
            g->addChild(std::make_unique<SnappedParameterBool>(id("VEL"), nm("Vel Rate"), false));

            for (int s = 1; s <= kSlotsPerLFO; ++s)
            {
                g->addChild(std::make_unique<juce::AudioParameterChoice>(
                    id("DEST" + juce::String(s)), nm("Dest" + juce::String(s)), destNames, 0));
                g->addChild(std::make_unique<juce::AudioParameterFloat>(
                    id("AMT" + juce::String(s)), nm("Amt" + juce::String(s)),
                    juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
            }

            groups.push_back(std::move(g));
        }
    }

    // --- Groupe Volume Shaper ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("shaper", "Volume Shaper", "|");
        g->addChild(std::make_unique<SnappedParameterBool>("SHAPER_ON", "Shaper On", false));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("SHAPER_SYNC", "Shaper Sync",
            juce::StringArray{ "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                               "1/4T", "1/8T", "1/16T" }, 0));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("SHAPER_RATE", "Shaper Rate",
            juce::NormalisableRange<float>(0.1f, 20.0f, 0.0f, 0.4f), 4.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("SHAPER_DEPTH", "Shaper Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Global ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("global", "Global", "|");
        g->addChild(std::make_unique<juce::AudioParameterFloat>("VOLUME", "Volume",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive",
            juce::NormalisableRange<float>(1.0f, 10.0f, 0.01f, 0.5f), 1.0f));
        g->addChild(std::make_unique<SnappedParameterBool>("MONO", "Mono", true));
        g->addChild(std::make_unique<SnappedParameterBool>("RETRIG", "Retrigger", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PORTA", "Portamento",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DISP_AMT", "HemoFold",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("VORTEX", "Vortex",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("HELIX", "Helix",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PLASMA", "Plasma",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MACRO_TIME", "Time",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("OCTAVE", "Octave", -4, 4, 0));
        groups.push_back(std::move(g));
    }

    // --- Groupe "curves" : 224 params cachés pour undo Cmd+Z ---
    // Marked non-automatable to keep Ableton's automation list clean.
    // These sync bidirectionally with HarmonicTable / VolumeShaper / LFO::customTable.
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("curves", "Curves (internal)", "|");

        auto addStepFloat = [&g](const juce::String& id, const juce::String& name,
                                   float defaultVal)
        {
            g->addChild(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(id, 1),
                name,
                juce::NormalisableRange<float>(0.0f, 1.0f),
                defaultVal,
                juce::AudioParameterFloatAttributes().withAutomatable(false)));
        };

        // Harmonic tables: MOD1_H00..31, MOD2_H00..31, CAR_H00..31 (sine default = H0=1)
        for (auto prefix : { juce::String("MOD1_H"),
                              juce::String("MOD2_H"),
                              juce::String("CAR_H") })
        {
            for (int h = 0; h < 32; ++h)
            {
                auto id   = prefix + juce::String(h).paddedLeft('0', 2);
                auto name = prefix + juce::String(h);
                addStepFloat(id, name, h == 0 ? 1.0f : 0.0f);
            }
        }

        // Shaper steps: SHAPER_S00..31 (default = 1.0 flat)
        for (int s = 0; s < 32; ++s)
        {
            auto id   = "SHAPER_S" + juce::String(s).paddedLeft('0', 2);
            addStepFloat(id, id, 1.0f);
        }

        // LFO custom curves are undoable via a dedicated UndoableAction
        // (see LFOCurveEdit in LFOSection.cpp) — not through hidden params —
        // so the exact variable-length control-point structure is preserved
        // on Cmd+Z, matching Serum's behavior.

        groups.push_back(std::move(g));
    }

    return { groups.begin(), groups.end() };
}

// --- Préparation audio ---
void ParasiteProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* fmVoice = dynamic_cast<bb::FMVoice*>(synth.getVoice(i)))
            fmVoice->prepareToPlay(sampleRate, samplesPerBlock);
    }

    // Prepare global LFOs
    for (int i = 0; i < 3; ++i)
        globalLFO[i].prepare(sampleRate);

    // Prepare post-synth FX
    stereoDelay.prepare(sampleRate, samplesPerBlock);
    plateReverb.prepare(sampleRate, samplesPerBlock);
    liquidChorus.prepare(sampleRate, samplesPerBlock);
    rubberComb.prepare(sampleRate, samplesPerBlock);
    volumeShaper.prepare(sampleRate);

    // Stage shaping timing (sample-accurate — independent of host transport)
    stageCycleSamples = static_cast<int64_t>(bb::license::kStageCycleSeconds * sampleRate);
    stageQuietSamples = static_cast<int64_t>(bb::license::kStageQuietSeconds * sampleRate);
    stageFadeSamples  = static_cast<int>(bb::license::kStageFadeMs * 0.001f * sampleRate);
    if (stageCycleSamples < 1)        stageCycleSamples = 1;
    if (stageQuietSamples < 0)        stageQuietSamples = 0;
    if (stageFadeSamples  < 1)        stageFadeSamples  = 1;
    // Guarantee quiet window fits inside cycle with fade room
    if (stageQuietSamples + 2 * stageFadeSamples > stageCycleSamples)
        stageQuietSamples = stageCycleSamples - 2 * stageFadeSamples;
    stageSamplePos = 0;
    stageMaster.store(1.0f, std::memory_order_relaxed);
    voiceParams.stageA.store(1.0f, std::memory_order_relaxed);
    voiceParams.stageB.store(1.0f, std::memory_order_relaxed);
    plateReverb.setAuxScale(1.0f);
    stereoDelay.setAuxScale(1.0f);
}

// Sample-accurate envelope for periodic attenuation. Returns 1.0f when
// licensed or between cycles; dips to 0.0f during the configured quiet
// window with linear fades at either end. Advances stageSamplePos.
float ParasiteProcessor::computeStageEnvelope(int numSamples)
{
    if (dspGainToken.load(std::memory_order_relaxed) == kDspActive)
        return 1.0f;

    const int64_t pos     = stageSamplePos % stageCycleSamples;
    const int64_t muteHi  = stageCycleSamples - stageFadeSamples;
    const int64_t muteLo  = muteHi - stageQuietSamples;
    const int64_t fadeIn  = muteLo - stageFadeSamples;

    float g = 1.0f;
    if (pos >= muteLo && pos < muteHi)
        g = 0.0f;
    else if (pos >= fadeIn && pos < muteLo)
        g = 1.0f - static_cast<float>(pos - fadeIn) / static_cast<float>(stageFadeSamples);
    else if (pos >= muteHi)
        g = static_cast<float>(pos - muteHi) / static_cast<float>(stageFadeSamples);

    stageSamplePos += numSamples;
    // Keep counter from growing unbounded over very long sessions
    if (stageSamplePos >= stageCycleSamples * 1024)
        stageSamplePos %= stageCycleSamples;
    return g;
}

// --- Process audio ---
void ParasiteProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    // The host wants a silent buffer. We still run zero input through the
    // effect tail (reverb/delay) so resuming from bypass doesn't introduce
    // a hard cut when the tail would naturally fade. Simplest correct
    // implementation: output silence and drop MIDI — the existing internal
    // state is preserved automatically because we don't reset anything.
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midiMessages.clear();
}

void ParasiteProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Block-rate stage envelope + distributed publication. When the plugin is
    // licensed this is the identity (all factors = 1.0f). When unlicensed it
    // sweeps a periodic attenuation cycle across multiple DSP stages so the
    // attenuation cannot be bypassed from a single multiply patch.
    const float stageG = computeStageEnvelope(buffer.getNumSamples());
    const float s30    = std::pow(stageG, 0.30f);
    voiceParams.stageA.store(s30, std::memory_order_relaxed);

    // Serviced at the top of the block so a preset change that landed
    // between blocks starts from a clean slate: every voice is silenced
    // (with tail-off so the anti-click fade in FMVoice handles the pop),
    // every effect buffer is zeroed, incoming MIDI in this block is
    // dropped so a stale note-on from the previous patch can't arm the
    // new one.
    if (voicePanicPending.exchange(false, std::memory_order_acquire))
    {
        synth.allNotesOff(0, /*allowTailOff*/ false);
        stereoDelay.reset();
        plateReverb.reset();
        liquidChorus.reset();
        rubberComb.reset();
        volumeShaper.reset();
        midiMessages.clear();
    }

    // Preset preview notes (injected from GUI thread via atomic flags)
    if (previewNoteOn.exchange(false, std::memory_order_relaxed))
        midiMessages.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 0);
    if (previewNoteOff.exchange(false, std::memory_order_relaxed))
        midiMessages.addEvent(juce::MidiMessage::noteOff(1, 60, 0.0f), 0);

    // MIDI CC handling:
    //   CC1  (mod wheel)  → carrier fine-tune offset (+100 cents at full)
    //   CC11 (expression) → voice output multiplier
    // CC64 (sustain pedal) and CC123 (all-notes-off) are handled by JUCE's
    // Synthesiser default controller routing — no custom code needed.
    for (const auto metadata : midiMessages)
    {
        const auto& msg = metadata.getMessage();
        if (msg.isController())
        {
            const int cc  = msg.getControllerNumber();
            const float v = msg.getControllerValue() / 127.0f;
            if (cc == 1)
                voiceParams.modWheel.store(v, std::memory_order_relaxed);
            else if (cc == 11)
                voiceParams.expression.store(v, std::memory_order_relaxed);
        }
    }

    // Mono mode enforcement: when enabled, any new note-on releases the
    // currently-playing voices so polyphony is reduced to one. allowTailOff
    // keeps the transition click-free — the briefly-overlapping release
    // segment bridges the two notes musically (legato-style).
    if (voiceParams.mono && voiceParams.mono->load() > 0.5f)
    {
        for (const auto metadata : midiMessages)
        {
            if (metadata.getMessage().isNoteOn())
            {
                synth.allNotesOff(0, /*allowTailOff*/ true);
                break;
            }
        }
    }

    // --- LFO retrigger on note-on (only if retrig enabled per LFO) ---
    for (const auto metadata : midiMessages)
    {
        if (metadata.getMessage().isNoteOn())
        {
            for (int l = 0; l < 3; ++l)
                if (lfoCache[l].retrig && lfoCache[l].retrig->load() > 0.5f)
                    globalLFO[l].resetPhase();
            break; // one reset per block is enough
        }
    }

    // --- Global LFO routing: compute modulation sums ---
    {
        // Reset all modulation accumulators
        float modSums[static_cast<int>(bb::LFODest::Count)] = {};

        // Check if any LFO has velocity-rate enabled → swap velocity away from volume
        bool anyVel = false;
        for (int l = 0; l < 3; ++l)
            if (lfoCache[l].vel->load() > 0.5f)
                anyVel = true;
        voiceParams.velSwap.store(anyVel, std::memory_order_relaxed);

        float lastVel = voiceParams.lastVelocity.load(std::memory_order_relaxed);

        for (int l = 0; l < 3; ++l)
        {
            auto& c = lfoCache[l];
            // Fast exit when no slot on this LFO is routed — ticking the
            // oscillator, evaluating Catmull-Rom, and updating atomics is
            // pure waste. Still publish peak=1 so the GUI doesn't flicker
            // between "assigned but silent" and "truly unassigned".
            bool anyRouted = false;
            for (int s = 0; s < kSlotsPerLFO; ++s)
                if (static_cast<int>(c.dest[s]->load()) > 0) { anyRouted = true; break; }
            if (!anyRouted)
            {
                voiceParams.lfoPeak[l].store(1.0f, std::memory_order_relaxed);
                continue;
            }

            float lfoRate = c.rate->load();
            int lfoSyncIdx = static_cast<int>(c.sync->load());
            if (lfoSyncIdx > 0)
            {
                static constexpr float beatDurations[] = {
                    32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f,
                    2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f
                };
                float bpm = 120.0f;
                if (auto* ph = getPlayHead())
                {
                    auto pos = ph->getPosition();
                    if (pos.hasValue() && pos->getBpm().hasValue())
                        bpm = static_cast<float>(*pos->getBpm());
                }
                lfoRate = bpm / (60.0f * beatDurations[lfoSyncIdx - 1]);
            }

            // Velocity → LFO rate: scale rate by velocity (0.1x at vel=0, 1x at vel=1)
            if (c.vel->load() > 0.5f)
                lfoRate *= 0.1f + 0.9f * lastVel;

            globalLFO[l].setRate(lfoRate);
            globalLFO[l].setWaveType(static_cast<bb::LFOWaveType>(static_cast<int>(c.wave->load())));

            // Store unipolar peak for GUI arc scaling
            voiceParams.lfoPeak[l].store(globalLFO[l].getUniPeak(), std::memory_order_relaxed);

            // Tick once per block — advance phase by full block duration
            // Remap from bipolar [-1,+1] to unipolar [0,+1]
            float lfoVal = (globalLFO[l].tickBlock(buffer.getNumSamples()) + 1.0f) * 0.5f;

            for (int s = 0; s < kSlotsPerLFO; ++s)
            {
                int dest = static_cast<int>(c.dest[s]->load());
                if (dest > 0 && dest < static_cast<int>(bb::LFODest::Count))
                    modSums[dest] += lfoVal * c.amt[s]->load();
            }
        }

        // Store into voiceParams for FMVoice to read
        voiceParams.lfoModPitch.store(modSums[static_cast<int>(bb::LFODest::Pitch)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModCutoff.store(modSums[static_cast<int>(bb::LFODest::FilterCutoff)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModRes.store(modSums[static_cast<int>(bb::LFODest::FilterRes)],
                                     std::memory_order_relaxed);
        voiceParams.lfoModMod1Lvl.store(modSums[static_cast<int>(bb::LFODest::Mod1Level)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModMod2Lvl.store(modSums[static_cast<int>(bb::LFODest::Mod2Level)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModVolume.store(modSums[static_cast<int>(bb::LFODest::Volume)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModDrive.store(modSums[static_cast<int>(bb::LFODest::Drive)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModNoise.store(modSums[static_cast<int>(bb::LFODest::CarNoise)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModSpread.store(modSums[static_cast<int>(bb::LFODest::CarSpread)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModFold.store(modSums[static_cast<int>(bb::LFODest::FoldAmt)],
                                      std::memory_order_relaxed);
        voiceParams.lfoModMod1Fine.store(modSums[static_cast<int>(bb::LFODest::Mod1Fine)],
                                          std::memory_order_relaxed);
        voiceParams.lfoModMod2Fine.store(modSums[static_cast<int>(bb::LFODest::Mod2Fine)],
                                          std::memory_order_relaxed);
        voiceParams.lfoModCarDrift.store(modSums[static_cast<int>(bb::LFODest::CarDrift)],
                                          std::memory_order_relaxed);
        voiceParams.lfoModCarFine.store(modSums[static_cast<int>(bb::LFODest::CarFine)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModDlyTime.store(modSums[static_cast<int>(bb::LFODest::DlyTime)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModDlyFeed.store(modSums[static_cast<int>(bb::LFODest::DlyFeed)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModDlyMix.store(modSums[static_cast<int>(bb::LFODest::DlyMix)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModRevSize.store(modSums[static_cast<int>(bb::LFODest::RevSize)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModRevMix.store(modSums[static_cast<int>(bb::LFODest::RevMix)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModLiqDepth.store(modSums[static_cast<int>(bb::LFODest::LiqDepth)],
                                          std::memory_order_relaxed);
        voiceParams.lfoModLiqMix.store(modSums[static_cast<int>(bb::LFODest::LiqMix)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModRubWarp.store(modSums[static_cast<int>(bb::LFODest::RubWarp)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModRubMix.store(modSums[static_cast<int>(bb::LFODest::RubMix)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModPEnvAmt.store(modSums[static_cast<int>(bb::LFODest::PEnvAmt)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModRevDamp.store(modSums[static_cast<int>(bb::LFODest::RevDamp)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModRevWidth.store(modSums[static_cast<int>(bb::LFODest::RevWidth)],
                                          std::memory_order_relaxed);
        voiceParams.lfoModRevPdly.store(modSums[static_cast<int>(bb::LFODest::RevPdly)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModDlyDamp.store(modSums[static_cast<int>(bb::LFODest::DlyDamp)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModDlySpread.store(modSums[static_cast<int>(bb::LFODest::DlySpread)],
                                           std::memory_order_relaxed);
        voiceParams.lfoModLiqRate.store(modSums[static_cast<int>(bb::LFODest::LiqRate)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModLiqTone.store(modSums[static_cast<int>(bb::LFODest::LiqTone)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModLiqFeed.store(modSums[static_cast<int>(bb::LFODest::LiqFeed)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModRubTone.store(modSums[static_cast<int>(bb::LFODest::RubTone)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModRubStretch.store(modSums[static_cast<int>(bb::LFODest::RubStretch)],
                                            std::memory_order_relaxed);
        voiceParams.lfoModRubFeed.store(modSums[static_cast<int>(bb::LFODest::RubFeed)],
                                         std::memory_order_relaxed);
        voiceParams.lfoModPorta.store(modSums[static_cast<int>(bb::LFODest::Porta)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv1A.store(modSums[static_cast<int>(bb::LFODest::Env1A)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv1D.store(modSums[static_cast<int>(bb::LFODest::Env1D)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv1S.store(modSums[static_cast<int>(bb::LFODest::Env1S)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv1R.store(modSums[static_cast<int>(bb::LFODest::Env1R)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv2A.store(modSums[static_cast<int>(bb::LFODest::Env2A)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv2D.store(modSums[static_cast<int>(bb::LFODest::Env2D)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv2S.store(modSums[static_cast<int>(bb::LFODest::Env2S)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv2R.store(modSums[static_cast<int>(bb::LFODest::Env2R)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv3A.store(modSums[static_cast<int>(bb::LFODest::Env3A)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv3D.store(modSums[static_cast<int>(bb::LFODest::Env3D)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv3S.store(modSums[static_cast<int>(bb::LFODest::Env3S)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModEnv3R.store(modSums[static_cast<int>(bb::LFODest::Env3R)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModPEnvA.store(modSums[static_cast<int>(bb::LFODest::PEnvA)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModPEnvD.store(modSums[static_cast<int>(bb::LFODest::PEnvD)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModPEnvS.store(modSums[static_cast<int>(bb::LFODest::PEnvS)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModPEnvR.store(modSums[static_cast<int>(bb::LFODest::PEnvR)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModShaperRate.store(modSums[static_cast<int>(bb::LFODest::ShaperRate)],
                                            std::memory_order_relaxed);
        voiceParams.lfoModShaperDepth.store(modSums[static_cast<int>(bb::LFODest::ShaperDepth)],
                                             std::memory_order_relaxed);
        voiceParams.lfoModMod1Coarse.store(modSums[static_cast<int>(bb::LFODest::Mod1Coarse)],
                                            std::memory_order_relaxed);
        voiceParams.lfoModMod2Coarse.store(modSums[static_cast<int>(bb::LFODest::Mod2Coarse)],
                                            std::memory_order_relaxed);
        voiceParams.lfoModCarCoarse.store(modSums[static_cast<int>(bb::LFODest::CarCoarse)],
                                           std::memory_order_relaxed);
        voiceParams.lfoModTremor.store(modSums[static_cast<int>(bb::LFODest::Tremor)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModVein.store(modSums[static_cast<int>(bb::LFODest::Vein)],
                                      std::memory_order_relaxed);
        voiceParams.lfoModFlux.store(modSums[static_cast<int>(bb::LFODest::Flux)],
                                      std::memory_order_relaxed);
        voiceParams.lfoModVortex.store(modSums[static_cast<int>(bb::LFODest::Vortex)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModHelix.store(modSums[static_cast<int>(bb::LFODest::Helix)],
                                       std::memory_order_relaxed);
        voiceParams.lfoModPlasma.store(modSums[static_cast<int>(bb::LFODest::Plasma)],
                                        std::memory_order_relaxed);
        voiceParams.lfoModMacroTime.store(modSums[static_cast<int>(bb::LFODest::MacroTime)],
                                           std::memory_order_relaxed);
    }

    voiceParams.stageB.store(std::pow(stageG, 0.25f), std::memory_order_relaxed);
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();

    // --- Post-synth FX: Liquid Chorus (texture) ---
    if (liqOnParam->load() > 0.5f && buffer.getNumChannels() >= 2)
    {
        float liqDepth = juce::jlimit(0.0f, 1.0f, liqDepthParam->load()
                         + voiceParams.lfoModLiqDepth.load(std::memory_order_relaxed));
        float liqMix   = juce::jlimit(0.0f, 1.0f, liqMixParam->load()
                         + voiceParams.lfoModLiqMix.load(std::memory_order_relaxed));
        float liqRate = juce::jlimit(0.05f, 3.0f, liqRateParam->load()
                       + voiceParams.lfoModLiqRate.load(std::memory_order_relaxed));
        float liqTone = juce::jlimit(0.0f, 1.0f, liqToneParam->load()
                        + voiceParams.lfoModLiqTone.load(std::memory_order_relaxed));
        float liqFeed = juce::jlimit(0.0f, 1.0f, liqFeedParam->load()
                        + voiceParams.lfoModLiqFeed.load(std::memory_order_relaxed));
        liquidChorus.setParameters(liqRate, liqDepth,
                                   liqTone, liqFeed,
                                   liqMix);
        liquidChorus.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    // --- Post-synth FX: Rubber Comb (texture) ---
    if (rubOnParam->load() > 0.5f && buffer.getNumChannels() >= 2)
    {
        float rubWarp = juce::jlimit(0.0f, 1.0f, rubWarpParam->load()
                        + voiceParams.lfoModRubWarp.load(std::memory_order_relaxed));
        float rubMix  = juce::jlimit(0.0f, 1.0f, rubMixParam->load()
                        + voiceParams.lfoModRubMix.load(std::memory_order_relaxed));
        float rubTone = juce::jlimit(0.0f, 1.0f, rubToneParam->load()
                       + voiceParams.lfoModRubTone.load(std::memory_order_relaxed));
        float rubStretch = juce::jlimit(0.0f, 1.0f, rubStretchParam->load()
                           + voiceParams.lfoModRubStretch.load(std::memory_order_relaxed));
        float rubFeed = juce::jlimit(0.0f, 1.0f, rubFeedParam->load()
                        + voiceParams.lfoModRubFeed.load(std::memory_order_relaxed));
        rubberComb.setParameters(rubTone, rubStretch,
                                 rubWarp, rubMix, rubFeed);
        rubberComb.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    // --- Post-synth FX: Delay (spatial) ---
    {
        bool dlyOn = dlyOnParam->load() > 0.5f;
        if (dlyOn && !dlyWasOn) stereoDelay.reset();
        dlyWasOn = dlyOn;
    }
    // Always publish auxScale so the stored factor is current even when the
    // effect is off — if it gets toggled on mid-cycle we want the right gain.
    stereoDelay.setAuxScale(std::pow(stageG, 0.20f));
    if (dlyWasOn && buffer.getNumChannels() >= 2)
    {
        // Sync index: 0 = free (use DLY_TIME knob + LFO), 1..9 map to beat
        // divisions in quarter-note units. In sync mode, any LFO mapped to
        // DlyTime shifts the beat index (rounded) so modulation still has a
        // musical effect — it steps rhythmically through adjacent divisions.
        int dlySyncIdx = static_cast<int>(dlySyncParam->load());
        float dlyTime;
        if (dlySyncIdx > 0)
        {
            static constexpr float beatsQN[] = {
                4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f,   // 1/1 .. 1/32
                2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f    // 1/4T, 1/8T, 1/16T
            };
            const float lfoMod = voiceParams.lfoModDlyTime.load(std::memory_order_relaxed);
            const int idxOffset = static_cast<int>(std::lround(lfoMod * 4.0f));
            const int effectiveIdx = juce::jlimit(1, 9, dlySyncIdx + idxOffset);
            float bpm = 120.0f;
            if (auto* ph = getPlayHead())
            {
                auto pos = ph->getPosition();
                if (pos.hasValue() && pos->getBpm().hasValue())
                    bpm = static_cast<float>(*pos->getBpm());
            }
            const float beats = beatsQN[effectiveIdx - 1];
            dlyTime = juce::jlimit(0.01f, 2.0f, (60.0f / bpm) * beats);
        }
        else
        {
            dlyTime = juce::jlimit(0.01f, 2.0f, dlyTimeParam->load()
                        + voiceParams.lfoModDlyTime.load(std::memory_order_relaxed) * 0.5f);
        }
        float dlyFeed = juce::jlimit(0.0f, 0.99f, dlyFeedParam->load()
                        + voiceParams.lfoModDlyFeed.load(std::memory_order_relaxed));
        float dlyMix  = juce::jlimit(0.0f, 1.0f, dlyMixParam->load()
                        + voiceParams.lfoModDlyMix.load(std::memory_order_relaxed));
        float dlyDamp   = juce::jlimit(0.0f, 1.0f, dlyDampParam->load()
                          + voiceParams.lfoModDlyDamp.load(std::memory_order_relaxed));
        float dlySpread = juce::jlimit(0.0f, 1.0f, dlySpreadParam->load()
                          + voiceParams.lfoModDlySpread.load(std::memory_order_relaxed));
        stereoDelay.setParameters(dlyTime, dlyFeed,
                                  dlyDamp, dlyMix,
                                  dlyPingParam->load() > 0.5f,
                                  dlySpread);
        stereoDelay.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    // --- Post-synth FX: Plate Reverb (spatial) ---
    {
        bool revOn = revOnParam->load() > 0.5f;
        if (revOn && !revWasOn) plateReverb.reset();
        revWasOn = revOn;
    }
    plateReverb.setAuxScale(std::pow(stageG, 0.15f));
    if (revWasOn && buffer.getNumChannels() >= 2)
    {
        float revSize = juce::jlimit(0.0f, 1.0f, revSizeParam->load()
                        + voiceParams.lfoModRevSize.load(std::memory_order_relaxed));
        float revMix  = juce::jlimit(0.0f, 1.0f, revMixParam->load()
                        + voiceParams.lfoModRevMix.load(std::memory_order_relaxed));
        float revDamp  = juce::jlimit(0.0f, 1.0f, revDampParam->load()
                         + voiceParams.lfoModRevDamp.load(std::memory_order_relaxed));
        float revWidth = juce::jlimit(0.0f, 1.0f, revWidthParam->load()
                         + voiceParams.lfoModRevWidth.load(std::memory_order_relaxed));
        float revPdly  = juce::jlimit(0.0f, 200.0f, revPdlyParam->load()
                         + voiceParams.lfoModRevPdly.load(std::memory_order_relaxed) * 200.0f);
        plateReverb.setParameters(revSize, revDamp, revMix,
                                  revWidth, revPdly);
        plateReverb.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    // --- Post-FX: Volume Shaper ---
    if (shaperOnParam->load() > 0.5f)
    {
        int syncIdx = static_cast<int>(shaperSyncParam->load());
        float rate = shaperRateParam->load();

        if (syncIdx > 0)
        {
            static constexpr float beatDurations[] = {
                4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f,
                2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f
            };
            float bpm = 120.0f;
            if (auto* ph = getPlayHead())
            {
                auto pos = ph->getPosition();
                if (pos.hasValue() && pos->getBpm().hasValue())
                    bpm = static_cast<float>(*pos->getBpm());
            }
            float beats = beatDurations[syncIdx - 1];
            rate = bpm / (60.0f * beats);
        }

        rate = std::max(0.1f, rate + voiceParams.lfoModShaperRate.load(std::memory_order_relaxed) * 20.0f);
        volumeShaper.setRate(rate);
        volumeShaper.setDepth(juce::jlimit(0.0f, 1.0f,
            shaperDepthParam->load() + voiceParams.lfoModShaperDepth.load(std::memory_order_relaxed)));
        for (int i = 0; i < numSamples; ++i)
        {
            float gain = volumeShaper.tick();
            if (gain < 0.001f) gain = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer(ch)[i] *= gain;
        }
    }

    // --- Output stage trim (site 5: final compounding factor) ---
    // When licensed, s10 = 1.0^0.1 = 1.0 → no-op. When unlicensed and inside
    // the quiet window, all five site factors collapse the signal to silence.
    {
        const float s10 = std::pow(stageG, 0.10f);
        stageMaster.store(s10, std::memory_order_relaxed);
        if (s10 < 0.9999f)
        {
            if (s10 < 1.0e-4f)
                buffer.clear();
            else
                buffer.applyGain(s10);
        }
    }

    // Push L+R channels to visual buffers for GUI oscilloscope/FFT
    if (buffer.getNumChannels() > 0)
        visualBuffer.pushBlock(buffer.getReadPointer(0), numSamples);
    if (buffer.getNumChannels() > 1)
        visualBufferR.pushBlock(buffer.getReadPointer(1), numSamples);
}

// --- Programmes (presets) ---
int ParasiteProcessor::getNumPrograms() { return juce::jmax(1, getPresetCount()); }
int ParasiteProcessor::getCurrentProgram() { return currentPreset; }

void ParasiteProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < getPresetCount())
        loadPresetAt(index);
}

const juce::String ParasiteProcessor::getProgramName(int index)
{
    if (index >= 0 && index < static_cast<int>(presetRegistry.size()))
        return presetRegistry[static_cast<size_t>(index)].name;
    return {};
}

void ParasiteProcessor::changeProgramName(int, const juce::String&) {}

// --- Shared serialization helpers (DRY) ---
void ParasiteProcessor::serializeCustomData(juce::ValueTree& state) const
{
    state.setProperty("shaperTable", volumeShaper.serializeTable(), nullptr);
    for (int n = 0; n < 3; ++n)
    {
        auto prefix = "lfo" + juce::String(n + 1);
        state.setProperty(prefix + "Table", globalLFO[n].serializeTable(), nullptr);
        state.setProperty(prefix + "Curve", globalLFO[n].serializeCurve(), nullptr);
    }
    state.setProperty("mod1Harmonics", mod1Harmonics.serializeHarmonics(), nullptr);
    state.setProperty("mod2Harmonics", mod2Harmonics.serializeHarmonics(), nullptr);
    state.setProperty("carHarmonics", carHarmonics.serializeHarmonics(), nullptr);
}

void ParasiteProcessor::deserializeCustomData(const juce::ValueTree& tree)
{
    if (tree.hasProperty("shaperTable"))
        volumeShaper.deserializeTable(tree.getProperty("shaperTable").toString());
    else
        volumeShaper.resetTable();

    for (int n = 0; n < 3; ++n)
    {
        auto curveKey = "lfo" + juce::String(n + 1) + "Curve";
        auto tableKey = "lfo" + juce::String(n + 1) + "Table";
        if (tree.hasProperty(curveKey))
            globalLFO[n].deserializeCurve(tree.getProperty(curveKey).toString());
        else if (tree.hasProperty(tableKey))
            globalLFO[n].deserializeTable(tree.getProperty(tableKey).toString());
        else
            globalLFO[n].resetCurve();
    }

    auto deserializeHarm = [&](const juce::String& key, bb::HarmonicTable& ht) {
        if (tree.hasProperty(key))
            ht.deserializeHarmonics(tree.getProperty(key).toString());
        else
            ht.resetHarmonics();
    };
    deserializeHarm("mod1Harmonics", mod1Harmonics);
    deserializeHarm("mod2Harmonics", mod2Harmonics);
    deserializeHarm("carHarmonics", carHarmonics);
}

// --- State save/restore ---
void ParasiteProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    // Stamp the current schema version so future loads know which migrations
    // (if any) still need to run. Every save from this build onward carries it.
    state.setProperty("schemaVersion", kCurrentSchemaVersion, nullptr);
    state.setProperty("_presetIndex", currentPreset, nullptr);
    state.setProperty("_isUserPreset", isUserPresetLoaded, nullptr);
    state.setProperty("_userPresetName", currentUserPresetName, nullptr);
    state.setProperty("_displayName", getDisplayName(), nullptr);
    serializeCustomData(state);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ParasiteProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Validate the blob BEFORE mutating any internal state. A corrupt/foreign
    // chunk here must leave the plugin running with whatever it had loaded.
    if (data == nullptr || sizeInBytes <= 0)
    {
        BB_LOG_ERROR("setStateInformation: empty payload — keeping current state.");
        return;
    }

    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr)
    {
        BB_LOG_ERROR("setStateInformation: XML parse failed (size=" + juce::String(sizeInBytes)
                     + ") — keeping current state.");
        setLastLoadError("Invalid plugin state — XML parse failed");
        return;
    }
    if (!xml->hasTagName(apvts.state.getType()))
    {
        BB_LOG_ERROR("setStateInformation: unexpected root tag '"
                     + xml->getTagName() + "' — keeping current state.");
        setLastLoadError("Invalid plugin state — wrong root tag");
        return;
    }
    // Quiet everything before the new patch applies so stale voices /
    // tails don't morph into the new params mid-note.
    requestVoicePanic();
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        deserializeCustomData(tree);
        applyStateMigrations(tree);

        // Restore preset identity so the GUI shows the correct name
        if (tree.hasProperty("_presetIndex"))
            currentPreset = static_cast<int>(tree.getProperty("_presetIndex"));
        if (tree.hasProperty("_isUserPreset"))
            isUserPresetLoaded = static_cast<bool>(tree.getProperty("_isUserPreset"));
        if (tree.hasProperty("_userPresetName"))
            currentUserPresetName = tree.getProperty("_userPresetName").toString();
        if (tree.hasProperty("_displayName"))
            setDisplayName(tree.getProperty("_displayName").toString());

        // Strip meta-properties before replacing APVTS state
        tree.removeProperty("_presetIndex", nullptr);
        tree.removeProperty("_isUserPreset", nullptr);
        tree.removeProperty("_userPresetName", nullptr);
        tree.removeProperty("_displayName", nullptr);

        // replaceState calls setValueNotifyingHost on every changed param,
        // which would fire CurveListener and stomp the internal state we
        // just restored from the old-format serialized strings. Gate it.
        suppressCurveListener.store(true, std::memory_order_relaxed);
        apvts.replaceState(tree);
        suppressCurveListener.store(false, std::memory_order_relaxed);

        // Push the authoritative internal state into the params so any
        // stale H##/S## values in the loaded XML get overwritten.
        syncInternalToCurveParams();
        undoManager.clearUndoHistory();
        stateGeneration.fetch_add(1, std::memory_order_release);
    }
}

// --- User presets ---
juce::File ParasiteProcessor::getUserPresetsDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Voidscan").getChildFile("Parasite").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

void ParasiteProcessor::saveUserPreset(const juce::String& name, const juce::String& category)
{
    // Demo mode — user-preset writes are disabled. The editor greys the Save
    // button but a second guard here covers host-automation / API paths.
    if (! licenseManager.isLicensed())
    {
        setLastLoadError("Saving user presets requires a license — please activate.");
        BB_LOG_WARN("Preset save attempted in demo mode; ignored.");
        return;
    }

    // Sanitize filename to prevent path traversal
    auto safeName = juce::File::createLegalFileName(name);
    if (safeName.isEmpty()) return;

    auto state = apvts.copyState();
    serializeCustomData(state);

    auto xml = state.createXml();
    if (xml)
    {
        xml->setAttribute("name", name);
        xml->setAttribute("category", category);
        xml->setAttribute("pack", "User");

        // Cloud sync metadata
        auto file = getUserPresetsDir().getChildFile(safeName + ".prst");
        juce::String uuid;
        int version = 1;

        // Preserve existing UUID if overwriting
        if (file.existsAsFile())
        {
            auto existing = juce::parseXML(file);
            if (existing)
            {
                uuid = existing->getStringAttribute("uuid", "");
                version = existing->getIntAttribute("version", 0) + 1;
            }
        }
        if (uuid.isEmpty())
            uuid = juce::Uuid().toString();

        xml->setAttribute("uuid", uuid);
        xml->setAttribute("version", version);
        xml->setAttribute("updatedAt", juce::Time::getCurrentTime().toISO8601(true));

        // Atomic write via TemporaryFile — otherwise a concurrent save from
        // another instance (or a process crash mid-write) could leave the
        // target half-written and unreadable. overwriteTargetFileWithTemporary
        // maps to an atomic rename(2) on POSIX.
        juce::TemporaryFile tmp(file);
        bool ok = xml->writeTo(tmp.getFile())
               && tmp.overwriteTargetFileWithTemporary();
        if (!ok)
            DBG("Failed to save preset: " + file.getFullPathName());

        // Fire-and-forget cloud upload
        cloudPresetManager.uploadPreset(uuid);
    }
}

bool ParasiteProcessor::deleteUserPreset(const juce::String& name)
{
    auto file = getUserPresetsDir().getChildFile(name + ".prst");
    if (!file.existsAsFile()) return false;

    // Read UUID before deleting for cloud sync
    juce::String uuid;
    auto xml = juce::parseXML(file);
    if (xml)
        uuid = xml->getStringAttribute("uuid", "");

    if (file.deleteFile())
    {
        if (uuid.isNotEmpty())
            cloudPresetManager.deletePreset(uuid);
        return true;
    }
    return false;
}

// --- Favorites ---

bool ParasiteProcessor::isFavorite(const juce::String& presetName) const
{
    return favorites.contains(presetName);
}

void ParasiteProcessor::toggleFavorite(const juce::String& presetName)
{
    if (favorites.contains(presetName))
        favorites.removeString(presetName);
    else
        favorites.add(presetName);
    saveFavorites();
}

void ParasiteProcessor::saveFavorites()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Voidscan").getChildFile("Parasite");
    dir.createDirectory();
    auto file = dir.getChildFile("favorites.txt");

    // Atomic write: two instances toggling a favorite simultaneously would
    // otherwise race on replaceWithText and risk a truncated file.
    juce::TemporaryFile tmp(file);
    tmp.getFile().replaceWithText(favorites.joinIntoString("\n"));
    tmp.overwriteTargetFileWithTemporary();
}

void ParasiteProcessor::loadFavorites()
{
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("Voidscan").getChildFile("Parasite").getChildFile("favorites.txt");
    if (file.existsAsFile())
    {
        juce::StringArray lines;
        lines.addLines(file.loadFileAsString());
        favorites.clear();
        for (auto& l : lines)
            if (l.isNotEmpty())
                favorites.add(l);
    }
}

void ParasiteProcessor::loadUserPreset(const juce::String& name)
{
    auto file = getUserPresetsDir().getChildFile(name + ".prst");
    if (!file.existsAsFile())
    {
        BB_LOG_WARN("loadUserPreset: file not found '" + file.getFullPathName() + "'");
        setLastLoadError("Preset not found: " + name);
        return;
    }

    auto xml = juce::parseXML(file);
    if (xml == nullptr)
    {
        BB_LOG_ERROR("loadUserPreset: XML parse failed — '" + file.getFullPathName() + "'");
        setLastLoadError("Preset file is corrupt: " + name);
        return;
    }
    if (!xml->hasTagName(apvts.state.getType()))
    {
        BB_LOG_ERROR("loadUserPreset: wrong root tag '"
                     + xml->getTagName() + "' in '" + file.getFullPathName() + "'");
        setLastLoadError("Preset file has the wrong format: " + name);
        return;
    }
    requestVoicePanic();
    auto tree = juce::ValueTree::fromXml(*xml);
    applyStateMigrations(tree);
    suppressCurveListener.store(true, std::memory_order_relaxed);
    apvts.replaceState(tree);
    suppressCurveListener.store(false, std::memory_order_relaxed);
    deserializeCustomData(tree);
    syncInternalToCurveParams();
    undoManager.clearUndoHistory();
    stateGeneration.fetch_add(1, std::memory_order_release);
    isUserPresetLoaded = true;
    currentUserPresetName = name;
    setLastLoadError({});
    BB_LOG_INFO("Loaded user preset '" + name + "'");
}

// --- Shared preset loading from XML string ---
void ParasiteProcessor::loadPresetFromXml(const juce::String& xmlStr)
{
    // Direct apply without undo action — used by factory load / cloud sync.
    if (xmlStr.isEmpty())
    {
        BB_LOG_WARN("loadPresetFromXml: empty XML string — ignoring.");
        return;
    }
    auto xml = juce::parseXML(xmlStr);
    if (xml == nullptr)
    {
        BB_LOG_ERROR("loadPresetFromXml: XML parse failed — keeping current state.");
        lastLoadError = "Failed to parse preset XML";
        return;
    }
    if (!xml->hasTagName(apvts.state.getType()))
    {
        BB_LOG_ERROR("loadPresetFromXml: wrong root tag '" + xml->getTagName() + "'");
        lastLoadError = "Preset XML has the wrong format";
        return;
    }

    requestVoicePanic();
    auto tree = juce::ValueTree::fromXml(*xml);
    applyStateMigrations(tree);
    suppressCurveListener.store(true, std::memory_order_relaxed);
    apvts.replaceState(tree);
    suppressCurveListener.store(false, std::memory_order_relaxed);
    deserializeCustomData(tree);
    syncInternalToCurveParams();
    undoManager.clearUndoHistory();
    stateGeneration.fetch_add(1, std::memory_order_release);
    setLastLoadError({});
}

void ParasiteProcessor::loadFactoryPreset(const juce::String& resName)
{
    int size = 0;
    auto* data = BinaryData::getNamedResource(resName.toRawUTF8(), size);
    if (data != nullptr && size > 0)
        loadPresetFromXml(juce::String::fromUTF8(data, size));
}

// --- Build preset registry from BinaryData + user dir ---
void ParasiteProcessor::buildPresetRegistry()
{
    presetRegistry.clear();

    // Scan BinaryData for .prst resources
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        juce::String resName = BinaryData::namedResourceList[i];
        juce::String origName = BinaryData::originalFilenames[i];
        if (!origName.endsWith(".prst")) continue;

        int size = 0;
        auto* data = BinaryData::getNamedResource(resName.toRawUTF8(), size);
        if (data == nullptr || size <= 0) continue;

        auto xml = juce::parseXML(juce::String::fromUTF8(data, size));
        if (!xml) continue;

        PresetEntry entry;
        entry.category = xml->getStringAttribute("category", "Init");
        entry.name = xml->getStringAttribute("name", origName.upToLastOccurrenceOf(".", false, false));
        entry.pack = xml->getStringAttribute("pack", "Factory");
        entry.author = xml->getStringAttribute("author", "");
        entry.isFactory = true;
        entry.resourceName = resName;
        presetRegistry.push_back(std::move(entry));
    }

    // Sort factory presets by category order, then by name
    static const juce::StringArray categoryOrder { "Init", "Bass", "Lead", "Pluck", "Keys", "Pad", "Texture", "Drums", "FX" };
    std::sort(presetRegistry.begin(), presetRegistry.end(),
        [](const PresetEntry& a, const PresetEntry& b) {
            int catA = categoryOrder.indexOf(a.category);
            int catB = categoryOrder.indexOf(b.category);
            if (catA < 0) catA = 999;
            if (catB < 0) catB = 999;
            if (catA != catB) return catA < catB;
            return a.name.compareIgnoreCase(b.name) < 0;
        });

    // Scan user presets dir
    auto dir = getUserPresetsDir();
    juce::Array<juce::File> userFiles;
    userFiles.addArray(dir.findChildFiles(juce::File::findFiles, false, "*.prst"));

    juce::StringArray seenNames;
    for (auto& f : userFiles)
    {
        auto baseName = f.getFileNameWithoutExtension();
        if (seenNames.contains(baseName)) continue;
        seenNames.add(baseName);

        PresetEntry entry;
        entry.name = baseName;
        entry.category = "User";
        entry.pack = "User";
        entry.isFactory = false;
        entry.userFileName = baseName;

        // Try to read category, pack, and uuid from file
        auto xml = juce::parseXML(f);
        if (xml)
        {
            auto cat = xml->getStringAttribute("category", "User");
            if (cat.isNotEmpty()) entry.category = cat;
            auto p = xml->getStringAttribute("pack", "User");
            if (p.isNotEmpty()) entry.pack = p;
            auto u = xml->getStringAttribute("uuid", "");
            if (u.isNotEmpty()) entry.uuid = u;
            entry.author = xml->getStringAttribute("author", "");
        }

        presetRegistry.push_back(std::move(entry));
    }
}

juce::StringArray ParasiteProcessor::getAvailablePacks() const
{
    juce::StringArray packs;
    packs.add("All");
    for (auto& entry : presetRegistry)
    {
        if (entry.pack.isNotEmpty() && !packs.contains(entry.pack))
            packs.add(entry.pack);
    }
    return packs;
}

// --- Load preset by registry index ---
void ParasiteProcessor::loadPresetAt(int index)
{
    if (index < 0 || index >= static_cast<int>(presetRegistry.size())) return;

    auto& entry = presetRegistry[static_cast<size_t>(index)];
    if (entry.isFactory)
    {
        loadFactoryPreset(entry.resourceName);
        isUserPresetLoaded = false;
        currentUserPresetName.clear();
    }
    else
    {
        loadUserPreset(entry.userFileName);
    }
    currentPreset = index;
    setDisplayName({}); // clear override when loading a named preset
}

// ──────────────────────────────────────────────────────────────────────
// State schema migrations
// ──────────────────────────────────────────────────────────────────────

int ParasiteProcessor::getSchemaVersion(const juce::ValueTree& tree) noexcept
{
    // Pre-versioning presets default to v1. Anything saved by this build
    // onward will carry the explicit schemaVersion property.
    return tree.hasProperty("schemaVersion")
        ? static_cast<int>(tree.getProperty("schemaVersion"))
        : 1;
}

void ParasiteProcessor::applyStateMigrations(juce::ValueTree& tree)
{
    const int fromVersion = getSchemaVersion(tree);

    if (fromVersion < 2)
    {
        migrateOldPitchParams(tree);
        injectMissingLFODefaults(tree);
    }

    if (fromVersion < 3)
        renameMacroParamIds(tree);

    tree.setProperty("schemaVersion", kCurrentSchemaVersion, nullptr);
}

// v2 → v3: harmonic macros were renamed in code to match the UI labels —
// CORTEX → VORTEX, ICHOR → HELIX. Walk the tree and rewrite any PARAM
// child's `id` property so previously-saved values carry over without
// resetting to default. Idempotent; safe to run on v3+ trees (no PARAM
// children will match the old IDs after the first migration).
void ParasiteProcessor::renameMacroParamIds(juce::ValueTree& tree)
{
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild(i);
        if (!child.hasType("PARAM")) continue;
        const auto id = child.getProperty("id").toString();
        if (id == "CORTEX")
            child.setProperty("id", "VORTEX", nullptr);
        else if (id == "ICHOR")
            child.setProperty("id", "HELIX", nullptr);
    }
}

// Add default global-LFO params to presets that predate them. Idempotent —
// only inserts rows whose IDs are missing.
void ParasiteProcessor::injectMissingLFODefaults(juce::ValueTree& tree)
{
    bool hasLFO = false;
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild(i);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == "LFO1_RATE")
        { hasLFO = true; break; }
    }
    if (hasLFO) return;

    auto addP = [&tree](const juce::String& id, float value) {
        juce::ValueTree p("PARAM");
        p.setProperty("id", id, nullptr);
        p.setProperty("value", value, nullptr);
        tree.addChild(p, -1, nullptr);
    };
    for (int n = 1; n <= 3; ++n)
    {
        auto pfx = "LFO" + juce::String(n) + "_";
        addP(pfx + "RATE",   1.0f);
        addP(pfx + "WAVE",   0.0f);
        addP(pfx + "SYNC",   3.0f);
        addP(pfx + "RETRIG", 0.0f);
        addP(pfx + "VEL",    0.0f);
        for (int s = 1; s <= kSlotsPerLFO; ++s)
        {
            addP(pfx + "DEST" + juce::String(s), 0.0f);
            addP(pfx + "AMT"  + juce::String(s), 0.0f);
        }
    }
}

// --- Migration anciens presets : MOD_PITCH → COARSE+FINE ou FIXED_FREQ ---
void ParasiteProcessor::migrateOldPitchParams(juce::ValueTree& tree)
{
    // If MOD1_COARSE already exists, no migration needed
    bool hasCoarse = false;
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild(i);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == "MOD1_COARSE")
        {
            hasCoarse = true;
            break;
        }
    }
    if (hasCoarse) return;

    static constexpr double middleC = 261.6255653;

    auto addParam = [&tree](const juce::String& id, float value)
    {
        juce::ValueTree param("PARAM");
        param.setProperty("id", id, nullptr);
        param.setProperty("value", value, nullptr);
        tree.addChild(param, -1, nullptr);
    };

    // Helper: find best coarse index (0=0.5, 1-48=themselves) for a target ratio
    auto findBestCoarse = [](double targetRatio) -> std::pair<int, double>
    {
        int bestIdx = 1;
        double bestDist = 1e9;
        // Check index 0 (0.5x)
        double dist = std::abs(std::log2(targetRatio / 0.5));
        if (dist < bestDist) { bestDist = dist; bestIdx = 0; }
        // Check indices 1-48
        for (int i = 1; i <= 48; ++i)
        {
            dist = std::abs(std::log2(targetRatio / static_cast<double>(i)));
            if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }
        double ratio = (bestIdx == 0) ? 0.5 : static_cast<double>(bestIdx);
        double fineCents = 1200.0 * std::log2(targetRatio / ratio);
        return { bestIdx, fineCents };
    };

    // Migrate each modulator
    for (int mod = 1; mod <= 2; ++mod)
    {
        juce::String prefix = "MOD" + juce::String(mod);

        float pitch = 0.0f;
        bool kb = true;
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto child = tree.getChild(i);
            if (!child.hasType("PARAM")) continue;
            auto id = child.getProperty("id").toString();
            if (id == prefix + "_PITCH") pitch = static_cast<float>(child.getProperty("value"));
            if (id == prefix + "_KB")    kb = static_cast<float>(child.getProperty("value")) > 0.5f;
        }

        if (kb)
        {
            double targetRatio = std::pow(2.0, static_cast<double>(pitch) / 12.0);
            auto [bestIdx, fineCents] = findBestCoarse(targetRatio);
            addParam(prefix + "_COARSE", static_cast<float>(bestIdx));
            addParam(prefix + "_FINE", static_cast<float>(std::round(fineCents * 10.0) / 10.0));
            addParam(prefix + "_FIXED_FREQ", 440.0f);
            addParam(prefix + "_MULTI", 4.0f);
        }
        else
        {
            float fixedFreq = static_cast<float>(middleC * std::pow(2.0, static_cast<double>(pitch) / 12.0));
            fixedFreq = juce::jlimit(20.0f, 16000.0f, fixedFreq);
            addParam(prefix + "_COARSE", 1.0f);
            addParam(prefix + "_FINE", 0.0f);
            addParam(prefix + "_FIXED_FREQ", fixedFreq);
            addParam(prefix + "_MULTI", 4.0f);
        }
    }

    // Migrate carrier: CAR_OCTAVE → CAR_COARSE + CAR_FINE
    {
        int octave = 0;
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto child = tree.getChild(i);
            if (child.hasType("PARAM") && child.getProperty("id").toString() == "CAR_OCTAVE")
            {
                octave = static_cast<int>(static_cast<float>(child.getProperty("value")));
                break;
            }
        }
        // Octave → ratio: 2^octave
        double targetRatio = std::pow(2.0, static_cast<double>(octave));
        auto [bestIdx, fineCents] = findBestCoarse(targetRatio);
        fineCents = juce::jlimit(-1000.0, 1000.0, fineCents);
        addParam("CAR_COARSE", static_cast<float>(bestIdx));
        addParam("CAR_FINE", static_cast<float>(std::round(fineCents * 10.0) / 10.0));
        addParam("CAR_FIXED_FREQ", 440.0f);
        addParam("CAR_KB", 1.0f);
    }

    addParam("CAR_NOISE", 0.0f);
    addParam("CAR_SPREAD", 0.0f);
}


// --- Création de l'éditeur ---
#ifdef PARASITE_HEADLESS_TESTS
juce::AudioProcessorEditor* ParasiteProcessor::createEditor() { return nullptr; }
#else
juce::AudioProcessorEditor* ParasiteProcessor::createEditor()
{
    return new ParasiteEditor(*this);
}
#endif

// --- Point d'entrée JUCE ---
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParasiteProcessor();
}
