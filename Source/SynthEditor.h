#pragma once

#include "EnvelopePanel.h"
#include "FilterPanel.h"
#include "HarmonicSelector.h"
#include "HarmonicSliders.h"
#include "Keyboard.h"
#include "LfoPanel.h"
#include "MidiLinkMarkers.h"
#include "ModulationOverview.h"
#include "MidiPanel.h"
#include "ModulatableControls.h"
#include "PresetBar.h"
#include "SourceCard.h"
#include "SynthLookAndFeel.h"
#include "SynthProcessor.h"
#include "TuneSlider.h"
#include "WaveGrid.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The synth's window: a card for each oscillator and LFO, preset waves, the grid to draw the chosen one's wave on,
    the harmonics of the chosen oscillator or the controls the chosen LFO moves, a row with the volume knob, the
    envelopes and the filters, and a piano keyboard. Right-clicking a control opens the editor for which LFO or
    envelope moves it.
*/
class SynthEditor final : public juce::AudioProcessorEditor,
                          public ModulationHandler,
                          private juce::Timer
{
public:
    explicit SynthEditor (SynthProcessor&);
    ~SynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Brings the window up to date with the synth. It does this itself many times a second. */
    void refresh();

    /** Shows where the LFOs and envelopes have moved the controls they move. This too happens many times a second. */
    void showModulation();

    void showModulationEditor (juce::Component& control) override;

    /** Shows the screen listing everything that moves a control, or is linked to one, over the synth, or hides it. */
    void showModulationOverview (bool);
    bool isModulationOverviewShown() const   { return modulationOverview.isVisible(); }

    /** Command-Z undoes the last change to the sound, and Shift-Command-Z redoes it. */
    bool keyPressed (const juce::KeyPress&) override;

    /** Each click, scroll or key in the window, so the changes that follow count as the user's and can be undone. */
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void noteUserTouch()   { userTouched = true; }

    /** Makes whatever's changed since last time a step that can be undone, if the user changed it and has finished:
        once the mouse is up and a scroll has stopped. Changes that just happen, from the host, are taken into the
        step the history is on. The window does this many times a second.
    */
    void rememberChanges();

private:
    /** What the window shows depends on these. When any of them changes, it's all brought up to date. */
    struct Shown
    {
        int shapeVersion = -1;
        int modulationVersion = -1;
        int editedSource = -1;
        std::array<int, SynthProcessor::numOscillators> numHarmonics {};
        std::array<int, SynthProcessor::numOscillators> tuneCents {};
        std::array<bool, SynthProcessor::numSources> on {};
        std::array<waveform::Drawing, SynthProcessor::numSources> drawings {};
        std::array<bool, SynthProcessor::numLfos> random {};

        bool operator== (const Shown&) const = default;
    };

    void timerCallback() override;
    void paintNote (juce::Graphics&) const;
    void editSource (int);
    void setEditedWave (const waveform::Shape&, std::optional<waveform::Preset>);
    std::vector<waveform::Partial> getPlayedHarmonics (int oscillator) const;
    static juce::String getWaveName (std::optional<waveform::Preset>);
    juce::String getLfoWaveName (int lfo) const;

    SynthProcessor& synthProcessor;
    SynthLookAndFeel lookAndFeel;

    std::array<std::unique_ptr<OscillatorCard>, SynthProcessor::numOscillators> oscillatorCards;
    std::array<std::unique_ptr<LfoCard>, SynthProcessor::numLfos> lfoCards;
    WaveGrid grid;
    HarmonicSliders harmonicSliders;
    TuneSlider tuneSlider;
    LfoPanel lfoPanel;
    FilterPanel filterPanel;
    std::array<std::unique_ptr<EnvelopePanel>, SynthProcessor::numEnvelopes> envelopePanels;
    HarmonicSelector harmonicSelector;

    /** Gives the oscillator shown random harmonics, every one's amplitude and phase. */
    ModulatableTextButton randomizeButton { "Randomize" };
    juce::Random buttonRandom;
    std::array<juce::TextButton, waveform::allPresets.size()> presetButtons;
    PresetBar presetBar;
    ModulatableSlider volumeKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAttachment;
    Keyboard keyboard;

    /** Only in the standalone app, which plays from MIDI devices itself. */
    std::unique_ptr<MidiPanel> midiPanel;

    /** Tags the controls linked to MIDI controllers, over everything else. */
    MidiLinkMarkers midiLinkMarkers { synthProcessor };

    /** Over everything, while it's shown, and the header's button that shows it, which the plugins need, having no
        Options menu.
    */
    ModulationOverview modulationOverview { synthProcessor };
    juce::TextButton modulationButton { "Modulation" };

    /** Opens the modulation editor for a control, next to something standing for it, such as its row in the overview. */
    void showModulationEditor (int target, juce::Component& anchor);

    /** Names what's under the mouse, for controls that show a picture rather than a word, such as the filter types. */
    juce::TooltipWindow tooltipWindow { this, 500 };

    juce::Rectangle<int> titleArea, noteArea, sidePanel, harmonicsCaption, tuneDivider, volumePanel, volumeCaption, volumeValue;

    Shown shown;
    std::array<SynthProcessor::LiveWave, SynthProcessor::numOscillators> liveWaves;
    std::array<SynthProcessor::RandomCycle, SynthProcessor::numLfos> randomCycles;
    int shownNote = -2;
    bool focusGiven = false;

    int rememberedChangeCount = -1;
    bool userTouched = false;
    juce::uint32 lastScrollTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEditor)
};
