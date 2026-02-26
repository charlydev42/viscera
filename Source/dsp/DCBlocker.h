// DCBlocker.h — Filtre passe-haut du 1er ordre pour supprimer le DC offset
// y[n] = x[n] - x[n-1] + R × y[n-1]
// R ≈ 0.9999 → coupe à ~5 Hz pour sr=44100
// Essentiel après la FM et le XOR qui peuvent générer du DC
#pragma once

namespace bb {

class DCBlocker
{
public:
    void prepare(double sampleRate) noexcept
    {
        // R contrôle la fréquence de coupure
        // fc ≈ (1-R) × sr / (2π)
        // Pour fc ≈ 5 Hz : R = 1 - 2π×5/sr
        R = 1.0 - (2.0 * 3.14159265358979323846 * 5.0 / sampleRate);
        x1 = 0.0;
        y1 = 0.0;
    }

    float tick(float input) noexcept
    {
        double x0 = static_cast<double>(input);
        double y0 = x0 - x1 + R * y1;
        x1 = x0;
        y1 = y0;
        return static_cast<float>(y0);
    }

    void reset() noexcept
    {
        x1 = 0.0;
        y1 = 0.0;
    }

private:
    double R = 0.9999;
    double x1 = 0.0; // échantillon d'entrée précédent
    double y1 = 0.0; // échantillon de sortie précédent
};

} // namespace bb
