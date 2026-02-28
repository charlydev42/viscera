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
        // text colours inherited from LookAndFeel (supports dark mode)
    };
    setupButton(scopeButton);
    setupButton(fftButton);
    setupButton(stereoButton);

    // Default to stereo-only (no mode buttons)
    currentMode = Stereo;
    scopeButton.setVisible(false);
    fftButton.setVisible(false);
    stereoButton.setVisible(false);

    // Timer disabled — VisualizerDisplay replaced by FlubberVisualizer
    // startTimerHz(30);
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
    auto ellipse = displayArea.toFloat();

    juce::Path clipPath;
    clipPath.addEllipse(ellipse);
    g.reduceClipRegion(clipPath);

    // ======= Glass bubble — photorealistic, light top-left =======

    float cx = ellipse.getCentreX();
    float cy = ellipse.getCentreY();
    float rx = ellipse.getWidth() * 0.5f;
    float ry = ellipse.getHeight() * 0.5f;

    // 1) Base — neumorphic grey lit side, cool-grey shadow side
    {
        juce::Colour baseLight, baseDark, baseMid1, baseMid2, baseMid3;
        if (VisceraLookAndFeel::darkMode)
        {
            baseLight = juce::Colour(0xFF353C4A);
            baseDark  = juce::Colour(0xFF2A3038);
            baseMid1  = juce::Colour(0xFF323846);
            baseMid2  = juce::Colour(0xFF2E3440);
            baseMid3  = juce::Colour(0xFF2C323E);
        }
        else
        {
            baseLight = juce::Colour(0xFFECF0F3);
            baseDark  = juce::Colour(0xFFD4D9E2);
            baseMid1  = juce::Colour(0xFFE6EBF0);
            baseMid2  = juce::Colour(0xFFDDE2E8);
            baseMid3  = juce::Colour(0xFFD8DDE4);
        }
        juce::ColourGradient base(baseLight, cx - rx * 0.18f, cy - ry * 0.15f,
                                  baseDark, cx + rx * 0.55f, cy + ry * 0.55f,
                                  true);
        base.addColour(0.35, baseMid1);
        base.addColour(0.65, baseMid2);
        base.addColour(0.85, baseMid3);
        g.setGradientFill(base);
        g.fillEllipse(ellipse);
    }

    // 2) Fresnel rim darkening — glass edges reflect more (Schlick approx feel)
    {
        juce::ColourGradient fresnel(juce::Colour(0x00000000), cx, cy,
                                     juce::Colour(0x0E000008), cx, cy + ry,
                                     true);
        fresnel.addColour(0.65, juce::Colour(0x00000000));
        fresnel.addColour(0.82, juce::Colour(0x06000004));
        g.setGradientFill(fresnel);
        g.fillEllipse(ellipse);
    }

    // 3) Shadow hemisphere — cool-tinted shadow on bottom-right
    {
        juce::ColourGradient shadow(juce::Colour(0x00000000), cx - rx * 0.20f, cy - ry * 0.20f,
                                    juce::Colour(0x14040610), cx + rx * 0.48f, cy + ry * 0.48f,
                                    true);
        shadow.addColour(0.50, juce::Colour(0x00000000));
        shadow.addColour(0.75, juce::Colour(0x08020408));
        g.setGradientFill(shadow);
        g.fillEllipse(ellipse);
    }

    // 4) Warm light wash — broad, very soft from top-left
    {
        juce::ColourGradient warmWash(juce::Colour(0x0EFFFEF8), cx - rx * 0.38f, cy - ry * 0.38f,
                                      juce::Colour(0x00FFFFFF), cx + rx * 0.20f, cy + ry * 0.20f,
                                      true);
        g.setGradientFill(warmWash);
        g.fillEllipse(ellipse);
    }

    // --- Draw content (stereo viz) ---
    drawStereo(g, displayArea);

    // 5) Primary specular — broad diffuse glow
    {
        float hlW = rx * 0.80f;
        float hlH = ry * 0.32f;
        float hlX = cx - rx * 0.55f;
        float hlY = cy - ry * 0.78f;
        juce::ColourGradient spec(juce::Colour(0x44FFFFFF), hlX + hlW * 0.36f, hlY + hlH * 0.2f,
                                  juce::Colour(0x00FFFFFF), hlX + hlW * 0.52f, hlY + hlH * 1.3f,
                                  false);
        spec.addColour(0.25, juce::Colour(0x30FFFFFF));
        spec.addColour(0.55, juce::Colour(0x10FFFFFF));
        g.setGradientFill(spec);
        g.fillEllipse(hlX, hlY, hlW, hlH);
    }

    // 6) Specular core — tight bright kernel, slightly off-center in the glow
    {
        float coreW = rx * 0.22f;
        float coreH = ry * 0.08f;
        float coreX = cx - rx * 0.34f - coreW * 0.5f;
        float coreY = cy - ry * 0.58f - coreH * 0.5f;
        juce::ColourGradient core(juce::Colour(0x5CFFFFFF), coreX + coreW * 0.5f, coreY + coreH * 0.4f,
                                  juce::Colour(0x00FFFFFF), coreX + coreW * 0.5f, coreY + coreH * 1.6f,
                                  false);
        core.addColour(0.3, juce::Colour(0x38FFFFFF));
        g.setGradientFill(core);
        g.fillEllipse(coreX, coreY, coreW, coreH);
    }

    // 7) Edge catch — thin bright line wrapping the light-facing rim
    {
        juce::Path rimArc;
        rimArc.addCentredArc(cx, cy, rx - 1.0f, ry - 1.0f, 0.0f,
                             -2.3f, -0.5f, true);
        juce::ColourGradient edgeCatch(juce::Colour(0x38FFFFFF), cx - rx, cy - ry * 0.5f,
                                       juce::Colour(0x08FFFFFF), cx - rx * 0.2f, cy - ry,
                                       false);
        g.setGradientFill(edgeCatch);
        g.strokePath(rimArc, juce::PathStrokeType(0.7f));
    }

    // 8) Secondary caustic — internal light bounce, bottom-right, warm-tinted
    {
        float c2W = rx * 0.16f;
        float c2H = ry * 0.06f;
        float c2X = cx + rx * 0.15f;
        float c2Y = cy + ry * 0.48f;
        juce::ColourGradient caustic(juce::Colour(0x0CFFFFF8), c2X + c2W * 0.5f, c2Y,
                                     juce::Colour(0x00FFFFFF), c2X + c2W * 0.5f, c2Y + c2H * 1.5f,
                                     false);
        g.setGradientFill(caustic);
        g.fillEllipse(c2X, c2Y, c2W, c2H);
    }

    // 9) Opposite rim catch — very faint light wrapping around the dark side
    {
        juce::Path wrapArc;
        wrapArc.addCentredArc(cx, cy, rx - 1.0f, ry - 1.0f, 0.0f,
                              0.8f, 2.0f, true);
        g.setColour(juce::Colour(0x0AFFFFFF));
        g.strokePath(wrapArc, juce::PathStrokeType(0.5f));
    }

    // 10) Full rim — very thin, almost invisible structure line
    {
        juce::ColourGradient rim(juce::Colour(0x30FFFFFF), ellipse.getX(), ellipse.getY(),
                                 juce::Colour(0x10000008), ellipse.getRight(), ellipse.getBottom(),
                                 false);
        rim.addColour(0.35, juce::Colour(0x1CFFFFFF));
        rim.addColour(0.65, juce::Colour(0x08A0A0A8));
        g.setGradientFill(rim);
        g.drawEllipse(ellipse.reduced(0.5f), 0.6f);
    }
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
    float vpY = y + h * 0.15f;
    // Front center: true center of the display
    float frontX = x + w * 0.5f;
    float frontY = y + h * 0.5f;
    float frontScale = h * 0.42f;

    // Check if any trail frame has signal (avoid drawing guides on silence)
    bool hasSignal = false;
    for (int f = 0; f < kTrailFrames && !hasSignal; ++f)
        for (int i = 0; i < kTrailPoints && !hasSignal; ++i)
            if (std::abs(trailHistory[f][i].mid) >= 1e-4f || std::abs(trailHistory[f][i].side) >= 1e-4f)
                hasSignal = true;

    // Always draw crosshair guides (visible even without signal)
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

    if (!hasSignal)
        return;

    // --- Subtle labels ---
    g.setColour(juce::Colour(VisceraLookAndFeel::kToggleOff).withAlpha(0.3f));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    g.drawText("M", static_cast<int>(frontX) - 6, static_cast<int>(frontY - frontScale * 0.6f) - 12, 12, 10, juce::Justification::centred);
    g.drawText("L", static_cast<int>(frontX - frontScale * 0.6f) - 14, static_cast<int>(frontY) - 5, 12, 10, juce::Justification::centredLeft);
    g.drawText("R", static_cast<int>(frontX + frontScale * 0.6f) + 2, static_cast<int>(frontY) - 5, 12, 10, juce::Justification::centredRight);
}
