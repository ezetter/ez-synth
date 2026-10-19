#pragma once

#include "Waveform.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

/** The grid a wave is drawn on, one cycle wide. For an oscillator it shows the drawn wave, the sine waves it
    breaks down into, and their sum, which is what the synth plays. For an LFO, which plays its wave just as it's
    drawn, it shows the wave and where the LFO has got to.
*/
class WaveGrid final : public juce::Component
{
public:
    WaveGrid();

    /** Called as the user draws, with the changed wave. */
    std::function<void (const waveform::Shape&)> onDraw;

    void setShape (const waveform::Shape&);

    /** The sine waves to show, from the 1st harmonic up. */
    void setPartials (std::vector<waveform::Partial>);

    /** Says which oscillator's wave is shown, numbered from 1. One that's off is shown faintly. */
    void setOscillator (int number, bool isOn);

    /** Says which LFO's wave is shown, numbered from 1, how long its period is, and whether it runs once rather
        than looping.
    */
    void setLfo (int number, bool isOn, double periodSeconds, bool once);

    /** Where the LFO shown is in its period, from 0 to 1, and its value there, or nothing to hide the line marking
        it.
    */
    void setPlayhead (std::optional<float> position, float value = 0.0f);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getPlotArea() const;
    float toX (double position) const;
    float toY (float value) const;
    double toPosition (float x) const;
    float toValue (float y) const;

    void updateValueRange();
    void paintGrid (juce::Graphics&, juce::Rectangle<float> plot) const;
    void paintHarmonics (juce::Graphics&, float opacity) const;
    void paintHeader (juce::Graphics&, juce::Rectangle<float> bounds) const;
    juce::Path makeCurve (const std::function<float (double position)>& valueAt) const;
    juce::Path makeShapeCurve() const;
    void drawTo (juce::Point<float>);

    waveform::Shape shape {};
    std::vector<waveform::Partial> partials;
    float valueRange = 1.35f;   // how far above and below zero the grid shows
    int sourceNumber = 1;
    bool on = true;
    std::optional<double> lfoPeriod;   // the period of the LFO shown, or nothing for an oscillator
    bool runsOnce = false;   // whether the LFO shown runs once, so its wave doesn't repeat
    std::optional<float> playhead;
    float playheadValue = 0.0f;

    // Where the mouse last drew: a point number, which can fall between points, and a value.
    std::optional<std::pair<double, float>> lastDrawn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveGrid)
};
