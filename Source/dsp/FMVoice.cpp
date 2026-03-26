// FMVoice.cpp — Implémentation de la voix FM
// Tout le signal path est ici : modulation, routing, effets, sortie
#include "FMVoice.h"
#include "FMSound.h"
#include <cmath>

namespace bb {

// Constante 2π utilisée partout dans le calcul FM/PM
static constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
// Index de modulation maximum (en radians) — 12 rad = gros son FM
static constexpr double kMaxModIndex = 12.0;
FMVoice::FMVoice(VoiceParams& p)
    : params(p)
{
}

bool FMVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<FMSound*>(sound) != nullptr;
}

void FMVoice::prepareToPlay(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;

    mod1Osc.prepare(sr);
    mod2Osc.prepare(sr);
    carrierOsc.prepare(sr);
    carrierOscR.prepare(sr);

    env1.prepare(sr);
    env2.prepare(sr);
    env3.prepare(sr);
    pitchEnv.prepare(sr);

    lfo1.prepare(sr);
    lfo2.prepare(sr);

    filterL.prepare(sr);
    filterR.prepare(sr);
    dcBlockerL.prepare(sr);
    dcBlockerR.prepare(sr);
    hemoFoldL.prepare(sr);
    hemoFoldR.prepare(sr);

    // LFO rates fixes
    lfo1.setRate(3.5f);  // LFO1 pour tremor (pitch) et flux (mod index)
    lfo1.setWaveType(LFOWaveType::Sine);
    lfo2.setRate(2.0f);  // LFO2 pour vein (filter)
    lfo2.setWaveType(LFOWaveType::Sine);

    // SmoothedValues : temps de lissage 20ms (anti-zipper noise)
    smoothVolume.reset(sr, 0.02);
    smoothCutoff.reset(sr, 0.02);
    smoothMod1Level.reset(sr, 0.02);
    smoothMod2Level.reset(sr, 0.02);
    smoothCarNoise.reset(sr, 0.02);
    smoothCarSpread.reset(sr, 0.02);
    smoothGLfoPitch.reset(sr, 0.005);  // 5ms smooth for global LFO pitch
    smoothGLfoCutoff.reset(sr, 0.005); // 5ms smooth for global LFO cutoff

    // Reset filter coefficient cache
    lastFilterCutoff = -1.0f;
    lastFilterRes    = -1.0f;

    // Anti-click fade: ~5ms
    stealFadeLength = std::max(1, static_cast<int>(sr * 0.005));
    stealFadeSamples = 0;

    // Anti-click fade-in: ~3ms
    noteFadeInLength = std::max(1, static_cast<int>(sr * 0.003));
    noteFadeInSamples = 0;
}

void FMVoice::startNote(int midiNoteNumber, float velocity,
                         juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    noteVelocity = velocity;
    params.lastVelocity.store(velocity, std::memory_order_relaxed);
    stealFadeSamples = 0;  // cancel any in-progress steal fade

    // Convertir note MIDI → fréquence : f = 440 × 2^((note-69)/12)
    // Global octave shift applied here (saved per preset, like Serum)
    int octaveShift = static_cast<int>(params.octave ? params.octave->load() : 0.0f);
    noteFreqHz = 440.0 * std::pow(2.0, (midiNoteNumber - 69 + octaveShift * 12) / 12.0);
    targetNoteFreq = noteFreqHz;

    // Portamento : glide en mono si porta > 0
    bool isMono = params.mono->load() > 0.5f;
    bool shouldRetrig = params.retrig->load() > 0.5f;
    float portaTime = params.porta ? params.porta->load() : 0.0f;
    portaTime = juce::jlimit(0.0f, 1.0f, portaTime
                + params.lfoModPorta.load(std::memory_order_relaxed));

    // Serum-style: portamento only in mono mode, always glides from last note
    float lastFreq = params.lastNoteFreqHz.load(std::memory_order_relaxed);
    if (!isMono || portaTime < 0.001f || lastFreq <= 0.0f)
        currentFreq = noteFreqHz;
    else
        currentFreq = static_cast<double>(lastFreq);
    params.lastNoteFreqHz.store(static_cast<float>(noteFreqHz), std::memory_order_relaxed);

    // Portamento: exponential smoothing with time in seconds.
    // portaTime 0-1 maps to 0-2 seconds glide time.
    if (portaTime > 0.001f)
    {
        double glideTimeSec = static_cast<double>(portaTime) * 2.0; // 0-1 → 0-2s
        portamentoRate = std::exp(-1.0 / (glideTimeSec * sampleRate));
    }
    else
    {
        portamentoRate = 0.0;
    }

    // Pitch wheel
    pitchWheelMoved(currentPitchWheelPosition);

    // Reset oscillator phases for clean attack (retrigger or poly mode)
    if (shouldRetrig || !isMono)
    {
        mod1Osc.resetPhase();
        mod2Osc.resetPhase();
        carrierOsc.resetPhase();
        carrierOscR.resetPhase();

        if (!env3.isActive())
        {
            // Voice was idle — hard-reset envelopes (no pop, voice is silent)
            env1.reset();
            env2.reset();
            env3.reset();
            pitchEnv.reset();
        }
        // If env3 IS active (voice stealing), don't reset — ADSR retriggers
        // smoothly from the current level, avoiding pops.
    }

    // Lancer les enveloppes
    env1.setParameters(params.env1A->load(), params.env1D->load(),
                       params.env1S->load(), params.env1R->load());
    env2.setParameters(params.env2A->load(), params.env2D->load(),
                       params.env2S->load(), params.env2R->load());
    env3.setParameters(params.env3A->load(), params.env3D->load(),
                       params.env3S->load(), params.env3R->load());

    // Pitch envelope
    pitchEnv.setParameters(params.pitchEnvA->load(), params.pitchEnvD->load(),
                           params.pitchEnvS->load(), params.pitchEnvR->load());

    mod2FeedbackSample = 0.0f;
    env1.noteOn();
    env2.noteOn();
    env3.noteOn();
    pitchEnv.noteOn();

    // Anti-click fade-in for the first samples of the new note
    noteFadeInSamples = noteFadeInLength;
}

void FMVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    // Operator-style noteOff: if sustain ≈ 0, skip noteOff and let the
    // decay run its natural course.  This prevents clicks when releasing
    // during the decay phase (kicks, plucks, percussion).
    // When sustain > 0, noteOff triggers the normal release phase.
    float env1Sus = params.env1S->load();
    float env2Sus = params.env2S->load();
    float env3Sus = params.env3S->load();

    if (env1Sus > 0.01f) env1.noteOff();
    if (env2Sus > 0.01f) env2.noteOff();
    if (env3Sus > 0.01f) env3.noteOff();
    // pitchEnv never responds to noteOff (always completes A→D→S)

    if (!allowTailOff)
    {
        // Voice stealing: short fade-out
        stealFadeSamples = stealFadeLength;
    }
}

void FMVoice::pitchWheelMoved(int newPitchWheelValue)
{
    // Pitch wheel : ±2 semitones (standard)
    pitchBendSemitones = (newPitchWheelValue - 8192) / 8192.0 * 2.0;
}

void FMVoice::controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/)
{
}

void FMVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                               int startSample, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;

    if (!env3.isActive())
    {
        clearCurrentNote();
        return;
    }

    // --- Lire les paramètres une fois par bloc ---
    // Macros (read first, used by mod levels below)
    float cortexP      = juce::jlimit(0.0f, 1.0f,
        (params.cortex ? params.cortex->load() : 0.5f)
        + params.lfoModCortex.load(std::memory_order_relaxed));
    float ichorP       = juce::jlimit(0.0f, 1.0f,
        (params.ichor ? params.ichor->load() : 0.0f)
        + params.lfoModIchor.load(std::memory_order_relaxed));
    float plasmaP      = juce::jlimit(0.0f, 1.0f,
        (params.plasma ? params.plasma->load() : 0.5f)
        + params.lfoModPlasma.load(std::memory_order_relaxed));
    // Exponential FM depth: 0→0.25×, 0.5→1× (neutral), 1→4× (±12dB range)
    float plasmaMul    = std::pow(4.0f, plasmaP * 2.0f - 1.0f);

    bool  mod1OnP        = !params.mod1On || params.mod1On->load() > 0.5f;
    auto mod1WaveIdx     = static_cast<int>(params.mod1Wave->load());
    bool  mod1KB         = params.mod1KB->load() > 0.5f;
    float mod1LevelP     = (mod1OnP ? params.mod1Level->load() : 0.0f) * plasmaMul;
    int   mod1CoarseIdx  = juce::jlimit(0, kMaxCoarseIdx,
        static_cast<int>(params.mod1Coarse->load()
            + params.lfoModMod1Coarse.load(std::memory_order_relaxed) * 24.0f));
    float mod1FineCents  = params.mod1Fine->load()
                           + params.lfoModMod1Fine.load(std::memory_order_relaxed) * 100.0f;
    float mod1FixedHz    = params.mod1FixedFreq->load();
    int   mod1MultiVal   = static_cast<int>(params.mod1Multi->load());

    bool  mod2OnP        = !params.mod2On || params.mod2On->load() > 0.5f;
    auto mod2WaveIdx     = static_cast<int>(params.mod2Wave->load());
    bool  mod2KB         = params.mod2KB->load() > 0.5f;
    float mod2LevelP     = (mod2OnP ? params.mod2Level->load() : 0.0f) * plasmaMul;
    int   mod2CoarseIdx  = juce::jlimit(0, kMaxCoarseIdx,
        static_cast<int>(params.mod2Coarse->load()
            + params.lfoModMod2Coarse.load(std::memory_order_relaxed) * 24.0f));
    float mod2FineCents  = params.mod2Fine->load()
                           + params.lfoModMod2Fine.load(std::memory_order_relaxed) * 100.0f;
    float mod2FixedHz    = params.mod2FixedFreq->load();
    int   mod2MultiVal   = static_cast<int>(params.mod2Multi->load());

    auto carWaveIdx      = static_cast<int>(params.carWave->load());
    int   carCoarseIdx   = params.carCoarse
        ? juce::jlimit(0, kMaxCoarseIdx,
            static_cast<int>(params.carCoarse->load()
                + params.lfoModCarCoarse.load(std::memory_order_relaxed) * 24.0f))
        : 1;
    float carFineCents   = (params.carFine ? params.carFine->load() : 0.0f)
                           + params.lfoModCarFine.load(std::memory_order_relaxed) * 100.0f;
    float carFixedHz     = params.carFixedFreq ? params.carFixedFreq->load() : 440.0f;
    int   carMultiVal    = params.carMulti ? static_cast<int>(params.carMulti->load()) : 4;
    bool  carKB          = params.carKB ? params.carKB->load() > 0.5f : true;
    float carNoiseP      = params.carNoise ? params.carNoise->load() : 0.0f;
    float carSpreadP     = params.carSpread ? params.carSpread->load() : 0.0f;

    float tremorAmount = juce::jlimit(0.0f, 1.0f, params.tremor->load()
                         + params.lfoModTremor.load(std::memory_order_relaxed));
    float veinAmount   = juce::jlimit(0.0f, 1.0f, params.vein->load()
                         + params.lfoModVein.load(std::memory_order_relaxed));
    float fluxAmount   = juce::jlimit(0.0f, 1.0f, params.flux->load()
                         + params.lfoModFlux.load(std::memory_order_relaxed));

    // Global LFO modulation sums (from PluginProcessor) — smoothed for pitch + cutoff
    smoothGLfoPitch.setTargetValue(params.lfoModPitch.load(std::memory_order_relaxed));
    smoothGLfoCutoff.setTargetValue(params.lfoModCutoff.load(std::memory_order_relaxed));
    float gLfoModRes     = params.lfoModRes.load(std::memory_order_relaxed);
    float gLfoModMod1Lvl = params.lfoModMod1Lvl.load(std::memory_order_relaxed);
    float gLfoModMod2Lvl = params.lfoModMod2Lvl.load(std::memory_order_relaxed);
    float gLfoModVolume  = params.lfoModVolume.load(std::memory_order_relaxed);
    float gLfoModDrive   = params.lfoModDrive.load(std::memory_order_relaxed);
    float gLfoModNoise   = params.lfoModNoise.load(std::memory_order_relaxed);
    float gLfoModSpread  = params.lfoModSpread.load(std::memory_order_relaxed);
    float gLfoModFold    = params.lfoModFold.load(std::memory_order_relaxed);

    bool xorEnabled    = params.xorOn->load() > 0.5f;
    bool syncEnabled   = params.syncOn->load() > 0.5f;
    int  fmAlgo        = static_cast<int>(params.fmAlgo->load());

    bool pitchEnvEnabled = params.pitchEnvOn->load() > 0.5f;
    float pitchEnvAmt  = pitchEnvEnabled
        ? juce::jlimit(-96.0f, 96.0f, params.pitchEnvAmt->load()
              + params.lfoModPEnvAmt.load(std::memory_order_relaxed) * 96.0f)
        : 0.0f;

    bool filtEnabled   = params.filtOn->load() > 0.5f;
    float cutoffBase   = params.filtCutoff->load();
    float resonance    = params.filtRes->load();
    auto filterMode    = static_cast<FilterMode>(juce::jlimit(0, 3, static_cast<int>(params.filtType->load())));
    float volumeParam  = params.volume->load();
    float driveParam   = params.drive->load();
    float dispAmount   = params.dispAmt->load();
    float driftParam   = juce::jlimit(0.0f, 1.0f,
                           (params.carDrift ? params.carDrift->load() : 0.0f)
                           + params.lfoModCarDrift.load(std::memory_order_relaxed));

    // Wire harmonic tables to oscillators (for Custom waveform)
    mod1Osc.setHarmonicTable(params.mod1Harmonics);
    mod2Osc.setHarmonicTable(params.mod2Harmonics);
    carrierOsc.setHarmonicTable(params.carHarmonics);
    carrierOscR.setHarmonicTable(params.carHarmonics);

    // Configurer les oscillateurs
    mod1Osc.setWaveType(static_cast<WaveType>(mod1WaveIdx));
    mod2Osc.setWaveType(static_cast<WaveType>(mod2WaveIdx));
    carrierOsc.setWaveType(static_cast<WaveType>(carWaveIdx));
    carrierOscR.setWaveType(static_cast<WaveType>(carWaveIdx));

    // SmoothedValues : targets pour ce bloc
    smoothVolume.setTargetValue(volumeParam);
    smoothCutoff.setTargetValue(cutoffBase);
    smoothMod1Level.setTargetValue(mod1LevelP);
    smoothMod2Level.setTargetValue(mod2LevelP);
    smoothCarNoise.setTargetValue(carNoiseP);
    smoothCarSpread.setTargetValue(carSpreadP);

    // Envelope time macro: 0.5 = 1x, 0 = 0.25x, 1 = 4x (exponential)
    float timeMul = std::pow(4.0f, params.macroTime->load() * 2.0f - 1.0f);

    // Mettre à jour les paramètres d'enveloppe (+ LFO modulation + time macro)
    env1.setParameters(
        std::max(0.0f, (params.env1A->load() + params.lfoModEnv1A.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        std::max(0.0f, (params.env1D->load() + params.lfoModEnv1D.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        juce::jlimit(0.0f, 1.0f, params.env1S->load() + params.lfoModEnv1S.load(std::memory_order_relaxed)),
        std::max(0.0f, (params.env1R->load() + params.lfoModEnv1R.load(std::memory_order_relaxed) * 8.0f) * timeMul));
    env2.setParameters(
        std::max(0.0f, (params.env2A->load() + params.lfoModEnv2A.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        std::max(0.0f, (params.env2D->load() + params.lfoModEnv2D.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        juce::jlimit(0.0f, 1.0f, params.env2S->load() + params.lfoModEnv2S.load(std::memory_order_relaxed)),
        std::max(0.0f, (params.env2R->load() + params.lfoModEnv2R.load(std::memory_order_relaxed) * 8.0f) * timeMul));
    env3.setParameters(
        std::max(0.0f, (params.env3A->load() + params.lfoModEnv3A.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        std::max(0.0f, (params.env3D->load() + params.lfoModEnv3D.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        juce::jlimit(0.0f, 1.0f, params.env3S->load() + params.lfoModEnv3S.load(std::memory_order_relaxed)),
        std::max(0.0f, (params.env3R->load() + params.lfoModEnv3R.load(std::memory_order_relaxed) * 8.0f) * timeMul));
    pitchEnv.setParameters(
        std::max(0.0f, (params.pitchEnvA->load() + params.lfoModPEnvA.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        std::max(0.0f, (params.pitchEnvD->load() + params.lfoModPEnvD.load(std::memory_order_relaxed) * 5.0f) * timeMul),
        juce::jlimit(0.0f, 1.0f, params.pitchEnvS->load() + params.lfoModPEnvS.load(std::memory_order_relaxed)),
        std::max(0.0f, (params.pitchEnvR->load() + params.lfoModPEnvR.load(std::memory_order_relaxed) * 8.0f) * timeMul));

    // HemoFold (wavefolder) + global LFO fold mod
    float foldAmt = juce::jlimit(0.0f, 1.0f, dispAmount + gLfoModFold);
    hemoFoldL.setAmount(foldAmt);
    hemoFoldR.setAmount(foldAmt);

    // XOR mask
    uint16_t xorMask = xorEnabled ? 0x5A5A : 0x0000;
    xorDist.setMask(xorMask);

    // Pre-compute block-rate ratios (saves 3× exp2 + 3× pow per sample)
    // For KB-track mode: ratio includes cortex, ichor and fine shift.
    // For fixed mode: ratio is fixedFreqHz × multiValue (absolute freq, not multiplied by baseFreq).
    auto precomputeRatio = [&](int coarseIdx, float fineCents, bool kbTrack,
                                float fixedFreqHz, int multi) -> double
    {
        if (kbTrack) {
            double fineShift = std::exp2(static_cast<double>(fineCents) / 1200.0);
            int idx = juce::jlimit(0, kMaxCoarseIdx, coarseIdx);
            double ratio = static_cast<double>(coarseRatio(idx));
            if (ratio > 0.0)
                ratio = std::pow(ratio, static_cast<double>(cortexP) * 2.0);
            ratio += static_cast<double>(ichorP) * 0.3 * ratio;
            return ratio * fineShift;
        } else {
            return static_cast<double>(fixedFreqHz) * static_cast<double>(multiValue(multi));
        }
    };
    double mod1Ratio = precomputeRatio(mod1CoarseIdx, mod1FineCents, mod1KB, mod1FixedHz, mod1MultiVal);
    double mod2Ratio = precomputeRatio(mod2CoarseIdx, mod2FineCents, mod2KB, mod2FixedHz, mod2MultiVal);
    double carRatio  = precomputeRatio(carCoarseIdx,  carFineCents,  carKB,  carFixedHz,  carMultiVal);

    // Detuning: linear approximation of exp2(x) for |x| < 0.013 (max error < 0.01%)
    constexpr double kDetuneScale = 15.0 / 1200.0 * 0.693147180559945; // 15 cents × ln(2)

    // --- Boucle par échantillon ---
    for (int i = 0; i < numSamples; ++i)
    {
        // Portamento
        if (portamentoRate > 0.0)
            currentFreq += (targetNoteFreq - currentFreq) * (1.0 - portamentoRate);
        else
            currentFreq = targetNoteFreq;

        // LFO ticks (free-running)
        float lfo1Val = lfo1.tick(); // pour tremor (pitch) et flux (mod index)
        float lfo2Val = lfo2.tick(); // pour vein (filter)

        // Pitch envelope : amount × env value (en demi-tons)
        float pitchEnvVal = pitchEnv.tick();
        double pitchEnvSemitones = pitchEnvEnabled
            ? static_cast<double>(pitchEnvAmt * pitchEnvVal) : 0.0;

        // Pitch modulation via LFO "tremor" : ±2 semitones max + global LFO pitch (smoothed)
        float gLfoPitchSmoothed = smoothGLfoPitch.getNextValue();
        double pitchModSemitones = static_cast<double>(lfo1Val * tremorAmount) * 2.0
                                   + static_cast<double>(gLfoPitchSmoothed) * 2.0
                                   + pitchBendSemitones + pitchEnvSemitones;
        pitchModSemitones = juce::jlimit(-48.0, 48.0, pitchModSemitones);
        double pitchMod = std::exp2(pitchModSemitones / 12.0);

        double baseFreq = currentFreq * pitchMod;

        // Modulation index modulation via LFO "flux"
        float fluxMod = 1.0f + fluxAmount * lfo1Val;

        // Smooth parameters + apply global LFO modulations
        float vol      = juce::jlimit(0.0f, 1.0f, smoothVolume.getNextValue() + gLfoModVolume);
        float cutoff   = smoothCutoff.getNextValue();
        float m1Level  = std::max(0.0f, smoothMod1Level.getNextValue() + gLfoModMod1Lvl);
        float m2Level  = std::max(0.0f, smoothMod2Level.getNextValue() + gLfoModMod2Lvl);

        // --- Modulateur 1 --- (use pre-computed ratio: baseFreq × ratio or absolute)
        double mod1Freq = mod1KB ? baseFreq * mod1Ratio : mod1Ratio;
        mod1Osc.setFrequency(mod1Freq);
        float mod1Out = mod1Osc.tick();
        float env1Val = env1.tick();
        double mod1Signal = static_cast<double>(mod1Out * env1Val * m1Level * fluxMod)
                            * kMaxModIndex;

        // --- Modulateur 2 ---
        double mod2Freq = mod2KB ? baseFreq * mod2Ratio : mod2Ratio;
        mod2Osc.setFrequency(mod2Freq);

        double phaseMod = 0.0;
        float mixMod1Audio = 0.0f, mixMod2Audio = 0.0f;

        switch (fmAlgo)
        {
            case 0: // Series: Mod1 → Mod2 → Carrier
            {
                float mod2Out = mod2Osc.tick(mod1Signal);
                float env2Val = env2.tick();
                phaseMod = static_cast<double>(mod2Out * env2Val * m2Level * fluxMod)
                           * kMaxModIndex;
                break;
            }
            case 1: // Parallel: Mod1 → Carrier, Mod2 → Carrier
            {
                float mod2Out = mod2Osc.tick();
                float env2Val = env2.tick();
                double mod2Signal = static_cast<double>(mod2Out * env2Val * m2Level * fluxMod)
                                    * kMaxModIndex;
                phaseMod = mod1Signal + mod2Signal;
                break;
            }
            case 2: // Stack: Mod1 → Mod2 → Carrier + Mod1 → Carrier
            {
                float mod2Out = mod2Osc.tick(mod1Signal);
                float env2Val = env2.tick();
                double mod2Signal = static_cast<double>(mod2Out * env2Val * m2Level * fluxMod)
                                    * kMaxModIndex;
                phaseMod = mod1Signal + mod2Signal;
                break;
            }
            case 3: // Ring: Mod1 × Mod2 → Carrier
            {
                float mod2Out = mod2Osc.tick();
                float env2Val = env2.tick();
                float ringOut = mod1Out * env1Val * mod2Out * env2Val;
                phaseMod = static_cast<double>(ringOut * m1Level * m2Level * fluxMod)
                           * kMaxModIndex;
                break;
            }
            case 4: // Feedback: Mod1 → Mod2 → Carrier, Mod2 self-modulates
            {
                double fbSignal = static_cast<double>(mod2FeedbackSample)
                                  * kMaxModIndex * 0.5;
                float mod2Out = mod2Osc.tick(mod1Signal + fbSignal);
                float env2Val = env2.tick();
                mod2FeedbackSample = mod2Out * env2Val;
                phaseMod = static_cast<double>(mod2FeedbackSample * m2Level * fluxMod)
                           * kMaxModIndex;
                break;
            }
            case 5: // Mix: all 3 oscillators output independently, summed
            {
                float mod2Out = mod2Osc.tick();
                float env2Val = env2.tick();
                mixMod1Audio = mod1Out * env1Val * m1Level;
                mixMod2Audio = mod2Out * env2Val * m2Level;
                phaseMod = 0.0;
                break;
            }
            default: // fallback to series
            {
                float mod2Out = mod2Osc.tick(mod1Signal);
                float env2Val = env2.tick();
                phaseMod = static_cast<double>(mod2Out * env2Val * m2Level * fluxMod)
                           * kMaxModIndex;
                break;
            }
        }

        // --- Carrier --- (use pre-computed ratio)
        double carrierFreq = carKB ? baseFreq * carRatio : carRatio;
        carrierOsc.setFrequency(carrierFreq);
        carrierOsc.setDrift(driftParam);

        // Stereo spread: detune R carrier by up to ±15 cents (+ global LFO)
        // Linear approximation of exp2(x) for small x: 1 + x * ln(2)
        float spread = juce::jlimit(0.0f, 1.0f, smoothCarSpread.getNextValue() + gLfoModSpread);
        double detuneR = 1.0 + static_cast<double>(spread) * kDetuneScale;
        carrierOscR.setFrequency(carrierFreq * detuneR);
        carrierOscR.setDrift(driftParam);

        // Hard sync
        if (syncEnabled && mod1Osc.hasSyncPulse())
        {
            carrierOsc.hardSyncReset(mod1Osc.getSyncFraction());
            carrierOscR.hardSyncReset(mod1Osc.getSyncFraction());
        }

        float carrierOutL = carrierOsc.tick(phaseMod);
        float carrierOutR = carrierOscR.tick(phaseMod);
        float env3Val = env3.tick();

        // --- Carrier noise mix (+ global LFO) ---
        float noiseMix = juce::jlimit(0.0f, 1.0f, smoothCarNoise.getNextValue() + gLfoModNoise);
        float outputL, outputR;
        float velGain = params.velSwap.load(std::memory_order_relaxed) ? 1.0f : noteVelocity;
        if (noiseMix > 0.0001f)
        {
            // xorshift32 white noise: decorrelated L/R (independent seeds)
            noiseSeedL ^= noiseSeedL << 13;
            noiseSeedL ^= noiseSeedL >> 17;
            noiseSeedL ^= noiseSeedL << 5;
            float noiseL = static_cast<float>(static_cast<int32_t>(noiseSeedL))
                           / 2147483648.0f;
            noiseSeedR ^= noiseSeedR << 13;
            noiseSeedR ^= noiseSeedR >> 17;
            noiseSeedR ^= noiseSeedR << 5;
            float noiseR = static_cast<float>(static_cast<int32_t>(noiseSeedR))
                           / 2147483648.0f;
            outputL = (carrierOutL * (1.0f - noiseMix) + noiseL * noiseMix)
                      * env3Val * velGain;
            outputR = (carrierOutR * (1.0f - noiseMix) + noiseR * noiseMix)
                      * env3Val * velGain;
        }
        else
        {
            outputL = carrierOutL * env3Val * velGain;
            outputR = carrierOutR * env3Val * velGain;
        }

        // Mix algo: add mod oscillators as audio (each with their own envelope)
        if (fmAlgo == 5)
        {
            float modAudio = (mixMod1Audio + mixMod2Audio) * velGain;
            outputL += modAudio;
            outputR += modAudio;
        }

        // --- XOR distortion ---
        if (xorEnabled)
        {
            outputL = xorDist.process(outputL);
            outputR = xorDist.process(outputR);
        }

        // --- Filtre SVF ---
        if (filtEnabled)
        {
            // Vein modulation: multiplicative ±2 octaves
            float veinMod = (veinAmount > 0.001f) ? std::exp2f(veinAmount * lfo2Val * 2.0f) : 1.0f;
            // Global LFO: additive in normalized knob space (skew=0.23, centre=1kHz, Serum/Vital style)
            // Forward: norm = ((hz-20)/19980)^skew  |  Inverse: hz = 20 + 19980 * norm^(1/skew)
            constexpr float kCutSkew = 0.2299f;
            constexpr float kCutInvSkew = 1.0f / kCutSkew; // ~4.35
            float gLfoCutSmoothed = smoothGLfoCutoff.getNextValue();
            float cutLin = juce::jlimit(0.0f, 1.0f, (cutoff - 20.0f) / 19980.0f);
            float cutNorm = std::pow(cutLin, kCutSkew);
            cutNorm = juce::jlimit(0.0f, 1.0f, cutNorm + gLfoCutSmoothed);
            float modulatedCutoff = (20.0f + 19980.0f * std::pow(cutNorm, kCutInvSkew)) * veinMod;
            modulatedCutoff = juce::jlimit(20.0f, 20000.0f, modulatedCutoff);
            float modulatedRes = juce::jlimit(0.0f, 1.0f, resonance + gLfoModRes);
            // Only recalculate filter coefficients when parameters changed audibly
            if (std::abs(modulatedCutoff - lastFilterCutoff) > 0.5f
                || std::abs(modulatedRes - lastFilterRes) > 0.001f)
            {
                filterL.setParameters(modulatedCutoff, modulatedRes);
                filterR.setParameters(modulatedCutoff, modulatedRes);
                lastFilterCutoff = modulatedCutoff;
                lastFilterRes    = modulatedRes;
            }
            outputL = filterL.tick(outputL, filterMode);
            outputR = filterR.tick(outputR, filterMode);
        }

        // --- DC Blocker ---
        outputL = dcBlockerL.tick(outputL);
        outputR = dcBlockerR.tick(outputR);

        // --- HemoFold (wavefolder) ---
        outputL = hemoFoldL.tick(outputL);
        outputR = hemoFoldR.tick(outputR);

        // --- Volume + drive saturation + soft clipper ---
        float drv = juce::jlimit(1.0f, 10.0f, driveParam + gLfoModDrive * 9.0f);
        outputL *= vol;  outputL *= drv;  outputL = std::tanh(outputL);
        outputR *= vol;  outputR *= drv;  outputR = std::tanh(outputR);

        // --- Anti-click fade-in for new notes ---
        if (noteFadeInSamples > 0)
        {
            float fadeGain = 1.0f - static_cast<float>(noteFadeInSamples) / static_cast<float>(noteFadeInLength);
            outputL *= fadeGain;
            outputR *= fadeGain;
            --noteFadeInSamples;
        }

        // --- Anti-click fade-out for voice stealing ---
        if (stealFadeSamples > 0)
        {
            float fadeGain = static_cast<float>(stealFadeSamples) / static_cast<float>(stealFadeLength);
            outputL *= fadeGain;
            outputR *= fadeGain;
            --stealFadeSamples;
            if (stealFadeSamples == 0)
            {
                env1.reset();
                env2.reset();
                env3.reset();
                pitchEnv.reset();
                clearCurrentNote();
                return;
            }
        }

        // --- NaN/Inf guard ---
        if (!std::isfinite(outputL)) outputL = 0.0f;
        if (!std::isfinite(outputR)) outputR = 0.0f;

        // --- Écrire dans le buffer de sortie (true stereo) ---
        if (outputBuffer.getNumChannels() >= 2)
        {
            outputBuffer.addSample(0, startSample + i, outputL);
            outputBuffer.addSample(1, startSample + i, outputR);
        }
        else
        {
            outputBuffer.addSample(0, startSample + i, outputL);
        }
    }

    if (!env3.isActive())
        clearCurrentNote();
}

} // namespace bb
