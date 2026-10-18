#pragma once

#include "Waveform.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

/** A slider for each harmonic the oscillator plays, to raise or lower it, with its strength and, once a note has
    been played, its frequency. Above them, a key to the grid's lines.
*/
class HarmonicSliders final : public juce::Component
{
public:
    HarmonicSliders();

    /** Called as a slider is dragged, with the harmonic, numbered from 1, and its new amplitude. */
    std::function<void (int harmonic, float amplitude)> onAmplitudeChange;

    /** The harmonics the oscillator plays, from the 1st up. */
    void setHarmonics (std::vector<waveform::Partial>);

    /** The pitch of the last note played, which is the frequency of the first harmonic. */
    void setFundamental (std::optional<double> hertz);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Layout
    {
        juce::Rectangle<int> waveKey, sumKey, divider, headings;
        std::array<juce::Rectangle<int>, waveform::maxHarmonics> rows;
    };

    /** Where each part goes. A row is split into its colour, number, slider, amplitude and frequency. */
    Layout getLayout() const;

    std::array<juce::Slider, waveform::maxHarmonics> sliders;
    std::vector<waveform::Partial> harmonics;
    std::optional<double> fundamental;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSliders)
};
