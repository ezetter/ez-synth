#pragma once

#include "FilterPanel.h"
#include "HarmonicSelector.h"
#include "HarmonicSliders.h"
#include "Keyboard.h"
#include "OscillatorCard.h"
#include "SynthLookAndFeel.h"
#include "SynthProcessor.h"
#include "WaveGrid.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The synth's window: a card for each oscillator, preset waves, the grid to draw the chosen oscillator's wave
    on, how many harmonics it plays with a slider for each, the filters, the volume knob and a piano keyboard.
*/
class SynthEditor final : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit SynthEditor (SynthProcessor&);
    ~SynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Brings the window up to date with the synth. It does this itself many times a second. */
    void refresh();

private:
    /** What the window shows depends on these. When any of them changes, it's all brought up to date. */
    struct Shown
    {
        int shapeVersion = -1;
        int editedOscillator = -1;
        std::array<int, SynthProcessor::numOscillators> numHarmonics {};
        std::array<bool, SynthProcessor::numOscillators> on {};

        bool operator== (const Shown&) const = default;
    };

    void timerCallback() override;
    void paintNote (juce::Graphics&) const;
    void editOscillator (int);
    std::vector<waveform::Partial> getPlayedHarmonics (int oscillator) const;
    juce::String getWaveName (int oscillator) const;

    SynthProcessor& synthProcessor;
    SynthLookAndFeel lookAndFeel;

    std::array<std::unique_ptr<OscillatorCard>, SynthProcessor::numOscillators> cards;
    WaveGrid grid;
    HarmonicSliders harmonicSliders;
    FilterPanel filterPanel;
    HarmonicSelector harmonicSelector;
    std::array<juce::TextButton, waveform::allPresets.size()> presetButtons;
    juce::Slider volumeKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAttachment;
    Keyboard keyboard;

    juce::Rectangle<int> titleArea, noteArea, sidePanel, harmonicsCaption, volumePanel, volumeCaption, volumeValue;

    Shown shown;
    int shownNote = -2;
    bool focusGiven = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEditor)
};
