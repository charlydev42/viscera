// ADSREnvelope.h — Wrapper léger autour de juce::ADSR
// ADSR = Attack-Decay-Sustain-Release, enveloppe standard pour synthés
// On utilise juce::ADSR directement (pas besoin de réimplémenter)
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace bb {

class ADSREnvelope
{
public:
    void prepare(double sampleRate) noexcept
    {
        adsr.setSampleRate(sampleRate);
    }

    void setParameters(float attack, float decay, float sustain, float release) noexcept
    {
        params.attack  = attack;
        params.decay   = decay;
        params.sustain = sustain;
        params.release = release;
        adsr.setParameters(params);
    }

    void noteOn() noexcept  { adsr.noteOn(); }
    void noteOff() noexcept { adsr.noteOff(); }
    void reset() noexcept   { adsr.reset(); }

    float tick() noexcept
    {
        return adsr.getNextSample();
    }

    bool isActive() const noexcept { return adsr.isActive(); }

private:
    juce::ADSR adsr;
    juce::ADSR::Parameters params;
};

} // namespace bb
