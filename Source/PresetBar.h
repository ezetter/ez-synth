#pragma once

#include "SoundDescription.h"
#include "SynthProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Loads and saves presets: arrows to step through the factory presets and those saved, the preset's name, which
    opens a menu of them, and a button to save the sound as a preset. A dot after the name shows the sound has been
    changed since.

    The menu lists the presets by category, each category a submenu of its factory presets, the user's own and the
    sounds from Claude, together by name, each with its label after it. Saving asks for the preset's name, category
    and label.
*/
class PresetBar final : public juce::Component
{
public:
    explicit PresetBar (SynthProcessor&);

    /** Brings the preset's name, and whether it's been edited, up to date with the synth. */
    void refresh();

    /** Asking Claude for a sound to match a sample, which the standalone app's Options menu offers, under AI.
        Experimental. Copying the prompt also saves it in the From Claude folder; pasting loads a sound Claude made
        from the clipboard, and says what it couldn't understand, if anything.
    */
    static void copyPrompt();
    void pasteFromClaude();
    static void showClaudeFolder();

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

    /** Every preset, factory, the user's own and the sounds from Claude, in the order the menu lists them: by
        category, and in each, by name and then label.
    */
    struct Listed
    {
        juce::String category, name, label;
        juce::File file;   // none for a factory preset
        bool fromAI = false;
    };

    static std::vector<Listed> listPresets();
    bool isLoaded (const Listed&) const;
    void loadListed (const Listed&);
    void step (int direction);
    void load (const juce::File&);
    void loadFactory (const juce::String& name);

    /** Loads a sound Claude made from a file, and says what it couldn't understand, if anything. */
    void loadFromClaude (const juce::File&);
    void showLoadResult (const sound::LoadResult&, const juce::String& from);
    static juce::File savePrompt();
    void open();
    /** Asks for the preset's name and category, and saves it among the user's own, asking first before saving over
        another preset.
    */
    void save();
    void saveAs (const juce::String& name, const juce::String& category, const juce::String& label);

    SynthProcessor& synthProcessor;
    StepButton previous { -1 }, next { 1 };
    juce::TextButton saveButton { "Save" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    // The sound from Claude loaded last, and the preset version it made, so the menu can tick it until another loads.
    juce::File loadedClaudeSound;
    int loadedClaudeVersion = -1;

    juce::Rectangle<int> nameArea;
    juce::String shownName;
    bool shownEdited = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
