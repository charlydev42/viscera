// XORDistortion.h — Distorsion digitale par opération XOR
// Quantize float [-1,1] → int16, applique un masque XOR variable, retour en float
// Crée des artefacts numériques caractéristiques (bit-crushing créatif)
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace bb {

class XORDistortion
{
public:
    // Le masque XOR détermine l'intensité de la distorsion
    // mask = 0 → pas d'effet, mask = 0xFFFF → destruction totale
    void setMask(uint16_t m) noexcept { mask = m; }

    float process(float input) noexcept
    {
        if (mask == 0) return input;

        // NaN/Inf guard
        if (!std::isfinite(input)) return 0.0f;

        // Clamper l'entrée à [-1, 1]
        float clamped = std::clamp(input, -1.0f, 1.0f);

        // Convertir float → int16 (quantization)
        auto quantized = static_cast<int16_t>(clamped * 32767.0f);

        // Appliquer le XOR bitwise
        quantized ^= static_cast<int16_t>(mask);

        // Retour en float [-1, 1]
        return static_cast<float>(quantized) / 32767.0f;
    }

private:
    uint16_t mask = 0;
};

} // namespace bb
