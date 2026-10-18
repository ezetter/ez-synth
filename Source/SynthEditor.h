#pragma once

#include "HarmonicSelector.h"
#include "Keyboard.h"
#include "Legend.h"
#include "SynthLookAndFeel.h"
#include "SynthProcessor.h"
#include "WaveGrid.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The synth's window: preset waves, the grid to draw on, how many harmonics to play, a legend, the volume
    knob and a piano keyboard.
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
    void timerCallback() override;
    void paintNote (juce::Graphics&) const;

    SynthProcessor& synthProcessor;
    SynthLookAndFeel lookAndFeel;

    WaveGrid grid;
    Legend legend;
    HarmonicSelector harmonicSelector;
    std::array<juce::TextButton, waveform::allPresets.size()> presetButtons;
    juce::Slider volumeKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAttachment;
    Keyboard keyboard;

    juce::Rectangle<int> titleArea, noteArea, sidePanel, harmonicsCaption, harmonicsHint, volumePanel,
                         volumeCaption, volumeValue;

    int shownShapeVersion = -1;
    int shownNumPartials = -1;
    int shownNote = -2;
    bool focusGiven = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthEditor)
};
