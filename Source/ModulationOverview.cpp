#include "ModulationOverview.h"

#include "SynthLookAndFeel.h"

namespace
{
    /** The amber MIDI links are shown in, apart from the blue of modulation. */
    const juce::Colour midiColour { 0xffffb347 };

    juce::String percent (float proportion)
    {
        return juce::String (juce::roundToInt (proportion * 100.0f)) + "%";
    }

    const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));

    /** What a trigger does, in a word or two: "randomizes", or "switches on or off". */
    juce::String triggerVerb (int target)
    {
        return modulation::describe (target).kind == modulation::Kind::harmonicSwitch ? "switches on or off" : "randomizes";
    }
    const juce::String plusOrMinus (juce::CharPointer_UTF8 ("\xc2\xb1"));
}

ModulationOverview::ModulationOverview (SynthProcessor& processor)
    : synthProcessor (processor)
{
    setWantsKeyboardFocus (true);

    doneButton.setMouseClickGrabsKeyboardFocus (false);
    doneButton.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };
    addAndMakeVisible (doneButton);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

std::vector<ModulationOverview::Link> ModulationOverview::getLinks() const
{
    std::vector<Link> links;

    // An LFO or envelope: how far it moves the control, and for an LFO, whether it's shifted or inverted.
    for (int source = 0; source < modulation::numSources; ++source)
    {
        for (int target = 0; target < modulation::numTargets; ++target)
        {
            const auto& assignment = synthProcessor.getAssignment (target);

            if (assignment.source != source)
                continue;

            juce::StringArray parts;
            const auto isLfo = ! modulation::isEnvelope (source);

            if (modulation::isTrigger (target))
                parts.add (triggerVerb (target) + " at " + modulation::thresholdToText (source, assignment.threshold));
            else if (isLfo)
                parts.add (plusOrMinus + percent (assignment.range * 0.5f) + " of its travel");
            else
                parts.add ((assignment.inverted ? minus : juce::String ("+")) + percent (assignment.range) + " of its travel");

            if (isLfo && assignment.inverted && ! modulation::isTrigger (target))
                parts.add ("inverted");

            if (isLfo && assignment.phase > 0.0f)
                parts.add (modulation::phaseToText (assignment.phase) + " behind");

            links.push_back ({ Link::Kind::source, source, target, parts.joinIntoString (", ") });
        }
    }

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (const auto& assignment = synthProcessor.getVelocityAssignment (target); assignment.isActive())
            links.push_back ({ Link::Kind::velocity, modulation::velocitySource, target,
                               modulation::isTrigger (target) ? triggerVerb (target) + " at " + percent (assignment.threshold)
                                                              : minus + percent (assignment.range) + " of its travel for the softest notes" });
    }

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (const auto& assignment = synthProcessor.getModWheelAssignment (target); assignment.isActive())
            links.push_back ({ Link::Kind::modWheel, modulation::modWheelSource, target,
                               modulation::isTrigger (target)
                                   ? triggerVerb (target) + " at " + percent (assignment.threshold)
                                   : (assignment.inverted ? minus : juce::String ("+")) + percent (assignment.range) + " of its travel at the top" });
    }

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (const auto controller = synthProcessor.getMidiController (target))
            links.push_back ({ Link::Kind::midi, -1, target,
                               "CC " + juce::String (*controller)
                                   + (modulation::isTrigger (target) ? ", " + triggerVerb (target) + " at " + percent (synthProcessor.getMidiThreshold (target))
                                                                  : juce::String (", across its whole travel")) });
    }

    return links;
}

juce::String ModulationOverview::getGroupName (const Link& link)
{
    switch (link.kind)
    {
        case Link::Kind::source:    return modulation::getSourceName (link.source);
        case Link::Kind::velocity:  return "Velocity";
        case Link::Kind::modWheel:  return "Mod wheel";
        case Link::Kind::midi:      return "MIDI controllers";
    }

    return {};
}

void ModulationOverview::remove (const Link& link)
{
    switch (link.kind)
    {
        case Link::Kind::source:    synthProcessor.setAssignment (link.target, {}); break;
        case Link::Kind::velocity:  synthProcessor.setVelocityAssignment (link.target, {}); break;
        case Link::Kind::modWheel:  synthProcessor.setModWheelAssignment (link.target, {}); break;
        case Link::Kind::midi:      synthProcessor.setMidiController (link.target, std::nullopt); break;
    }

    refresh();
}

void ModulationOverview::refresh()
{
    if (synthProcessor.getModulationVersion() == shownModulationVersion && synthProcessor.getMidiLearnVersion() == shownMidiLearnVersion)
        return;

    shownModulationVersion = synthProcessor.getModulationVersion();
    shownMidiLearnVersion = synthProcessor.getMidiLearnVersion();

    rows.clear();

    for (auto& link : getLinks())
    {
        rows.push_back (std::make_unique<Row> (*this, std::move (link)));
        list.addAndMakeVisible (*rows.back());
    }

    layOutRows();
    repaint();
}

void ModulationOverview::timerCallback()
{
    refresh();
}

void ModulationOverview::visibilityChanged()
{
    // It follows the synth only while it's shown.
    if (isVisible())
    {
        refresh();
        startTimerHz (10);
    }
    else
    {
        stopTimer();
    }
}

bool ModulationOverview::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose != nullptr)
            onClose();

        return true;
    }

    return false;
}

void ModulationOverview::resized()
{
    // A panel in the middle of the window, no wider than reads easily.
    panelArea = getLocalBounds().reduced (24).withSizeKeepingCentre (juce::jmin (920, getWidth() - 48), getHeight() - 48);
    auto area = panelArea.reduced (24, 20);

    auto header = area.removeFromTop (30);
    doneButton.setBounds (header.removeFromRight (90));
    titleArea = header;
    subtitleArea = area.removeFromTop (24);
    area.removeFromTop (12);

    viewport.setBounds (area);
    emptyArea = area.removeFromTop (60);
    layOutRows();
}

void ModulationOverview::layOutRows()
{
    const auto width = viewport.getMaximumVisibleWidth();
    list.headings.clear();
    auto y = 0;
    juce::String group;

    for (auto& row : rows)
    {
        if (const auto name = getGroupName (row->link); name != group)
        {
            group = name;
            list.headings.push_back ({ name, { 0, y, width, headingHeight } });
            y += headingHeight;
        }

        row->setBounds (0, y, width, rowHeight);
        y += rowHeight;
    }

    list.setSize (width, juce::jmax (y, 1));
}

void ModulationOverview::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.withAlpha (0.96f));

    g.setColour (colours::panel);
    g.fillRoundedRectangle (panelArea.toFloat(), 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), 10.0f, 1.0f);

    g.setFont (fonts::caption());
    g.setColour (colours::modulation);
    g.drawText ("MODULATION", titleArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (colours::dimText);
    g.drawText ("Everything that moves a control, or is linked to one. Click a row to edit it, or its cross to take it off.",
                subtitleArea, juce::Justification::centredLeft, true);

    if (rows.empty())
    {
        g.setColour (colours::dimText);
        g.drawFittedText ("Nothing moves any control yet. Right-click, or Control-click, a control to have an LFO, an envelope, "
                          "velocity or the mod wheel move it, or to link it to a MIDI controller.",
                          emptyArea, juce::Justification::topLeft, 3);
    }
}

void ModulationOverview::List::paint (juce::Graphics& g)
{
    for (const auto& [name, area] : headings)
    {
        g.setFont (fonts::caption());
        g.setColour (name == "MIDI controllers" ? midiColour : colours::modulation);
        g.drawText (name.toUpperCase(), area.withTrimmedTop (12), juce::Justification::centredLeft);
        g.setColour (colours::outline);
        g.fillRect (area.withTop (area.getBottom() - 1));
    }
}

//==============================================================================
ModulationOverview::Row::Row (ModulationOverview& owner, Link linkShown)
    : link (std::move (linkShown)),
      overview (owner)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (getGroupName (link) + " moves " + modulation::getName (link.target));
    removeButton.setTooltip ("Take this off");
    removeButton.setMouseClickGrabsKeyboardFocus (false);
    removeButton.onClick = [this] { overview.remove (link); };
    addAndMakeVisible (removeButton);
}

void ModulationOverview::Row::resized()
{
    removeButton.setBounds (getLocalBounds().removeFromRight (28).reduced (3));
}

void ModulationOverview::Row::mouseDown (const juce::MouseEvent&)
{
    if (overview.onEdit != nullptr)
        overview.onEdit (link.target, *this);
}

void ModulationOverview::Row::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();

    if (isMouseOver (true))
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRect (area);
    }

    area.removeFromRight (36);
    g.setFont (fonts::body (13.5f));
    g.setColour (colours::text);
    g.drawText (modulation::getName (link.target), area.removeFromLeft (300).withTrimmedLeft (4), juce::Justification::centredLeft, true);
    g.setColour (colours::dimText);
    g.drawText (link.description, area, juce::Justification::centredLeft, true);
}
