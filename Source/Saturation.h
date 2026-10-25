#pragma once
#include <cmath>

// Saturaatiokäyrät. Käytetään sekä äänenkäsittelyssä että käyttöliittymän käyrässä.
namespace Sat
{
    inline float shape (float x, int style)
    {
        switch (style)
        {
            case 0:  // Tape - pehmeä symmetrinen
                return std::tanh (x);

            case 1:  // Tube - epäsymmetrinen (parilliset yläsävelet)
                return (std::tanh (x + 0.35f) - 0.3364f) / 0.8869f;

            case 2:  // Transformer - pyöreä, hitaasti kyllästyvä
                return x / std::sqrt (1.0f + x * x);

            case 3:  // Diode - terävä, epäsymmetrinen
                return x >= 0.0f ? 1.0f - std::exp (-x)
                                 : -(1.0f - std::exp (1.5f * x)) / 1.5f;

            default: // Fold - aaltomuodon taittaminen
            {
                const float z = 4.7f * std::tanh (x / 4.7f);
                return std::sin (z);
            }
        }
    }

    inline float driveDb (float drive, bool punish)
    {
        return drive * 3.2f + (punish ? 12.0f : 0.0f);
    }
}
