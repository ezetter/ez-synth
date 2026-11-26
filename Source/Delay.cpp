#include "Delay.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct Division
    {
        const char* name;
        double beats;
    };

    // Note lengths, in beats of a quarter note: plain, triplet (two thirds) and dotted (one and a half).
    constexpr std::array<Division, 15> divisions { {
        { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16D", 0.375 },
        { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8D", 0.75 },
        { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 }, { "1/4D", 1.5 },
        { "1/2T", 4.0 / 3.0 }, { "1/2", 2.0 }, { "1/2D", 3.0 },
        { "1 bar", 4.0 }, { "2 bars", 8.0 }
    } };

    // Saturation that leaves quiet repeats as they are, and rounds off loud ones short of 2.
    float saturate (float value)
    {
        const auto size = std::abs (value);

        if (size <= 1.0f)
            return value;

        return std::copysign (1.0f + std::tanh (size - 1.0f), value);
    }

    // How fast the settings glide, per sample, about 50 ms to settle.
    float glideShare (double sampleRate)   { return (float) (1.0 - std::exp (-1.0 / (0.05 * sampleRate))); }
}

juce::StringArray Delay::getDivisionNames()
{
    juce::StringArray names;

    for (const auto& division : divisions)
        names.add (division.name);

    return names;
}

double Delay::getDivisionBeats (int division)
{
    return divisions[(size_t) juce::jlimit (0, (int) divisions.size() - 1, division)].beats;
}

void Delay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    const auto size = (size_t) std::ceil (longestTime * 0.001 * sampleRate) + 4;
    leftLine.assign (size, 0.0f);
    rightLine.assign (size, 0.0f);
    reset();
}

void Delay::reset()
{
    std::fill (leftLine.begin(), leftLine.end(), 0.0f);
    std::fill (rightLine.begin(), rightLine.end(), 0.0f);
    position = 0;
    glide = false;
    resting = false;
    quietSamples = ringingSamples = 0;
}

void Delay::process (float* left, float* right, int numSamples, const Settings& settings)
{
    if (leftLine.empty())
        return;

    Settings target = settings;
    target.time = std::clamp (target.time, shortestTime, longestTime);
    target.feedback = std::clamp (target.feedback, 0.0f, 1.0f);
    target.mix = std::clamp (target.mix, 0.0f, 1.0f);

    const auto silent = [numSamples] (const float* samples)
    {
        return std::all_of (samples, samples + numSamples, [] (float sample) { return sample == 0.0f; });
    };

    // With none of it mixed in, it's off, and what went in comes out as it is; it starts again from silence when
    // it's turned up. Once its repeats have died away, with nothing more sent in, it's silent too.
    const auto off = glide && current.mix <= 0.0f && target.mix <= 0.0f;

    if (off || (glide && ! isRinging() && silent (left) && silent (right)))
    {
        if (off && ! resting)
        {
            reset();
            resting = true;
        }

        current = target;
        glide = true;
        return;
    }

    resting = false;

    if (! glide)
    {
        current = target;
        glide = true;
    }

    // Repeats fall 60 dB after as many as it takes at the feedback set; at 100%, they never do.
    const auto feedback = std::max (current.feedback, target.feedback);
    const auto repeats = feedback >= 0.999f ? 1.0e6f : 1.0f + 3.0f / std::max (0.01f, -std::log10 (std::max (feedback, 1.0e-3f)));
    ringingSamples = (int) std::min (2.0e9, (double) repeats * std::max (current.time, target.time) * 0.001 * sampleRate + sampleRate * 0.05);

    const auto share = glideShare (sampleRate);
    const auto toward = [share] (float& value, float to)
    {
        value += (to - value) * share;

        if (std::abs (to - value) < 1.0e-4f * std::max (1.0f, std::abs (to)))
            value = to;
    };

    const auto size = (int) leftLine.size();
    const auto millisecondsToSamples = 0.001f * (float) sampleRate;

    for (int s = 0; s < numSamples; ++s)
    {
        toward (current.time, target.time);
        toward (current.feedback, target.feedback);
        toward (current.mix, target.mix);

        const auto dryLeft = left[s], dryRight = right[s];
        quietSamples = dryLeft != 0.0f || dryRight != 0.0f ? 0 : std::min (quietSamples + 1, ringingSamples);

        // What went in the delay's time ago, read between samples, so the time can glide.
        const auto back = std::clamp (current.time * millisecondsToSamples, 1.0f, (float) (size - 2));
        const auto whole = (int) back;
        const auto fraction = back - (float) whole;
        auto first = position - whole;
        first += first < 0 ? size : 0;
        auto second = first - 1;
        second += second < 0 ? size : 0;

        const auto wetLeft = leftLine[(size_t) first] + fraction * (leftLine[(size_t) second] - leftLine[(size_t) first]);
        const auto wetRight = rightLine[(size_t) first] + fraction * (rightLine[(size_t) second] - rightLine[(size_t) first]);

        leftLine[(size_t) position] = saturate (dryLeft + current.feedback * wetLeft);
        rightLine[(size_t) position] = saturate (dryRight + current.feedback * wetRight);

        if (++position == size)
            position = 0;

        left[s] = (1.0f - current.mix) * dryLeft + current.mix * wetLeft;
        right[s] = (1.0f - current.mix) * dryRight + current.mix * wetRight;
    }
}
