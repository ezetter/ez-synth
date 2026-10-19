#include "MidiPanel.h"

#include "SynthLookAndFeel.h"

namespace
{
    /** How long the light stays on after MIDI arrives, so even a single note shows. */
    constexpr juce::uint32 lightMilliseconds = 150;
}

MidiPanel::MidiPanel()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setMouseClickGrabsKeyboardFocus (false);
    setTitle ("MIDI devices");
}

void MidiPanel::refresh (int midiReceived)
{
    const auto now = juce::Time::getMillisecondCounter();

    if (midiReceived != lastReceived)
    {
        lastReceived = midiReceived;
        litUntil = now + lightMilliseconds;
    }

    auto current = MidiInputs::get() != nullptr ? MidiInputs::get()->getInputs() : std::vector<MidiInputs::Input>();
    const auto shouldLight = now < litUntil;

    if (current != inputs || shouldLight != lit)
    {
        inputs = std::move (current);
        lit = shouldLight;
        repaint();
    }
}

juce::String MidiPanel::describe (const std::vector<MidiInputs::Input>& inputs)
{
    if (inputs.empty())
        return "No MIDI devices plugged in";

    juce::StringArray on;

    for (const auto& input : inputs)
        if (input.on)
            on.add (input.name);

    if (on.isEmpty())
        return "Off. Click to choose a device";

    return on.size() == 1 ? on[0] : juce::String (on.size()) + " devices";
}

void MidiPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (isMouseOver() ? colours::outline.brighter (0.3f) : colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto area = getLocalBounds().reduced (12, 10);
    auto top = area.removeFromTop (16);

    const auto light = top.removeFromRight (10).toFloat().withSizeKeepingCentre (8.0f, 8.0f);
    g.setColour (lit ? colours::accent : colours::outline.brighter (0.25f));
    g.fillEllipse (light);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("MIDI IN", top, juce::Justification::centredLeft);

    // A hint at the foot, when there's room for it.
    if (area.getHeight() >= 70)
    {
        g.setFont (fonts::body (11.5f));
        g.setColour (colours::dimText.withAlpha (0.8f));
        g.drawText ("Click to choose", area.removeFromBottom (14), juce::Justification::bottomLeft);
    }

    area.removeFromTop (6);
    const auto anyOn = std::any_of (inputs.begin(), inputs.end(), [] (const MidiInputs::Input& input) { return input.on; });
    g.setFont (fonts::body (12.5f));
    g.setColour (anyOn ? colours::text : colours::dimText);
    g.drawFittedText (describe (inputs), area, juce::Justification::topLeft, 4, 1.0f);
}

void MidiPanel::mouseEnter (const juce::MouseEvent&)
{
    repaint();
}

void MidiPanel::mouseExit (const juce::MouseEvent&)
{
    repaint();
}

void MidiPanel::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Play from");

    if (inputs.empty())
        menu.addItem ("No MIDI devices plugged in", false, false, [] {});

    for (const auto& input : inputs)
    {
        menu.addItem (input.name, true, input.on, [identifier = input.identifier, on = input.on]
        {
            if (auto* midiInputs = MidiInputs::get())
                midiInputs->setOn (identifier, ! on);
        });
    }

    menu.addSeparator();
    menu.addItem ("Audio/MIDI Settings...", []
    {
        if (auto* midiInputs = MidiInputs::get())
            midiInputs->showSettings();
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}
