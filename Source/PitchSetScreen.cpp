#include "PitchSetScreen.h"

#include "SynthLookAndFeel.h"

PitchSetScreen::PitchSetScreen (SynthProcessor& processor)
    : synthProcessor (processor)
{
    setWantsKeyboardFocus (true);

    for (auto* button : { &doneButton, &newButton, &cancelButton })
    {
        button->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }

    doneButton.onClick = [this]
    {
        cancelWaiting();

        if (onClose != nullptr)
            onClose();
    };

    newButton.setTooltip ("Add a pitch set for a range of keys, sounding as the active one does to start with");
    newButton.onClick = [this] { startNewPitchSet(); };
    cancelButton.onClick = [this] { cancelWaiting(); };
    cancelButton.setVisible (false);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

juce::Colour PitchSetScreen::getColour (int index)
{
    if (index <= 0)
        return juce::Colour (0xff5c6373);

    static const juce::Colour colours[] = { juce::Colour (0xffc79bff), juce::Colour (0xffffb347), juce::Colour (0xff6fd6a8),
                                            juce::Colour (0xffff7a8a), juce::Colour (0xff7ab8ff), juce::Colour (0xffe6d86a) };
    return colours[(size_t) (index - 1) % std::size (colours)];
}

void PitchSetScreen::startWaiting (int index)
{
    if (index < 0 && synthProcessor.getNumPitchSets() >= SynthProcessor::maxPitchSets)
    {
        message = "There can only be " + juce::String (SynthProcessor::maxPitchSets) + " pitch sets, the default among them.";
        repaint();
        return;
    }

    waiting = Waiting::lowest;
    waitingFor = index;
    seenNoteOns = synthProcessor.getNoteOnCount();
    message = {};
    newButton.setVisible (false);
    cancelButton.setVisible (true);
    resized();
    repaint();

    if (onWaitingForKeys != nullptr)
        onWaitingForKeys (true);
}

void PitchSetScreen::cancelWaiting()
{
    if (waiting == Waiting::no)
        return;

    waiting = Waiting::no;
    newButton.setVisible (true);
    cancelButton.setVisible (false);
    resized();
    repaint();

    if (onWaitingForKeys != nullptr)
        onWaitingForKeys (false);
}

void PitchSetScreen::keyPlayed (int note)
{
    if (waiting == Waiting::lowest)
    {
        lowestKey = note;
        waiting = Waiting::highest;
        repaint();
        return;
    }

    if (waiting != Waiting::highest)
        return;

    // The keys can be pressed either way round, and the same key twice makes a pitch set of that key alone.
    const SynthProcessor::KeyRange range { std::min (lowestKey, note), std::max (lowestKey, note) };

    if (waitingFor < 0)
    {
        const auto index = synthProcessor.addPitchSet (range);

        if (index >= 0)
            synthProcessor.setActivePitchSet (index);
    }
    else
    {
        synthProcessor.setPitchSetRange (waitingFor, range);
    }

    cancelWaiting();
    refresh();
}

void PitchSetScreen::timerCallback()
{
    const auto count = synthProcessor.getNoteOnCount();

    if (count != seenNoteOns)
    {
        seenNoteOns = count;

        if (waiting != Waiting::no && synthProcessor.getLastNote() >= 0)
            keyPlayed (synthProcessor.getLastNote());
    }

    refresh();
}

void PitchSetScreen::refresh()
{
    const auto version = synthProcessor.getPitchSetVersion();

    if (version == shownVersion && (int) rows.size() == synthProcessor.getNumPitchSets())
        return;

    shownVersion = version;
    rows.clear();

    for (int i = 0; i < synthProcessor.getNumPitchSets(); ++i)
    {
        rows.push_back (std::make_unique<Row> (*this, i));
        list.addAndMakeVisible (*rows.back());
    }

    layOutRows();
    repaint();
}

void PitchSetScreen::visibilityChanged()
{
    // It follows the synth, and listens for keys, only while it's shown.
    if (isVisible())
    {
        seenNoteOns = synthProcessor.getNoteOnCount();
        refresh();
        startTimerHz (30);
    }
    else
    {
        cancelWaiting();
        stopTimer();
    }
}

bool PitchSetScreen::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (isWaitingForKeys())
            cancelWaiting();
        else if (onClose != nullptr)
            onClose();

        return true;
    }

    return false;
}

void PitchSetScreen::resized()
{
    panelArea = getLocalBounds().reduced (24).withSizeKeepingCentre (juce::jmin (920, getWidth() - 48), getHeight() - 48);
    auto area = panelArea.reduced (24, 20);

    auto header = area.removeFromTop (30);
    doneButton.setBounds (header.removeFromRight (90));
    header.removeFromRight (10);
    newButton.setBounds (header.removeFromRight (130));
    cancelButton.setBounds (newButton.getBounds().withWidth (90).withX (newButton.getRight() - 90));
    titleArea = header;
    subtitleArea = area.removeFromTop (40);
    area.removeFromTop (10);
    keyMapArea = area.removeFromTop (46);
    area.removeFromTop (12);
    promptArea = area.removeFromTop (isWaitingForKeys() || message.isNotEmpty() ? 34 : 0);

    viewport.setBounds (area);
    layOutRows();
}

void PitchSetScreen::layOutRows()
{
    const auto width = viewport.getMaximumVisibleWidth();
    auto y = 0;

    for (auto& row : rows)
    {
        row->setBounds (0, y, width, rowHeight);
        y += rowHeight;
    }

    list.setSize (width, juce::jmax (y, 1));
}

void PitchSetScreen::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.withAlpha (0.96f));

    g.setColour (colours::panel);
    g.fillRoundedRectangle (panelArea.toFloat(), 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), 10.0f, 1.0f);

    g.setFont (fonts::caption());
    g.setColour (getColour (1));
    g.drawText ("PITCH SETS", titleArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (colours::dimText);
    g.drawFittedText ("Every setting can be different for a range of keys. The synth's controls show and set the active pitch "
                      "set, ticked: click another to set it. Each key plays the narrowest pitch set it's in, or the default.",
                      subtitleArea, juce::Justification::centredLeft, 2);

    paintKeyMap (g);

    if (! promptArea.isEmpty())
    {
        juce::String prompt = message;

        if (waiting == Waiting::lowest)
            prompt = juce::String (waitingFor < 0 ? "New pitch set: press" : "Keys for " + synthProcessor.getPitchSetName (waitingFor) + ": press")
                     + " its lowest key, on a MIDI keyboard or the one below.";
        else if (waiting == Waiting::highest)
            prompt = "Lowest " + juce::MidiMessage::getMidiNoteName (lowestKey, true, true, 4)
                     + ". Now press its highest key, or the same key again for that key alone.";

        g.setColour (isWaitingForKeys() ? getColour (1).withAlpha (0.16f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (promptArea.toFloat().reduced (0.0f, 2.0f), 6.0f);
        g.setColour (colours::text);
        g.setFont (fonts::body (14.0f));
        g.drawText (prompt, promptArea.reduced (10, 0), juce::Justification::centredLeft, true);
    }
}

void PitchSetScreen::paintKeyMap (juce::Graphics& g) const
{
    // A column for each key from C1 to C8, in the colour of the pitch set that plays it, the active one's brighter,
    // with each C named under it.
    const auto bar = keyMapArea.withHeight (26).toFloat();
    const auto numKeys = lastMappedKey - firstMappedKey + 1;
    const auto keyWidth = bar.getWidth() / (float) numKeys;
    const auto active = synthProcessor.getActivePitchSet();

    for (int note = firstMappedKey; note <= lastMappedKey; ++note)
    {
        const auto index = synthProcessor.getPitchSetForNote (note);
        const auto key = juce::Rectangle<float> (bar.getX() + (float) (note - firstMappedKey) * keyWidth, bar.getY(), keyWidth, bar.getHeight());
        const auto black = juce::MidiMessage::isMidiNoteBlack (note);
        auto colour = getColour (index).withAlpha (index == active ? 1.0f : 0.45f);

        if (black)
            colour = colour.darker (0.35f);

        g.setColour (colour);
        g.fillRect (key.reduced (0.5f, 0.0f));

        if (waiting == Waiting::highest && note == lowestKey)
        {
            g.setColour (colours::text);
            g.drawRect (key, 2.0f);
        }

        if (note % 12 == 0)
        {
            g.setColour (colours::dimText);
            g.setFont (fonts::body (11.0f));
            g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                        juce::Rectangle<float> (key.getX() - 2.0f, bar.getBottom() + 2.0f, 40.0f, 16.0f), juce::Justification::centredLeft);
        }
    }
}

//==============================================================================
PitchSetScreen::Row::Row (PitchSetScreen& owner, int indexShown)
    : index (indexShown),
      screen (owner)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Pitch set " + screen.synthProcessor.getPitchSetName (index));

    keysButton.setTooltip ("Set this pitch set's range by pressing its lowest key, then its highest");
    keysButton.onClick = [this] { screen.startSettingKeys (index); };
    deleteButton.setTooltip ("Take this pitch set away, and its sound with it");
    deleteButton.onClick = [this]
    {
        // The list is brought up to date by the screen's timer, after this row's gone from the synth.
        screen.cancelWaiting();
        screen.synthProcessor.removePitchSet (index);
    };

    for (auto* button : { &keysButton, &deleteButton })
    {
        button->setMouseClickGrabsKeyboardFocus (false);

        // The default pitch set plays every key no other takes in, so it has no range, and is always there.
        if (index > 0)
            addAndMakeVisible (*button);
    }
}

void PitchSetScreen::Row::resized()
{
    auto area = getLocalBounds().reduced (4, 7);
    deleteButton.setBounds (area.removeFromRight (80));
    area.removeFromRight (8);
    keysButton.setBounds (area.removeFromRight (90));
}

void PitchSetScreen::Row::mouseDown (const juce::MouseEvent&)
{
    screen.synthProcessor.setActivePitchSet (index);
}

void PitchSetScreen::Row::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    const auto active = screen.synthProcessor.getActivePitchSet() == index;

    if (active || isMouseOver (true))
    {
        g.setColour (juce::Colours::white.withAlpha (active ? 0.07f : 0.04f));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
    }

    g.setColour (colours::outline);
    g.fillRect (area.removeFromBottom (1));

    // A dot in the pitch set's colour, filled in for the active one.
    area.removeFromLeft (10);
    const auto dot = area.removeFromLeft (16).withSizeKeepingCentre (14, 14).toFloat();
    g.setColour (getColour (index));

    if (active)
        g.fillEllipse (dot);
    else
        g.drawEllipse (dot.reduced (1.0f), 1.5f);

    area.removeFromLeft (12);
    area.removeFromRight (200);

    g.setFont (juce::FontOptions (15.0f, active ? juce::Font::bold : juce::Font::plain));
    g.setColour (colours::text);
    g.drawText (screen.synthProcessor.getPitchSetName (index), area.removeFromLeft (180), juce::Justification::centredLeft, true);

    juce::String description;

    if (const auto range = screen.synthProcessor.getPitchSetRange (index))
    {
        const auto keys = range->highest - range->lowest + 1;
        auto playing = 0;

        for (int note = range->lowest; note <= range->highest; ++note)
            if (screen.synthProcessor.getPitchSetForNote (note) == index)
                ++playing;

        description = juce::String (keys) + (keys == 1 ? " key" : " keys");

        if (playing < keys)
            description << ", " << (playing == 0 ? juce::String ("all") : juce::String (keys - playing)) << " played by narrower pitch sets";
    }
    else
    {
        description = "Every key no other pitch set takes in";
    }

    if (active)
        description << "  " << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "  the controls set this one";

    g.setFont (fonts::body (13.0f));
    g.setColour (colours::dimText);
    g.drawText (description, area, juce::Justification::centredLeft, true);
}
