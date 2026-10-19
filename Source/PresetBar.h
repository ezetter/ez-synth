#pragma once

#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Loads and saves presets: arrows to step through the factory presets and those saved, the preset's name, which
    opens a menu of them, and a button to save the sound as a preset. A dot after the name shows the sound has been
    changed since.
*/
class PresetBar final : public juce::Component
{
public:
    explicit PresetBar (SynthProcessor&);

    /** Brings the preset's name, and whether it's been edited, up to date with the synth. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    /** A button with a chevron pointing back or forward, for stepping through the presets. */
    class StepButton final : public juce::Button
    {
    public:
        explicit StepButton (int direction);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        const int direction;
    };

    void showMenu();
    void step (int direction);
    void load (const juce::File&);
    void loadFactory (const juce::String& name);
    void open();
    void save();

    SynthProcessor& synthProcessor;
    StepButton previous { -1 }, next { 1 };
    juce::TextButton saveButton { "Save" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> nameArea;
    juce::String shownName;
    bool shownEdited = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
