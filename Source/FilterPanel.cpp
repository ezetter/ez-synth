#include "FilterPanel.h"

#include "SynthLookAndFeel.h"

FilterPanel::Filter::Filter (juce::AudioProcessorValueTreeState& parameters, int filterNumber)
    : ids (SynthProcessor::filterIds (filterNumber)),
      name ("Filter " + juce::String (filterNumber + 1)),
      onAttachment (parameters, ids.on, onSwitch),
      typeAttachment (*parameters.getParameter (ids.type), [this] (float value)
      {
          type.setChosen (juce::roundToInt (value));

          if (onTypeChange != nullptr)
              onTypeChange();
      }),
      cutoffAttachment (parameters, ids.cutoff, cutoff),
      resonanceAttachment (parameters, ids.resonance, resonance),
      widthAttachment (parameters, ids.width, width)
{
    modulatable::setTarget (onSwitch, modulation::filterOnTarget (filterNumber));
    modulatable::setTarget (cutoff, modulation::filterCutoffTarget (filterNumber));
    modulatable::setTarget (resonance, modulation::filterResonanceTarget (filterNumber));
    modulatable::setTarget (width, modulation::filterWidthTarget (filterNumber));
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
    filter.type.onChoose = [&filter] (int index) { filter.typeAttachment.setValueAsCompleteGesture ((float) index); };
    addAndMakeVisible (filter.type);

    for (auto [knob, id, title] : { std::tuple (&filter.cutoff, filter.ids.cutoff, "cutoff"),
                                    std::tuple (&filter.resonance, filter.ids.resonance, "resonance"),
                                    std::tuple (&filter.width, filter.ids.width, "width") })
    {
        const auto& parameter = *parameters.getParameter (id);
        knob->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob->setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
        knob->setTitle (filter.name + " " + title);
        knob->setMouseClickGrabsKeyboardFocus (false);
        knob->setWantsKeyboardFocus (false);
        knob->onValueChange = [this] { repaint(); };
        addChildComponent (*knob);
    }

    filter.cutoff.setVisible (true);

    filter.onTypeChange = [this, &filter] { showSettings (filter); };
    filter.typeAttachment.sendInitialUpdate();
}

void FilterPanel::showSettings (Filter& filter)
{
    // The dial beside the cutoff is the resonance for a low or high pass, and the width for a mid pass.
    const auto midPass = filter.getType() == FilterType::midPass;
    filter.resonance.setVisible (! midPass);
    filter.width.setVisible (midPass);

    // A filter that's out keeps its settings, shown faintly, ready for when it's switched in.
    const auto colour = filter.isOn() ? colours::accent : colours::dimText.withAlpha (0.6f);

    for (auto* knob : { &filter.cutoff, &filter.resonance, &filter.width })
        knob->setColour (juce::Slider::rotarySliderFillColourId, colour);

    filter.type.setAlpha (filter.isOn() ? 1.0f : 0.55f);
    repaint();
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

        auto cutoffArea = filterArea.removeFromLeft (filterArea.getWidth() / 2);
        auto& secondArea = filterArea;

        filter->cutoffCaption = cutoffArea.removeFromTop (14);
        filter->cutoffValue = cutoffArea.removeFromBottom (16);
        filter->cutoff.setBounds (cutoffArea);

        filter->secondCaption = secondArea.removeFromTop (14);
        filter->secondValue = secondArea.removeFromBottom (16);
        filter->resonance.setBounds (secondArea);
        filter->width.setBounds (secondArea);
    }
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
    auto& secondDial = filter.getSecondDial();

    g.setColour (colours::dimText);
    g.setFont (fonts::caption());
    g.drawText ("CUTOFF", filter.cutoffCaption, juce::Justification::centred);
    g.drawText (filter.getType() == FilterType::midPass ? "WIDTH" : "RESONANCE", filter.secondCaption, juce::Justification::centred);

    // The parameters' own text, such as "1.00 kHz", "40%" and "2.0 oct".
    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (fonts::body (13.0f));
    g.drawText (filter.cutoff.getTextFromValue (filter.cutoff.getValue()), filter.cutoffValue, juce::Justification::centred);
    g.drawText (secondDial.getTextFromValue (secondDial.getValue()), filter.secondValue, juce::Justification::centred);
}
