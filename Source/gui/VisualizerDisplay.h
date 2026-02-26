// VisualizerDisplay.h — Oscilloscope + FFT spectrum + Stereo Lissajous
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/AudioVisualBuffer.h"

class VisualizerDisplay : public juce::Component, private juce::Timer
{
public:
    VisualizerDisplay(bb::AudioVisualBuffer& bufferL, bb::AudioVisualBuffer& bufferR);
    ~VisualizerDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    bb::AudioVisualBuffer& audioBufferL;
    bb::AudioVisualBuffer& audioBufferR;

    // Mode toggle
    enum Mode { Scope, FFT, Stereo };
    Mode currentMode = Scope;

    juce::TextButton scopeButton  { "Scope" };
    juce::TextButton fftButton    { "FFT" };
    juce::TextButton stereoButton { "Stereo" };

    // Scope data
    static constexpr int kScopeSize = 512;
    std::array<float, bb::AudioVisualBuffer::kSize> rawBufferL {};
    std::array<float, bb::AudioVisualBuffer::kSize> rawBufferR {};

    // FFT
    static constexpr int kFFTOrder = 10; // 1024-point
    static constexpr int kFFTSize  = 1 << kFFTOrder;
    juce::dsp::FFT fft { kFFTOrder };
    std::array<float, kFFTSize * 2> fftData {};
    std::array<float, kFFTSize / 2> smoothedMagnitudes {};

    void drawScope(juce::Graphics& g, juce::Rectangle<int> area);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<int> area);
    void drawStereo(juce::Graphics& g, juce::Rectangle<int> area);

    void setMode(Mode m);

    // Pseudo-3D trail history for stereo goniometer
    static constexpr int kTrailFrames = 28;
    static constexpr int kTrailPoints = 128;
    struct TrailPoint { float mid = 0.0f, side = 0.0f; };
    std::array<std::array<TrailPoint, kTrailPoints>, kTrailFrames> trailHistory {};
    int trailHead = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualizerDisplay)
};
