#include "RandomWave.h"

namespace randomwave
{
    namespace
    {
        /** A number from 0 to 1 that's the same for the same seed, index and purpose, and unrelated for any other: the
            SplitMix64 hash.
        */
        double hashToUnit (std::uint32_t seed, std::int64_t index, std::uint64_t purpose)
        {
            auto z = ((std::uint64_t) seed << 32) ^ (std::uint64_t) index * 0x9e3779b97f4a7c15ull ^ purpose * 0xbf58476d1ce4e5b9ull;
            z += 0x9e3779b97f4a7c15ull;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            z ^= z >> 31;
            return (double) (z >> 11) * (1.0 / 9007199254740992.0);
        }

        constexpr std::uint64_t forLevels = 1, forTimes = 2, forSteps = 3;

        /** A level from -1 to 1. */
        double levelFor (std::uint32_t seed, std::int64_t index, std::uint64_t purpose)
        {
            return 2.0 * hashToUnit (seed, index, purpose) - 1.0;
        }

        /** The smooth wave's levels are at random times: level k is somewhere in the middle 60% of its own stretch
            of the wave, so the gaps between them run from 0.4 to 1.6 times their average.
        */
        double timeOfLevel (std::uint32_t seed, std::int64_t k)
        {
            return levelSpacing * ((double) k + 0.2 + 0.6 * hashToUnit (seed, k, forTimes));
        }

        /** How steeply the curve passes through a level, from the levels either side: flat at a peak or a trough,
            and otherwise between the slopes either side, so it never overshoots (Fritsch and Carlson's monotone
            cubic interpolation).
        */
        double slopeAt (std::uint32_t seed, std::int64_t k)
        {
            const auto before = timeOfLevel (seed, k - 1), at = timeOfLevel (seed, k), after = timeOfLevel (seed, k + 1);
            const auto slopeIn = (levelFor (seed, k, forLevels) - levelFor (seed, k - 1, forLevels)) / (at - before);
            const auto slopeOut = (levelFor (seed, k + 1, forLevels) - levelFor (seed, k, forLevels)) / (after - at);

            if (slopeIn * slopeOut <= 0.0)
                return 0.0;

            const auto gapIn = at - before, gapOut = after - at;
            const auto weightIn = 2.0 * gapOut + gapIn, weightOut = gapOut + 2.0 * gapIn;
            return (weightIn + weightOut) / (weightIn / slopeIn + weightOut / slopeOut);
        }

        float smoothValueAt (std::uint32_t seed, double cycles)
        {
            // The levels either side of the time: level k is in its own stretch, so it's this stretch's or the one before's.
            auto k = (std::int64_t) std::floor (cycles / levelSpacing);

            if (cycles < timeOfLevel (seed, k))
                --k;

            const auto start = timeOfLevel (seed, k), end = timeOfLevel (seed, k + 1);
            const auto gap = end - start;
            const auto x = (cycles - start) / gap;

            // The cubic Hermite curve between the two levels, leaving and arriving at their slopes.
            const auto x2 = x * x, x3 = x2 * x;
            return (float) ((2.0 * x3 - 3.0 * x2 + 1.0) * levelFor (seed, k, forLevels)
                          + (x3 - 2.0 * x2 + x) * gap * slopeAt (seed, k)
                          + (-2.0 * x3 + 3.0 * x2) * levelFor (seed, k + 1, forLevels)
                          + (x3 - x2) * gap * slopeAt (seed, k + 1));
        }
    }

    float valueAt (std::uint32_t seed, double cycles, const waveform::Drawing& drawing)
    {
        if (! drawing.stepped)
            return juce::jlimit (-1.0f, 1.0f, smoothValueAt (seed, cycles));

        // Each step of each cycle has its own level.
        return (float) levelFor (seed, (std::int64_t) std::floor (cycles * drawing.numSteps), forSteps);
    }

    float onceValueAt (std::uint32_t seed, double cycles, const waveform::Drawing& drawing)
    {
        // Just short of the end of the cycle is still in its last step.
        return valueAt (seed, juce::jlimit (0.0, std::nextafter (1.0, 0.0), cycles), drawing);
    }

    waveform::Shape getCycle (std::uint32_t seed, std::int64_t cycle, const waveform::Drawing& drawing)
    {
        waveform::Shape shape {};

        for (int point = 0; point < waveform::numPoints; ++point)
            shape[(size_t) point] = valueAt (seed, (double) cycle + (double) point / waveform::numPoints, drawing);

        return shape;
    }
}
