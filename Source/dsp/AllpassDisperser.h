// AllpassDisperser.h — Chaîne d'allpass pour dispersion de phase
// Simule l'effet "Disperser" : rotation de phase dépendante de la fréquence
// Smear les transitoires, crée un son "dispersé" / "étalement spectral"
// Implémentation : 8 filtres allpass du premier ordre en cascade
#pragma once
#include <cmath>
#include <algorithm>

namespace bb {

class AllpassDisperser
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < kNumStages; ++i)
        {
            x1[i] = 0.0f;
            y1[i] = 0.0f;
        }
    }

    // amount: 0-1 (0 = bypass, 1 = full dispersion)
    // Contrôle combien de stages sont actifs et l'intensité
    void setAmount(float amount)
    {
        amt = std::max(0.0f, std::min(amount, 1.0f));
        // Fréquence de coupure de l'allpass : de 200Hz (amt=0) à 8000Hz (amt=1)
        // Mapping exponentiel pour un contrôle plus musical
        float freq = 200.0f * std::pow(40.0f, amt);
        freq = std::min(freq, static_cast<float>(sampleRate * 0.45));

        // Coefficient allpass : a = (tan(pi*f/sr) - 1) / (tan(pi*f/sr) + 1)
        float w = std::tan(3.14159265358979f * freq / static_cast<float>(sampleRate));
        coeff = (w - 1.0f) / (w + 1.0f);
    }

    float tick(float input)
    {
        if (amt < 0.001f)
            return input;

        float signal = input;

        // Nombre de stages actifs proportionnel à l'amount
        int activeStages = static_cast<int>(amt * kNumStages + 0.5f);
        activeStages = std::max(1, std::min(activeStages, kNumStages));

        for (int i = 0; i < activeStages; ++i)
        {
            // Allpass 1er ordre : y[n] = a * x[n] + x[n-1] - a * y[n-1]
            float y = coeff * signal + x1[i] - coeff * y1[i];
            x1[i] = signal;
            y1[i] = y;
            signal = y;
        }

        // Mix dry/wet basé sur l'amount
        return input + (signal - input) * amt;
    }

private:
    static constexpr int kNumStages = 8;
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float amt = 0.0f;
    float x1[kNumStages] = {};
    float y1[kNumStages] = {};
};

} // namespace bb
