#include "LfoPanel.h"

#include "ModulatableControls.h"
#include "SynthLookAndFeel.h"

LfoPanel::Row::Row (SynthProcessor& processor, int targetToShow)
    : synthProcessor (processor),
      target (targetToShow)
{
    // The row stands for its control, so clicking it opens the same editor as right-clicking the control.
    modulatable::setTarget (*this, target);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);

    removeButton.setTitle ("Stop moving " + modulation::getName (target));
    removeButton.setTooltip ("Stop this LFO moving it");
    removeButton.setMouseClickGrabsKeyboardFocus (false);
    removeButton.setWantsKeyboardFocus (false);
    removeButton.onClick = [this] { synthProcessor.setAssignment (target, {}); };
    addAndMakeVisible (removeButton);
}

void LfoPanel::Row::resized()
{
    removeButton.setBounds (getLocalBounds().removeFromRight (24).withSizeKeepingCentre (22, 20));
}

void LfoPanel::Row::paint (juce::Graphics& g)
{
    const auto& assignment = synthProcessor.getAssignment (target);
    auto area = getLocalBounds().withTrimmedRight (30);

    g.setColour (colours::panel.brighter (0.05f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.0f, 2.0f), 5.0f);

    area.removeFromLeft (8);
    const auto detail = area.removeFromRight (140);

    g.setColour (colours::text);
    g.setFont (fonts::body (13.0f));
    g.drawText (modulation::getName (target), area, juce::Justification::centredLeft, true);

    g.setColour (colours::dimText);
    g.setFont (fonts::body (12.5f));
    g.drawText ("range " + juce::String (juce::roundToInt (assignment.range * 100.0f)) + "%  "
                    + modulation::phaseToText (assignment.phase) + "  "
                    + (assignment.inverted ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String ("+")),
                detail, juce::Justification::centredRight);
}

void LfoPanel::Row::mouseDown (const juce::MouseEvent&)
{
    modulatable::requestEditor (*this);
}

//==============================================================================
LfoPanel::LfoPanel (SynthProcessor& processor)
    : synthProcessor (processor)
{
}

void LfoPanel::setLfo (int newLfo)
{
    lfo = newLfo;
    refresh();
}

void LfoPanel::refresh()
{
    rows.clear();

    for (int target = 0; target < modulation::numTargets; ++target)
    {
        if (synthProcessor.getAssignment (target).source == lfo)
        {
            rows.push_back (std::make_unique<Row> (synthProcessor, target));
            addChildComponent (*rows.back());
        }
    }

    resized();
    repaint();
}

void LfoPanel::resized()
{
    auto area = getLocalBounds();
    captionArea = area.removeFromTop (18);
    area.removeFromTop (10);
    footerArea = area.removeFromBottom (juce::jmin (112, area.getHeight() / 2));
    area.removeFromBottom (8);
    listArea = area;

    // As many rows as fit are shown; the footer says how many more there are.
    const auto numFitting = juce::jmax (0, listArea.getHeight() / rowHeight);
    auto rowArea = listArea;
    numHidden = 0;

    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto fits = (int) i < numFitting - (rows.size() > (size_t) numFitting ? 1 : 0);
        rows[i]->setVisible (fits);

        if (fits)
            rows[i]->setBounds (rowArea.removeFromTop (rowHeight));
        else
            ++numHidden;
    }
}

void LfoPanel::paint (juce::Graphics& g)
{
    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("LFO " + juce::String (lfo + 1) + " MOVES", captionArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));

    if (rows.empty())
    {
        g.drawFittedText ("Nothing yet.", listArea.removeFromTop (rowHeight), juce::Justification::centredLeft, 1);
    }
    else if (numHidden > 0)
    {
        const auto shown = (int) rows.size() - numHidden;
        g.drawText ("and " + juce::String (numHidden) + " more",
                    listArea.withTrimmedTop (shown * rowHeight).removeFromTop (rowHeight).withTrimmedLeft (8),
                    juce::Justification::centredLeft);
    }

    g.setFont (fonts::body (12.5f));
    g.drawFittedText ("Right-click, or Control-click, a dial, slider, switch or harmonic button to have an LFO move "
                      "it as the LFO's wave rises and falls.\n\n"
                      "Loop runs the LFO all the time. Once runs it through one period each time a key is pressed, "
                      "then holds its last value, like an envelope.",
                      footerArea, juce::Justification::bottomLeft, 8);
}
