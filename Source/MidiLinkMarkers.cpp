#include "MidiLinkMarkers.h"

#include "ModulatableControls.h"
#include "SynthLookAndFeel.h"

namespace
{
    /** The colour of MIDI links: amber, apart from the green of the controls and the blue of modulation. */
    const juce::Colour midiColour { 0xffffb347 };
}

MidiLinkMarkers::MidiLinkMarkers (SynthProcessor& processor)
    : synthProcessor (processor)
{
    setInterceptsMouseClicks (false, false);
}

void MidiLinkMarkers::refresh()
{
    auto markers = getMarkers();

    if (markers == shown)
        return;

    for (const auto* list : { &shown, &markers })
        for (const auto& marker : *list)
            repaint (marker.area.expanded (1.0f).getSmallestIntegerContainer());

    shown = std::move (markers);
}

std::vector<MidiLinkMarkers::Marker> MidiLinkMarkers::getMarkers() const
{
    std::vector<Marker> markers;
    const auto learning = synthProcessor.getMidiLearnTarget();
    const auto* window = getParentComponent();

    if (window == nullptr)
        return markers;

    // Every control shown that's linked, or being learned, wherever it is in the window.
    std::function<void (const juce::Component&)> find = [&] (const juce::Component& component)
    {
        if (! component.isVisible() || &component == this)
            return;

        if (const auto target = modulatable::getTarget (component))
        {
            const auto controller = synthProcessor.getMidiController (*target);
            const auto isLearning = learning == *target;

            if (controller.has_value() || isLearning)
            {
                const auto bounds = getLocalArea (&component, component.getLocalBounds()).toFloat();
                const auto text = isLearning ? juce::String ("MIDI") : juce::String (*controller);
                const auto width = juce::jmax (14.0f, (float) text.length() * 6.5f + 7.0f);
                const auto tag = juce::Rectangle<float> (width, 12.0f);
                const auto* slider = dynamic_cast<const juce::Slider*> (&component);

                // On a dial, at the top right of its knob; on a slider, just above the end of its track; and
                // otherwise, at the control's top right corner.
                juce::Point<float> topRight { bounds.getRight() + 2.0f, bounds.getY() - 4.0f };

                if (slider != nullptr && slider->isRotary())
                {
                    const auto knob = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()),
                                                                    juce::jmin (bounds.getWidth(), bounds.getHeight()));
                    topRight = { knob.getRight() + 2.0f, knob.getY() };
                }
                else if (slider != nullptr && slider->isHorizontal())
                {
                    topRight = { bounds.getRight(), bounds.getCentreY() - 16.0f };
                }

                markers.push_back ({ tag.withPosition (topRight.x - width, topRight.y), text, isLearning });
            }
        }

        for (const auto* child : component.getChildren())
            find (*child);
    };

    for (const auto* child : window->getChildren())
        find (*child);

    return markers;
}

void MidiLinkMarkers::paint (juce::Graphics& g)
{
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));

    for (const auto& marker : getMarkers())
    {
        g.setColour (marker.learning ? midiColour.withAlpha (0.25f) : midiColour);
        g.fillRoundedRectangle (marker.area, 4.0f);
        g.setColour (marker.learning ? midiColour : colours::background);

        if (marker.learning)
            g.drawRoundedRectangle (marker.area.reduced (0.5f), 4.0f, 1.0f);

        g.drawText (marker.text, marker.area, juce::Justification::centred);
    }
}
