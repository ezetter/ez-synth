#pragma once

#include "ChoiceButtons.h"
#include "ModulatableControls.h"
#include "Tuning.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Tunes an oscillator away from the note, from an octave below it to an octave above, with the intervals from the
    2nd to the octave marked along it either way. Dragging it snaps to whole semitones, unless Option is held.

    The zoom slider above it shows less of the scale, down to a semitone either side, so small tunings are easy to
    set. Zoomed in, it shows the part around the whole semitone nearest the tuning, which stays put while the tune is
    dragged. Like the other controls, the tune can be right-clicked to have an LFO, envelope, velocity or the mod wheel
    move it. The zoom is the oscillator's own, saved with the sound, and sets how far they move it: at their full
    range, across as much of the scale as is shown.

    Under it, the oscillator's glide: Off, Glide, to slide into every note from the pitch of the one before, or
    Legato, to slide only into notes played while another key's still held, and how long the slide takes.
*/
class TuneSlider final : public juce::Component
{
public:
    TuneSlider();

    /** Shows the tune of the given oscillator, numbered from 0, how far it's zoomed in, and its glide, and sets them. */
    void setOscillator (juce::AudioProcessorValueTreeState&, int oscillator);

    /** How far it's zoomed in, from 0, an octave either side of the note, to 1, a semitone either side. */
    void setZoom (float zoom);
    float getZoom() const;

    /** Which glide button is chosen: 0 for Off, 1 for Glide, 2 for Legato. */
    int getGlideMode() const noexcept;

    /** The part of the scale shown. */
    tuning::Window getWindow() const noexcept   { return window; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 104;

private:
    void showTune (float semitones);
    void moveWindow();
    double getSnapDistance() const;

    ModulatableSlider slider;
    juce::Slider zoomSlider;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    ChoiceButtons glideMode { { "Off", "Glide", "Legato" } };
    juce::Slider glideTimeSlider;

    // After the sliders, so they're let go of before the sliders go.
    std::unique_ptr<juce::SliderParameterAttachment> zoomAttachment, glideTimeAttachment;
    std::unique_ptr<juce::ParameterAttachment> glideModeAttachment;
    float tune = 0.0f;
    tuning::Window window;
    bool dragging = false;
    juce::Rectangle<int> captionArea, zoomCaptionArea, zoomValueArea, tickArea, glideCaptionArea, glideValueArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSlider)
};
