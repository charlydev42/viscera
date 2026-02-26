// Oscillator.h — Oscillateur avec phase accumulator, PolyBLEP et entrée PM
// Phase accumulator : phase += freq/sampleRate, wrap à [0,1)
// PolyBLEP : correction polynomiale aux discontinuités (saw, square, pulse)
// PM : on ajoute un offset de phase venant des modulateurs FM
#pragma once
#include <cmath>
#include <cstdint>
#include <array>

namespace bb {

// --- Types de forme d'onde ---
enum class WaveType : int { Sine = 0, Saw, Square, Triangle, Pulse, Count };

// --- Table de sinus précalculée (constexpr) ---
// 4096 échantillons = bon compromis précision/cache
constexpr int SINE_TABLE_SIZE = 4096;

struct SineTable
{
    std::array<float, SINE_TABLE_SIZE + 1> data {}; // +1 pour interpolation linéaire

    SineTable()
    {
        for (int i = 0; i <= SINE_TABLE_SIZE; ++i)
            data[i] = static_cast<float>(
                std::sin(2.0 * 3.14159265358979323846 * static_cast<double>(i)
                         / static_cast<double>(SINE_TABLE_SIZE)));
    }
};

// Instance statique globale (Meyers singleton pour thread-safety)
inline const SineTable& getSineTable()
{
    static const SineTable table;
    return table;
}

// --- Oscillateur ---
class Oscillator
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        phase = 0.0;
        syncPulse = false;
    }

    void setFrequency(double freqHz) noexcept
    {
        freq = freqHz;
        inc = freq / sr;
    }

    void setWaveType(WaveType type) noexcept { waveType = type; }

    // Analog drift: slow random pitch wandering (0 = clean, 1 = max drift)
    void setDrift(float amount) noexcept { driftAmount = amount; }

    // Avancer d'un échantillon avec modulation de phase externe
    // phaseModulation est en radians (convention Yamaha), on divise par 2π
    float tick(double phaseModulation = 0.0) noexcept
    {
        // --- Analog drift ---
        // Slow wandering LFO whose frequency itself drifts
        double driftOffset = 0.0;
        if (driftAmount > 0.0f)
        {
            // Drift LFO frequency wanders (0.05–5 Hz) — more aggressive random walk
            driftLFOFreq += (driftRNG() * 2.0 - 1.0) / sr; // random walk on freq
            driftLFOFreq = std::max(0.05, std::min(driftLFOFreq, 5.0));

            driftLFOPhase += driftLFOFreq / sr;
            if (driftLFOPhase >= 1.0) driftLFOPhase -= 1.0;

            // Phase offset: up to ±4% of one cycle at max drift
            // 0.0 = clean, 0.5 = warm analog, 1.0 = experimental wobble
            double amount = static_cast<double>(driftAmount);
            amount *= amount; // exponential curve: subtle at low, wild at high
            driftOffset = amount * 0.04
                        * std::sin(driftLFOPhase * 2.0 * 3.14159265358979323846);
        }

        // Calcul de la phase modulée
        double modPhase = phase + phaseModulation / (2.0 * 3.14159265358979323846) + driftOffset;
        modPhase -= std::floor(modPhase); // wrap [0,1)

        float out = renderWave(waveType, modPhase);

        // Avancer la phase interne (non modulée — la PM ne touche que la lecture)
        double prevPhase = phase;
        phase += inc;

        // Détection de sync pulse : la phase a dépassé 1.0
        syncPulse = (phase >= 1.0);
        // Position fractionnelle du crossing pour interpolation subsample
        if (syncPulse)
            syncFraction = static_cast<float>((1.0 - prevPhase) / inc);

        phase -= std::floor(phase); // wrap [0,1)

        return out;
    }

    // Hard sync : reset de la phase avec offset subsample
    void hardSyncReset(float fraction) noexcept
    {
        phase = static_cast<double>(fraction) * inc;
    }

    bool hasSyncPulse() const noexcept { return syncPulse; }
    float getSyncFraction() const noexcept { return syncFraction; }
    double getPhase() const noexcept { return phase; }

    void resetPhase() noexcept { phase = 0.0; }

    // Accès public à la table sinus (utilisé par le LFO)
    static float lookupSinePublic(double phase) noexcept
    {
        return lookupSine(phase);
    }

private:
    double sr = 44100.0;
    double freq = 440.0;
    double inc = 0.01;      // freq / sampleRate
    double phase = 0.0;     // [0, 1)
    WaveType waveType = WaveType::Sine;
    bool syncPulse = false;
    float syncFraction = 0.0f;

    // Analog drift state
    float driftAmount = 0.0f;
    double driftLFOPhase = 0.0;
    double driftLFOFreq = 0.5; // Hz, wanders slowly

    // Minimal deterministic RNG for drift (xorshift32, no <random> header needed)
    uint32_t driftSeed = 0x12345678;
    double driftRNG() noexcept
    {
        driftSeed ^= driftSeed << 13;
        driftSeed ^= driftSeed >> 17;
        driftSeed ^= driftSeed << 5;
        return static_cast<double>(driftSeed) / static_cast<double>(0xFFFFFFFF);
    }

    // --- PolyBLEP : correction aux discontinuités ---
    // t = phase normalisée [0,1), dt = incrément de phase
    static double polyBlep(double t, double dt) noexcept
    {
        // Début de période (discontinuité à t=0)
        if (t < dt)
        {
            double x = t / dt;
            return x + x - x * x - 1.0; // 2x - x² - 1
        }
        // Fin de période (discontinuité à t=1)
        if (t > 1.0 - dt)
        {
            double x = (t - 1.0) / dt;
            return x * x + x + x + 1.0; // x² + 2x + 1
        }
        return 0.0;
    }

    float renderWave(WaveType type, double p) const noexcept
    {
        switch (type)
        {
        case WaveType::Sine:
            return lookupSine(p);

        case WaveType::Saw:
        {
            // Saw naïve : 2*p - 1
            double out = 2.0 * p - 1.0;
            out -= polyBlep(p, inc);
            return static_cast<float>(out);
        }

        case WaveType::Square:
        {
            // Square naïve : +1 si p < 0.5, -1 sinon
            double out = (p < 0.5) ? 1.0 : -1.0;
            out += polyBlep(p, inc);                          // discontinuité à 0
            out -= polyBlep(std::fmod(p + 0.5, 1.0), inc);   // discontinuité à 0.5
            return static_cast<float>(out);
        }

        case WaveType::Triangle:
        {
            // Triangle via intégration du square PolyBLEP (leaky integrator)
            // Approximation simple : 2*|2p-1| - 1
            double out = 2.0 * std::abs(2.0 * p - 1.0) - 1.0;
            return static_cast<float>(out);
        }

        case WaveType::Pulse:
        {
            // Pulse étroite (25% duty cycle)
            double out = (p < 0.25) ? 1.0 : -1.0;
            out += polyBlep(p, inc);
            out -= polyBlep(std::fmod(p + 0.75, 1.0), inc);
            return static_cast<float>(out);
        }

        default:
            return 0.0f;
        }
    }

    // Lookup dans la table sinus avec interpolation linéaire
    static float lookupSine(double phase) noexcept
    {
        const auto& table = getSineTable();
        double idx = phase * SINE_TABLE_SIZE;
        int i0 = static_cast<int>(idx);
        float frac = static_cast<float>(idx - i0);
        return table.data[i0] + frac * (table.data[i0 + 1] - table.data[i0]);
    }
};

} // namespace bb
