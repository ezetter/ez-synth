#include "PresetBar.h"

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
    menu.addSectionHeader ("Factory");

    for (const auto& name : SynthProcessor::getFactoryPresetNames())
        menu.addItem (name, true, synthProcessor.isFactoryPreset() && name == synthProcessor.getPresetName(), [this, name] { loadFactory (name); });

    menu.addSeparator();
    menu.addSectionHeader ("Your presets");

    const auto presets = SynthProcessor::findPresets();

    if (presets.isEmpty())
        menu.addItem ("No saved presets yet", false, false, nullptr);

    for (const auto& file : presets)
        menu.addItem (file.getFileNameWithoutExtension(), true, file == current, [this, file] { load (file); });

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
    // Stepping goes through the factory presets, then the user's own, and round again.
    const auto factory = SynthProcessor::getFactoryPresetNames();
    const auto saved = SynthProcessor::findPresets();
    const auto count = factory.size() + saved.size();

    if (count == 0)
        return;

    auto index = -1;

    if (synthProcessor.isFactoryPreset())
        index = factory.indexOf (synthProcessor.getPresetName());
    else if (const auto savedIndex = saved.indexOf (synthProcessor.getPresetFile()); savedIndex >= 0)
        index = factory.size() + savedIndex;

    // From Init, or a preset that isn't among them, stepping starts at one end of the list.
    const auto nextIndex = index < 0 ? (direction > 0 ? 0 : count - 1)
                                     : (index + direction + count) % count;

    if (nextIndex < factory.size())
        loadFactory (factory[nextIndex]);
    else
        load (saved[nextIndex - factory.size()]);
}

void PresetBar::load (const juce::File& file)
{
    if (! synthProcessor.loadPreset (file))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load the preset",
                                                file.getFullPathName() + " isn't an EZ Synth preset.");

    refresh();
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
    const auto folder = SynthProcessor::getPresetFolder();
    folder.createDirectory();

    // Saving suggests the current preset's name, so saving again over it is quick. Init isn't saved over, and a
    // factory preset, which has no file, is saved as a copy among the user's own, under its name.
    const auto name = shownName == SynthProcessor::initPresetName ? juce::String ("New preset") : shownName;
    const auto suggested = synthProcessor.getPresetFile() != juce::File() ? synthProcessor.getPresetFile()
                                                                          : folder.getChildFile (name + SynthProcessor::presetExtension);

    chooser = std::make_unique<juce::FileChooser> ("Save the sound as a preset", suggested,
                                                   juce::String ("*") + SynthProcessor::presetExtension);

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& finished)
                          {
                              const auto file = finished.getResult();

                              if (file == juce::File())
                                  return;

                              if (! synthProcessor.savePreset (file))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Couldn't save the preset",
                                                                          "EZ Synth couldn't write " + file.getFullPathName() + ".");

                              refresh();
                          });
}
