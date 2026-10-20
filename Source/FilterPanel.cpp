#include "FilterPanel.h"

#include "SynthLookAndFeel.h"

namespace
{
    juce::StringArray getTypeNames()
    {
        juce::StringArray names;

        for (const auto type : allFilterTypes)
            names.add (FilterStage::getName (type));

        return names;
    }
}

FilterPanel::Filter::Filter (juce::AudioProcessorValueTreeState& parameters, int filterNumber)
    : ids (SynthProcessor::filterIds (filterNumber)),
      name ("Filter " + juce::String (filterNumber + 1)),
      type (getTypeNames()),
      onAttachment (parameters, ids.on, onSwitch),
      typeAttachment (*parameters.getParameter (ids.type), [this] (float value)
      {
          type.setChosen (juce::roundToInt (value));

          if (onTypeChange != nullptr)
              onTypeChange();
      }),
      cutoffAttachment (parameters, ids.cutoff, cutoff.knob),
      resonanceAttachment (parameters, ids.resonance, resonance.knob),
      widthAttachment (parameters, ids.width, width.knob),
      qAttachment (parameters, ids.q, q.knob),
      depthAttachment (parameters, ids.depth, depth.knob)
{
    cutoff.caption = "CUTOFF";
    resonance.caption = "RESONANCE";
    width.caption = "WIDTH";
    q.caption = "Q";
    depth.caption = "DEPTH";

    modulatable::setTarget (onSwitch, modulation::filterOnTarget (filterNumber));
    modulatable::setTarget (cutoff.knob, modulation::filterCutoffTarget (filterNumber));
    modulatable::setTarget (resonance.knob, modulation::filterResonanceTarget (filterNumber));
    modulatable::setTarget (width.knob, modulation::filterWidthTarget (filterNumber));
    modulatable::setTarget (q.knob, modulation::filterQTarget (filterNumber));
    modulatable::setTarget (depth.knob, modulation::filterDepthTarget (filterNumber));
}

std::vector<FilterPanel::Dial*> FilterPanel::Filter::getDials()
{
    switch (getType())
    {
        case FilterType::midPass:   return { &cutoff, &width };
        case FilterType::notch:     return { &cutoff, &q, &depth };
        case FilterType::lowPass:
        case FilterType::highPass:  break;
    }

    return { &cutoff, &resonance };
}

FilterPanel::FilterPanel (juce::AudioProcessorValueTreeState& parameters)
    : first (parameters, 0),
      second (parameters, 1)
{
    setUp (first, parameters);
    setUp (second, parameters);
}

void FilterPanel::setUp (Filter& filter, juce::AudioProcessorValueTreeState& parameters)
{
    filter.onSwitch.setTitle (filter.name + " on");
    filter.onSwitch.setMouseClickGrabsKeyboardFocus (false);
    filter.onSwitch.setWantsKeyboardFocus (false);
    filter.onSwitch.onStateChange = [this, &filter] { showSettings (filter); };
    addAndMakeVisible (filter.onSwitch);

    filter.type.setTitle (filter.name + " type");
    filter.type.drawPicture = [] (juce::Graphics& g, juce::Rectangle<float> area, int index)
    {
        drawTypePicture (g, area, allFilterTypes[(size_t) index]);
    };
    filter.type.onChoose = [&filter] (int index) { filter.typeAttachment.setValueAsCompleteGesture ((float) index); };
    addAndMakeVisible (filter.type);

    for (auto [dial, id] : { std::pair (&filter.cutoff, filter.ids.cutoff), std::pair (&filter.resonance, filter.ids.resonance),
                             std::pair (&filter.width, filter.ids.width), std::pair (&filter.q, filter.ids.q),
                             std::pair (&filter.depth, filter.ids.depth) })
    {
        const auto& parameter = *parameters.getParameter (id);
        auto& knob = dial->knob;
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
        knob.setTitle (filter.name + " " + (dial->caption == "Q" ? dial->caption : dial->caption.toLowerCase()));
        knob.setMouseClickGrabsKeyboardFocus (false);
        knob.setWantsKeyboardFocus (false);
        knob.onValueChange = [this] { repaint(); };
        addChildComponent (knob);
    }

    // The Q's tooltip gives the bandwidth it makes, in octaves.
    const auto describeBandwidth = [&knob = filter.q.knob]
    {
        const auto q = (float) knob.getValue();
        knob.setTooltip ("Q " + knob.getTextFromValue (q) + ": a bandwidth of " + juce::String (FilterStage::qToOctaves (q), 2)
                         + " octaves between the points where the notch lets through half the power, at full depth");
    };

    filter.q.knob.onValueChange = [this, describeBandwidth]
    {
        describeBandwidth();
        repaint();
    };

    describeBandwidth();
    filter.onTypeChange = [this, &filter] { showSettings (filter); };
    filter.typeAttachment.sendInitialUpdate();
}

void FilterPanel::showSettings (Filter& filter)
{
    const auto dials = filter.getDials();

    for (auto* dial : { &filter.cutoff, &filter.resonance, &filter.width, &filter.q, &filter.depth })
        dial->knob.setVisible (std::find (dials.begin(), dials.end(), dial) != dials.end());

    // A filter that's out keeps its settings, shown faintly, ready for when it's switched in.
    const auto colour = filter.isOn() ? colours::accent : colours::dimText.withAlpha (0.6f);

    for (auto* dial : { &filter.cutoff, &filter.resonance, &filter.width, &filter.q, &filter.depth })
        dial->knob.setColour (juce::Slider::rotarySliderFillColourId, colour);

    filter.type.setAlpha (filter.isOn() ? 1.0f : 0.55f);
    layOutDials (filter);
    repaint();
}

void FilterPanel::layOutDials (Filter& filter)
{
    // The dials share the width: two side by side, or three for a notch.
    const auto dials = filter.getDials();
    auto area = filter.dialArea;
    const auto dialWidth = area.getWidth() / (int) dials.size();

    for (auto* dial : dials)
    {
        auto dialArea = area.removeFromLeft (dialWidth);
        dial->captionArea = dialArea.removeFromTop (14);
        dial->valueArea = dialArea.removeFromBottom (16);
        dial->knob.setBounds (dialArea);
    }
}

void FilterPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    captionArea = area.removeFromTop (18);
    area.removeFromTop (8);

    constexpr auto gap = 24;
    const auto halfWidth = (area.getWidth() - gap) / 2;
    first.area = area.removeFromLeft (halfWidth);
    area.removeFromLeft (gap);
    second.area = area;

    for (auto* filter : { &first, &second })
    {
        auto filterArea = filter->area;
        auto top = filterArea.removeFromTop (22);
        filter->onSwitch.setBounds (top.removeFromLeft (36));
        top.removeFromLeft (8);
        filter->type.setBounds (top);
        filterArea.removeFromTop (6);
        filter->dialArea = filterArea;
        layOutDials (*filter);
    }
}

void FilterPanel::drawTypePicture (juce::Graphics& g, juce::Rectangle<float> area, FilterType type)
{
    // Frequency runs left to right, and what's let through bottom to top.
    const auto box = area.withSizeKeepingCentre (juce::jmin (20.0f, area.getWidth() - 6.0f), juce::jmin (10.0f, area.getHeight() - 8.0f));
    const auto left = box.getX(), right = box.getRight(), top = box.getY(), bottom = box.getBottom();
    const auto middle = box.getCentreX();
    juce::Path curve;

    switch (type)
    {
        case FilterType::lowPass:
            curve.startNewSubPath (left, top);
            curve.lineTo (left + box.getWidth() * 0.4f, top);
            curve.cubicTo (left + box.getWidth() * 0.7f, top, right - box.getWidth() * 0.12f, bottom - 2.0f, right, bottom);
            break;

        case FilterType::highPass:
            curve.startNewSubPath (left, bottom);
            curve.cubicTo (left + box.getWidth() * 0.12f, bottom - 2.0f, left + box.getWidth() * 0.3f, top, left + box.getWidth() * 0.6f, top);
            curve.lineTo (right, top);
            break;

        case FilterType::midPass:
            curve.startNewSubPath (left, bottom);
            curve.cubicTo (middle - box.getWidth() * 0.2f, bottom, middle - box.getWidth() * 0.18f, top, middle, top);
            curve.cubicTo (middle + box.getWidth() * 0.18f, top, middle + box.getWidth() * 0.2f, bottom, right, bottom);
            break;

        case FilterType::notch:
            curve.startNewSubPath (left, top);
            curve.lineTo (middle - box.getWidth() * 0.28f, top);
            curve.cubicTo (middle - box.getWidth() * 0.1f, top, middle - box.getWidth() * 0.08f, bottom, middle, bottom);
            curve.cubicTo (middle + box.getWidth() * 0.08f, bottom, middle + box.getWidth() * 0.1f, top, middle + box.getWidth() * 0.28f, top);
            curve.lineTo (right, top);
            break;
    }

    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void FilterPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("FILTERS", captionArea, juce::Justification::centredLeft);
    g.setFont (fonts::body (12.5f));
    g.drawText ("On all the oscillators together", captionArea, juce::Justification::centredRight);

    const auto dividerX = (float) (first.area.getRight() + second.area.getX()) / 2.0f;
    g.setColour (colours::outline);
    g.drawVerticalLine (juce::roundToInt (dividerX), (float) first.area.getY(), (float) first.area.getBottom());

    paintFilter (g, first);
    paintFilter (g, second);
}

void FilterPanel::paintFilter (juce::Graphics& g, Filter& filter)
{
    const auto on = filter.isOn();
    const auto dials = filter.getDials();

    // Three dials share the room two do, so their values are a little smaller.
    const auto valueFont = fonts::body (dials.size() > 2 ? 12.0f : 13.0f);

    for (auto* dial : dials)
    {
        g.setColour (colours::dimText);
        g.setFont (fonts::caption());
        g.drawText (dial->caption, dial->captionArea, juce::Justification::centred);

        // The parameters' own text, such as "1.00 kHz", "40%", "2.0 oct" and "60 dB".
        g.setColour (on ? colours::text : colours::dimText);
        g.setFont (valueFont);
        g.drawText (dial->knob.getTextFromValue (dial->knob.getValue()), dial->valueArea, juce::Justification::centred);
    }
}
