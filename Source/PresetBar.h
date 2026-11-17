#pragma once

#include "SoundDescription.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Loads and saves presets: arrows to step through the factory presets and those saved, the preset's name, which
    opens a menu of them, and a button to save the sound as a preset. A dot after the name shows the sound has been
    changed since.

    The menu lists the presets by category, each category a submenu of its factory presets and then the user's own,
    then all the user's own. Saving asks for the preset's name and its category.
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

    /** Every preset, factory and the user's own, in the order the menu lists them: by category, and in each, the
        factory presets and then the user's own, each by name.
    */
    struct Listed
    {
        juce::String category, name;
        juce::File file;   // none for a factory preset
    };

    static std::vector<Listed> listPresets();
    void step (int direction);
    void load (const juce::File&);
    void loadFactory (const juce::String& name);

    /** Loads a sound Claude made, from a file or the clipboard, and says what it couldn't understand, if anything. */
    void loadFromClaude (const juce::File&);
    void pasteFromClaude();
    void showLoadResult (const sound::LoadResult&, const juce::String& from);

    /** Copies the prompt for Claude, and saves it in the From Claude folder, which saving it returns. */
    static void copyPrompt();
    static juce::File savePrompt();
    void open();
    /** Asks for the preset's name and category, and saves it among the user's own, asking first before saving over
        another preset.
    */
    void save();
    void saveAs (const juce::String& name, const juce::String& category);

    SynthProcessor& synthProcessor;
    StepButton previous { -1 }, next { 1 };
    juce::TextButton saveButton { "Save" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    juce::Rectangle<int> nameArea;
    juce::String shownName;
    bool shownEdited = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
