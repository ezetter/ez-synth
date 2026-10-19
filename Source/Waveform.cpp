#include "Waveform.h"

#include <cmath>
#include <vector>

namespace waveform
{
    namespace
    {
        constexpr auto twoPi = juce::MathConstants<double>::twoPi;

        double presetValue (Preset preset, int point)
        {
            const auto t = (double) point / numPoints;

            // A jump between -1 and 1 is given the value halfway, at 0, where it falls exactly on a point. That keeps
            // the sampled wave as symmetrical as the real one, so its sine waves line up with the textbook ones.
            switch (preset)
            {
                case Preset::sine:      return std::sin (twoPi * t);
                case Preset::sawtooth:  return point == 0 ? 0.0 : 2.0 * t - 1.0;
                case Preset::square:    return point == 0 || point == numPoints / 2 ? 0.0 : (t < 0.5 ? 1.0 : -1.0);
                case Preset::triangle:  return t < 0.25 ? 4.0 * t : (t < 0.75 ? 2.0 - 4.0 * t : 4.0 * t - 4.0);
            }

            return 0.0;
        }
    }

    Shape makePreset (Preset preset)
    {
        Shape shape {};

        for (int i = 0; i < numPoints; ++i)
            shape[(size_t) i] = (float) presetValue (preset, i);

        return shape;
    }

    juce::String getName (Preset preset)
    {
        switch (preset)
        {
            case Preset::sine:      return "Sine";
            case Preset::sawtooth:  return "Sawtooth";
            case Preset::square:    return "Square";
            case Preset::triangle:  return "Triangle";
        }

        return {};
    }

    std::optional<Preset> findPreset (const juce::String& name)
    {
        for (const auto preset : allPresets)
            if (getName (preset) == name)
                return preset;

        return std::nullopt;
    }

    float Partial::valueAt (double t) const noexcept
    {
        return amplitude * (float) std::sin (twoPi * harmonic * t + phase);
    }

    Decomposition decompose (const Shape& shape)
    {
        // sin (2π k n / N) only ever needs (k * n) mod N, so one table of N sines covers every harmonic.
        static const auto sines = []
        {
            std::array<double, numPoints> table {};

            for (int i = 0; i < numPoints; ++i)
                table[(size_t) i] = std::sin (twoPi * i / numPoints);

            return table;
        }();

        Decomposition result;

        for (int k = 1; k <= maxHarmonics; ++k)
        {
            double sineSum = 0.0, cosineSum = 0.0;

            for (int n = 0; n < numPoints; ++n)
            {
                const auto index = (k * n) % numPoints;
                sineSum   += shape[(size_t) n] * sines[(size_t) index];
                cosineSum += shape[(size_t) n] * sines[(size_t) ((index + numPoints / 4) % numPoints)];
            }

            // The harmonic is b sin θ + a cos θ, which is the same as A sin (θ + φ).
            const auto b = 2.0 * sineSum / numPoints;
            const auto a = 2.0 * cosineSum / numPoints;
            const auto amplitude = std::hypot (a, b);

            // Anything 100 dB below full scale is a rounding error, such as a sine's 2nd harmonic.
            if (amplitude > 1.0e-5)
                result.harmonics[(size_t) k - 1] = { k, (float) amplitude, (float) std::atan2 (a, b) };
            else
                result.harmonics[(size_t) k - 1] = { k, 0.0f, 0.0f };
        }

        // Sample the sums finely enough to catch the peaks of even the highest harmonic.
        constexpr int resolution = 2048;
        std::vector<float> sum ((size_t) resolution, 0.0f);

        for (int n = 1; n <= maxHarmonics; ++n)
        {
            const auto& partial = result.harmonics[(size_t) n - 1];
            auto peak = 0.0f;

            for (int i = 0; i < resolution; ++i)
            {
                sum[(size_t) i] += partial.valueAt ((double) i / resolution);
                peak = std::max (peak, std::abs (sum[(size_t) i]));
            }

            result.peaks[(size_t) n] = peak;
        }

        return result;
    }

    float valueAt (const Shape& shape, double position, bool repeats) noexcept
    {
        const auto point = juce::jlimit (0.0, 1.0, position) * numPoints;
        const auto before = std::min ((int) point, numPoints - 1);
        const auto after = repeats ? (before + 1) % numPoints : std::min (before + 1, numPoints - 1);
        const auto proportion = (float) (point - before);
        return shape[(size_t) before] + proportion * (shape[(size_t) after] - shape[(size_t) before]);
    }

    void addHarmonic (Shape& shape, int harmonic, float amplitude, float phase)
    {
        for (int i = 0; i < numPoints; ++i)
            shape[(size_t) i] += Partial { harmonic, amplitude, phase }.valueAt ((double) i / numPoints);
    }
}
