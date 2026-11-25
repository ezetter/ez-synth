#include "Reverb.h"

#include <algorithm>
#include <cmath>

namespace
{
    // The tail's echoes, in milliseconds, in the largest room, spaced so no two line up often.
    constexpr std::array<float, 8> lineMilliseconds { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };

    // The early reflections off the walls, after the pre-delay, in the largest room, each side its own, and how loud
    // each comes back, the later quieter, some turned over.
    constexpr std::array<float, 6> leftTaps { 7.1f, 13.3f, 19.7f, 27.1f, 36.3f, 47.9f };
    constexpr std::array<float, 6> rightTaps { 8.9f, 15.7f, 23.3f, 31.1f, 41.3f, 53.7f };
    constexpr std::array<float, 6> tapGains { 0.8f, -0.65f, 0.55f, -0.45f, 0.36f, -0.28f };

    // The diffusers that smear what goes into the tail, in milliseconds in the largest room, and how much each
    // smears.
    constexpr std::array<float, 4> diffuserMilliseconds { 4.7f, 3.6f, 5.3f, 3.1f };
    constexpr float diffusion = 0.6f;

    // How the room's size, from 1 to 100, scales everything from the largest room: down to a small box.
    float sizeToScale (float size)
    {
        return 0.08f + 0.92f * (std::clamp (size, 1.0f, 100.0f) / 100.0f);
    }

    // The largest room's echoes, at the largest scale.
    constexpr float largestScale = 1.0f;

    // How loud the reverb is beside what went in: about as loud, for a steady sound.
    constexpr float tailLevel = 0.6f, earlyLevel = 0.35f;

    // How fast the settings glide, per sample, about 50 ms to settle.
    float glideShare (double sampleRate)   { return (float) (1.0 - std::exp (-1.0 / (0.05 * sampleRate))); }
}

void Reverb::Line::write (float value) noexcept
{
    buffer[(size_t) position] = value;

    if (++position == (int) buffer.size())
        position = 0;
}

float Reverb::Line::read (float delaySamples) const noexcept
{
    // Between two samples, by how far between them it falls. The newest written is a sample back.
    const auto size = (int) buffer.size();
    const auto back = std::clamp (delaySamples, 1.0f, (float) (size - 2));
    const auto whole = (int) back;
    const auto fraction = back - (float) whole;
    auto first = position - whole;
    first += first < 0 ? size : 0;
    auto second = first - 1;
    second += second < 0 ? size : 0;
    return buffer[(size_t) first] + fraction * (buffer[(size_t) second] - buffer[(size_t) first]);
}

float Reverb::Diffuser::process (float input, float delaySamples) noexcept
{
    const auto delayed = line.read (delaySamples);
    const auto stored = input + diffusion * delayed;
    line.write (stored);
    return delayed - diffusion * stored;
}

void Reverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    const auto samples = [this] (float milliseconds) { return (size_t) std::ceil (milliseconds * 0.001 * sampleRate) + 4; };

    // The pre-delay, then the latest early reflection after it.
    for (auto* side : { &leftIn, &rightIn })
        side->buffer.assign (samples (longestDelay + rightTaps.back() * largestScale), 0.0f);

    for (size_t i = 0; i < lines.size(); ++i)
        lines[i].buffer.assign (samples (lineMilliseconds[i] * largestScale), 0.0f);

    for (size_t i = 0; i < diffusers.size(); ++i)
    {
        diffusers[i].baseMilliseconds = diffuserMilliseconds[i];
        diffusers[i].line.buffer.assign (samples (diffuserMilliseconds[i] * largestScale), 0.0f);
    }

    reset();
}

void Reverb::reset()
{
    for (auto* line : { &leftIn, &rightIn })
    {
        std::fill (line->buffer.begin(), line->buffer.end(), 0.0f);
        line->position = 0;
    }

    for (auto& line : lines)
    {
        std::fill (line.buffer.begin(), line.buffer.end(), 0.0f);
        line.position = 0;
        line.filtered = 0.0f;
    }

    for (auto& diffuser : diffusers)
    {
        std::fill (diffuser.line.buffer.begin(), diffuser.line.buffer.end(), 0.0f);
        diffuser.line.position = 0;
    }

    glide = false;
    untilUpdate = 0;
    quietSamples = ringingSamples = 0;
}

void Reverb::updateTail()
{
    // Each echo's length, and how much it keeps each time round to fall 60 dB over the decay: its lows, and its
    // highs, which fall twice as fast.
    const auto decay = std::clamp (current.decay, shortestDecay, longestDecay);
    auto meanSquare = 0.0f;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        lengths[i] = lineMilliseconds[i] * scale * 0.001f * (float) sampleRate;
        const auto seconds = lengths[i] / (float) sampleRate;
        const auto low = std::pow (10.0f, -3.0f * seconds / decay);
        const auto high = std::pow (10.0f, -3.0f * seconds / (0.5f * decay));
        const auto ratio = high / low;
        gains[i] = low;
        poles[i] = (1.0f - ratio) / (1.0f + ratio);
        meanSquare += low * low;
    }

    // As much goes in as the tail keeps from fading, so it's about as loud, for a steady sound, however long it
    // rings.
    meanSquare /= (float) lines.size();
    inputGain = std::sqrt (std::max (0.0f, 1.0f - meanSquare));
}

void Reverb::process (float* left, float* right, int numSamples, const Settings& settings)
{
    if (leftIn.buffer.empty())
        return;

    // Rung out, with nothing more sent in, it's silent, and needn't be played.
    const auto silent = [numSamples] (const float* samples)
    {
        return std::all_of (samples, samples + numSamples, [] (float sample) { return sample == 0.0f; });
    };

    // With none of it mixed in, and none still fading out, it's off: what went in comes out as it is. It starts
    // again from silence when it's turned up.
    const auto off = glide && current.mix <= 0.0f && settings.mix <= 0.0f;

    if ((! isRinging() && glide && silent (left) && silent (right)) || off)
    {
        current = settings;
        scale = sizeToScale (settings.size);

        if (off && ! resting)
        {
            reset();
            glide = true;
            resting = true;
            current = settings;
        }

        return;
    }

    resting = false;

    const auto share = glideShare (sampleRate);
    const auto toward = [share] (float& value, float target)
    {
        value += (target - value) * share;

        if (std::abs (target - value) < 1.0e-4f * std::max (1.0f, std::abs (target)))
            value = target;
    };

    if (! glide)
    {
        current = settings;
        scale = sizeToScale (settings.size);
        glide = true;
        untilUpdate = 0;
    }

    const auto targetScale = sizeToScale (settings.size);
    const auto millisecondsToSamples = 0.001f * (float) sampleRate;

    // Rings on for the longest the tail takes to fall away, after the pre-delay and the reflections.
    ringingSamples = (int) ((std::max (current.decay, settings.decay) * 1.5f + 0.3f) * (float) sampleRate);

    for (int s = 0; s < numSamples; ++s)
    {
        toward (current.delay, std::clamp (settings.delay, 0.0f, longestDelay));
        toward (scale, targetScale);
        toward (current.decay, std::clamp (settings.decay, shortestDecay, longestDecay));
        toward (current.mix, std::clamp (settings.mix, 0.0f, 1.0f));

        if (--untilUpdate <= 0)
        {
            updateTail();
            untilUpdate = 16;
        }

        const auto dryLeft = left[s], dryRight = right[s];
        quietSamples = dryLeft != 0.0f || dryRight != 0.0f ? 0 : std::min (quietSamples + 1, ringingSamples);

        leftIn.write (dryLeft);
        rightIn.write (dryRight);

        // After the pre-delay, the early reflections, and what goes into the tail, smeared.
        const auto delay = current.delay * millisecondsToSamples;
        auto earlyLeft = 0.0f, earlyRight = 0.0f;

        for (size_t t = 0; t < tapGains.size(); ++t)
        {
            earlyLeft += tapGains[t] * leftIn.read (delay + leftTaps[t] * scale * millisecondsToSamples);
            earlyRight += tapGains[t] * rightIn.read (delay + rightTaps[t] * scale * millisecondsToSamples);
        }

        auto intoLeft = leftIn.read (delay + 1.0f), intoRight = rightIn.read (delay + 1.0f);
        intoLeft = diffusers[0].process (intoLeft, diffusers[0].baseMilliseconds * scale * millisecondsToSamples);
        intoLeft = diffusers[1].process (intoLeft, diffusers[1].baseMilliseconds * scale * millisecondsToSamples);
        intoRight = diffusers[2].process (intoRight, diffusers[2].baseMilliseconds * scale * millisecondsToSamples);
        intoRight = diffusers[3].process (intoRight, diffusers[3].baseMilliseconds * scale * millisecondsToSamples);

        // The tail: each echo, its highs fading faster, fed back through every other, so it grows dense.
        std::array<float, numLines> echoes {};
        auto sum = 0.0f;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto& line = lines[i];
            line.filtered = gains[i] * (1.0f - poles[i]) * line.read (lengths[i]) + poles[i] * line.filtered;
            echoes[i] = line.filtered;
            sum += echoes[i];
        }

        const auto feedback = 2.0f / (float) numLines * sum;

        for (size_t i = 0; i < lines.size(); ++i)
            lines[i].write (echoes[i] - feedback + inputGain * ((i % 2) == 0 ? intoLeft : intoRight));

        // Each side hears the echoes in its own blend, so the two sides differ, and the reverb is wide.
        const auto tailLeft = echoes[0] - echoes[1] + echoes[2] - echoes[3] + echoes[4] - echoes[5] + echoes[6] - echoes[7];
        const auto tailRight = echoes[0] + echoes[1] - echoes[2] - echoes[3] + echoes[4] + echoes[5] - echoes[6] - echoes[7];

        const auto wetLeft = earlyLevel * earlyLeft + tailLevel * tailLeft;
        const auto wetRight = earlyLevel * earlyRight + tailLevel * tailRight;

        left[s] = (1.0f - current.mix) * dryLeft + current.mix * wetLeft;
        right[s] = (1.0f - current.mix) * dryRight + current.mix * wetRight;
    }
}
