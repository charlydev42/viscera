// FMSound.h — SynthesiserSound trivial
// JUCE exige un SynthesiserSound pour que les voix sachent quelles notes jouer
// Pour un synthé simple, on accepte toutes les notes et tous les canaux MIDI
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace bb {

class FMSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

} // namespace bb
