#pragma once

#include "Waveform.h"

#include <cstdint>

/** The random waves an LFO can play instead of its drawn one: a new wave every cycle, or for an LFO that runs once, a
    new one with every key. A random wave is worked out from a seed and the time, in cycles, so the same seed always
    gives the same wave, and any moment of it can be found straight away, even one from the cycle before.

    Drawn smoothly, it's a curve that rises and falls through random levels, turning smoothly at each, at random times
    from 0.08 to 0.32 of a cycle apart, so its rises and falls last different lengths, as a curve drawn by hand might.
    It runs on from one cycle into the next, so each cycle starts where the last one ended, and never goes beyond -1
    or 1. Drawn in steps, each step is held at a level from -1 to 1, every level as likely as any other, the first
    and last steps of a cycle as much as the others.
*/
namespace randomwave
{
    /** The wave at a time, in cycles from when it started. */
    float valueAt (std::uint32_t seed, double cycles, const waveform::Drawing&);

    /** The wave of an LFO that runs once: its first cycle, starting at its start and holding its end. */
    float onceValueAt (std::uint32_t seed, double cycles, const waveform::Drawing&);

    /** One cycle of the wave, numbered from 0, for showing. */
    waveform::Shape getCycle (std::uint32_t seed, std::int64_t cycle, const waveform::Drawing&);

    /** How far apart the smooth wave's levels are, on average, in cycles. */
    inline constexpr double levelSpacing = 0.2;
}
