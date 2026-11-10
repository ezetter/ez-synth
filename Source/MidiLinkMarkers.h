#pragma once

#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Laid over the whole window, a small tag at the top right of each control linked to a MIDI controller, with its
    number, such as "74", and one reading "MIDI" on the control waiting to learn one. It never takes the mouse, so
    the controls under it work as ever.
*/
class MidiLinkMarkers final : public juce::Component
{
public:
    explicit MidiLinkMarkers (SynthProcessor&);

    /** Repaints where the tags have changed since last time: a link learned or forgotten, learning started or
        stopped, or the controls moved, shown or hidden. The window does this many times a second.
    */
    void refresh();

    /** The tags to draw, each with where it goes in this component and what it says, for the controls shown. */
    struct Marker
    {
        juce::Rectangle<float> area;
        juce::String text;
        bool learning = false;

        bool operator== (const Marker&) const = default;
    };

    std::vector<Marker> getMarkers() const;

    void paint (juce::Graphics&) override;

private:
    SynthProcessor& synthProcessor;
    std::vector<Marker> shown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLinkMarkers)
};
