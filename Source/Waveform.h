#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>

/** The wave drawn on the grid, and its breakdown into sine waves. */
namespace waveform
{
    /** How many evenly spaced points describe the one wavelength drawn on the grid. */
    inline constexpr int numPoints = 512;

    /** The highest harmonic the breakdown looks for. */
    inline constexpr int maxHarmonic = 64;

    /** The most sine waves a wave can be played with. */
    inline constexpr int maxPartials = 9;

    /** How many sine waves a wave is played with until the user picks another number. */
    inline constexpr int defaultPartials = 3;

    /** One wavelength, as values from -1 to 1. Point i is i / numPoints of the way through the cycle. */
    using Shape = std::array<float, numPoints>;

    enum class Preset { sine, sawtooth, square, triangle };

    inline constexpr std::array allPresets { Preset::sine, Preset::sawtooth, Preset::square, Preset::triangle };

    Shape makePreset (Preset);
    juce::String getName (Preset);
    std::optional<Preset> findPreset (const juce::String& name);

    /** One of the sine waves a shape is made of: amplitude * sin (2π * harmonic * t + phase), where t goes
        from 0 to 1 over the shape's wavelength.
    */
    struct Partial
    {
        int harmonic = 1;
        float amplitude = 0.0f;
        float phase = 0.0f;

        float valueAt (double t) const noexcept;
    };

    /** A shape's strongest harmonics. It holds no heap memory, so the audio thread can copy it. */
    struct Decomposition
    {
        /** The strongest harmonics, strongest first. Ones too faint to matter are left out, so a sine has just one. */
        std::array<Partial, maxPartials> strongest {};
        int numStrongest = 0;

        /** peaks[n] is the highest the sum of the n strongest harmonics reaches, above or below zero. */
        std::array<float, maxPartials + 1> peaks {};

        /** How many sine waves asking for `count` of them gives, as some shapes have fewer. */
        int getNumUsed (int count) const noexcept   { return juce::jlimit (0, numStrongest, count); }
    };

    /** Breaks a shape down into the sine waves at its harmonics, with a Fourier transform. Any offset from zero
        is left out, as it can't be heard.
    */
    Decomposition decompose (const Shape&);
}
