// VisualizerDisplay.cpp — Oscilloscope + FFT spectrum + Stereo Lissajous
#include "VisualizerDisplay.h"
#include "VisceraLookAndFeel.h"
#include <cmath>

VisualizerDisplay::VisualizerDisplay(bb::AudioVisualBuffer& bufferL,
                                     bb::AudioVisualBuffer& bufferR)
    : audioBufferL(bufferL), audioBufferR(bufferR)
{
    smoothedMagnitudes.fill(0.0f);

    auto setupButton = [](juce::TextButton& btn)
    {
        btn.setClickingTogglesState(false);
        btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(VisceraLookAndFeel::kAccentColor).withAlpha(0.3f));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(VisceraLookAndFeel::kTextColor));
        btn.setColour(juce::TextButton::textColourOnId, juce::Colour(VisceraLookAndFeel::kAccentColor));
    };
    setupButton(scopeButton);
    setupButton(fftButton);
    setupButton(stereoButton);

    // Default to stereo-only (no mode buttons)
    currentMode = Stereo;
    scopeButton.setVisible(false);
    fftButton.setVisible(false);
    stereoButton.setVisible(false);

    startTimerHz(30);
}

VisualizerDisplay::~VisualizerDisplay()
{
    stopTimer();
}

void VisualizerDisplay::setMode(Mode m)
{
    currentMode = m;
    scopeButton.setToggleState(m == Scope, juce::dontSendNotification);
    fftButton.setToggleState(m == FFT, juce::dontSendNotification);
    stereoButton.setToggleState(m == Stereo, juce::dontSendNotification);
}

void VisualizerDisplay::timerCallback()
{
    repaint();
}

void VisualizerDisplay::resized()
{
    // No mode buttons — full area used for display
}

void VisualizerDisplay::paint(juce::Graphics& g)
{
    auto displayArea = getLocalBounds();

    g.setColour(juce::Colour(VisceraLookAndFeel::kDisplayBg));
    g.fillRect(displayArea);

    drawStereo(g, displayArea);
}

void VisualizerDisplay::drawScope(juce::Graphics& g, juce::Rectangle<int> area)
{
    audioBufferL.copyTo(rawBufferL.data(), bb::AudioVisualBuffer::kSize);

    // Find a zero-crossing trigger point (positive-going)
    int triggerPoint = 0;
    int searchEnd = bb::AudioVisualBuffer::kSize - kScopeSize;
    for (int i = 1; i < searchEnd; ++i)
    {
        if (rawBufferL[i - 1] <= 0.0f && rawBufferL[i] > 0.0f)
        {
            triggerPoint = i;
            break;
        }
    }

    float x = static_cast<float>(area.getX());
    float y = static_cast<float>(area.getY());
    float w = static_cast<float>(area.getWidth());
    float h = static_cast<float>(area.getHeight());
    float midY = y + h * 0.5f;

    // Draw center line
    g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff));
    g.drawHorizontalLine(static_cast<int>(midY), x, x + w);

    // Draw waveform
    juce::Path path;
    for (int i = 0; i < kScopeSize; ++i)
    {
        float sample = juce::jlimit(-1.0f, 1.0f, rawBufferL[triggerPoint + i]);
        float px = x + (static_cast<float>(i) / static_cast<float>(kScopeSize - 1)) * w;
        float py = midY - sample * (h * 0.45f);

        if (i == 0)
            path.startNewSubPath(px, py);
        else
            path.lineTo(px, py);
    }

    g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor));
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void VisualizerDisplay::drawSpectrum(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Copy samples and apply Hanning window
    std::array<float, bb::AudioVisualBuffer::kSize> tempSamples;
    audioBufferL.copyTo(tempSamples.data(), bb::AudioVisualBuffer::kSize);

    fftData.fill(0.0f);
    for (int i = 0; i < kFFTSize; ++i)
    {
        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                 * static_cast<float>(i) / static_cast<float>(kFFTSize)));
        fftData[i] = tempSamples[bb::AudioVisualBuffer::kSize - kFFTSize + i] * window;
    }

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    float x = static_cast<float>(area.getX());
    float y = static_cast<float>(area.getY());
    float w = static_cast<float>(area.getWidth());
    float h = static_cast<float>(area.getHeight());

    int numBins = kFFTSize / 2;
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float minDb = -80.0f;
    float maxDb = 0.0f;
    float logMinFreq = std::log10(minFreq);
    float logMaxFreq = std::log10(maxFreq);

    // Draw frequency grid lines
    g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff).darker(0.2f));
    for (float freq : { 100.0f, 1000.0f, 10000.0f })
    {
        float normX = (std::log10(freq) - logMinFreq) / (logMaxFreq - logMinFreq);
        g.drawVerticalLine(static_cast<int>(x + normX * w), y, y + h);
    }
    // Draw dB grid lines
    for (float db : { -60.0f, -40.0f, -20.0f })
    {
        float normY = 1.0f - (db - minDb) / (maxDb - minDb);
        g.drawHorizontalLine(static_cast<int>(y + normY * h), x, x + w);
    }

    // Assume 44100 sample rate for bin → frequency mapping
    float sampleRate = 44100.0f;

    // Build and draw spectrum path
    juce::Path path;
    bool pathStarted = false;

    for (int i = 0; i < static_cast<int>(w); ++i)
    {
        float normX = static_cast<float>(i) / w;
        float freq = std::pow(10.0f, logMinFreq + normX * (logMaxFreq - logMinFreq));
        int bin = static_cast<int>(freq / (sampleRate / static_cast<float>(kFFTSize)));
        if (bin < 1) bin = 1;
        if (bin >= numBins) bin = numBins - 1;

        float magnitude = fftData[bin];
        float db = (magnitude > 1e-10f)
                     ? 20.0f * std::log10(magnitude)
                     : minDb;

        // Smooth
        float smoothIdx = static_cast<float>(bin);
        int si = juce::jlimit(0, numBins - 1, static_cast<int>(smoothIdx));
        smoothedMagnitudes[si] = smoothedMagnitudes[si] * 0.7f + db * 0.3f;
        db = smoothedMagnitudes[si];

        float normY = 1.0f - juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        float px = x + static_cast<float>(i);
        float py = y + normY * h;

        if (!pathStarted)
        {
            path.startNewSubPath(px, py);
            pathStarted = true;
        }
        else
        {
            path.lineTo(px, py);
        }
    }

    g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor));
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void VisualizerDisplay::drawStereo(juce::Graphics& g, juce::Rectangle<int> area)
{
    audioBufferL.copyTo(rawBufferL.data(), bb::AudioVisualBuffer::kSize);
    audioBufferR.copyTo(rawBufferR.data(), bb::AudioVisualBuffer::kSize);

    // --- Capture current frame into trail history ---
    {
        int startIdx = bb::AudioVisualBuffer::kSize - kScopeSize;
        int step = kScopeSize / kTrailPoints;
        auto& frame = trailHistory[trailHead];
        for (int i = 0; i < kTrailPoints; ++i)
        {
            int si = startIdx + i * step;
            float sL = juce::jlimit(-1.0f, 1.0f, rawBufferL[si]);
            float sR = juce::jlimit(-1.0f, 1.0f, rawBufferR[si]);
            frame[i].mid  = (sL + sR) * 0.5f;
            frame[i].side = (sL - sR) * 0.5f;
        }
        trailHead = (trailHead + 1) % kTrailFrames;
    }

    float x = static_cast<float>(area.getX());
    float y = static_cast<float>(area.getY());
    float w = static_cast<float>(area.getWidth());
    float h = static_cast<float>(area.getHeight());

    // Vanishing point: center-top of the display
    float vpX = x + w * 0.5f;
    float vpY = y + h * 0.12f;
    // Front center: where the newest frame draws
    float frontX = x + w * 0.5f;
    float frontY = y + h * 0.55f;
    float frontScale = std::min(w, h) * 0.42f;

    // Check if any trail frame has signal (avoid drawing guides on silence)
    bool hasSignal = false;
    for (int f = 0; f < kTrailFrames && !hasSignal; ++f)
        for (int i = 0; i < kTrailPoints && !hasSignal; ++i)
            if (std::abs(trailHistory[f][i].mid) >= 1e-4f || std::abs(trailHistory[f][i].side) >= 1e-4f)
                hasSignal = true;

    if (!hasSignal)
        return;

    // Subtle crosshair at front center
    g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff).withAlpha(0.15f));
    g.drawVerticalLine(static_cast<int>(frontX), frontY - frontScale * 0.6f, frontY + frontScale * 0.6f);
    g.drawHorizontalLine(static_cast<int>(frontY), frontX - frontScale * 0.6f, frontX + frontScale * 0.6f);

    auto accentCol = juce::Colour(VisceraLookAndFeel::kAccentColor);

    // --- Draw trail frames: oldest (back) to newest (front) ---
    for (int f = 0; f < kTrailFrames; ++f)
    {
        // Frame index in ring buffer: oldest first
        int frameIdx = (trailHead + f) % kTrailFrames;

        // Depth: 0.0 = oldest/back, 1.0 = newest/front
        float depth = static_cast<float>(f) / static_cast<float>(kTrailFrames - 1);

        // Perspective: interpolate center from vanishing point to front
        float cx = vpX + (frontX - vpX) * depth;
        float cy = vpY + (frontY - vpY) * depth;

        // Scale shrinks toward vanishing point
        float scale = frontScale * (0.08f + 0.92f * depth);

        // Alpha: exponential fade — older frames nearly invisible
        float alpha = depth * depth * 0.7f;
        if (f == kTrailFrames - 1) alpha = 0.85f; // newest frame bright

        // Point size: smaller in back, larger in front
        float ptSize = 0.8f + depth * 1.2f;

        g.setColour(accentCol.withAlpha(alpha));

        auto& frame = trailHistory[frameIdx];
        for (int i = 0; i < kTrailPoints; ++i)
        {
            // Skip silent samples — avoids center dot cluster at zero
            if (std::abs(frame[i].mid) < 1e-4f && std::abs(frame[i].side) < 1e-4f)
                continue;

            float px = cx + frame[i].side * scale;
            float py = cy - frame[i].mid  * scale;
            g.fillRect(px, py, ptSize, ptSize);
        }
    }

    // --- Subtle labels ---
    g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff).withAlpha(0.3f));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    g.drawText("M", static_cast<int>(frontX) - 6, static_cast<int>(frontY - frontScale * 0.6f) - 12, 12, 10, juce::Justification::centred);
    g.drawText("L", static_cast<int>(frontX - frontScale * 0.6f) - 14, static_cast<int>(frontY) - 5, 12, 10, juce::Justification::centredLeft);
    g.drawText("R", static_cast<int>(frontX + frontScale * 0.6f) + 2, static_cast<int>(frontY) - 5, 12, 10, juce::Justification::centredRight);
}
