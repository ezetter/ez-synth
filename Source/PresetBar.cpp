#include "PresetBar.h"

#include "SoundDescription.h"
#include "SoundPrompt.h"
#include "SynthLookAndFeel.h"

PresetBar::StepButton::StepButton (int stepDirection)
    : Button (stepDirection < 0 ? "Previous preset" : "Next preset"),
      direction (stepDirection)
{
}

void PresetBar::StepButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, colours::panel, highlighted, down);

    const auto centre = getLocalBounds().toFloat().getCentre();
    const auto size = 4.5f;
    const auto pointing = (float) direction;
    const auto tipX = centre.x + pointing * size / 2.0f;

    juce::Path chevron;
    chevron.startNewSubPath (tipX - pointing * size, centre.y - size);
    chevron.lineTo (tipX, centre.y);
    chevron.lineTo (tipX - pointing * size, centre.y + size);

    g.setColour (colours::text);
    g.strokePath (chevron, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

PresetBar::PresetBar (SynthProcessor& processor)
    : synthProcessor (processor)
{
    previous.setTooltip ("The preset before");
    next.setTooltip ("The next preset");
    saveButton.setTooltip ("Save the sound as a preset");

    previous.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    saveButton.onClick = [this] { save(); };

    for (auto* button : std::initializer_list<juce::Button*> { &previous, &next, &saveButton })
    {
        button->setMouseClickGrabsKeyboardFocus (false);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);
    refresh();
}

void PresetBar::refresh()
{
    const auto name = synthProcessor.getPresetName();
    const auto edited = synthProcessor.isPresetEdited();

    if (name != shownName || edited != shownEdited)
    {
        shownName = name;
        shownEdited = edited;
        repaint (nameArea);
    }
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    saveButton.setBounds (area.removeFromRight (64));
    area.removeFromRight (8);
    previous.setBounds (area.removeFromLeft (30));
    next.setBounds (area.removeFromRight (30));
    area.reduce (4, 0);
    nameArea = area;
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto bounds = nameArea.toFloat().reduced (0.5f);
    g.setColour (isMouseOver() && nameArea.contains (getMouseXYRelative()) ? colours::panel.brighter (0.12f) : colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    auto area = nameArea.reduced (10, 0);
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("PRESET", area.removeFromLeft (52), juce::Justification::centredLeft);

    // A small downward triangle says the name opens a menu.
    const auto arrowArea = area.removeFromRight (12).toFloat();
    juce::Path arrow;
    arrow.addTriangle (arrowArea.getCentreX() - 4.0f, arrowArea.getCentreY() - 2.0f,
                       arrowArea.getCentreX() + 4.0f, arrowArea.getCentreY() - 2.0f,
                       arrowArea.getCentreX(), arrowArea.getCentreY() + 3.0f);
    g.fillPath (arrow);
    area.removeFromRight (6);

    const auto font = juce::Font (juce::FontOptions (14.0f, juce::Font::bold));
    g.setFont (font);
    g.setColour (colours::text);

    // An edited preset has a dot after its name.
    const auto dotSpace = shownEdited ? 14 : 0;
    const auto nameWidth = juce::jmin (area.getWidth() - dotSpace,
                                       juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, shownName)) + 2);
    const auto textArea = area.removeFromLeft (nameWidth);
    g.drawText (shownName, textArea, juce::Justification::centredLeft, true);

    if (shownEdited)
    {
        g.setColour (colours::accent);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) textArea.getRight() + 8.0f,
                                                                         (float) textArea.getCentreY() }));
    }
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    if (nameArea.contains (e.getPosition()))
        showMenu();
}

void PresetBar::mouseMove (const juce::MouseEvent&)
{
    repaint (nameArea);
}

void PresetBar::mouseExit (const juce::MouseEvent&)
{
    repaint (nameArea);
}

std::vector<PresetBar::Listed> PresetBar::listPresets()
{
    std::vector<Listed> all;

    for (const auto& name : SynthProcessor::getFactoryPresetNames())
        all.push_back ({ SynthProcessor::getFactoryPresetCategory (name), name, SynthProcessor::factoryLabel, {}, false });

    for (const auto& file : SynthProcessor::findPresets())
        all.push_back ({ SynthProcessor::getPresetCategory (file), file.getFileNameWithoutExtension(),
                         SynthProcessor::getPresetLabel (file), file, false });

    for (const auto& file : sound::findClaudeSounds())
        all.push_back ({ SynthProcessor::otherCategory, file.getFileNameWithoutExtension(), SynthProcessor::aiLabel, file, true });

    // Category by category, and in each, by name, numbers counted as numbers, then by label.
    const auto categories = SynthProcessor::getPresetCategories();

    std::stable_sort (all.begin(), all.end(), [&categories] (const Listed& a, const Listed& b)
    {
        if (a.category != b.category)
            return categories.indexOf (a.category) < categories.indexOf (b.category);

        if (const auto byName = a.name.compareNatural (b.name); byName != 0)
            return byName < 0;

        return a.label.compareNatural (b.label) < 0;
    });

    return all;
}

bool PresetBar::isLoaded (const Listed& preset) const
{
    if (preset.fromAI)
        return preset.file == loadedClaudeSound && synthProcessor.getPresetVersion() == loadedClaudeVersion;

    if (preset.file == juce::File())
        return synthProcessor.isFactoryPreset() && preset.name == synthProcessor.getPresetName();

    return preset.file == synthProcessor.getPresetFile();
}

void PresetBar::loadListed (const Listed& preset)
{
    if (preset.fromAI)
        loadFromClaude (preset.file);
    else if (preset.file == juce::File())
        loadFactory (preset.name);
    else
        load (preset.file);
}

void PresetBar::showMenu()
{
    const auto current = synthProcessor.getPresetFile();
    juce::PopupMenu menu;

    menu.addItem ("Init", true, current == juce::File() && shownName == SynthProcessor::initPresetName, [this]
    {
        synthProcessor.loadInitPreset();
        refresh();
    });

    menu.addSeparator();

    // Each category's presets, factory, the user's own and Claude's together, each labelled, the category of the
    // one loaded ticked.
    const auto listed = listPresets();

    for (const auto& category : SynthProcessor::getPresetCategories())
    {
        juce::PopupMenu presets;
        auto holdsCurrent = false;

        for (const auto& preset : listed)
        {
            if (preset.category != category)
                continue;

            const auto ticked = isLoaded (preset);
            holdsCurrent = holdsCurrent || ticked;
            presets.addItem (preset.name + " (" + preset.label + ")", true, ticked, [this, preset] { loadListed (preset); });
        }

        if (presets.getNumItems() == 0)
            presets.addItem ("No presets yet", false, false, nullptr);

        menu.addSubMenu (category, presets, true, nullptr, holdsCurrent);
    }

    menu.addSeparator();
    menu.addItem ("Open a preset file...", [this] { open(); });
    menu.addItem ("Show the presets folder", []
    {
        const auto folder = SynthProcessor::getPresetFolder();
        folder.createDirectory();
        folder.startAsProcess();
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (localAreaToGlobal (nameArea))
                                                  .withMinimumWidth (nameArea.getWidth()));
}

void PresetBar::step (int direction)
{
    // Stepping goes through the presets as the menu lists them, category by category, and round again.
    const auto listed = listPresets();
    const auto count = (int) listed.size();

    if (count == 0)
        return;

    auto index = -1;

    for (int i = 0; i < count; ++i)
        if (isLoaded (listed[(size_t) i]))
            index = i;

    // From Init, or a preset that isn't among them, stepping starts at one end of the list.
    const auto nextIndex = index < 0 ? (direction > 0 ? 0 : count - 1)
                                     : (index + direction + count) % count;
    loadListed (listed[(size_t) nextIndex]);
}

void PresetBar::load (const juce::File& file)
{
    if (! synthProcessor.loadPreset (file))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the preset",
                                                file.getFullPathName() + " isn't an EZ Synth preset.");

    refresh();
}

juce::File PresetBar::savePrompt()
{
    const auto folder = sound::getClaudeFolder();
    folder.createDirectory();
    const auto file = folder.getChildFile ("Prompt for Claude.md");
    file.replaceWithText (prompt::make());
    return file;
}

void PresetBar::copyPrompt()
{
    const auto file = savePrompt();
    juce::SystemClipboard::copyTextToClipboard (file.loadFileAsString());

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "The prompt for Claude is copied",
        "Start a new Claude Code conversation, paste the prompt, and give it your sample, such as:\n\n"
        "  The sample is ~/Music/cello.wav\n\n"
        "It's saved too, as " + file.getFullPathName() + ", so you can tell Claude to read that instead.\n\n"
        "Claude saves the sound it makes in " + file.getParentDirectory().getFullPathName()
        + ", and it's listed in the preset menu, under Other, labelled " + SynthProcessor::aiLabel + ".");
}

void PresetBar::showLoadResult (const sound::LoadResult& result, const juce::String& from)
{
    if (! result.loaded)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the sound",
                                                from + " isn't a sound from Claude. " + result.problems.joinIntoString (" "));
    else if (! result.problems.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Loaded, with some things left out",
                                                "Parts of the sound couldn't be understood, so they were left as Init has them:\n\n- "
                                                    + result.problems.joinIntoString ("\n- "));

    refresh();
}

void PresetBar::loadFromClaude (const juce::File& file)
{
    showLoadResult (sound::loadFile (synthProcessor, file), file.getFileName());
    loadedClaudeSound = file;
    loadedClaudeVersion = synthProcessor.getPresetVersion();
}

void PresetBar::showClaudeFolder()
{
    savePrompt();
    sound::getClaudeFolder().startAsProcess();
}

void PresetBar::pasteFromClaude()
{
    showLoadResult (sound::load (synthProcessor, juce::SystemClipboard::getTextFromClipboard()), "What's on the clipboard");
}

void PresetBar::loadFactory (const juce::String& name)
{
    synthProcessor.loadFactoryPreset (name);
    refresh();
}

void PresetBar::open()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a preset", SynthProcessor::getPresetFolder(),
                                                   juce::String ("*") + SynthProcessor::presetExtension);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& finished)
                          {
                              if (const auto file = finished.getResult(); file != juce::File())
                                  load (file);
                          });
}

void PresetBar::save()
{
    // Saving suggests the current preset's name, category and label, so saving again over it is quick. Init isn't saved
    // over, and a factory preset is saved as a copy among the user's own, under its name and in its category.
    const auto name = shownName == SynthProcessor::initPresetName ? juce::String ("New preset") : shownName;
    const auto categories = SynthProcessor::getPresetCategories();

    saveWindow = std::make_unique<juce::AlertWindow> ("Save the sound as a preset", "", juce::MessageBoxIconType::NoIcon);
    saveWindow->addTextEditor ("name", name, "Name");
    saveWindow->addComboBox ("category", categories, "Category");
    saveWindow->getComboBoxComponent ("category")->setSelectedItemIndex (juce::jmax (0, categories.indexOf (synthProcessor.getPresetCategory())),
                                                                         juce::dontSendNotification);
    const auto presetFile = synthProcessor.getPresetFile();
    saveWindow->addTextEditor ("label", presetFile.existsAsFile() ? SynthProcessor::getPresetLabel (presetFile)
                                                                  : juce::String (SynthProcessor::customLabel), "Label");
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    if (auto* editor = saveWindow->getTextEditor ("name"))
    {
        editor->selectAll();
        editor->grabKeyboardFocus();
    }

    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        const auto chosenName = juce::File::createLegalFileName (saveWindow->getTextEditorContents ("name").trim());
        const auto chosenCategory = saveWindow->getComboBoxComponent ("category")->getText();
        const auto chosenLabel = saveWindow->getTextEditorContents ("label").trim();
        saveWindow.reset();

        if (result == 1 && chosenName.isNotEmpty())
            saveAs (chosenName, chosenCategory, chosenLabel);
    }));
}

void PresetBar::saveAs (const juce::String& name, const juce::String& category, const juce::String& label)
{
    const auto folder = SynthProcessor::getPresetFolder();
    folder.createDirectory();
    const auto file = folder.getChildFile (name + SynthProcessor::presetExtension);

    const auto write = [this, file, category, label]
    {
        if (! synthProcessor.savePreset (file, category, label))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save the preset",
                                                    "EZ Synth couldn't write " + file.getFullPathName() + ".");

        refresh();
    };

    // Saving over the preset loaded is what Save is for; saving over another one asks first.
    if (! file.existsAsFile() || file == synthProcessor.getPresetFile())
    {
        write();
        return;
    }

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Replace \"" + name + "\"?",
                                        "You already have a preset called \"" + name + "\". Saving replaces it.",
                                        "Replace", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([write] (int result)
                                        {
                                            if (result == 1)
                                                write();
                                        }));
}
