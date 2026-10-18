#pragma once

#include "Waveform.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <vector>

/** Says what each line on the grid is: the drawn wave, the sum, and each sine wave with its harmonic,
    its strength and, once a note has been played, its frequency.
*/
class Legend final : public juce::Component
{
public:
    Legend();

    /** The sine waves being played, strongest first, and how many were asked for. */
    void setPartials (std::vector<waveform::Partial>, int numRequested);

    /** The pitch of the last note played, which is the frequency of the first harmonic. */
    void setFundamental (std::optional<double> hertz);

    void paint (juce::Graphics&) override;

private:
    std::vector<waveform::Partial> partials;
    int numRequested = 0;
    std::optional<double> fundamental;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Legend)
};
