#include "Tuning.h"

namespace tuning
{
    namespace
    {
        const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
    }

    float toPosition (float semitones)
    {
        return (semitones - lowest) / (highest - lowest);
    }

    float fromPosition (float position)
    {
        return lowest + position * (highest - lowest);
    }

    double snap (double semitones, double distance)
    {
        const auto nearest = std::round (semitones);
        return std::abs (semitones - nearest) <= distance ? nearest : semitones;
    }

    std::optional<int> toWholeSemitones (float semitones)
    {
        const auto nearest = juce::roundToInt (semitones);
        return std::abs (semitones - (float) nearest) < 0.005f ? std::optional (nearest) : std::nullopt;
    }

    juce::String toText (float semitones)
    {
        const auto whole = toWholeSemitones (semitones);

        if (whole == 0)
            return "0 st";

        const auto size = whole.has_value() ? juce::String (std::abs (*whole)) : juce::String (std::abs (semitones), 2);
        return (semitones < 0.0f ? minus : juce::String ("+")) + size + " st";
    }

    juce::String getIntervalName (int semitones)
    {
        static const std::array<const char*, 13> names { "unison", "minor 2nd", "major 2nd", "minor 3rd", "major 3rd",
                                                         "perfect 4th", "tritone", "perfect 5th", "minor 6th", "major 6th",
                                                         "minor 7th", "major 7th", "octave" };

        const juce::String name (names[(size_t) juce::jlimit (0, (int) names.size() - 1, std::abs (semitones))]);
        return semitones < 0 ? name + " below" : name;
    }

    double toRatio (double semitones)
    {
        return std::pow (2.0, semitones / 12.0);
    }

    float zoomToReach (float zoom)
    {
        return widestReach * std::pow (narrowestReach / widestReach, juce::jlimit (0.0f, 1.0f, zoom));
    }

    Window getWindow (float semitones, float reach)
    {
        reach = juce::jlimit (narrowestReach, widestReach, reach);
        const auto middle = std::round (juce::jlimit (lowest, highest, semitones));
        const auto low = juce::jlimit (lowest, highest - 2.0f * reach, middle - reach);
        return { low, low + 2.0f * reach };
    }
}
