#pragma once

#include "Waveform.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

/** The grid a wave is drawn on, one wavelength wide. It shows the drawn wave, the sine waves it breaks down
    into, and their sum, which is what the synth plays.
*/
class WaveGrid final : public juce::Component
{
public:
    WaveGrid();

    /** Called as the user draws, with the changed wave. */
    std::function<void (const waveform::Shape&)> onDraw;

    void setShape (const waveform::Shape&);

    /** The sine waves to show, strongest first. */
    void setPartials (std::vector<waveform::Partial>);

    /** Says which oscillator's wave is shown, numbered from 1. One that's off is shown faintly. */
    void setOscillator (int number, bool isOn);

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

    void paintGrid (juce::Graphics&, juce::Rectangle<float> plot) const;
    juce::Path makeCurve (const std::function<float (double position)>& valueAt) const;
    juce::Path makeShapeCurve() const;
    void drawTo (juce::Point<float>);

    waveform::Shape shape {};
    std::vector<waveform::Partial> partials;
    int oscillatorNumber = 1;
    bool on = true;

    // Where the mouse last drew: a point number, which can fall between points, and a value.
    std::optional<std::pair<double, float>> lastDrawn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveGrid)
};
