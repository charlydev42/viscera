// PluginProcessor.cpp — Implémentation du processeur principal
// Contient : layout des paramètres, synthesiser, presets factory, state save/load
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/FMSound.h"

// --- Constructeur ---
VisceraProcessor::VisceraProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VisceraState", createParameterLayout())
{
    cacheParameterPointers();

    // Ajouter un sound et une voix (mono par défaut)
    synth.addSound(new bb::FMSound());
    auto* voice = new bb::FMVoice(voiceParams);
    synth.addVoice(voice);

}

// --- Cacher les pointeurs atomiques vers les paramètres ---
void VisceraProcessor::cacheParameterPointers()
{
    voiceParams.mod1On        = apvts.getRawParameterValue("MOD1_ON");
    voiceParams.mod1Wave      = apvts.getRawParameterValue("MOD1_WAVE");
    voiceParams.mod1Pitch     = apvts.getRawParameterValue("MOD1_PITCH");
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
    voiceParams.mod2Pitch     = apvts.getRawParameterValue("MOD2_PITCH");
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
    voiceParams.carOctave    = apvts.getRawParameterValue("CAR_OCTAVE");
    voiceParams.carCoarse    = apvts.getRawParameterValue("CAR_COARSE");
    voiceParams.carFine      = apvts.getRawParameterValue("CAR_FINE");
    voiceParams.carFixedFreq = apvts.getRawParameterValue("CAR_FIXED_FREQ");
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

    // FX on/off pointers
    dlyOnParam   = apvts.getRawParameterValue("DLY_ON");
    revOnParam   = apvts.getRawParameterValue("REV_ON");

    // FX param pointers
    dlyTimeParam = apvts.getRawParameterValue("DLY_TIME");
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
        lfoCache[n].rate = apvts.getRawParameterValue(id("RATE"));
        lfoCache[n].wave = apvts.getRawParameterValue(id("WAVE"));
        lfoCache[n].sync = apvts.getRawParameterValue(id("SYNC"));
        for (int s = 0; s < kSlotsPerLFO; ++s)
        {
            lfoCache[n].dest[s] = apvts.getRawParameterValue(id("DEST" + juce::String(s + 1)));
            lfoCache[n].amt[s]  = apvts.getRawParameterValue(id("AMT" + juce::String(s + 1)));
        }
    }
}

// --- Layout des paramètres ---
juce::AudioProcessorValueTreeState::ParameterLayout
VisceraProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::AudioProcessorParameterGroup>> groups;

    juce::StringArray waveNames { "Sine", "Saw", "Square", "Triangle", "Pulse" };

    // --- Groupe Modulateur 1 ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("mod1", "Modulator 1", "|");
        g->addChild(std::make_unique<juce::AudioParameterBool>("MOD1_ON", "Mod1 On", true));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("MOD1_WAVE", "Mod1 Wave", waveNames, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_PITCH", "Mod1 Pitch",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f)); // legacy
        g->addChild(std::make_unique<juce::AudioParameterBool>("MOD1_KB", "Mod1 KB", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_LEVEL", "Mod1 Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD1_COARSE", "Mod1 Coarse", 0, 48, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_FINE", "Mod1 Fine",
            juce::NormalisableRange<float>(-1000.0f, 1000.0f, 0.1f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD1_FIXED_FREQ", "Mod1 Fixed Freq",
            juce::NormalisableRange<float>(20.0f, 16000.0f, 0.0f, 0.3f), 440.0f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD1_MULTI", "Mod1 Multi", 0, 5, 4));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_A", "Env1 Attack",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.01f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_D", "Env1 Decay",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_S", "Env1 Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV1_R", "Env1 Release",
            juce::NormalisableRange<float>(0.001f, 8.0f, 0.0f, 0.3f), 0.3f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Modulateur 2 ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("mod2", "Modulator 2", "|");
        g->addChild(std::make_unique<juce::AudioParameterBool>("MOD2_ON", "Mod2 On", true));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("MOD2_WAVE", "Mod2 Wave", waveNames, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_PITCH", "Mod2 Pitch",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f)); // legacy
        g->addChild(std::make_unique<juce::AudioParameterBool>("MOD2_KB", "Mod2 KB", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_LEVEL", "Mod2 Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD2_COARSE", "Mod2 Coarse", 0, 48, 1));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_FINE", "Mod2 Fine",
            juce::NormalisableRange<float>(-1000.0f, 1000.0f, 0.1f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("MOD2_FIXED_FREQ", "Mod2 Fixed Freq",
            juce::NormalisableRange<float>(20.0f, 16000.0f, 0.0f, 0.3f), 440.0f));
        g->addChild(std::make_unique<juce::AudioParameterInt>("MOD2_MULTI", "Mod2 Multi", 0, 5, 4));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_A", "Env2 Attack",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.01f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_D", "Env2 Decay",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_S", "Env2 Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV2_R", "Env2 Release",
            juce::NormalisableRange<float>(0.001f, 8.0f, 0.0f, 0.3f), 0.3f));
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
        g->addChild(std::make_unique<juce::AudioParameterBool>("CAR_KB", "Carrier KB", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_A", "Env3 Attack",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.01f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_D", "Env3 Decay",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_S", "Env3 Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("ENV3_R", "Env3 Release",
            juce::NormalisableRange<float>(0.001f, 8.0f, 0.0f, 0.3f), 0.3f));
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
        g->addChild(std::make_unique<juce::AudioParameterBool>("XOR_ON", "XOR", false));
        g->addChild(std::make_unique<juce::AudioParameterBool>("SYNC", "Sync", false));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("FM_ALGO", "FM Algorithm",
            juce::StringArray{ "Series", "Parallel", "Stack", "Ring", "Feedback" }, 0));
        groups.push_back(std::move(g));
    }

    // --- Groupe Pitch Envelope ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("pitchenv", "Pitch Envelope", "|");
        g->addChild(std::make_unique<juce::AudioParameterBool>("PENV_ON", "Pitch Env On", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_AMT", "Pitch Env Amount",
            juce::NormalisableRange<float>(-96.0f, 96.0f, 0.1f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_A", "Pitch Env Attack",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.001f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_D", "Pitch Env Decay",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.15f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_S", "Pitch Env Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PENV_R", "Pitch Env Release",
            juce::NormalisableRange<float>(0.001f, 8.0f, 0.0f, 0.3f), 0.1f));
        groups.push_back(std::move(g));
    }

    // --- Groupe Filtre ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("filter", "Filter", "|");
        g->addChild(std::make_unique<juce::AudioParameterBool>("FILT_ON", "Filter On", true));
        g->addChild(std::make_unique<juce::AudioParameterChoice>("FILT_TYPE", "Filter Type",
            juce::StringArray{ "LP", "HP", "BP", "Notch" }, 0));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("FILT_CUTOFF", "Filter Cutoff",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 20000.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("FILT_RES", "Filter Resonance",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        groups.push_back(std::move(g));
    }

    // --- Groupe FX (Delay + Reverb) ---
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>("fx", "FX", "|");
        g->addChild(std::make_unique<juce::AudioParameterBool>("DLY_ON", "Delay On", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_TIME", "Delay Time",
            juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.4f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_FEED", "Delay Feedback",
            juce::NormalisableRange<float>(0.0f, 0.9f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_DAMP", "Delay Damp",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_MIX", "Delay Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        g->addChild(std::make_unique<juce::AudioParameterBool>("DLY_PING", "Delay Ping-Pong", false));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DLY_SPREAD", "Delay Spread",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterBool>("REV_ON", "Reverb On", false));
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
        g->addChild(std::make_unique<juce::AudioParameterBool>("LIQ_ON", "Liquid On", false));
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
        g->addChild(std::make_unique<juce::AudioParameterBool>("RUB_ON", "Rubber On", false));
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
                                      "Tremor", "Vein", "Flux" };

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
        g->addChild(std::make_unique<juce::AudioParameterBool>("SHAPER_ON", "Shaper On", false));
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
        g->addChild(std::make_unique<juce::AudioParameterBool>("MONO", "Mono", true));
        g->addChild(std::make_unique<juce::AudioParameterBool>("RETRIG", "Retrigger", true));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("PORTA", "Portamento",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));
        g->addChild(std::make_unique<juce::AudioParameterFloat>("DISP_AMT", "HemoFold",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        groups.push_back(std::move(g));
    }

    return { groups.begin(), groups.end() };
}

// --- Préparation audio ---
void VisceraProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
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
}

// --- Process audio ---
void VisceraProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Injecter les notes du clavier GUI dans le buffer MIDI
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // --- LFO retrigger on note-on ---
    for (const auto metadata : midiMessages)
    {
        if (metadata.getMessage().isNoteOn())
        {
            for (int l = 0; l < 3; ++l)
                globalLFO[l].resetPhase();
            break; // one reset per block is enough
        }
    }

    // --- Global LFO routing: compute modulation sums ---
    {
        // Reset all modulation accumulators
        float modSums[static_cast<int>(bb::LFODest::Count)] = {};

        for (int l = 0; l < 3; ++l)
        {
            auto& c = lfoCache[l];
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
    }

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();

    // --- Post-synth FX: Liquid Chorus (texture) ---
    if (liqOnParam->load() > 0.5f && buffer.getNumChannels() >= 2)
    {
        float liqDepth = juce::jlimit(0.0f, 1.0f, liqDepthParam->load()
                         + voiceParams.lfoModLiqDepth.load(std::memory_order_relaxed));
        float liqMix   = juce::jlimit(0.0f, 1.0f, liqMixParam->load()
                         + voiceParams.lfoModLiqMix.load(std::memory_order_relaxed));
        float liqRate = juce::jlimit(0.0f, 1.0f, liqRateParam->load()
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
    if (dlyWasOn && buffer.getNumChannels() >= 2)
    {
        float dlyTime = juce::jlimit(0.01f, 2.0f, dlyTimeParam->load()
                        + voiceParams.lfoModDlyTime.load(std::memory_order_relaxed) * 0.5f);
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

    // Push L+R channels to visual buffers for GUI oscilloscope/FFT
    if (buffer.getNumChannels() > 0)
        visualBuffer.pushBlock(buffer.getReadPointer(0), numSamples);
    if (buffer.getNumChannels() > 1)
        visualBufferR.pushBlock(buffer.getReadPointer(1), numSamples);
}

// --- Programmes (presets) ---
int VisceraProcessor::getNumPrograms() { return kNumPresets; }
int VisceraProcessor::getCurrentProgram() { return currentPreset; }

void VisceraProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < kNumPresets)
        loadPreset(index);
}

const juce::String VisceraProcessor::getProgramName(int index)
{
    auto& names = getPresetNames();
    if (index >= 0 && index < names.size())
        return names[index];
    return {};
}

void VisceraProcessor::changeProgramName(int, const juce::String&) {}

// --- State save/restore ---
void VisceraProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("shaperTable", volumeShaper.serializeTable(), nullptr);
    state.setProperty("lfo1Table", globalLFO[0].serializeTable(), nullptr);
    state.setProperty("lfo2Table", globalLFO[1].serializeTable(), nullptr);
    state.setProperty("lfo3Table", globalLFO[2].serializeTable(), nullptr);
    state.setProperty("lfo1Curve", globalLFO[0].serializeCurve(), nullptr);
    state.setProperty("lfo2Curve", globalLFO[1].serializeCurve(), nullptr);
    state.setProperty("lfo3Curve", globalLFO[2].serializeCurve(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VisceraProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.hasProperty("shaperTable"))
            volumeShaper.deserializeTable(tree.getProperty("shaperTable").toString());
        // Load curve data if present, otherwise fall back to table (old presets)
        for (int n = 0; n < 3; ++n)
        {
            auto curveKey = "lfo" + juce::String(n + 1) + "Curve";
            auto tableKey = "lfo" + juce::String(n + 1) + "Table";
            if (tree.hasProperty(curveKey))
                globalLFO[n].deserializeCurve(tree.getProperty(curveKey).toString());
            else if (tree.hasProperty(tableKey))
                globalLFO[n].deserializeTable(tree.getProperty(tableKey).toString());
        }
        migrateOldPitchParams(tree);

        // Migrate: inject default global LFO params if absent
        {
            bool hasLFO = false;
            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                auto child = tree.getChild(i);
                if (child.hasType("PARAM") && child.getProperty("id").toString() == "LFO1_RATE")
                { hasLFO = true; break; }
            }
            if (!hasLFO)
            {
                auto addP = [&tree](const juce::String& id, float value) {
                    juce::ValueTree p("PARAM");
                    p.setProperty("id", id, nullptr);
                    p.setProperty("value", value, nullptr);
                    tree.addChild(p, -1, nullptr);
                };
                for (int n = 1; n <= 3; ++n)
                {
                    auto pfx = "LFO" + juce::String(n) + "_";
                    addP(pfx + "RATE", 1.0f);
                    addP(pfx + "WAVE", 0.0f);
                    for (int s = 1; s <= kSlotsPerLFO; ++s)
                    {
                        addP(pfx + "DEST" + juce::String(s), 0.0f);
                        addP(pfx + "AMT" + juce::String(s), 0.0f);
                    }
                }
            }
        }

        apvts.replaceState(tree);
    }
}

// --- User presets ---
juce::File VisceraProcessor::getUserPresetsDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Viscera").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

juce::StringArray VisceraProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    auto dir = getUserPresetsDir();
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        names.add(f.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

void VisceraProcessor::saveUserPreset(const juce::String& name)
{
    auto state = apvts.copyState();
    state.setProperty("shaperTable", volumeShaper.serializeTable(), nullptr);
    state.setProperty("lfo1Table", globalLFO[0].serializeTable(), nullptr);
    state.setProperty("lfo2Table", globalLFO[1].serializeTable(), nullptr);
    state.setProperty("lfo3Table", globalLFO[2].serializeTable(), nullptr);
    state.setProperty("lfo1Curve", globalLFO[0].serializeCurve(), nullptr);
    state.setProperty("lfo2Curve", globalLFO[1].serializeCurve(), nullptr);
    state.setProperty("lfo3Curve", globalLFO[2].serializeCurve(), nullptr);

    auto xml = state.createXml();
    if (xml)
    {
        auto file = getUserPresetsDir().getChildFile(name + ".xml");
        xml->writeTo(file);
    }
}

void VisceraProcessor::loadUserPreset(const juce::String& name)
{
    auto file = getUserPresetsDir().getChildFile(name + ".xml");
    if (!file.existsAsFile()) return;

    auto xml = juce::parseXML(file);
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.hasProperty("shaperTable"))
            volumeShaper.deserializeTable(tree.getProperty("shaperTable").toString());
        for (int n = 0; n < 3; ++n)
        {
            auto curveKey = "lfo" + juce::String(n + 1) + "Curve";
            auto tableKey = "lfo" + juce::String(n + 1) + "Table";
            if (tree.hasProperty(curveKey))
                globalLFO[n].deserializeCurve(tree.getProperty(curveKey).toString());
            else if (tree.hasProperty(tableKey))
                globalLFO[n].deserializeTable(tree.getProperty(tableKey).toString());
        }
        migrateOldPitchParams(tree);
        apvts.replaceState(tree);
        isUserPresetLoaded = true;
        currentUserPresetName = name;
    }
}

// --- Migration anciens presets : MOD_PITCH → COARSE+FINE ou FIXED_FREQ ---
void VisceraProcessor::migrateOldPitchParams(juce::ValueTree& tree)
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

// --- Noms des presets ---
const juce::StringArray& VisceraProcessor::getPresetNames()
{
    static juce::StringArray names {
        "soft pulse", "nasal drone", "ethereal pad", "fm kick",
        "metal bell", "saw lead", "dark drone", "bright pluck",
        "fm organ", "digital harsh", "sync lead", "wobble bass",
        "alien fx", "crystal", "chaos engine", "soft texture",
        "microwave kick",
        "glide kick"
    };
    return names;
}

// --- Chargement d'un preset ---
void VisceraProcessor::loadPreset(int index)
{
    if (index < 0 || index >= kNumPresets) return;

    const char* xmlStr = getPresetXML(index);
    if (auto xml = juce::parseXML(xmlStr))
    {
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
    currentPreset = index;
    isUserPresetLoaded = false;
    currentUserPresetName.clear();
}

// --- Presets factory (16 presets) ---
const char* VisceraProcessor::getPresetXML(int index)
{
    static const char* presets[] = {
        // 0: soft pulse — son doux, peu de modulation
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.2"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.5"/>
  <PARAM id="ENV1_S" value="0.3"/>
  <PARAM id="ENV1_R" value="0.5"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="0"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.1"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.8"/>
  <PARAM id="ENV2_S" value="0.2"/>
  <PARAM id="ENV2_R" value="0.6"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.3"/>
  <PARAM id="ENV3_S" value="1.0"/>
  <PARAM id="ENV3_R" value="0.5"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="20000"/>
  <PARAM id="FILT_RES" value="0"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 1: nasal drone — nasal, ratio non-harmonique
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="1"/>
  <PARAM id="MOD1_PITCH" value="7"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.6"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="700"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.4"/>
  <PARAM id="ENV1_S" value="0.5"/>
  <PARAM id="ENV1_R" value="0.3"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="12"/>
  <PARAM id="MOD2_KB" value="0"/>
  <PARAM id="MOD2_LEVEL" value="0.3"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="523.25"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.2"/>
  <PARAM id="ENV2_S" value="0.6"/>
  <PARAM id="ENV2_R" value="0.4"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.5"/>
  <PARAM id="ENV3_S" value="0.8"/>
  <PARAM id="ENV3_R" value="0.4"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0.2"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="3000"/>
  <PARAM id="FILT_RES" value="0.4"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 2: ethereal pad — pad éthéré, attaque lente
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.4"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="1.0"/>
  <PARAM id="ENV1_D" value="2.0"/>
  <PARAM id="ENV1_S" value="0.6"/>
  <PARAM id="ENV1_R" value="2.0"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="12"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.3"/>
  <PARAM id="MOD2_COARSE" value="2"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="1.5"/>
  <PARAM id="ENV2_D" value="1.0"/>
  <PARAM id="ENV2_S" value="0.4"/>
  <PARAM id="ENV2_R" value="2.5"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.8"/>
  <PARAM id="ENV3_D" value="0.5"/>
  <PARAM id="ENV3_S" value="0.9"/>
  <PARAM id="ENV3_R" value="3.0"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0.15"/>
  <PARAM id="VEIN" value="0.1"/>
  <PARAM id="FLUX" value="0.1"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="8000"/>
  <PARAM id="FILT_RES" value="0.1"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="0"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0.2"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 3: fm kick — bass percussif avec pitch env
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.8"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.001"/>
  <PARAM id="ENV1_D" value="0.15"/>
  <PARAM id="ENV1_S" value="0.0"/>
  <PARAM id="ENV1_R" value="0.1"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="0"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.5"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.001"/>
  <PARAM id="ENV2_D" value="0.2"/>
  <PARAM id="ENV2_S" value="0.0"/>
  <PARAM id="ENV2_R" value="0.1"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="-1"/>
  <PARAM id="ENV3_A" value="0.001"/>
  <PARAM id="ENV3_D" value="0.4"/>
  <PARAM id="ENV3_S" value="0.0"/>
  <PARAM id="ENV3_R" value="0.2"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="0"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="1"/>
  <PARAM id="PENV_AMT" value="24"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.08"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="2000"/>
  <PARAM id="FILT_RES" value="0.2"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.6"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 4: metal bell — cloche métallique
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="7.02"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.7"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="702"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.001"/>
  <PARAM id="ENV1_D" value="1.5"/>
  <PARAM id="ENV1_S" value="0.0"/>
  <PARAM id="ENV1_R" value="1.0"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="12"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.4"/>
  <PARAM id="MOD2_COARSE" value="2"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.001"/>
  <PARAM id="ENV2_D" value="2.0"/>
  <PARAM id="ENV2_S" value="0.0"/>
  <PARAM id="ENV2_R" value="1.5"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="1"/>
  <PARAM id="ENV3_A" value="0.001"/>
  <PARAM id="ENV3_D" value="2.5"/>
  <PARAM id="ENV3_S" value="0.0"/>
  <PARAM id="ENV3_R" value="2.0"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="2"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="15000"/>
  <PARAM id="FILT_RES" value="0.0"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.4"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 5: saw lead — lead agressif
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="1"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.9"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.3"/>
  <PARAM id="ENV1_S" value="0.8"/>
  <PARAM id="ENV1_R" value="0.2"/>
  <PARAM id="MOD2_WAVE" value="2"/>
  <PARAM id="MOD2_PITCH" value="12"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.5"/>
  <PARAM id="MOD2_COARSE" value="2"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.2"/>
  <PARAM id="ENV2_S" value="0.7"/>
  <PARAM id="ENV2_R" value="0.2"/>
  <PARAM id="CAR_WAVE" value="1"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.1"/>
  <PARAM id="ENV3_S" value="1.0"/>
  <PARAM id="ENV3_R" value="0.15"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="1"/>
  <PARAM id="PENV_AMT" value="5"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.1"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="5000"/>
  <PARAM id="FILT_RES" value="0.3"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 6: dark drone — drone sombre, LFO sur tout
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="1"/>
  <PARAM id="MOD1_PITCH" value="-12"/>
  <PARAM id="MOD1_KB" value="0"/>
  <PARAM id="MOD1_LEVEL" value="0.6"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="130.81"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="2.0"/>
  <PARAM id="ENV1_D" value="1.0"/>
  <PARAM id="ENV1_S" value="0.8"/>
  <PARAM id="ENV1_R" value="3.0"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="5"/>
  <PARAM id="MOD2_KB" value="0"/>
  <PARAM id="MOD2_LEVEL" value="0.4"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="349.23"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="1.5"/>
  <PARAM id="ENV2_D" value="2.0"/>
  <PARAM id="ENV2_S" value="0.7"/>
  <PARAM id="ENV2_R" value="3.0"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="-1"/>
  <PARAM id="ENV3_A" value="1.0"/>
  <PARAM id="ENV3_D" value="1.0"/>
  <PARAM id="ENV3_S" value="0.9"/>
  <PARAM id="ENV3_R" value="4.0"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="0"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0.3"/>
  <PARAM id="VEIN" value="0.4"/>
  <PARAM id="FLUX" value="0.3"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="1500"/>
  <PARAM id="FILT_RES" value="0.5"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="0"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0.3"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 7: bright pluck — pluck court, harmoniques brillantes
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="1.0"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.001"/>
  <PARAM id="ENV1_D" value="0.08"/>
  <PARAM id="ENV1_S" value="0.0"/>
  <PARAM id="ENV1_R" value="0.05"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="19"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.6"/>
  <PARAM id="MOD2_COARSE" value="3"/>
  <PARAM id="MOD2_FINE" value="-2"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.001"/>
  <PARAM id="ENV2_D" value="0.12"/>
  <PARAM id="ENV2_S" value="0.0"/>
  <PARAM id="ENV2_R" value="0.08"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.001"/>
  <PARAM id="ENV3_D" value="0.6"/>
  <PARAM id="ENV3_S" value="0.0"/>
  <PARAM id="ENV3_R" value="0.3"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="12000"/>
  <PARAM id="FILT_RES" value="0.1"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 8: fm organ — orgue FM classique
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="12"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.4"/>
  <PARAM id="MOD1_COARSE" value="2"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.5"/>
  <PARAM id="ENV1_S" value="0.6"/>
  <PARAM id="ENV1_R" value="0.3"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="0"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.3"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.4"/>
  <PARAM id="ENV2_S" value="0.5"/>
  <PARAM id="ENV2_R" value="0.3"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.1"/>
  <PARAM id="ENV3_S" value="1.0"/>
  <PARAM id="ENV3_R" value="0.2"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0.05"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="20000"/>
  <PARAM id="FILT_RES" value="0.0"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 9: digital harsh — XOR activé
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="2"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.8"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.3"/>
  <PARAM id="ENV1_S" value="0.7"/>
  <PARAM id="ENV1_R" value="0.2"/>
  <PARAM id="MOD2_WAVE" value="1"/>
  <PARAM id="MOD2_PITCH" value="7"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.7"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="700"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.2"/>
  <PARAM id="ENV2_S" value="0.6"/>
  <PARAM id="ENV2_R" value="0.2"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.2"/>
  <PARAM id="ENV3_S" value="0.9"/>
  <PARAM id="ENV3_R" value="0.2"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0.2"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="1"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="6000"/>
  <PARAM id="FILT_RES" value="0.3"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.35"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 10: sync lead
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="1"/>
  <PARAM id="MOD1_PITCH" value="-5"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.5"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="-500"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.3"/>
  <PARAM id="ENV1_S" value="0.6"/>
  <PARAM id="ENV1_R" value="0.3"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="0"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.3"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.2"/>
  <PARAM id="ENV2_S" value="0.5"/>
  <PARAM id="ENV2_R" value="0.3"/>
  <PARAM id="CAR_WAVE" value="1"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.1"/>
  <PARAM id="ENV3_S" value="1.0"/>
  <PARAM id="ENV3_R" value="0.15"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="1"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="8000"/>
  <PARAM id="FILT_RES" value="0.2"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 11: wobble bass — flux + vein actifs
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.7"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.4"/>
  <PARAM id="ENV1_S" value="0.6"/>
  <PARAM id="ENV1_R" value="0.3"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="0"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.5"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.3"/>
  <PARAM id="ENV2_S" value="0.5"/>
  <PARAM id="ENV2_R" value="0.3"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="-1"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.2"/>
  <PARAM id="ENV3_S" value="0.9"/>
  <PARAM id="ENV3_R" value="0.3"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="0"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0.1"/>
  <PARAM id="VEIN" value="0.5"/>
  <PARAM id="FLUX" value="0.6"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="1200"/>
  <PARAM id="FILT_RES" value="0.6"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.55"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 12: alien fx — non-harmonique + XOR + pitch env
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="4"/>
  <PARAM id="MOD1_PITCH" value="3.5"/>
  <PARAM id="MOD1_KB" value="0"/>
  <PARAM id="MOD1_LEVEL" value="0.9"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="320.24"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.5"/>
  <PARAM id="ENV1_D" value="1.0"/>
  <PARAM id="ENV1_S" value="0.4"/>
  <PARAM id="ENV1_R" value="2.0"/>
  <PARAM id="MOD2_WAVE" value="3"/>
  <PARAM id="MOD2_PITCH" value="-7"/>
  <PARAM id="MOD2_KB" value="0"/>
  <PARAM id="MOD2_LEVEL" value="0.6"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="174.61"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.3"/>
  <PARAM id="ENV2_D" value="1.5"/>
  <PARAM id="ENV2_S" value="0.3"/>
  <PARAM id="ENV2_R" value="2.0"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.2"/>
  <PARAM id="ENV3_D" value="1.0"/>
  <PARAM id="ENV3_S" value="0.5"/>
  <PARAM id="ENV3_R" value="2.0"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0.2"/>
  <PARAM id="VEIN" value="0.3"/>
  <PARAM id="FLUX" value="0.4"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="1"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="1"/>
  <PARAM id="PENV_AMT" value="-12"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.5"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="4000"/>
  <PARAM id="FILT_RES" value="0.4"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.35"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="0"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0.4"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 13: crystal — cristallin, haut registre
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="24"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.3"/>
  <PARAM id="MOD1_COARSE" value="4"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.001"/>
  <PARAM id="ENV1_D" value="1.0"/>
  <PARAM id="ENV1_S" value="0.1"/>
  <PARAM id="ENV1_R" value="1.5"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="12"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.2"/>
  <PARAM id="MOD2_COARSE" value="2"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.001"/>
  <PARAM id="ENV2_D" value="0.8"/>
  <PARAM id="ENV2_S" value="0.1"/>
  <PARAM id="ENV2_R" value="1.0"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="2"/>
  <PARAM id="ENV3_A" value="0.001"/>
  <PARAM id="ENV3_D" value="1.5"/>
  <PARAM id="ENV3_S" value="0.0"/>
  <PARAM id="ENV3_R" value="1.0"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="4"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="18000"/>
  <PARAM id="FILT_RES" value="0.0"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.35"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0.15"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 14: chaos engine — saw + sync + XOR = chaos
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="1"/>
  <PARAM id="MOD1_PITCH" value="-7"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="1.0"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="-700"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.01"/>
  <PARAM id="ENV1_D" value="0.3"/>
  <PARAM id="ENV1_S" value="0.8"/>
  <PARAM id="ENV1_R" value="0.2"/>
  <PARAM id="MOD2_WAVE" value="2"/>
  <PARAM id="MOD2_PITCH" value="5"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.8"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="500"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.01"/>
  <PARAM id="ENV2_D" value="0.2"/>
  <PARAM id="ENV2_S" value="0.7"/>
  <PARAM id="ENV2_R" value="0.2"/>
  <PARAM id="CAR_WAVE" value="1"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.01"/>
  <PARAM id="ENV3_D" value="0.1"/>
  <PARAM id="ENV3_S" value="1.0"/>
  <PARAM id="ENV3_R" value="0.15"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0.1"/>
  <PARAM id="VEIN" value="0.2"/>
  <PARAM id="FLUX" value="0.3"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="1"/>
  <PARAM id="SYNC" value="1"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="4000"/>
  <PARAM id="FILT_RES" value="0.5"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.3"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 15: soft texture — texture douce, tous LFOs subtils
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="3"/>
  <PARAM id="MOD1_PITCH" value="12"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.3"/>
  <PARAM id="MOD1_COARSE" value="2"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.5"/>
  <PARAM id="ENV1_D" value="1.0"/>
  <PARAM id="ENV1_S" value="0.5"/>
  <PARAM id="ENV1_R" value="2.0"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="0"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.2"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.8"/>
  <PARAM id="ENV2_D" value="1.5"/>
  <PARAM id="ENV2_S" value="0.4"/>
  <PARAM id="ENV2_R" value="2.5"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="0"/>
  <PARAM id="ENV3_A" value="0.3"/>
  <PARAM id="ENV3_D" value="0.5"/>
  <PARAM id="ENV3_S" value="0.8"/>
  <PARAM id="ENV3_R" value="3.0"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="1"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0.2"/>
  <PARAM id="VEIN" value="0.3"/>
  <PARAM id="FLUX" value="0.15"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="0"/>
  <PARAM id="PENV_AMT" value="0"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.15"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.1"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="6000"/>
  <PARAM id="FILT_RES" value="0.2"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.5"/>
  <PARAM id="DRIVE" value="1.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="0"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0.25"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 16: microwave kick — hardcore sustained, pitch sweep + XOR + drive
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.95"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.001"/>
  <PARAM id="ENV1_D" value="0.15"/>
  <PARAM id="ENV1_S" value="0.6"/>
  <PARAM id="ENV1_R" value="0.1"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="7"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.7"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="700"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.001"/>
  <PARAM id="ENV2_D" value="0.1"/>
  <PARAM id="ENV2_S" value="0.4"/>
  <PARAM id="ENV2_R" value="0.08"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="-1"/>
  <PARAM id="ENV3_A" value="0.001"/>
  <PARAM id="ENV3_D" value="0.2"/>
  <PARAM id="ENV3_S" value="0.9"/>
  <PARAM id="ENV3_R" value="0.15"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="0"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="1"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="1"/>
  <PARAM id="PENV_AMT" value="48"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.04"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.05"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="1500"/>
  <PARAM id="FILT_RES" value="0.4"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.65"/>
  <PARAM id="DRIVE" value="6.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)",

        // 17: glide kick — portamento-style pitch drop kick
        R"(<VisceraState>
  <PARAM id="MOD1_WAVE" value="0"/>
  <PARAM id="MOD1_PITCH" value="0"/>
  <PARAM id="MOD1_KB" value="1"/>
  <PARAM id="MOD1_LEVEL" value="0.3"/>
  <PARAM id="MOD1_COARSE" value="1"/>
  <PARAM id="MOD1_FINE" value="0"/>
  <PARAM id="MOD1_FIXED_FREQ" value="440"/>
  <PARAM id="MOD1_MULTI" value="4"/>
  <PARAM id="ENV1_A" value="0.001"/>
  <PARAM id="ENV1_D" value="0.12"/>
  <PARAM id="ENV1_S" value="0.0"/>
  <PARAM id="ENV1_R" value="0.08"/>
  <PARAM id="MOD2_WAVE" value="0"/>
  <PARAM id="MOD2_PITCH" value="0"/>
  <PARAM id="MOD2_KB" value="1"/>
  <PARAM id="MOD2_LEVEL" value="0.15"/>
  <PARAM id="MOD2_COARSE" value="1"/>
  <PARAM id="MOD2_FINE" value="0"/>
  <PARAM id="MOD2_FIXED_FREQ" value="440"/>
  <PARAM id="MOD2_MULTI" value="4"/>
  <PARAM id="ENV2_A" value="0.001"/>
  <PARAM id="ENV2_D" value="0.18"/>
  <PARAM id="ENV2_S" value="0.0"/>
  <PARAM id="ENV2_R" value="0.1"/>
  <PARAM id="CAR_WAVE" value="0"/>
  <PARAM id="CAR_OCTAVE" value="-1"/>
  <PARAM id="ENV3_A" value="0.001"/>
  <PARAM id="ENV3_D" value="0.5"/>
  <PARAM id="ENV3_S" value="0.0"/>
  <PARAM id="ENV3_R" value="0.2"/>
  <PARAM id="CAR_DRIFT" value="0"/>
  <PARAM id="CAR_NOISE" value="0"/>
  <PARAM id="CAR_SPREAD" value="0"/>
  <PARAM id="CAR_COARSE" value="0"/>
  <PARAM id="CAR_FINE" value="0"/>
  <PARAM id="CAR_FIXED_FREQ" value="440"/>
  <PARAM id="CAR_KB" value="1"/>
  <PARAM id="TREMOR" value="0"/>
  <PARAM id="VEIN" value="0"/>
  <PARAM id="FLUX" value="0"/>
  <PARAM id="FM_ALGO" value="0"/>
  <PARAM id="XOR_ON" value="0"/>
  <PARAM id="SYNC" value="0"/>
  <PARAM id="PENV_ON" value="1"/>
  <PARAM id="PENV_AMT" value="36"/>
  <PARAM id="PENV_A" value="0.001"/>
  <PARAM id="PENV_D" value="0.12"/>
  <PARAM id="PENV_S" value="0"/>
  <PARAM id="PENV_R" value="0.08"/>
  <PARAM id="FILT_ON" value="1"/>
  <PARAM id="FILT_CUTOFF" value="3000"/>
  <PARAM id="FILT_RES" value="0.15"/>
  <PARAM id="FILT_TYPE" value="0"/>
  <PARAM id="DLY_ON" value="0"/>
  <PARAM id="DLY_TIME" value="0.3"/>
  <PARAM id="DLY_FEED" value="0.3"/>
  <PARAM id="DLY_DAMP" value="0.3"/>
  <PARAM id="DLY_MIX" value="0"/>
  <PARAM id="DLY_PING" value="0"/>
  <PARAM id="REV_ON" value="0"/>
  <PARAM id="REV_SIZE" value="0.3"/>
  <PARAM id="REV_DAMP" value="0.5"/>
  <PARAM id="REV_MIX" value="0"/>
  <PARAM id="VOLUME" value="0.6"/>
  <PARAM id="DRIVE" value="2.0"/>
  <PARAM id="MONO" value="1"/>
  <PARAM id="RETRIG" value="1"/>
  <PARAM id="SHAPER_ON" value="0"/>
  <PARAM id="SHAPER_SYNC" value="0"/>
  <PARAM id="SHAPER_RATE" value="4.0"/>
  <PARAM id="SHAPER_DEPTH" value="0.75"/>
  <PARAM id="DISP_AMT" value="0"/>
  <PARAM id="LIQ_ON" value="0"/>
  <PARAM id="LIQ_RATE" value="0.8"/>
  <PARAM id="LIQ_DEPTH" value="0.5"/>
  <PARAM id="LIQ_TONE" value="0.5"/>
  <PARAM id="LIQ_FEED" value="0.2"/>
  <PARAM id="LIQ_MIX" value="0.6"/>
  <PARAM id="RUB_ON" value="0"/>
  <PARAM id="RUB_TONE" value="0.5"/>
  <PARAM id="RUB_STRETCH" value="0.3"/>
  <PARAM id="RUB_WARP" value="0"/>
  <PARAM id="RUB_MIX" value="0.6"/>
</VisceraState>)"
    };

    if (index >= 0 && index < kNumPresets)
        return presets[index];
    return presets[0];
}

// --- Création de l'éditeur ---
juce::AudioProcessorEditor* VisceraProcessor::createEditor()
{
    return new VisceraEditor(*this);
}

// --- Point d'entrée JUCE ---
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VisceraProcessor();
}
