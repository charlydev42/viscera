// SVFilter.h — Cytomic TPT State Variable Filter
// Basé sur l'article d'Andrew Simper (Cytomic)
// TPT = Topology Preserving Transform : stable à toutes les fréquences
// Contrairement au filtre de Chamberlin qui diverge aux hautes fréquences
#pragma once
#include <cmath>
#include <algorithm>

namespace bb {

enum class FilterMode { LP, HP, BP, Notch };

class SVFilter
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        ic1eq = 0.0;
        ic2eq = 0.0;
    }

    void setParameters(float cutoffHz, float resonance) noexcept
    {
        // Clamper le cutoff pour éviter les instabilités
        double fc = std::clamp(static_cast<double>(cutoffHz), 20.0, sr * 0.49);
        double res = std::clamp(static_cast<double>(resonance), 0.0, 1.0);

        // Coefficients Cytomic TPT SVF
        // g = tan(π × fc / sr) — la transformation bilinéaire
        g = std::tan(3.14159265358979323846 * fc / sr);
        // k = damping = 2 - 2*resonance (Q = 1/(2-2*res) quand res→1, Q→∞)
        // On limite Q pour éviter l'auto-oscillation destructrice
        k = 2.0 - 2.0 * res * 0.98; // légère limitation

        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // Traite un échantillon selon le mode choisi
    float tick(float input, FilterMode mode = FilterMode::LP) noexcept
    {
        double v0 = static_cast<double>(input);

        // TPT SVF equations (Cytomic)
        double v3 = v0 - ic2eq;
        double v1 = a1 * ic1eq + a2 * v3;
        double v2 = ic2eq + a2 * ic1eq + a3 * v3;

        // Mise à jour des états (intégrateurs)
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;

        // Sorties : v2 = LP, v1 = BP, v0 - k*v1 - v2 = HP
        switch (mode)
        {
            case FilterMode::HP:    return static_cast<float>(v0 - k * v1 - v2);
            case FilterMode::BP:    return static_cast<float>(v1);
            case FilterMode::Notch: return static_cast<float>(v0 - k * v1); // LP + HP = v2 + (v0-k*v1-v2)
            case FilterMode::LP:
            default:                return static_cast<float>(v2);
        }
    }

    void reset() noexcept
    {
        ic1eq = 0.0;
        ic2eq = 0.0;
    }

private:
    double sr = 44100.0;
    double g = 0.0, k = 0.0;
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double ic1eq = 0.0; // état intégrateur 1
    double ic2eq = 0.0; // état intégrateur 2
};

} // namespace bb
