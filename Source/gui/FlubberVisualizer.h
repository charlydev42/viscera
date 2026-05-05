// FlubberVisualizer.h — OpenGL raymarched audio-reactive blob
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/AudioVisualBuffer.h"

class FlubberVisualizer : public juce::Component,
                          public juce::OpenGLRenderer,
                          private juce::Timer
{
public:
    FlubberVisualizer(bb::AudioVisualBuffer& bufL, bb::AudioVisualBuffer& bufR);
    ~FlubberVisualizer() override;

    void paint(juce::Graphics&) override;

    // Force GL context to re-render (call after dark mode switch)
    void triggerGLRepaint() { glContext.triggerRepaint(); }

    // Heartbeat hook called by ParasiteEditor::paint(). The parent
    // editor is repainted by JUCE only when the OS reports its surface
    // needs to be drawn; we use that as the cross-platform "is the user
    // really seeing this instance?" signal. The visualiser's own paint()
    // doesn't work for this purpose because setComponentPaintingEnabled
    // is off, which means JUCE routes Component::repaint() straight to
    // the GL render path without ever calling paint().
    void noteHostPainted() noexcept
    {
        lastHostPaintMs.store(juce::Time::getMillisecondCounter(),
                              std::memory_order_relaxed);
    }

    // OpenGLRenderer callbacks (called on GL thread)
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    // Cross-platform "is this editor actually being drawn to the screen?"
    // detection. JUCE's Component::isShowing() can lie when a host (e.g.
    // Ableton's device-view tab swap) keeps the JUCE visibility chain
    // intact while hiding the OS-level NSView/HWND of the inactive editor
    // — all the stale editors think they're showing and burn GPU.
    //
    // The signal we trust instead: the OS only emits paint events for
    // surfaces actually on screen, so JUCE only calls Component::paint()
    // when this editor is genuinely visible to the user. Each timer tick
    // we ping JUCE with a 1-px repaint(); if our paint() runs in time,
    // the heartbeat timestamp stays fresh and we trigger a GL frame.
    // Stale heartbeat → freeze on the last frame, zero GPU. Works on
    // Mac, Windows and Linux without any platform-specific code.
    void timerCallback() override;

    std::atomic<juce::uint32> lastHostPaintMs { 0 };
    static constexpr juce::uint32 kPaintFreshnessMs = 500;
    // After this long without a paint heartbeat we fully detach the GL
    // context, freeing the shader programs / textures / VBO / render
    // thread / CALayer composite. Re-attach on the next paint pings JUCE
    // to recompile shaders (~20–50 ms first frame). The hysteresis is
    // generous so quick track switches don't thrash detach/attach.
    static constexpr juce::uint32 kDetachAfterMs    = 5000;
    bool glAttached = false;

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
