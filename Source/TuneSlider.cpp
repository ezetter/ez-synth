#include "TuneSlider.h"

#include "Modulation.h"
#include "SynthLookAndFeel.h"
#include "Tuning.h"

TuneSlider::TuneSlider()
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, colours::accent);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setMouseClickGrabsKeyboardFocus (false);
    slider.setWantsKeyboardFocus (false);
    slider.snap = [] (double semitones) { return tuning::snap (semitones); };
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void TuneSlider::setOscillator (juce::RangedAudioParameter& tune, int oscillator)
{
    slider.setTitle ("Osc " + juce::String (oscillator + 1) + " tune");
    modulatable::setTarget (slider, modulation::oscillatorTuneTarget (oscillator));

    // The old oscillator's attachment goes first, or it would set the old oscillator's tune to the new one's.
    attachment.reset();
    attachment = std::make_unique<juce::SliderParameterAttachment> (tune, slider);
    repaint();
}

void TuneSlider::resized()
{
    auto area = getLocalBounds();
    captionArea = area.removeFromTop (18);
    valueArea = captionArea;
    area.removeFromTop (4);
    slider.setBounds (area.removeFromTop (22));
    tickArea = area;
}

void TuneSlider::paint (juce::Graphics& g)
{
    const auto semitones = (float) slider.getValue();
    const auto whole = tuning::toWholeSemitones (semitones);

    g.setFont (fonts::caption());
    g.setColour (colours::dimText);
    g.drawText ("TUNE", captionArea, juce::Justification::centredLeft);

    g.setFont (fonts::body (13.0f));
    g.setColour (colours::text);
    g.drawText (tuning::toText (semitones) + (whole.has_value() ? "  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  "
                                                                      + tuning::getIntervalName (*whole)
                                                                : juce::String()),
                valueArea, juce::Justification::centredRight);

    // A short tick at each semitone, and a longer one, numbered, at each interval marked. The interval the slider is
    // on is picked out.
    const auto ticksTop = (float) tickArea.getY();
    g.setFont (fonts::body (11.5f));

    for (int semitone = 0; semitone <= (int) tuning::octave; ++semitone)
    {
        const auto x = (float) slider.getX() + (float) slider.getPositionOfValue (semitone);
        const auto marked = std::find_if (tuning::markedIntervals.begin(), tuning::markedIntervals.end(),
                                          [semitone] (const tuning::Interval& interval) { return interval.semitones == semitone; });

        if (marked == tuning::markedIntervals.end())
        {
            g.setColour (colours::outline.brighter (0.4f));
            g.fillRect (juce::Rectangle<float> (1.0f, 3.0f).withPosition (x - 0.5f, ticksTop));
            continue;
        }

        const auto isSet = whole == semitone;
        g.setColour (isSet ? colours::accent : colours::dimText);
        g.fillRect (juce::Rectangle<float> (1.0f, 6.0f).withPosition (x - 0.5f, ticksTop));
        g.drawText (juce::String (marked->number), juce::Rectangle<float> (20.0f, 12.0f).withCentre ({ x, ticksTop + 14.0f }),
                    juce::Justification::centred);
    }
}
