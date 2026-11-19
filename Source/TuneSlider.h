#pragma once

#include "ChoiceButtons.h"
#include "Modulation.h"
#include "ModulatableControls.h"
#include "Tuning.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Tunes an oscillator away from the note, from two octaves below it to two octaves above, with the intervals from
    the 2nd to the 15th, two octaves, marked along it either way. Dragging it snaps to whole semitones, unless Option is held.

    Above it all, where the oscillator's panned, from all the way left to all the way right, 50% each side to begin
    with. Double-clicking it puts it back in the middle. Like the tune, it can be right-clicked to have an LFO,
    envelope, velocity or the mod wheel move it, and the figure beside it then follows where it's moved to.

    The zoom slider above it shows less of the scale, down to a semitone either side, so small tunings are easy to
    set. Zoomed in, it shows the part around the whole semitone nearest the tuning, which stays put while the tune is
    dragged. Like the other controls, the tune can be right-clicked to have an LFO, envelope, velocity or the mod wheel
    move it. The zoom is the oscillator's own, saved with the sound, and sets how far they move it: at their full
    range, across as much of the scale as is shown.

    At the foot, the oscillator's routing: buttons for screens setting where its sound goes, for now the Filters one,
    which sets which filters it's sent through, with which those are beside it.

    Under it, the oscillator's glide: Off, Glide, to slide into every note from the pitch of the one before, or
    Legato, to slide only into notes played while another key's still held, and how long the slide takes.
*/
class TuneSlider final : public juce::Component
{
public:
    TuneSlider();

    /** Shows the tune of the given oscillator, numbered from 0, how far it's zoomed in, and its glide, and sets them. */
    void setOscillator (juce::AudioProcessorValueTreeState&, int oscillator);

    /** How far it's zoomed in, from 0, two octaves either side of the note, to 1, a semitone either side. */
    void setZoom (float zoom);
    float getZoom() const;

    /** Shows only the pan, for an oscillator playing noise, which has no tune or glide. */
    void setPanOnly (bool);

    /** Called when the Filters button's clicked, to show the screen setting which filters the oscillator's sent through. */
    std::function<void()> onShowFilterRouting;
    bool isPanOnly() const noexcept   { return panOnly; }

    /** Which glide button is chosen: 0 for Off, 1 for Glide, 2 for Legato. */
    int getGlideMode() const noexcept;

    /** The part of the scale shown. */
    tuning::Window getWindow() const noexcept   { return window; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 153;

private:
    void showTune (float semitones);
    void moveWindow();
    double getSnapDistance() const;

    ModulatableSlider slider, panSlider;
    juce::Slider zoomSlider;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    ChoiceButtons glideMode { { "Off", "Glide", "Legato" } };
    juce::TextButton filtersButton { "Filters" };
    std::array<std::unique_ptr<juce::ParameterAttachment>, modulation::numFilters> sendAttachments;
    std::array<bool, modulation::numFilters> sends { true, true, true, true };
    juce::Slider glideTimeSlider;

    // After the sliders, so they're let go of before the sliders go.
    std::unique_ptr<juce::SliderParameterAttachment> panAttachment, zoomAttachment, glideTimeAttachment;
    std::unique_ptr<juce::ParameterAttachment> glideModeAttachment;
    float tune = 0.0f;
    tuning::Window window;
    bool dragging = false;
    bool panOnly = false;
    juce::Rectangle<int> routingCaptionArea, routingValueArea, panCaptionArea, panValueArea, captionArea, zoomCaptionArea, zoomValueArea, tickArea, glideCaptionArea, glideValueArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneSlider)
};
