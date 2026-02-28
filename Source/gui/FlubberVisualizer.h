// FlubberVisualizer.h — OpenGL raymarched audio-reactive blob
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/AudioVisualBuffer.h"

class FlubberVisualizer : public juce::Component,
                          public juce::OpenGLRenderer
{
public:
    FlubberVisualizer(bb::AudioVisualBuffer& bufL, bb::AudioVisualBuffer& bufR);
    ~FlubberVisualizer() override;

    void paint(juce::Graphics&) override;

    // OpenGLRenderer callbacks (called on GL thread)
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    bb::AudioVisualBuffer& audioL;
    bb::AudioVisualBuffer& audioR;

    juce::OpenGLContext glContext;

    // Two shader programs: light and dark mode
    struct ShaderSet {
        std::unique_ptr<juce::OpenGLShaderProgram> program;
        GLint uTime = -1, uResolution = -1, uChannel0 = -1, uBgColor = -1;
    };
    ShaderSet lightShader, darkShader;

    // Audio texture (512 x 2: row 0 = FFT, row 1 = waveform)
    GLuint audioTex = 0;
    static constexpr int kTexW = 512;

    // Fullscreen quad VAO/VBO
    GLuint vao = 0, vbo = 0;

    // FFT
    static constexpr int kFFTOrder = 9;
    static constexpr int kFFTSize = 1 << kFFTOrder; // 512
    juce::dsp::FFT fft { kFFTOrder };

    // Audio data for texture upload
    std::array<float, kFFTSize * 2> fftData {};
    std::array<float, kTexW> fftRow {};
    std::array<float, kTexW> waveRow {};
    std::array<float, kTexW> smoothedFFT {};

    double startTime = 0.0;

    void updateAudioTexture();
    bool compileShader(ShaderSet& ss, const char* fragSrc);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlubberVisualizer)
};
