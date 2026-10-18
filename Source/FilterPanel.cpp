#include "FilterPanel.h"

#include "SynthLookAndFeel.h"

FilterPanel::Filter::Filter (juce::AudioProcessorValueTreeState& parameters, const SynthProcessor::FilterIds& ids,
                             const juce::String& filterName)
    : name (filterName),
      onAttachment (parameters, ids.on, onSwitch),
      cutoffAttachment (parameters, ids.cutoff, cutoff),
      resonanceAttachment (parameters, ids.resonance, resonance)
{
}

FilterPanel::FilterPanel (juce::AudioProcessorValueTreeState& parameters)
    : lowPass (parameters, SynthProcessor::lowPassIds, "Low pass"),
      highPass (parameters, SynthProcessor::highPassIds, "High pass")
{
    setUp (lowPass, parameters, SynthProcessor::lowPassIds);
    setUp (highPass, parameters, SynthProcessor::highPassIds);
}

void FilterPanel::setUp (Filter& filter, juce::AudioProcessorValueTreeState& parameters, const SynthProcessor::FilterIds& ids)
{
    filter.onSwitch.setTitle (filter.name + " filter on");
    filter.onSwitch.setMouseClickGrabsKeyboardFocus (false);
    filter.onSwitch.setWantsKeyboardFocus (false);
    filter.onSwitch.onStateChange = [this, &filter] { showWhetherOn (filter); };
    addAndMakeVisible (filter.onSwitch);

    for (auto [knob, id, title] : { std::tuple (&filter.cutoff, ids.cutoff, "cutoff"),
                                    std::tuple (&filter.resonance, ids.resonance, "resonance") })
    {
        const auto& parameter = *parameters.getParameter (id);
        knob->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob->setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
        knob->setTitle (filter.name + " " + title);
        knob->setMouseClickGrabsKeyboardFocus (false);
        knob->setWantsKeyboardFocus (false);
        knob->onValueChange = [this] { repaint(); };
        addAndMakeVisible (*knob);
    }

    showWhetherOn (filter);
}

void FilterPanel::showWhetherOn (Filter& filter)
{
    // A filter that's out keeps its settings, shown faintly, ready for when it's switched in.
    const auto colour = filter.isOn() ? colours::accent : colours::dimText.withAlpha (0.6f);
    filter.cutoff.setColour (juce::Slider::rotarySliderFillColourId, colour);
    filter.resonance.setColour (juce::Slider::rotarySliderFillColourId, colour);
    repaint();
}

void FilterPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    captionArea = area.removeFromTop (18);
    area.removeFromTop (8);

    constexpr auto gap = 24;
    const auto halfWidth = (area.getWidth() - gap) / 2;
    lowPass.area = area.removeFromLeft (halfWidth);
    area.removeFromLeft (gap);
    highPass.area = area;

    for (auto* filter : { &lowPass, &highPass })
    {
        auto filterArea = filter->area;
        auto top = filterArea.removeFromTop (22);
        filter->onSwitch.setBounds (top.removeFromLeft (36));
        top.removeFromLeft (8);
        filter->nameArea = top;
        filterArea.removeFromTop (6);

        auto cutoffArea = filterArea.removeFromLeft (filterArea.getWidth() / 2);
        auto& resonanceArea = filterArea;

        filter->cutoffCaption = cutoffArea.removeFromTop (14);
        filter->cutoffValue = cutoffArea.removeFromBottom (16);
        filter->cutoff.setBounds (cutoffArea);

        filter->resonanceCaption = resonanceArea.removeFromTop (14);
        filter->resonanceValue = resonanceArea.removeFromBottom (16);
        filter->resonance.setBounds (resonanceArea);
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

    const auto dividerX = (float) (lowPass.area.getRight() + highPass.area.getX()) / 2.0f;
    g.setColour (colours::outline);
    g.drawVerticalLine (juce::roundToInt (dividerX), (float) lowPass.area.getY(), (float) lowPass.area.getBottom());

    paintFilter (g, lowPass);
    paintFilter (g, highPass);
}

void FilterPanel::paintFilter (juce::Graphics& g, Filter& filter)
{
    const auto on = filter.isOn();

    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)).withExtraKerningFactor (0.08f));
    g.drawText (filter.name.toUpperCase(), filter.nameArea, juce::Justification::centredLeft);

    g.setColour (colours::dimText);
    g.setFont (fonts::caption());
    g.drawText ("CUTOFF", filter.cutoffCaption, juce::Justification::centred);
    g.drawText ("RESONANCE", filter.resonanceCaption, juce::Justification::centred);

    // The parameters' own text, such as "1.00 kHz" and "40%".
    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (fonts::body (13.0f));
    g.drawText (filter.cutoff.getTextFromValue (filter.cutoff.getValue()), filter.cutoffValue, juce::Justification::centred);
    g.drawText (filter.resonance.getTextFromValue (filter.resonance.getValue()), filter.resonanceValue,
                juce::Justification::centred);
}
