#include "EnvelopePanel.h"

#include "SynthLookAndFeel.h"

EnvelopePanel::EnvelopePanel (SynthProcessor& processor, int envelopeToShow)
    : synthProcessor (processor),
      envelope (envelopeToShow)
{
    const std::array ids { SynthProcessor::attackId (envelope), SynthProcessor::decayId (envelope),
                           SynthProcessor::sustainId (envelope), SynthProcessor::releaseId (envelope) };
    const std::array captions { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };

    for (size_t i = 0; i < dials.size(); ++i)
    {
        auto& dial = dials[i];
        const auto& parameter = *processor.parameters.getParameter (ids[i]);

        dial.caption = captions[i];
        dial.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        dial.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        dial.slider.setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
        dial.slider.setTitle ("Env " + juce::String (envelope + 1) + " " + dial.caption.toLowerCase());
        dial.slider.setMouseClickGrabsKeyboardFocus (false);
        dial.slider.setWantsKeyboardFocus (false);
        dial.slider.onValueChange = [this] { repaint(); };
        dial.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, ids[i],
                                                                                                  dial.slider);
        addAndMakeVisible (dial.slider);
    }

    refresh();
}

void EnvelopePanel::refresh()
{
    if (const auto version = synthProcessor.getModulationVersion(); version != shownModulationVersion)
    {
        shownModulationVersion = version;
        use = describeUse();
        repaint (captionArea);
    }

    const auto display = synthProcessor.getEnvelopeDisplay (envelope);
    const auto changed = display.has_value() != shownDisplay.has_value()
                      || (display.has_value() && (display->stage != shownDisplay->stage
                                                  || ! juce::approximatelyEqual (display->progress, shownDisplay->progress)));

    if (changed)
    {
        shownDisplay = display;
        repaint (shapeArea);
    }
}

juce::String EnvelopePanel::describeUse() const
{
    juce::StringArray targets;

    for (int target = 0; target < modulation::numTargets; ++target)
        if (synthProcessor.getAssignment (target).source == modulation::envelopeSource (envelope))
            targets.add (modulation::getName (target));

    if (targets.isEmpty())
        return "Right-click a control to use it";

    if (targets.size() == 1)
        return "Moves " + targets[0].toLowerCase();

    return "Moves " + juce::String (targets.size()) + " controls";
}

void EnvelopePanel::resized()
{
    auto area = getLocalBounds().reduced (12, 12);
    captionArea = area.removeFromTop (18);
    area.removeFromTop (4);
    shapeArea = area.removeFromTop (34);
    area.removeFromTop (6);

    const auto dialWidth = area.getWidth() / (int) dials.size();

    for (auto& dial : dials)
    {
        auto column = area.removeFromLeft (dialWidth);
        dial.captionArea = column.removeFromTop (12);
        dial.valueArea = column.removeFromBottom (15);
        dial.slider.setBounds (column);
    }
}

void EnvelopePanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    g.setFont (fonts::caption());
    g.setColour (colours::text);
    g.drawText ("ENV " + juce::String (envelope + 1), captionArea, juce::Justification::centredLeft);
    g.setFont (fonts::body (12.0f));
    g.setColour (colours::dimText);
    g.drawText (use, captionArea.withTrimmedLeft (52), juce::Justification::centredRight, true);

    paintShape (g);

    // The dials sit close together, so their captions are a little smaller than the others.
    const auto dialCaptionFont = juce::Font (juce::FontOptions (10.0f, juce::Font::bold)).withExtraKerningFactor (0.08f);

    for (auto& dial : dials)
    {
        g.setFont (dialCaptionFont);
        g.setColour (colours::dimText);
        g.drawText (dial.caption, dial.captionArea, juce::Justification::centred);
        g.setFont (fonts::body (12.0f));
        g.setColour (colours::text);
        g.drawText (dial.slider.getTextFromValue (dial.slider.getValue()), dial.valueArea, juce::Justification::centred);
    }
}

void EnvelopePanel::paintShape (juce::Graphics& g) const
{
    const auto area = shapeArea.toFloat();
    g.setColour (colours::plot);
    g.fillRoundedRectangle (area, 5.0f);

    const auto settings = synthProcessor.getEnvelopeSettings (envelope);
    const auto inner = area.reduced (6.0f, 4.0f);

    // The sustain takes a fixed share of the width. The other stages share the rest by the square root of their
    // times, so short ones stay visible beside long ones.
    const auto share = [] (float seconds) { return std::sqrt (std::max (0.0f, seconds)) + 0.05f; };
    const auto total = share (settings.attack) + share (settings.decay) + share (settings.release);
    const auto stagesWidth = inner.getWidth() * 0.78f;
    const auto attackEnd = inner.getX() + stagesWidth * share (settings.attack) / total;
    const auto decayEnd = attackEnd + stagesWidth * share (settings.decay) / total;
    const auto sustainEnd = decayEnd + inner.getWidth() * 0.22f;
    const auto releaseEnd = inner.getRight();
    const auto toY = [&inner] (float level) { return inner.getBottom() - level * inner.getHeight(); };

    juce::Path shape;
    shape.startNewSubPath (inner.getX(), toY (0.0f));
    shape.lineTo (attackEnd, toY (1.0f));
    shape.lineTo (decayEnd, toY (settings.sustain));
    shape.lineTo (sustainEnd, toY (settings.sustain));
    shape.lineTo (releaseEnd, toY (0.0f));

    auto filled = shape;
    filled.closeSubPath();
    g.setColour (colours::accent.withAlpha (0.15f));
    g.fillPath (filled);
    g.setColour (colours::accent);
    g.strokePath (shape, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Where the newest note is, on the shape.
    if (shownDisplay.has_value() && shownDisplay->stage != EnvelopeRun::Stage::idle)
    {
        const auto progress = shownDisplay->progress;
        const auto x = [&]
        {
            switch (shownDisplay->stage)
            {
                case EnvelopeRun::Stage::attack:   return juce::jmap (progress, inner.getX(), attackEnd);
                case EnvelopeRun::Stage::decay:    return juce::jmap (progress, attackEnd, decayEnd);
                case EnvelopeRun::Stage::sustain:  return (decayEnd + sustainEnd) / 2.0f;
                case EnvelopeRun::Stage::release:  return juce::jmap (progress, sustainEnd, releaseEnd);
                case EnvelopeRun::Stage::idle:     break;
            }

            return inner.getX();
        }();

        g.setColour (colours::modulation);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, toY (shownDisplay->level) }));
    }
}
