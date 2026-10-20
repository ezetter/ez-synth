#pragma once

#include "ChoiceButtons.h"
#include "NumberStepper.h"
#include "Waveform.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

/** The grid a wave is drawn on, one cycle wide. For an oscillator it shows the drawn wave, the sine waves it
    breaks down into, and their sum, which is what the synth plays. For an LFO, which plays its wave just as it's
    drawn, it shows the wave and where the LFO has got to.

    A wave is drawn smoothly, following the mouse, or in steps: the cycle split into a number of equal steps, from 2
    to 20, each held at one level. Clicking in a step sets it to where the mouse is, and dragging across the steps
    sets each one the mouse passes through.

    An LFO can play a random wave instead, drawn smoothly or in steps, which the grid shows as it plays, and which
    can't be drawn on.
*/
class WaveGrid final : public juce::Component
{
public:
    WaveGrid();

    /** Called as the user draws, with the changed wave. */
    std::function<void (const waveform::Shape&)> onDraw;

    /** Shows how the wave is drawn, smoothly or in steps, without calling onDrawingChange. */
    void setDrawing (const waveform::Drawing&);
    const waveform::Drawing& getDrawing() const noexcept   { return drawing; }

    /** Called when the user changes how the wave is drawn. */
    std::function<void (const waveform::Drawing&)> onDrawingChange;

    /** Shows whether an LFO plays a random wave, with a switch for it, or for an oscillator, nothing, which hides the
        switch. A random wave can't be drawn on.
    */
    void setRandom (std::optional<bool>);

    /** Called when the user switches an LFO's random wave on or off. */
    std::function<void (bool)> onRandomChange;

    /** Sets the wave to show. A wave that repeats ends where it began, unless it's given an end, such as a random
        wave's, which runs on into its next cycle.
    */
    void setShape (const waveform::Shape&, std::optional<float> end = std::nullopt);

    /** The sine waves to show, from the 1st harmonic up, as they're set. */
    void setPartials (std::vector<waveform::Partial>);

    /** Where LFOs, envelopes or velocity have moved the sine waves, and how many there are, as the newest note plays
        them, or nothing to show them as set. They and their sum are drawn there, while the drawn wave stays as drawn.
    */
    void setMovedPartials (std::optional<std::vector<waveform::Partial>>);

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
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getPlotArea() const;
    float toX (double position) const;
    float toY (float value) const;
    double toPosition (float x) const;
    float toValue (float y) const;

    void updateValueRange();
    static float getHighestValue (const std::vector<waveform::Partial>&);
    void paintGrid (juce::Graphics&, juce::Rectangle<float> plot) const;
    void paintHarmonics (juce::Graphics&, float opacity) const;
    void paintSteps (juce::Graphics&, juce::Rectangle<float> plot) const;
    void paintHeader (juce::Graphics&) const;
    juce::Rectangle<float> getHeaderArea() const;
    juce::Path makeCurve (const std::function<float (double position)>& valueAt) const;
    juce::Path makeShapeCurve() const;
    void drawTo (juce::Point<float>);
    void drawStepTo (juce::Point<float>);
    bool isDrawing() const noexcept   { return lastDrawn.has_value() || lastStep.has_value(); }
    void changeDrawing (const waveform::Drawing&);

    waveform::Shape shape {};
    std::optional<float> shapeEnd;
    std::vector<waveform::Partial> partials;
    std::optional<std::vector<waveform::Partial>> movedPartials;
    float valueRange = 1.35f;   // how far above and below zero the grid shows
    int sourceNumber = 1;
    bool on = true;
    std::optional<double> lfoPeriod;   // the period of the LFO shown, or nothing for an oscillator
    bool runsOnce = false;   // whether the LFO shown runs once, so its wave doesn't repeat
    std::optional<float> playhead;
    float playheadValue = 0.0f;

    // Where the mouse last drew: a point number, which can fall between points, and a value. In steps, the step it
    // last set, and the level.
    std::optional<std::pair<double, float>> lastDrawn;
    std::optional<std::pair<int, float>> lastStep;

    /** A switch drawn like the buttons beside it, lit when it's on. */
    class Switch final : public juce::Button
    {
    public:
        explicit Switch (const juce::String& name);
        void paintButton (juce::Graphics&, bool highlighted, bool pressed) override;
    };

    bool canDraw() const noexcept   { return ! random; }

    waveform::Drawing drawing;
    int hoveredStep = -1;
    bool random = false;
    ChoiceButtons drawMode { { "Smooth", "Steps" } };
    NumberStepper stepCount { waveform::fewestSteps, waveform::mostSteps };
    Switch randomSwitch { "Random" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveGrid)
};
