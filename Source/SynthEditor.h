#pragma once

#include "EnvelopePanel.h"
#include "FilterPanel.h"
#include "HarmonicSelector.h"
#include "HarmonicSliders.h"
#include "Keyboard.h"
#include "LfoPanel.h"
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

        bool operator== (const Shown&) const = default;
    };

    void timerCallback() override;
    void paintNote (juce::Graphics&) const;
    void editSource (int);
    void setEditedWave (const waveform::Shape&, std::optional<waveform::Preset>);
    std::vector<waveform::Partial> getPlayedHarmonics (int oscillator) const;
    static juce::String getWaveName (std::optional<waveform::Preset>);

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
    std::array<juce::TextButton, waveform::allPresets.size()> presetButtons;
    PresetBar presetBar;
    ModulatableSlider volumeKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAttachment;
    Keyboard keyboard;

    /** Only in the standalone app, which plays from MIDI devices itself. */
    std::unique_ptr<MidiPanel> midiPanel;

    /** Names what's under the mouse, for controls that show a picture rather than a word, such as the filter types. */
    juce::TooltipWindow tooltipWindow { this, 500 };

    juce::Rectangle<int> titleArea, noteArea, sidePanel, harmonicsCaption, tuneDivider, volumePanel, volumeCaption, volumeValue;

    Shown shown;
    std::array<SynthProcessor::LiveWave, SynthProcessor::numOscillators> liveWaves;
    int shownNote = -2;
    bool focusGiven = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEditor)
};
