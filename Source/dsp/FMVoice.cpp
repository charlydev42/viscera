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
// Middle C en MIDI (pour KB tracking off)
static constexpr double kMiddleCFreq = 261.6255653;

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
}

void FMVoice::startNote(int midiNoteNumber, float velocity,
                         juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    currentNote = midiNoteNumber;
    noteVelocity = velocity;

    // Convertir note MIDI → fréquence : f = 440 × 2^((note-69)/12)
    noteFreqHz = 440.0 * std::pow(2.0, (midiNoteNumber - 69) / 12.0);
    targetNoteFreq = noteFreqHz;

    // Portamento : glide en mono si porta > 0
    bool isMono = params.mono->load() > 0.5f;
    bool shouldRetrig = params.retrig->load() > 0.5f;
    float portaTime = params.porta ? params.porta->load() : 0.0f;

    if (portaTime < 0.001f || currentFreq <= 0.0)
        currentFreq = noteFreqHz;

    // portamentoRate: 0 = instant, ~1 = slow glide. Map porta 0-1 to rate.
    portamentoRate = (portaTime > 0.001f)
        ? std::pow(0.999, 1.0 / (1.0 + portaTime * 200.0)) : 0.0;

    // Pitch wheel
    pitchWheelMoved(currentPitchWheelPosition);

    // Reset des oscillateurs si retrigger activé
    if (shouldRetrig || !isMono)
    {
        mod1Osc.resetPhase();
        mod2Osc.resetPhase();
        carrierOsc.resetPhase();
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

    env1.noteOn();
    env2.noteOn();
    env3.noteOn();
    pitchEnv.noteOn();
}

void FMVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        env1.noteOff();
        env2.noteOff();
        env3.noteOff();
        pitchEnv.noteOff();
    }
    else
    {
        env1.reset();
        env2.reset();
        env3.reset();
        pitchEnv.reset();
        clearCurrentNote();
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

double FMVoice::calcModFreq(double baseFreq, int coarseIdx, float fineCents,
                             float fixedFreqHz, int multi, bool kbTrack) const
{
    if (kbTrack)
    {
        // Ratio mode: freq = baseFreq × coarseRatio(idx) × 2^(fineCents/1200)
        double fineShift = std::pow(2.0, static_cast<double>(fineCents) / 1200.0);
        int idx = juce::jlimit(0, kMaxCoarseIdx, coarseIdx);
        double ratio = static_cast<double>(coarseRatio(idx));
        return baseFreq * ratio * fineShift;
    }
    else
    {
        // Fixed mode: freq = fixedFreqHz × multiValue(idx) — no fine cents
        float mv = multiValue(multi);
        return static_cast<double>(fixedFreqHz) * static_cast<double>(mv);
    }
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
    bool  mod1OnP        = !params.mod1On || params.mod1On->load() > 0.5f;
    auto mod1WaveIdx     = static_cast<int>(params.mod1Wave->load());
    bool  mod1KB         = params.mod1KB->load() > 0.5f;
    float mod1LevelP     = mod1OnP ? params.mod1Level->load() : 0.0f;
    int   mod1CoarseIdx  = static_cast<int>(params.mod1Coarse->load());
    float mod1FineCents  = params.mod1Fine->load()
                           + params.lfoModMod1Fine.load(std::memory_order_relaxed) * 100.0f;
    float mod1FixedHz    = params.mod1FixedFreq->load();
    int   mod1MultiVal   = static_cast<int>(params.mod1Multi->load());

    bool  mod2OnP        = !params.mod2On || params.mod2On->load() > 0.5f;
    auto mod2WaveIdx     = static_cast<int>(params.mod2Wave->load());
    bool  mod2KB         = params.mod2KB->load() > 0.5f;
    float mod2LevelP     = mod2OnP ? params.mod2Level->load() : 0.0f;
    int   mod2CoarseIdx  = static_cast<int>(params.mod2Coarse->load());
    float mod2FineCents  = params.mod2Fine->load()
                           + params.lfoModMod2Fine.load(std::memory_order_relaxed) * 100.0f;
    float mod2FixedHz    = params.mod2FixedFreq->load();
    int   mod2MultiVal   = static_cast<int>(params.mod2Multi->load());

    auto carWaveIdx      = static_cast<int>(params.carWave->load());
    int   carCoarseIdx   = params.carCoarse ? static_cast<int>(params.carCoarse->load()) : 1;
    float carFineCents   = (params.carFine ? params.carFine->load() : 0.0f)
                           + params.lfoModCarFine.load(std::memory_order_relaxed) * 100.0f;
    float carFixedHz     = params.carFixedFreq ? params.carFixedFreq->load() : 440.0f;
    bool  carKB          = params.carKB ? params.carKB->load() > 0.5f : true;
    float carNoiseP      = params.carNoise ? params.carNoise->load() : 0.0f;
    float carSpreadP     = params.carSpread ? params.carSpread->load() : 0.0f;

    float tremorAmount = params.tremor->load();  // LFO → pitch (0-1)
    float veinAmount   = params.vein->load();    // LFO → filter cutoff (0-1)
    float fluxAmount   = params.flux->load();    // LFO → mod index (0-1)

    // Global LFO modulation sums (from PluginProcessor)
    float gLfoModPitch   = params.lfoModPitch.load(std::memory_order_relaxed);
    float gLfoModCutoff  = params.lfoModCutoff.load(std::memory_order_relaxed);
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
        ? juce::jlimit(-48.0f, 48.0f, params.pitchEnvAmt->load()
              + params.lfoModPEnvAmt.load(std::memory_order_relaxed) * 48.0f)
        : 0.0f;

    bool filtEnabled   = params.filtOn->load() > 0.5f;
    float cutoffBase   = params.filtCutoff->load();
    float resonance    = params.filtRes->load();
    auto filterMode    = static_cast<FilterMode>(static_cast<int>(params.filtType->load()));
    float volumeParam  = params.volume->load();
    float driveParam   = params.drive->load();
    float dispAmount   = params.dispAmt->load();
    float driftParam   = juce::jlimit(0.0f, 1.0f,
                           (params.carDrift ? params.carDrift->load() : 0.0f)
                           + params.lfoModCarDrift.load(std::memory_order_relaxed));

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

    // Mettre à jour les paramètres d'enveloppe
    env1.setParameters(params.env1A->load(), params.env1D->load(),
                       params.env1S->load(), params.env1R->load());
    env2.setParameters(params.env2A->load(), params.env2D->load(),
                       params.env2S->load(), params.env2R->load());
    env3.setParameters(params.env3A->load(), params.env3D->load(),
                       params.env3S->load(), params.env3R->load());
    pitchEnv.setParameters(params.pitchEnvA->load(), params.pitchEnvD->load(),
                           params.pitchEnvS->load(), params.pitchEnvR->load());

    // HemoFold (wavefolder) + global LFO fold mod
    float foldAmt = juce::jlimit(0.0f, 1.0f, dispAmount + gLfoModFold);
    hemoFoldL.setAmount(foldAmt);
    hemoFoldR.setAmount(foldAmt);

    // XOR mask
    uint16_t xorMask = xorEnabled ? 0x5A5A : 0x0000;
    xorDist.setMask(xorMask);

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

        // Pitch modulation via LFO "tremor" : ±2 semitones max + global LFO pitch
        double pitchModSemitones = static_cast<double>(lfo1Val * tremorAmount) * 2.0
                                   + static_cast<double>(gLfoModPitch) * 2.0
                                   + pitchBendSemitones + pitchEnvSemitones;
        double pitchMod = std::pow(2.0, pitchModSemitones / 12.0);

        double baseFreq = currentFreq * pitchMod;

        // Modulation index modulation via LFO "flux"
        float fluxMod = 1.0f + fluxAmount * lfo1Val;

        // Smooth parameters + apply global LFO modulations
        float vol      = juce::jlimit(0.0f, 1.0f, smoothVolume.getNextValue() + gLfoModVolume);
        float cutoff   = smoothCutoff.getNextValue();
        float m1Level  = juce::jlimit(0.0f, 1.0f, smoothMod1Level.getNextValue() + gLfoModMod1Lvl);
        float m2Level  = juce::jlimit(0.0f, 1.0f, smoothMod2Level.getNextValue() + gLfoModMod2Lvl);

        // --- Modulateur 1 ---
        double mod1Freq = calcModFreq(baseFreq, mod1CoarseIdx, mod1FineCents,
                                       mod1FixedHz, mod1MultiVal, mod1KB);
        mod1Osc.setFrequency(mod1Freq);
        float mod1Out = mod1Osc.tick();
        float env1Val = env1.tick();
        double mod1Signal = static_cast<double>(mod1Out * env1Val * m1Level * fluxMod)
                            * kMaxModIndex;

        // --- Modulateur 2 ---
        double mod2Freq = calcModFreq(baseFreq, mod2CoarseIdx, mod2FineCents,
                                       mod2FixedHz, mod2MultiVal, mod2KB);
        mod2Osc.setFrequency(mod2Freq);

        double phaseMod = 0.0;

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
                double fbSignal = static_cast<double>(mod2FeedbackSample * m2Level * fluxMod)
                                  * kMaxModIndex * 0.5;
                float mod2Out = mod2Osc.tick(mod1Signal + fbSignal);
                float env2Val = env2.tick();
                mod2FeedbackSample = mod2Out * env2Val;
                phaseMod = static_cast<double>(mod2FeedbackSample * m2Level * fluxMod)
                           * kMaxModIndex;
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

        // --- Carrier ---
        double carrierFreq = calcModFreq(baseFreq, carCoarseIdx, carFineCents,
                                          carFixedHz, 1, carKB);
        carrierOsc.setFrequency(carrierFreq);
        carrierOsc.setDrift(driftParam);

        // Stereo spread: detune R carrier by up to ±15 cents (+ global LFO)
        float spread = juce::jlimit(0.0f, 1.0f, smoothCarSpread.getNextValue() + gLfoModSpread);
        double detuneR = std::pow(2.0, static_cast<double>(spread) * 15.0 / 1200.0);
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
        if (noiseMix > 0.0001f)
        {
            // xorshift32 white noise: [-1, +1]
            noiseSeed ^= noiseSeed << 13;
            noiseSeed ^= noiseSeed >> 17;
            noiseSeed ^= noiseSeed << 5;
            float whiteNoise = static_cast<float>(static_cast<int32_t>(noiseSeed))
                               / static_cast<float>(INT32_MAX);
            outputL = (carrierOutL * (1.0f - noiseMix) + whiteNoise * noiseMix)
                      * env3Val * noteVelocity;
            outputR = (carrierOutR * (1.0f - noiseMix) + whiteNoise * noiseMix)
                      * env3Val * noteVelocity;
        }
        else
        {
            outputL = carrierOutL * env3Val * noteVelocity;
            outputR = carrierOutR * env3Val * noteVelocity;
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
            // Modulation du cutoff par LFO "vein" + global LFO : ±2 octaves
            float veinMod = std::pow(2.0f, veinAmount * lfo2Val * 2.0f);
            float gLfoCutoffMod = std::pow(2.0f, gLfoModCutoff * 2.0f);
            float modulatedCutoff = cutoff * veinMod * gLfoCutoffMod;
            modulatedCutoff = std::max(20.0f, std::min(modulatedCutoff, 20000.0f));
            float modulatedRes = juce::jlimit(0.0f, 1.0f, resonance + gLfoModRes * 0.5f);
            filterL.setParameters(modulatedCutoff, modulatedRes);
            filterR.setParameters(modulatedCutoff, modulatedRes);
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
