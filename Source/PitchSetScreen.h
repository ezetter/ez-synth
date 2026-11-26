#pragma once

#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** A screen of its own, over the window but for the keyboard, listing the pitch sets: the default, which plays every
    key no other takes in, and each other, with its range of keys. Clicking a pitch set makes it the active one, which
    the synth's controls show and set. A new pitch set, or a new range for one, is set by pressing its lowest key and
    then its highest, on a MIDI keyboard or the one on screen. A bar across the top shows which pitch set plays each
    key. It's the Pitch Sets tab's; Escape goes back to the synth.
*/
class PitchSetScreen final : public juce::Component,
                             private juce::Timer
{
public:
    explicit PitchSetScreen (SynthProcessor&);

    /** Called when the screen's closed, and as it starts and stops waiting for keys, so the window can let the
        computer's keys play the keyboard.
    */
    std::function<void()> onClose;
    std::function<void (bool waiting)> onWaitingForKeys;

    /** Starts waiting for two keys: for a new pitch set's range, or, given one, a new range for it. */
    void startNewPitchSet()             { startWaiting (-1); }
    void startSettingKeys (int index)   { startWaiting (index); }
    void cancelWaiting();

    /** Whether it's waiting for a range's lowest key, or its highest, having had the lowest. */
    bool isWaitingForKeys() const noexcept   { return waiting != Waiting::no; }

    /** Takes a key as if it had been pressed, as the timer does with the keys played. */
    void keyPlayed (int note);

    /** Brings the list up to date with the synth. */
    void refresh();

    /** A colour for each pitch set, by where it is among them, the default grey. */
    static juce::Colour getColour (int index);

    juce::TextButton newButton { "New Pitch Set" }, cancelButton { "Cancel" };

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    class Row final : public juce::Component
    {
    public:
        Row (PitchSetScreen&, int index);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { repaint(); }

        const int index;
        juce::TextButton keysButton { "Set Keys" }, deleteButton { "Delete" };

    private:
        PitchSetScreen& screen;
    };

    enum class Waiting { no, lowest, highest };

    void startWaiting (int index);
    void timerCallback() override;
    void layOutRows();
    void paintKeyMap (juce::Graphics&) const;

    SynthProcessor& synthProcessor;
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<Row>> rows;

    Waiting waiting = Waiting::no;
    int waitingFor = -1;   // the pitch set whose keys are being set, or -1 for a new one
    int lowestKey = 0;
    int seenNoteOns = 0;
    juce::String message;

    int shownVersion = -1;
    juce::Rectangle<int> panelArea, titleArea, subtitleArea, keyMapArea, promptArea;

    static constexpr int rowHeight = 40;
    static constexpr int firstMappedKey = 24, lastMappedKey = 108;   // C1 to C8

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchSetScreen)
};
