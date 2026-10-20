#include "Envelope.h"

void EnvelopeRun::start() noexcept
{
    stage = Stage::attack;
    level = 0.0f;
    releaseLeft = 1.0f;
    released = false;
}

void EnvelopeRun::release() noexcept
{
    if (stage != Stage::idle && stage != Stage::release)
    {
        releaseFrom = level;
        releaseLeft = 1.0f;
        released = true;
        stage = Stage::release;
    }
}

void EnvelopeRun::stop() noexcept
{
    stage = Stage::idle;
    level = 0.0f;
}

void EnvelopeRun::advance (const EnvelopeSettings& settings, double seconds) noexcept
{
    const auto sustain = juce::jlimit (0.0f, 1.0f, settings.sustain);

    // Several stages can pass in one step, when they're short or take no time at all.
    for (;;)
    {
        switch (stage)
        {
            case Stage::idle:
                level = 0.0f;
                return;

            case Stage::sustain:
                // Turning the sustain dial while the key is held moves the level with it.
                level = sustain;
                return;

            case Stage::attack:
            {
                const auto timeLeft = settings.attack > 0.0f ? (1.0 - level) * settings.attack : 0.0;

                if (seconds < timeLeft)
                {
                    level += (float) (seconds / settings.attack);
                    return;
                }

                seconds -= timeLeft;
                level = 1.0f;
                stage = Stage::decay;
                break;
            }

            case Stage::decay:
            {
                // The decay falls from the peak to the sustain level in the time it's set to.
                const auto rate = settings.decay > 0.0f ? (1.0 - sustain) / settings.decay : 0.0;
                const auto timeLeft = level > sustain && rate > 0.0 ? (level - sustain) / rate : 0.0;

                if (seconds < timeLeft)
                {
                    level -= (float) (rate * seconds);
                    return;
                }

                seconds -= timeLeft;
                level = sustain;
                stage = Stage::sustain;
                break;
            }

            case Stage::release:
            {
                // The release falls from wherever the key was let go to 0 in the time it's set to.
                if (settings.release > 0.0f)
                    releaseLeft -= (float) (seconds / settings.release);

                if (settings.release > 0.0f && releaseLeft > 0.0f)
                {
                    level = releaseFrom * releaseLeft;
                    return;
                }

                releaseLeft = 0.0f;
                stop();
                return;
            }
        }
    }
}

float EnvelopeRun::getProgress (const EnvelopeSettings& settings) const noexcept
{
    switch (stage)
    {
        case Stage::idle:     return 0.0f;
        case Stage::attack:   return level;
        case Stage::decay:    return settings.sustain < 1.0f ? juce::jlimit (0.0f, 1.0f, (1.0f - level) / (1.0f - settings.sustain)) : 1.0f;
        case Stage::sustain:  return 0.5f;
        case Stage::release:  return juce::jlimit (0.0f, 1.0f, 1.0f - releaseLeft);
    }

    return 0.0f;
}
