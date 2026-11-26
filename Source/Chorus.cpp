#include "Chorus.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    // Saturation that leaves the copy as it is, and rounds off what builds up past 1 at a high feedback.
    float saturate (float value)
    {
        const auto size = std::abs (value);

        if (size <= 1.0f)
            return value;

        return std::copysign (1.0f + std::tanh (size - 1.0f), value);
    }

    // How fast the settings glide, per sample, about 50 ms to settle.
    float glideShare (double sampleRate)   { return (float) (1.0 - std::exp (-1.0 / (0.05 * sampleRate))); }

    // What a line held, a fractional number of samples before the one about to be written, read smoothly
    // between them, so the sweep doesn't dull the top.
    float readBack (const std::vector<float>& line, int position, float back)
    {
        const auto size = (int) line.size();
        const auto whole = (int) std::ceil (back);
        const auto fraction = (float) whole - back;
        const auto at = [&line, size] (int index) { return line[(size_t) (index < 0 ? index + size : index)]; };

        auto index = position - whole;
        index += index < 0 ? size : 0;

        const auto x0 = at (index - 1), x1 = line[(size_t) index], x2 = at ((index + 1) % size), x3 = at ((index + 2) % size);
        return x1 + 0.5f * fraction * (x2 - x0 + fraction * (2.0f * x0 - 5.0f * x1 + 4.0f * x2 - x3
                                                             + fraction * (3.0f * (x1 - x2) + x3 - x0)));
    }
}

void Chorus::prepare (double newSampleRate, int numPairs)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    // The time can swing to twice the longest delay.
    const auto size = (size_t) std::ceil (2.0 * longestDelay * 0.001 * sampleRate) + 8;
    lanes.resize ((size_t) std::max (1, numPairs));

    for (auto& lane : lanes)
    {
        lane.left.assign (size, 0.0f);
        lane.right.assign (size, 0.0f);
    }

    reset();
}

void Chorus::reset()
{
    for (auto& lane : lanes)
    {
        std::fill (lane.left.begin(), lane.left.end(), 0.0f);
        std::fill (lane.right.begin(), lane.right.end(), 0.0f);
        lane.quietSamples = 0;
    }

    position = 0;
    phase = 0.0;
    glide = false;
    resting = false;
    ringingSamples = 0;
}

void Chorus::process (const Pair* pairs, int numPairs, int numSamples, const Settings& settings)
{
    if (lanes.empty())
        return;

    numPairs = std::min (numPairs, (int) lanes.size());

    Settings target = settings;
    target.rate = std::clamp (target.rate, slowestRate, fastestRate);
    target.depth = std::clamp (target.depth, 0.0f, 1.0f);
    target.delay = std::clamp (target.delay, shortestDelay, longestDelay);
    target.feedback = std::clamp (target.feedback, 0.0f, mostFeedback);
    target.mix = std::clamp (target.mix, 0.0f, 1.0f);

    // With none of it mixed in, it's off, and what went in comes out as it is; it starts again from silence when
    // it's turned up.
    if (glide && current.mix <= 0.0f && target.mix <= 0.0f)
    {
        if (! resting)
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

    // The copy goes round until it's fallen 60 dB, each time round at most twice the delay.
    const auto feedback = std::max (current.feedback, target.feedback);
    const auto rounds = feedback <= 0.001f ? 1.0f : 1.0f + 3.0f / -std::log10 (feedback);
    ringingSamples = (int) ((double) rounds * 2.0 * std::max (current.delay, target.delay) * 0.001 * sampleRate + sampleRate * 0.05);

    // A pair that's silent, with nothing still ringing in its line, is skipped, its line emptied once, so it starts
    // from silence when something's sent through it again.
    std::array<Pair, 16> playingPairs {};
    std::array<Lane*, 16> playingLanes {};
    auto numPlaying = 0;

    for (int p = 0; p < std::min (numPairs, (int) playingPairs.size()); ++p)
    {
        auto& lane = lanes[(size_t) p];
        const auto& pair = pairs[p];
        const auto silent = pair.left == nullptr
                         || (std::all_of (pair.left, pair.left + numSamples, [] (float sample) { return sample == 0.0f; })
                             && std::all_of (pair.right, pair.right + numSamples, [] (float sample) { return sample == 0.0f; }));

        if (silent && ! isRinging (p))
        {
            if (lane.quietSamples <= ringingSamples)
            {
                std::fill (lane.left.begin(), lane.left.end(), 0.0f);
                std::fill (lane.right.begin(), lane.right.end(), 0.0f);
                lane.quietSamples = ringingSamples + 1;
            }

            continue;
        }

        playingPairs[(size_t) numPlaying] = pair;
        playingLanes[(size_t) numPlaying] = &lane;
        ++numPlaying;
    }

    const auto share = glideShare (sampleRate);
    const auto toward = [share] (float& value, float to)
    {
        value += (to - value) * share;

        if (std::abs (to - value) < 1.0e-4f * std::max (1.0f, std::abs (to)))
            value = to;
    };

    const auto size = (int) lanes.front().left.size();
    const auto millisecondsToSamples = 0.001f * (float) sampleRate;
    const auto longest = (float) (size - 3);

    for (int s = 0; s < numSamples; ++s)
    {
        toward (current.rate, target.rate);
        toward (current.depth, target.depth);
        toward (current.delay, target.delay);
        toward (current.feedback, target.feedback);
        toward (current.mix, target.mix);

        // The left side's sweep a quarter of a cycle ahead of the right's.
        const auto angle = juce::MathConstants<double>::twoPi * phase;
        const auto sweepLeft = (float) std::cos (angle), sweepRight = (float) std::sin (angle);
        phase += current.rate / sampleRate;
        phase -= phase >= 1.0 ? 1.0 : 0.0;

        const auto centre = current.delay * millisecondsToSamples;
        const auto backLeft = std::clamp (centre * (1.0f + current.depth * sweepLeft), 3.0f, longest);
        const auto backRight = std::clamp (centre * (1.0f + current.depth * sweepRight), 3.0f, longest);

        for (int p = 0; p < numPlaying; ++p)
        {
            auto& lane = *playingLanes[(size_t) p];
            auto* left = playingPairs[(size_t) p].left;
            auto* right = playingPairs[(size_t) p].right;
            const auto dryLeft = left[s], dryRight = right[s];
            lane.quietSamples = dryLeft != 0.0f || dryRight != 0.0f ? 0 : std::min (lane.quietSamples + 1, ringingSamples);

            const auto wetLeft = readBack (lane.left, position, backLeft);
            const auto wetRight = readBack (lane.right, position, backRight);

            lane.left[(size_t) position] = saturate (dryLeft + current.feedback * wetLeft);
            lane.right[(size_t) position] = saturate (dryRight + current.feedback * wetRight);

            left[s] = (1.0f - current.mix) * dryLeft + current.mix * wetLeft;
            right[s] = (1.0f - current.mix) * dryRight + current.mix * wetRight;
        }

        if (++position == size)
            position = 0;
    }
}
