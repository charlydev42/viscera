// FMVoice.h — Voix FM complète : 2 modulateurs + 1 carrier + effets
// Chaque voix contient tout le signal path d'une note :
// Mod1/Mod2 → (routing série) → Carrier → XOR → Filter → DC Block → HemoFold → Output
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "Oscillator.h"
#include "ADSREnvelope.h"
#include "LFO.h"
#include "SVFilter.h"
#include "XORDistortion.h"
#include "DCBlocker.h"
#include "HemoFold.h"

namespace bb {

// LFO destination enum for assignable global LFOs
enum class LFODest : int {
    None = 0,
    Pitch,         // vibrato (±2 semitones)
    FilterCutoff,  // ±2 octaves
    FilterRes,     // ±resonance
    Mod1Level,     // ±mod index 1
    Mod2Level,     // ±mod index 2
    Volume,        // tremolo
    Drive,         // saturation
    CarNoise,      // noise mix
    CarSpread,     // stereo width
    FoldAmt,       // wavefolder
    Mod1Fine,      // ±mod1 fine tune (cents)
    Mod2Fine,      // ±mod2 fine tune (cents)
    CarDrift,      // ±carrier drift amount
    CarFine,       // ±carrier fine tune (cents)
    DlyTime,       // ±delay time
    DlyFeed,       // ±delay feedback
    DlyMix,        // ±delay mix
    RevSize,       // ±reverb size
    RevMix,        // ±reverb mix
    LiqDepth,      // ±liquid depth
    LiqMix,        // ±liquid mix
    RubWarp,       // ±rubber warp
    RubMix,        // ±rubber mix
    PEnvAmt,       // ±pitch env amount
    Count
};

// Operator-style coarse ratio: index 0 = 0.5x, index 1-48 = 1x..48x
static constexpr int kMaxCoarseIdx = 48;
inline float coarseRatio(int idx) { return (idx == 0) ? 0.5f : static_cast<float>(idx); }

// Operator-style fixed-mode multi values (decades): index 0-5
static constexpr float kMultiValues[] = { 0.0f, 0.001f, 0.01f, 0.1f, 1.0f, 10.0f };
static constexpr int kNumMultiValues = 6;
inline float multiValue(int idx) {
    return kMultiValues[juce::jlimit(0, kNumMultiValues - 1, idx)];
}

// Structure pour cacher les pointeurs atomiques vers les paramètres APVTS
// On les lit une fois par bloc dans renderNextBlock (pas de hash lookup)
struct VoiceParams
{
    std::atomic<float>* mod1On        = nullptr;
    std::atomic<float>* mod1Wave      = nullptr;
    std::atomic<float>* mod1Pitch     = nullptr; // legacy, kept for backward-compat
    std::atomic<float>* mod1KB        = nullptr;
    std::atomic<float>* mod1Level     = nullptr;
    std::atomic<float>* mod1Coarse    = nullptr;
    std::atomic<float>* mod1Fine      = nullptr;
    std::atomic<float>* mod1FixedFreq = nullptr;
    std::atomic<float>* mod1Multi     = nullptr;
    std::atomic<float>* env1A         = nullptr;
    std::atomic<float>* env1D         = nullptr;
    std::atomic<float>* env1S         = nullptr;
    std::atomic<float>* env1R         = nullptr;

    std::atomic<float>* mod2On        = nullptr;
    std::atomic<float>* mod2Wave      = nullptr;
    std::atomic<float>* mod2Pitch     = nullptr; // legacy, kept for backward-compat
    std::atomic<float>* mod2KB        = nullptr;
    std::atomic<float>* mod2Level     = nullptr;
    std::atomic<float>* mod2Coarse    = nullptr;
    std::atomic<float>* mod2Fine      = nullptr;
    std::atomic<float>* mod2FixedFreq = nullptr;
    std::atomic<float>* mod2Multi     = nullptr;
    std::atomic<float>* env2A         = nullptr;
    std::atomic<float>* env2D         = nullptr;
    std::atomic<float>* env2S         = nullptr;
    std::atomic<float>* env2R         = nullptr;

    std::atomic<float>* carWave      = nullptr;
    std::atomic<float>* carOctave   = nullptr; // legacy, kept for backward-compat
    std::atomic<float>* carCoarse   = nullptr;
    std::atomic<float>* carFine     = nullptr;
    std::atomic<float>* carFixedFreq = nullptr;
    std::atomic<float>* carKB       = nullptr;
    std::atomic<float>* carNoise    = nullptr;
    std::atomic<float>* carSpread   = nullptr;
    std::atomic<float>* env3A       = nullptr;
    std::atomic<float>* env3D      = nullptr;
    std::atomic<float>* env3S      = nullptr;
    std::atomic<float>* env3R      = nullptr;

    std::atomic<float>* tremor     = nullptr; // LFO → pitch (vibrato)
    std::atomic<float>* vein       = nullptr; // LFO → filter cutoff
    std::atomic<float>* flux       = nullptr; // LFO → mod index

    std::atomic<float>* xorOn      = nullptr;
    std::atomic<float>* syncOn     = nullptr;
    std::atomic<float>* fmAlgo    = nullptr; // 0=Series, 1=Parallel, 2=Stack

    // Pitch Envelope
    std::atomic<float>* pitchEnvOn  = nullptr; // on/off toggle
    std::atomic<float>* pitchEnvAmt = nullptr; // amount en demi-tons (±48)
    std::atomic<float>* pitchEnvA   = nullptr;
    std::atomic<float>* pitchEnvD   = nullptr;
    std::atomic<float>* pitchEnvS   = nullptr;
    std::atomic<float>* pitchEnvR   = nullptr;

    std::atomic<float>* filtOn     = nullptr;
    std::atomic<float>* filtCutoff = nullptr;
    std::atomic<float>* filtRes    = nullptr;
    std::atomic<float>* filtType   = nullptr;

    std::atomic<float>* volume     = nullptr;
    std::atomic<float>* drive      = nullptr; // Saturation drive (1-10)
    std::atomic<float>* mono       = nullptr;
    std::atomic<float>* retrig     = nullptr;
    std::atomic<float>* porta      = nullptr; // Portamento time (0-1)
    std::atomic<float>* dispAmt    = nullptr; // Disperser amount
    std::atomic<float>* carDrift   = nullptr; // Carrier analog drift

    // Global LFO modulation sums (written by processor, read by voice)
    std::atomic<float> lfoModPitch   { 0.0f };
    std::atomic<float> lfoModCutoff  { 0.0f };
    std::atomic<float> lfoModRes     { 0.0f };
    std::atomic<float> lfoModMod1Lvl { 0.0f };
    std::atomic<float> lfoModMod2Lvl { 0.0f };
    std::atomic<float> lfoModVolume  { 0.0f };
    std::atomic<float> lfoModDrive   { 0.0f };
    std::atomic<float> lfoModNoise   { 0.0f };
    std::atomic<float> lfoModSpread  { 0.0f };
    std::atomic<float> lfoModFold    { 0.0f };
    std::atomic<float> lfoModMod1Fine { 0.0f };
    std::atomic<float> lfoModMod2Fine { 0.0f };
    std::atomic<float> lfoModCarDrift { 0.0f };
    std::atomic<float> lfoModCarFine  { 0.0f };
    std::atomic<float> lfoModDlyTime  { 0.0f };
    std::atomic<float> lfoModDlyFeed  { 0.0f };
    std::atomic<float> lfoModDlyMix   { 0.0f };
    std::atomic<float> lfoModRevSize  { 0.0f };
    std::atomic<float> lfoModRevMix   { 0.0f };
    std::atomic<float> lfoModLiqDepth { 0.0f };
    std::atomic<float> lfoModLiqMix   { 0.0f };
    std::atomic<float> lfoModRubWarp  { 0.0f };
    std::atomic<float> lfoModRubMix   { 0.0f };
    std::atomic<float> lfoModPEnvAmt  { 0.0f };

    // Per-LFO unipolar peak (for arc scaling in GUI)
    std::atomic<float> lfoPeak[3]    { {1.0f}, {1.0f}, {1.0f} };
};

class FMVoice : public juce::SynthesiserVoice
{
public:
    FMVoice(VoiceParams& p);

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);

private:
    VoiceParams& params;

    // Oscillateurs
    Oscillator mod1Osc, mod2Osc, carrierOsc, carrierOscR;
    float mod2FeedbackSample = 0.0f;

    // Enveloppes (une par oscillateur + une pour le pitch)
    ADSREnvelope env1, env2, env3;
    ADSREnvelope pitchEnv;

    // LFOs (free-running, par voix)
    LFO lfo1, lfo2;

    // Effets (L+R for stereo spread)
    SVFilter filterL, filterR;
    XORDistortion xorDist;
    DCBlocker dcBlockerL, dcBlockerR;
    HemoFold hemoFoldL, hemoFoldR;

    // État de la note en cours
    double noteFreqHz = 440.0;
    float noteVelocity = 0.0f;
    int currentNote = -1;

    // Portamento
    double targetNoteFreq = 440.0;
    double currentFreq = 440.0;
    double portamentoRate = 0.0;

    // Pitch wheel
    double pitchBendSemitones = 0.0;

    // SmoothedValues pour les paramètres continus (anti-zipper)
    juce::SmoothedValue<float> smoothVolume;
    juce::SmoothedValue<float> smoothCutoff;
    juce::SmoothedValue<float> smoothMod1Level;
    juce::SmoothedValue<float> smoothMod2Level;
    juce::SmoothedValue<float> smoothCarNoise;
    juce::SmoothedValue<float> smoothCarSpread;

    // White noise generator (xorshift32)
    uint32_t noiseSeed = 0x12345678;

    double sampleRate = 44100.0;

    // Calcul de la fréquence d'un modulateur (Operator-style: ratio or fixed mode)
    double calcModFreq(double baseFreq, int coarseIdx, float fineCents,
                       float fixedFreqHz, int multi, bool kbTrack) const;
};

} // namespace bb
