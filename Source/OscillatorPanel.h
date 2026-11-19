#pragma once

#include "ChoiceButtons.h"
#include "Modulation.h"
#include "ModulatableControls.h"
#include "Tuning.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The chosen oscillator's controls, other than its wave and harmonics, down a panel to the left of the wave, in
    sections from top to bottom:

    Where it's panned, from all the way left to all the way right, 50% each side to begin with. Double-clicking it
    puts it back in the middle.

    Its unison: how many voices of it play, stacked, from Off, one, to 8, how far apart they're detuned, as the
    cents the outermost are from its pitch, either way, and how widely they're spread from side to side either
    side of its pan.

    Its tune, from two octaves below the note to two octaves above, with the intervals from the 2nd to the 15th, two
    octaves, marked along it either way. Dragging it snaps to whole semitones, unless Option is held. The zoom slider
    above it shows less of the scale, down to a semitone either side, so small tunings are easy to set. Zoomed in, it
    shows the part around the whole semitone nearest the tuning, which stays put while the tune is dragged. The zoom
    is the oscillator's own, saved with the sound, and sets how far modulation moves the tune: at its full range,
    across as much of the scale as is shown.

    Its glide: Off, Glide, to slide into every note from the pitch of the one before, or Legato, to slide only into
    notes played while another key's still held, and how long the slide takes.

    At the foot, its routing: buttons for screens setting where its sound goes, for now the Filters one, which sets
    which filters it's sent through, with which those are beside it.

    Like the other controls, the pan, the unison's sliders and the tune can be right-clicked to have an LFO, envelope,
    velocity or the mod wheel move them, and the figures beside them then follow where they're moved to.
*/
class OscillatorPanel final : public juce::Component
{
public:
    OscillatorPanel();

    /** Shows the tune of the given oscillator, numbered from 0, how far it's zoomed in, and its glide, and sets them. */
    void setOscillator (juce::AudioProcessorValueTreeState&, int oscillator);

    /** How far it's zoomed in, from 0, two octaves either side of the note, to 1, a semitone either side. */
    void setZoom (float zoom);
    float getZoom() const;

    /** Shows only the pan and the routing, for an oscillator playing noise, which has no unison, tune or glide. */
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

    /** The least height it fits in. Given more, it spreads its sections apart. */
    static constexpr int preferredHeight = 347;

private:
    void showTune (float semitones);
    void moveWindow();
    double getSnapDistance() const;

    void showUnison();

    ModulatableSlider slider, panSlider, voicesSlider, detuneSlider, spreadSlider;
    juce::Slider zoomSlider;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    ChoiceButtons glideMode { { "Off", "Glide", "Legato" } };
    juce::TextButton filtersButton { "Filters" };
    std::array<std::unique_ptr<juce::ParameterAttachment>, modulation::numFilters> sendAttachments;
    std::array<bool, modulation::numFilters> sends { true, true, true, true };
    juce::Slider glideTimeSlider;

    // After the sliders, so they're let go of before the sliders go.
    std::unique_ptr<juce::SliderParameterAttachment> panAttachment, voicesAttachment, detuneAttachment, spreadAttachment,
                                                      zoomAttachment, glideTimeAttachment;
    std::unique_ptr<juce::ParameterAttachment> glideModeAttachment;
    float tune = 0.0f;
    tuning::Window window;
    bool dragging = false;
    bool panOnly = false;
    juce::Rectangle<int> routingCaptionArea, routingValueArea, panCaptionArea, unisonCaptionArea, voicesCaptionArea,
                         detuneCaptionArea, spreadCaptionArea, captionArea, zoomCaptionArea, tickArea, glideCaptionArea, glideValueArea;
    std::vector<juce::Rectangle<int>> dividers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscillatorPanel)
};
