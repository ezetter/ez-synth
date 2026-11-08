#pragma once

#include "ChoiceButtons.h"
#include "ModulatableControls.h"
#include "Modulation.h"
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

    /** Sets the two lines under the name. A card can put a control in place of the second. The first line can end
        with a detail, shown at its right.
    */
    void setLines (const juce::String& first, const juce::String& second, const juce::String& firstDetail = {});

    /** The value shown under the dial, and whether it's been moved from where the dial is set, so it's shown in the
        modulation colour. It's the dial's own value, unless a card shows another.
    */
    virtual std::pair<juce::String, bool> getDialText();
    void repaintDialValue()   { repaint (dialValueArea); }

    /** Has the dial set a parameter, in place of the one it set before. */
    void attachDial (const juce::String& parameterId);

    /** Widens the area for the dial's value, out to the left over the picture of the wave, for longer values. */
    void setDialValueWidth (int width);

    juce::Rectangle<int> getSecondLineArea() const noexcept   { return secondLineArea; }
    bool isOn() const   { return onSwitch.getToggleState(); }

    ModulatableToggle onSwitch;
    ModulatableSlider dial;

private:
    void paintWave (juce::Graphics&) const;

    juce::String name, dialCaption, firstLine, secondLine, firstLineDetail;
    std::vector<float> waveValues;
    bool selected = false;

    juce::AudioProcessorValueTreeState::ButtonAttachment onAttachment;
    juce::AudioProcessorValueTreeState& parameters;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dialAttachment;
    int dialValueWidth = 48;

    juce::Rectangle<int> nameArea, firstLineArea, secondLineArea, waveArea, dialCaptionArea, dialValueArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceCard)
};

/** An oscillator's card: its switch, its wave, how many harmonics it plays and how far it's tuned up, and its level. */
class OscillatorCard final : public SourceCard
{
public:
    OscillatorCard (int oscillator, juce::AudioProcessorValueTreeState&);

    /** Shows the harmonics the oscillator plays, the name of its wave, and how many semitones it's tuned up. */
    void setWave (const std::vector<waveform::Partial>& harmonics, const juce::String& waveName, float tune);
};

/** An LFO's card: its switch, its wave, whether it loops or starts with each key, and how many times it plays then,
    and its rate.
*/
class LfoCard final : public SourceCard
{
public:
    LfoCard (int lfo, juce::AudioProcessorValueTreeState&);

    /** Shows a wave, which ends where it began, unless it's given an end, such as a random wave's, which runs on
        into its next cycle, and the name of the wave.
    */
    void setWave (const waveform::Shape&, const juce::String& waveName, std::optional<float> end = std::nullopt);

    void resized() override;

    /** Shows the rate as the mod wheel has moved it, as a position along the dial's travel, or as it's set. */
    void showMovedRate (std::optional<float> position);

    /** Shows the LFO synced to the tempo, its dial turning through the cycles a beat it can run, such as
        "1/2 per beat", or not synced, its dial setting its rate in hertz.
    */
    void showSynced (bool);

    /** The rate shown under the dial, such as "2.00 Hz" or "1/2 per beat", and whether it's as the mod wheel has
        moved it.
    */
    std::pair<juce::String, bool> getShownRate()   { return getDialText(); }

    /** The second button's label: how many times the LFO plays from each key, as last chosen. */
    juce::String getTimesLabel() const   { return mode.getLabel (1); }

private:
    void showMode (int);
    void chooseTimes();
    std::pair<juce::String, bool> getDialText() override;

    const int lfoNumber;
    std::optional<float> movedRate;
    bool synced = false;

    // Loop, or a button showing how many times it plays from each key, which opens a menu of them.
    ChoiceButtons mode { { "Loop", modulation::getLfoModeNames()[1] } };
    juce::ParameterAttachment modeAttachment;
    int timesMode = 1;
};
