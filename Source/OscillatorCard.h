#pragma once

#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

/** One oscillator's controls: a switch to turn it on, a picture of the wave it plays, and a dial for its level.
    Clicking the card picks the oscillator to show and edit on the grid.
*/
class OscillatorCard final : public juce::Component
{
public:
    OscillatorCard (int oscillator, juce::AudioProcessorValueTreeState&);

    /** Called when the card is clicked. */
    std::function<void()> onSelect;

    void setSelected (bool);

    /** Shows the sine waves the oscillator plays, and the name of its wave. */
    void setWave (std::vector<waveform::Partial>, const juce::String& waveName);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void paintWave (juce::Graphics&, juce::Rectangle<float> area, bool on) const;

    const int oscillator;
    juce::ToggleButton onSwitch;
    juce::Slider levelKnob;
    juce::AudioProcessorValueTreeState::ButtonAttachment onAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment;

    std::vector<waveform::Partial> partials;
    juce::String waveName;
    bool selected = false;

    juce::Rectangle<int> nameArea, waveNameArea, harmonicsArea, waveArea, levelTextArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscillatorCard)
};
