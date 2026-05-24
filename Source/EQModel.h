#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <complex>

namespace EQModel
{
enum class BandType
{
    lowShelf,
    peak,
    highShelf
};

struct Band
{
    BandType type;
    float frequencyHz;
    float gainDb;
    float q;
};

inline constexpr size_t bandCount = 10;

struct Profile
{
    const char* name;
    const char* shortName;
    std::array<Band, bandCount> bands;
};

inline constexpr std::array<Profile, 2> profiles {{
    {
        "RODE M2",
        "M2",
        {{
            { BandType::lowShelf,    92.0f,  9.00f, 0.72f },
            { BandType::peak,       205.0f, -2.19f, 1.20f },
            { BandType::peak,       500.0f, -1.15f, 1.75f },
            { BandType::peak,      1320.0f, -0.42f, 2.60f },
            { BandType::peak,      3120.0f, -2.09f, 2.20f },
            { BandType::peak,      4550.0f, -1.99f, 2.00f },
            { BandType::peak,      5900.0f,  5.97f, 1.10f },
            { BandType::peak,      8200.0f, -2.50f, 2.30f },
            { BandType::peak,      9700.0f,  2.26f, 2.10f },
            { BandType::highShelf,16200.0f, -2.71f, 0.65f },
        }}
    },
    {
        "Shure SM7B",
        "SM7B",
        {{
            { BandType::lowShelf,    90.0f, -0.38f, 0.70f },
            { BandType::peak,       180.0f, -6.33f, 1.10f },
            { BandType::peak,       350.0f, -2.07f, 1.40f },
            { BandType::peak,       800.0f,-10.00f, 1.35f },
            { BandType::peak,      1400.0f, -0.53f, 1.90f },
            { BandType::peak,      2600.0f,  0.19f, 1.70f },
            { BandType::peak,      4200.0f, -1.41f, 1.55f },
            { BandType::peak,      6500.0f,  2.58f, 1.25f },
            { BandType::peak,      9800.0f,  0.74f, 1.60f },
            { BandType::highShelf,14500.0f,  1.79f, 0.70f },
        }}
    },
}};

inline int getValidProfileIndex (int index)
{
    return juce::jlimit (0, static_cast<int> (profiles.size()) - 1, index);
}

inline const Profile& getProfile (int index)
{
    return profiles[static_cast<size_t> (getValidProfileIndex (index))];
}

inline juce::StringArray getProfileNames()
{
    juce::StringArray names;

    for (const auto& profile : profiles)
        names.add (profile.name);

    return names;
}

inline juce::String getBandTypeName (BandType type)
{
    switch (type)
    {
        case BandType::lowShelf:  return "Low shelf";
        case BandType::peak:      return "Bell";
        case BandType::highShelf: return "High shelf";
    }

    return {};
}

inline float normaliseBlendPercent (float blendPercent)
{
    return juce::jlimit (0.0f, 2.0f, blendPercent / 100.0f);
}

inline double clampFrequency (double frequencyHz, double sampleRate)
{
    return juce::jlimit (20.0, juce::jmax (20.0, sampleRate * 0.45), frequencyHz);
}

inline juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients (const Band& band,
                                                                  double sampleRate,
                                                                  float blend01)
{
    const auto frequency = static_cast<float> (clampFrequency (band.frequencyHz, sampleRate));
    const auto gain = juce::Decibels::decibelsToGain (band.gainDb * juce::jlimit (0.0f, 2.0f, blend01));

    switch (band.type)
    {
        case BandType::lowShelf:
            return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, frequency, band.q, gain);

        case BandType::peak:
            return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, frequency, band.q, gain);

        case BandType::highShelf:
            return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, frequency, band.q, gain);
    }

    return nullptr;
}

struct BiquadCoefficients
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a0 = 1.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

inline BiquadCoefficients makeDisplayBiquad (const Band& band, double sampleRate, float blend01)
{
    const auto frequency = clampFrequency (band.frequencyHz, sampleRate);
    const auto scaledGainDb = static_cast<double> (band.gainDb * juce::jlimit (0.0f, 2.0f, blend01));
    const auto a = std::pow (10.0, scaledGainDb / 40.0);
    const auto omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const auto sinOmega = std::sin (omega);
    const auto cosOmega = std::cos (omega);
    const auto alpha = sinOmega / (2.0 * static_cast<double> (band.q));
    const auto sqrtA = std::sqrt (a);

    BiquadCoefficients c;

    switch (band.type)
    {
        case BandType::peak:
            c.b0 = 1.0 + alpha * a;
            c.b1 = -2.0 * cosOmega;
            c.b2 = 1.0 - alpha * a;
            c.a0 = 1.0 + alpha / a;
            c.a1 = -2.0 * cosOmega;
            c.a2 = 1.0 - alpha / a;
            break;

        case BandType::lowShelf:
            c.b0 = a * ((a + 1.0) - (a - 1.0) * cosOmega + 2.0 * sqrtA * alpha);
            c.b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cosOmega);
            c.b2 = a * ((a + 1.0) - (a - 1.0) * cosOmega - 2.0 * sqrtA * alpha);
            c.a0 = (a + 1.0) + (a - 1.0) * cosOmega + 2.0 * sqrtA * alpha;
            c.a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cosOmega);
            c.a2 = (a + 1.0) + (a - 1.0) * cosOmega - 2.0 * sqrtA * alpha;
            break;

        case BandType::highShelf:
            c.b0 = a * ((a + 1.0) + (a - 1.0) * cosOmega + 2.0 * sqrtA * alpha);
            c.b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cosOmega);
            c.b2 = a * ((a + 1.0) + (a - 1.0) * cosOmega - 2.0 * sqrtA * alpha);
            c.a0 = (a + 1.0) - (a - 1.0) * cosOmega + 2.0 * sqrtA * alpha;
            c.a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cosOmega);
            c.a2 = (a + 1.0) - (a - 1.0) * cosOmega - 2.0 * sqrtA * alpha;
            break;
    }

    return c;
}

inline double magnitudeDbAt (double frequencyHz, double sampleRate, float blend01, int profileIndex)
{
    auto magnitude = 1.0;
    const auto angle = -2.0 * juce::MathConstants<double>::pi * frequencyHz / sampleRate;
    const auto z1 = std::complex<double> { std::cos (angle), std::sin (angle) };
    const auto z2 = z1 * z1;

    for (const auto& band : getProfile (profileIndex).bands)
    {
        const auto c = makeDisplayBiquad (band, sampleRate, blend01);
        const auto numerator = c.b0 + c.b1 * z1 + c.b2 * z2;
        const auto denominator = c.a0 + c.a1 * z1 + c.a2 * z2;

        if (std::abs (denominator) > 0.0)
            magnitude *= std::abs (numerator / denominator);
    }

    return juce::Decibels::gainToDecibels (magnitude, -96.0);
}
} // namespace EQModel
