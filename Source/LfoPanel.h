#pragma once

#include "ModulatableControls.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Shown beside the grid in place of the harmonics when an LFO is chosen: a switch to sync its rate to the host's
    tempo, then the controls the LFO moves, and how to give it more.
*/
class LfoPanel final : public juce::Component
{
public:
    explicit LfoPanel (SynthProcessor&);

    void setLfo (int lfo);

    /** Brings the list of controls up to date with the synth. */
    void refresh();

    /** Brings the tempo shown up to date, which the window does many times a second. */
    void showTempo();

    /** The switch that syncs the LFO's rate to the tempo. */
    ModulatableToggle syncSwitch;

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

    std::unique_ptr<juce::ButtonParameterAttachment> syncAttachment;
    juce::String shownTempo;

    juce::Rectangle<int> syncCaptionArea, syncLabelArea, tempoArea, syncHintArea, captionArea, listArea, footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LfoPanel)
};
