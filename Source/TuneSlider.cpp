#include "TuneSlider.h"

#include "Modulation.h"
#include "SynthLookAndFeel.h"

namespace
{
    const juce::String dot (juce::CharPointer_UTF8 ("\xc2\xb7"));
    const juce::String plusOrMinus (juce::CharPointer_UTF8 ("\xc2\xb1"));

    /** How near a whole semitone, on screen, dragging the tune snaps to it, however far it's zoomed in. */
    constexpr double snapPixels = 3.0;
}

TuneSlider::TuneSlider()
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, colours::accent);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setMouseClickGrabsKeyboardFocus (false);
    slider.setWantsKeyboardFocus (false);
    slider.fillOrigin = 0.0;
    slider.snap = [this] (double semitones) { return tuning::snap (semitones, getSnapDistance()); };
    slider.positionToProportion = [this] (float position)
    {
        return (tuning::fromPosition (position) - window.low) / (window.high - window.low);
    };

    // The slider shows only part of the tune's range, so it sets the tune itself, and only when the user moves it.
    // Showing the tune never sets it, even if the tune lies beyond the part shown.
    slider.onDragStart = [this]
    {
        dragging = true;

        if (attachment != nullptr)
            attachment->beginGesture();
    };

    slider.onDragEnd = [this]
    {
        dragging = false;

        if (attachment != nullptr)
            attachment->endGesture();
    };

    slider.onValueChange = [this]
    {
        if (attachment == nullptr)
            return;

        if (dragging)
            attachment->setValueAsPartOfGesture ((float) slider.getValue());
        else
            attachment->setValueAsCompleteGesture ((float) slider.getValue());
    };

    addAndMakeVisible (slider);

    zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange (0.0, 1.0);
    zoomSlider.setColour (juce::Slider::trackColourId, colours::dimText);
    zoomSlider.setDoubleClickReturnValue (true, 0.0);
    zoomSlider.setMouseClickGrabsKeyboardFocus (false);
    zoomSlider.setWantsKeyboardFocus (false);
    zoomSlider.setTitle ("Tune zoom");
    zoomSlider.onValueChange = [this] { moveWindow(); };
    addAndMakeVisible (zoomSlider);

    moveWindow();
}

void TuneSlider::setOscillator (juce::RangedAudioParameter& tuneParameter, int oscillator)
{
    slider.setTitle ("Osc " + juce::String (oscillator + 1) + " tune");
    modulatable::setTarget (slider, modulation::oscillatorTuneTarget (oscillator));

    attachment.reset();
    attachment = std::make_unique<juce::ParameterAttachment> (tuneParameter, [this] (float semitones) { showTune (semitones); });
    attachment->sendInitialUpdate();
    moveWindow();
}

void TuneSlider::setZoom (float zoom)
{
    zoomSlider.setValue (zoom, juce::dontSendNotification);
    moveWindow();
}

float TuneSlider::getZoom() const
{
    return (float) zoomSlider.getValue();
}

void TuneSlider::showTune (float semitones)
{
    tune = semitones;

    // A tune set from elsewhere, such as a preset, that lies beyond the part of the scale shown moves it there.
    if (! dragging && ! window.contains (tune))
        moveWindow();
    else
        slider.setValue (tune, juce::dontSendNotification);

    repaint();
}

void TuneSlider::moveWindow()
{
    window = tuning::getWindow (tune, tuning::zoomToReach (getZoom()));
    slider.setRange (window.low, window.high);
    slider.setValue (tune, juce::dontSendNotification);
    repaint();
}

double TuneSlider::getSnapDistance() const
{
    return snapPixels * (window.high - window.low) / std::max (1, slider.getWidth());
}

void TuneSlider::resized()
{
    auto area = getLocalBounds();
    captionArea = area.removeFromTop (16);
    area.removeFromTop (3);

    auto zoomRow = area.removeFromTop (16);
    zoomCaptionArea = zoomRow.removeFromLeft (48);
    zoomValueArea = zoomRow.removeFromRight (56);
    zoomRow.removeFromRight (8);
    zoomSlider.setBounds (zoomRow);
    area.removeFromTop (3);

    slider.setBounds (area.removeFromTop (22));
    tickArea = area;
}

void TuneSlider::paint (juce::Graphics& g)
{
    const auto whole = tuning::toWholeSemitones (tune);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("TUNE", captionArea, juce::Justification::centredLeft);
    g.drawText ("ZOOM", zoomCaptionArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (colours::text);
    g.drawText (tuning::toText (tune) + (whole.has_value() ? "  " + dot + "  " + tuning::getIntervalName (*whole) : juce::String()),
                captionArea, juce::Justification::centredRight);

    // How far either side the scale reaches.
    const auto reach = (window.high - window.low) / 2.0f;
    g.setFont (fonts::body (12.0f));
    g.setColour (colours::dimText);
    g.drawText (plusOrMinus + (tuning::toWholeSemitones (reach).has_value() ? juce::String (juce::roundToInt (reach)) : juce::String (reach, 1)) + " st",
                zoomValueArea, juce::Justification::centredRight);

    const auto ticksTop = (float) tickArea.getY();
    const auto xOf = [this] (float semitones) { return (float) slider.getX() + (float) slider.getPositionOfValue (semitones); };
    const auto pixelsPerSemitone = (float) slider.getWidth() / (window.high - window.low);

    // Zoomed in, the finest steps between semitones that fit, as faint ticks.
    auto step = 0.0f;

    for (const auto candidate : { 0.5f, 0.25f, 0.1f })
        if (candidate * pixelsPerSemitone >= 7.0f)
            step = candidate;

    if (step > 0.0f)
    {
        g.setColour (colours::outline.brighter (0.25f));

        for (auto i = (int) std::ceil (window.low / step - 0.001f); (float) i * step <= window.high + 0.001f; ++i)
            if (! tuning::toWholeSemitones ((float) i * step).has_value())
                g.fillRect (juce::Rectangle<float> (1.0f, 2.0f).withPosition (xOf ((float) i * step) - 0.5f, ticksTop));
    }

    // A tick at each semitone: a longer, bright one at the note itself, and longer, numbered ones at the intervals
    // marked, above the note and below it. The interval the tune is on is picked out.
    g.setFont (fonts::body (11.5f));

    for (auto semitone = (int) std::ceil (window.low); semitone <= (int) std::floor (window.high); ++semitone)
    {
        const auto x = xOf ((float) semitone);
        const auto marked = std::find_if (tuning::markedIntervals.begin(), tuning::markedIntervals.end(),
                                          [semitone] (const tuning::Interval& interval) { return interval.semitones == std::abs (semitone); });

        if (semitone == 0)
        {
            g.setColour (whole == 0 ? colours::accent : colours::text.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (1.5f, 9.0f).withPosition (x - 0.75f, ticksTop));
        }
        else if (marked == tuning::markedIntervals.end())
        {
            g.setColour (colours::outline.brighter (0.45f));
            g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x - 0.5f, ticksTop));
        }
        else
        {
            g.setColour (whole == semitone ? colours::accent : colours::dimText);
            g.fillRect (juce::Rectangle<float> (1.0f, 6.0f).withPosition (x - 0.5f, ticksTop));
            g.drawText (juce::String (marked->number), juce::Rectangle<float> (20.0f, 10.0f).withCentre ({ x, ticksTop + 11.0f }),
                        juce::Justification::centred);
        }
    }
}
