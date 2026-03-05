// TestHelpers.h — Utility functions for Viscera DSP tests
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

namespace test {

// Check if buffer contains any NaN or Inf values
inline bool hasNaN(const float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        if (!std::isfinite(data[i])) return true;
    return false;
}

inline bool hasNaN(const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        if (hasNaN(buf.getReadPointer(ch), buf.getNumSamples())) return true;
    return false;
}

// Peak absolute amplitude
inline float peakAmplitude(const float* data, int numSamples)
{
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max(peak, std::fabs(data[i]));
    return peak;
}

inline float peakAmplitude(const juce::AudioBuffer<float>& buf)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        peak = std::max(peak, peakAmplitude(buf.getReadPointer(ch), buf.getNumSamples()));
    return peak;
}

// Check if buffer is essentially silent
inline bool isSilent(const float* data, int numSamples, float threshold = 1e-6f)
{
    return peakAmplitude(data, numSamples) < threshold;
}

inline bool isSilent(const juce::AudioBuffer<float>& buf, float threshold = 1e-6f)
{
    return peakAmplitude(buf) < threshold;
}

// RMS level
inline float rms(const float* data, int numSamples)
{
    if (numSamples == 0) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sum += static_cast<double>(data[i]) * data[i];
    return static_cast<float>(std::sqrt(sum / numSamples));
}

// DC offset (mean value)
inline float dcOffset(const float* data, int numSamples)
{
    if (numSamples == 0) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sum += data[i];
    return static_cast<float>(sum / numSamples);
}

// Zero crossing count
inline int zeroCrossings(const float* data, int numSamples)
{
    int count = 0;
    for (int i = 1; i < numSamples; ++i)
        if ((data[i - 1] >= 0.0f) != (data[i] >= 0.0f))
            ++count;
    return count;
}

// Estimate frequency from zero crossings
inline float estimatedFrequency(const float* data, int numSamples, double sampleRate)
{
    int crossings = zeroCrossings(data, numSamples);
    double duration = static_cast<double>(numSamples) / sampleRate;
    return static_cast<float>(crossings / (2.0 * duration));
}

// Create a MIDI buffer with a note-on event
inline juce::MidiBuffer createNoteOnBuffer(int note = 60, float velocity = 0.8f, int samplePos = 0)
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, velocity), samplePos);
    return midi;
}

// Create a MIDI buffer with note-on at start and note-off later
inline juce::MidiBuffer createNoteOnOffBuffer(int note = 60, float velocity = 0.8f,
                                                int offSamplePos = 4410)
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, velocity), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, note, 0.0f), offSamplePos);
    return midi;
}

// Render N samples through an oscillator
template <typename OscType>
inline void renderOscillator(OscType& osc, float* dest, int numSamples, double pm = 0.0)
{
    for (int i = 0; i < numSamples; ++i)
        dest[i] = osc.tick(pm);
}

} // namespace test
