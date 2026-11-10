#pragma once

#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** A screen of its own, over the whole window, listing everything that moves a control or is linked to one: each
    LFO and envelope, velocity and the mod wheel, with what each moves and by how much, and the MIDI controllers
    learned. Clicking a row opens the control's modulation editor; its cross takes the link off. Done, or Escape,
    goes back to the synth.
*/
class ModulationOverview final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit ModulationOverview (SynthProcessor&);

    /** Called with a control and the row it's on, to edit what moves it, and when the screen's closed. */
    std::function<void (int target, juce::Component& row)> onEdit;
    std::function<void()> onClose;

    /** One link, as a row: what it comes from, the control, and what it does. */
    struct Link
    {
        enum class Kind { source, velocity, modWheel, midi };

        Kind kind = Kind::source;
        int source = 0;   // for a source: the LFO or envelope
        int target = 0;
        juce::String description;
    };

    /** Every link, grouped by what it comes from: the LFOs, the envelopes, velocity, the mod wheel, then MIDI
        controllers, each in the controls' order.
    */
    std::vector<Link> getLinks() const;

    /** What a group of links is called, such as "LFO 2", "Velocity" or "MIDI controllers". */
    static juce::String getGroupName (const Link&);

    /** Takes a link off. */
    void remove (const Link&);

    /** Brings the list up to date with the synth. */
    void refresh();

    juce::TextButton doneButton { "Done" };

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    class Row final : public juce::Component
    {
    public:
        Row (ModulationOverview&, Link);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { repaint(); }

        const Link link;
        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };

    private:
        ModulationOverview& overview;
    };

    /** The rows, under a heading for each group, inside the scrolling view. */
    class List final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;

        std::vector<std::pair<juce::String, juce::Rectangle<int>>> headings;
    };

    void timerCallback() override;
    void layOutRows();

    SynthProcessor& synthProcessor;
    juce::Viewport viewport;
    List list;
    std::vector<std::unique_ptr<Row>> rows;
    int shownModulationVersion = -1, shownMidiLearnVersion = -1;
    juce::Rectangle<int> panelArea, titleArea, subtitleArea, emptyArea;

    static constexpr int rowHeight = 30, headingHeight = 34;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationOverview)
};
