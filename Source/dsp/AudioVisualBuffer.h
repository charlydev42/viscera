// AudioVisualBuffer.h — Lock-free ring buffer for GUI visualization
// Used to pass audio data from processBlock to the editor's oscilloscope/FFT
#pragma once
#include <array>
#include <atomic>
#include <algorithm>

namespace bb {

class AudioVisualBuffer
{
public:
    static constexpr int kSize = 2048;

    void push(float sample) noexcept
    {
        int wi = writeIndex.load(std::memory_order_relaxed);
        buffer[wi] = sample;
        writeIndex.store((wi + 1) % kSize, std::memory_order_release);
    }

    void pushBlock(const float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            push(samples[i]);
    }

    // Copy the most recent n samples into dest (oldest first)
    void copyTo(float* dest, int n) const noexcept
    {
        int wi = writeIndex.load(std::memory_order_acquire);
        int start = (wi - n + kSize) % kSize;
        for (int i = 0; i < n; ++i)
            dest[i] = buffer[(start + i) % kSize];
    }

private:
    std::array<float, kSize> buffer {};
    std::atomic<int> writeIndex { 0 };
};

} // namespace bb
