#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>

/** The drawn waves, and their breakdown into sine waves. */
namespace waveform
{
    /** How many evenly spaced points describe the one wavelength drawn on the grid. */
    inline constexpr int numPoints = 512;

    /** The most harmonics a wave can be played with: the 1st up to the 9th. */
    inline constexpr int maxHarmonics = 9;

    /** How many harmonics a wave is played with until the user picks another number. */
    inline constexpr int defaultHarmonics = 3;

    /** The highest a harmonic can be raised to. A wave drawn between -1 and 1 can't have a harmonic stronger than
        4/π, which a square wave's fundamental is.
    */
    inline constexpr float maxAmplitude = 1.5f;

    /** One wavelength, as values at evenly spaced points: point i is i / numPoints of the way through the cycle.
        Drawing keeps values between -1 and 1, but raising a harmonic can take a wave beyond that.
    */
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

    /** A shape's first harmonics. It holds no heap memory, so the audio thread can copy it. */
    struct Decomposition
    {
        /** Harmonics 1 to maxHarmonics, in order, so harmonics[0] is the fundamental. A harmonic the shape
            doesn't have has an amplitude of 0, as the even harmonics of a square do.
        */
        std::array<Partial, maxHarmonics> harmonics {};

        /** peaks[n] is the highest the sum of harmonics 1 to n reaches, above or below zero. */
        std::array<float, maxHarmonics + 1> peaks {};
    };

    /** Breaks a shape down into the sine waves at its first harmonics, with a Fourier transform. Any offset from
        zero is left out, as it can't be heard.
    */
    Decomposition decompose (const Shape&);

    /** A shape's value at a point in its cycle, from 0 to 1, read along the straight line between its points. A wave
        that repeats heads from its last point back to its first; one played only once holds its last point's value.
    */
    float valueAt (const Shape&, double position, bool repeats = true) noexcept;

    /** Adds a sine wave at one of a shape's harmonics. As harmonics are independent of each other, this changes
        that harmonic of the shape and no other. A negative amplitude takes the sine wave away.
    */
    void addHarmonic (Shape&, int harmonic, float amplitude, float phase);
}
