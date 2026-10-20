#pragma once

#include "ModulatableControls.h"
#include "Tuning.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Tunes an oscillator away from the note, from an octave below it to an octave above, with the intervals from the
    2nd to the octave marked along it either way. Dragging it snaps to whole semitones, unless Option is held.

    The zoom slider above it shows less of the scale, down to a semitone either side, so small tunings are easy to
    set. Zoomed in, it shows the part around the whole semitone nearest the tuning, which stays put while the tune is
    dragged. Like the other controls, the tune can be right-clicked to have an LFO, envelope or velocity move it.
*/
class TuneSlider final : public juce::Component
{
public:
    TuneSlider();

    /** Shows the tune of the given oscillator, numbered from 0, and sets it. */
    void setOscillator (juce::RangedAudioParameter& tune, int oscillator);

    /** How far it's zoomed in, from 0, an octave either side of the note, to 1, a semitone either side. */
    void setZoom (float zoom);
    float getZoom() const;

    /** The part of the scale shown. */
    tuning::Window getWindow() const noexcept   { return window; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 76;

private:
    void showTune (float semitones);
    void moveWindow();
    double getSnapDistance() const;

    ModulatableSlider slider;
    juce::Slider zoomSlider;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float tune = 0.0f;
    tuning::Window window;
    bool dragging = false;
    juce::Rectangle<int> captionArea, zoomCaptionArea, zoomValueArea, tickArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSlider)
};
