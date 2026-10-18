#pragma once

#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Shown beside the grid in place of the harmonics when an LFO is chosen: the controls the LFO moves, and how to
    give it more.
*/
class LfoPanel final : public juce::Component
{
public:
    explicit LfoPanel (SynthProcessor&);

    void setLfo (int lfo);

    /** Brings the list of controls up to date with the synth. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** One control the LFO moves. Clicking it opens the modulation editor; its cross stops the LFO moving it. */
    class Row final : public juce::Component
    {
    public:
        Row (SynthProcessor&, int target);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };

    private:
        SynthProcessor& synthProcessor;
        const int target;
    };

    static constexpr int rowHeight = 28;

    SynthProcessor& synthProcessor;
    int lfo = 0;
    std::vector<std::unique_ptr<Row>> rows;
    int numHidden = 0;

    juce::Rectangle<int> captionArea, listArea, footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LfoPanel)
};
