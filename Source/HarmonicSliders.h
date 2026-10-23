#pragma once

#include "ModulatableControls.h"
#include "Waveform.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

/** A slider for each harmonic the oscillator plays, to raise or lower it, with its strength and, once a note has
    been played, its frequency. Beside each harmonic above the fundamental, a small dial tunes it away from its whole
    number, up to halfway to the harmonics either side, for inharmonic sounds. Beside every harmonic, another sets its
    phase, from 0 round to a whole turn. Above them, a key to the grid's lines.
*/
class HarmonicSliders final : public juce::Component
{
public:
    HarmonicSliders();

    /** Called as a slider is dragged, with the harmonic, numbered from 1, and its new amplitude. */
    std::function<void (int harmonic, float amplitude)> onAmplitudeChange;

    /** Called as a tune dial is turned, with the harmonic, numbered from 2, and how far it's now tuned, in harmonics. */
    std::function<void (int harmonic, float tune)> onTuneChange;

    /** Called as a phase dial is turned, with the harmonic, numbered from 1, and its new phase, in radians. */
    std::function<void (int harmonic, float phase)> onPhaseChange;

    /** The harmonics the oscillator plays, from the 1st up. */
    void setHarmonics (std::vector<waveform::Partial>);

    /** Says which oscillator's harmonics the sliders set, so an LFO can be assigned to each. */
    void setOscillator (int oscillator);

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

    /** Where each part goes. A row is split into its colour, number, tune and phase dials, slider, amplitude and
        frequency.
    */
    Layout getLayout() const;

    std::array<ModulatableSlider, waveform::maxHarmonics> sliders;

    /** The tune dials, from the 1st harmonic up, though the 1st can't be tuned and its dial is never shown. */
    std::array<ModulatableSlider, waveform::maxHarmonics> tuners;

    /** The phase dials, in degrees, from the 1st harmonic up. */
    std::array<ModulatableSlider, waveform::maxHarmonics> phaseDials;
    std::vector<waveform::Partial> harmonics;
    std::optional<double> fundamental;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicSliders)
};
