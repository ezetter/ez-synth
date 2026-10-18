#pragma once

#include "ChoiceButtons.h"
#include "ModulatableControls.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

/** A card in the row along the top, for an oscillator or an LFO: a switch, its name, two lines about it, a picture of
    its wave and a dial. Clicking the card picks it to show and edit on the grid.
*/
class SourceCard : public juce::Component
{
public:
    SourceCard (const juce::String& name, juce::AudioProcessorValueTreeState&, const juce::String& onId,
                const juce::String& dialId, const juce::String& dialCaption);

    /** Called when the card is clicked. */
    std::function<void()> onSelect;

    void setSelected (bool);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

protected:
    /** Sets the picture of the wave, from values across one cycle. It's stretched to fill the picture's height. */
    void setWaveValues (std::vector<float>);

    /** Sets the two lines under the name. A card can put a control in place of the second. */
    void setLines (const juce::String& first, const juce::String& second);

    juce::Rectangle<int> getSecondLineArea() const noexcept   { return secondLineArea; }
    bool isOn() const   { return onSwitch.getToggleState(); }

    ModulatableToggle onSwitch;
    ModulatableSlider dial;

private:
    void paintWave (juce::Graphics&) const;

    juce::String name, dialCaption, firstLine, secondLine;
    std::vector<float> waveValues;
    bool selected = false;

    juce::AudioProcessorValueTreeState::ButtonAttachment onAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment dialAttachment;

    juce::Rectangle<int> nameArea, firstLineArea, secondLineArea, waveArea, dialCaptionArea, dialValueArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceCard)
};

/** An oscillator's card: its switch, its wave and how many harmonics it plays, and its level. */
class OscillatorCard final : public SourceCard
{
public:
    OscillatorCard (int oscillator, juce::AudioProcessorValueTreeState&);

    /** Shows the harmonics the oscillator plays, and the name of its wave. */
    void setWave (const std::vector<waveform::Partial>& harmonics, const juce::String& waveName);
};

/** An LFO's card: its switch, its wave, whether it loops or runs once, and its period. */
class LfoCard final : public SourceCard
{
public:
    LfoCard (int lfo, juce::AudioProcessorValueTreeState&);

    void setWave (const waveform::Shape&, const juce::String& waveName);

    void resized() override;

private:
    ChoiceButtons mode { { "Loop", "Once" } };
    juce::ParameterAttachment modeAttachment;
};
