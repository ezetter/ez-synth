// Checks that waves break down into the right harmonics, that the synth plays them, and that the LFOs move what
// they're given to move. If asked with --snapshots <folder>, it also saves pictures of the window.

#include "../Source/MidiInputs.h"
#include "../Source/MidiPanel.h"
#include "../Source/ModulationEditor.h"
#include "../Source/RandomWave.h"
#include "../Source/SynthEditor.h"
#include "../Source/SynthProcessor.h"
#include "../Source/Waveform.h"

#include <CoreFoundation/CoreFoundation.h>

#include <complex>
#include <iostream>

namespace
{
    constexpr auto pi = juce::MathConstants<double>::pi;
    constexpr auto twoPi = juce::MathConstants<double>::twoPi;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    int failures = 0;

    void check (bool passed, const juce::String& description)
    {
        std::cout << (passed ? "  pass  " : "  FAIL  ") << description.toStdString() << std::endl;

        if (! passed)
            ++failures;
    }

    bool isNear (double value, double expected, double proportion)
    {
        return std::abs (value - expected) <= std::abs (expected) * proportion;
    }

    void setParameter (SynthProcessor& synth, const juce::String& id, float value)
    {
        auto& parameter = *synth.parameters.getParameter (id);
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    }

    void setNumHarmonics (SynthProcessor& synth, int count, int oscillator = 0)
    {
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) count);
    }

    void setFilter (SynthProcessor& synth, const SynthProcessor::FilterIds& ids, bool on, float cutoff, float resonance = 0.0f)
    {
        setParameter (synth, ids.on, on ? 1.0f : 0.0f);
        setParameter (synth, ids.cutoff, cutoff);
        setParameter (synth, ids.resonance, resonance);
    }

    /** How much a filter should pass at a frequency. The filters are digital copies of the analog state variable
        filter, made with the bilinear transform, which squeezes frequencies so the cutoff lands where it should.
    */
    double expectedGain (bool lowPass, double frequency, double cutoff, double q)
    {
        const auto w = std::tan (pi * frequency / sampleRate) / std::tan (pi * cutoff / sampleRate);
        const auto denominator = std::sqrt (std::pow (1.0 - w * w, 2.0) + std::pow (w / q, 2.0));
        return (lowPass ? 1.0 : w * w) / denominator;
    }

    /** How much a mid pass should pass at a frequency: the state variable filter's band, scaled to let its middle
        through as it is.
    */
    double expectedMidPassGain (double frequency, double centre, double q)
    {
        const auto w = std::tan (pi * frequency / sampleRate) / std::tan (pi * centre / sampleRate);
        return (w / q) / std::sqrt (std::pow (1.0 - w * w, 2.0) + std::pow (w / q, 2.0));
    }

    /** How much a notch should pass at a frequency: the sound, less the share of the mid pass's band that its depth
        cuts, from 0 to 1. At full depth that's the standard notch, (s² + 1) / (s² + s / Q + 1).
    */
    double expectedNotchGain (double frequency, double centre, double q, double cut)
    {
        const auto w = std::tan (pi * frequency / sampleRate) / std::tan (pi * centre / sampleRate);
        return std::abs (std::complex<double> (1.0 - w * w, (1.0 - cut) * w / q))
             / std::abs (std::complex<double> (1.0 - w * w, w / q));
    }

    void setFilterType (SynthProcessor& synth, int filter, FilterType type)
    {
        setParameter (synth, SynthProcessor::filterIds (filter).type, (float) type);
    }

    void setPreset (SynthProcessor& synth, int oscillator, waveform::Preset preset)
    {
        synth.setShape (oscillator, waveform::makePreset (preset), preset);
    }

    /** The amplitude of the sine wave at this frequency in a signal, from a Fourier transform with a Hann window. */
    double amplitudeAt (const std::vector<float>& signal, double frequency)
    {
        double real = 0.0, imaginary = 0.0;
        const auto size = signal.size();

        for (size_t i = 0; i < size; ++i)
        {
            const auto window = 0.5 - 0.5 * std::cos (twoPi * (double) i / (double) (size - 1));
            const auto angle = twoPi * frequency * (double) i / sampleRate;
            real += signal[i] * window * std::cos (angle);
            imaginary += signal[i] * window * std::sin (angle);
        }

        return 4.0 * std::hypot (real, imaginary) / (double) size;
    }

    /** Plays a note, lets it go after holdSeconds, and returns the left channel. Leave time for the note to fade
        away, or it will still be heard at the start of the next one.
    */
    std::vector<float> play (SynthProcessor& synth, int note, double holdSeconds, double totalSeconds, float velocity = 1.0f)
    {
        synth.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> output;
        const auto holdSamples = (int) (holdSeconds * sampleRate);
        const auto totalSamples = (int) (totalSeconds * sampleRate);

        for (int start = 0; start < totalSamples; start += blockSize)
        {
            juce::MidiBuffer midi;

            if (start == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);

            if (start <= holdSamples && holdSamples < start + blockSize)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), holdSamples - start);

            synth.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }

        return output;
    }

    std::vector<float> slice (const std::vector<float>& signal, double fromSeconds, double toSeconds)
    {
        return { signal.begin() + (std::ptrdiff_t) (fromSeconds * sampleRate),
                 signal.begin() + (std::ptrdiff_t) (toSeconds * sampleRate) };
    }

    float peakOf (const std::vector<float>& signal)
    {
        auto peak = 0.0f;

        for (const auto sample : signal)
            peak = std::max (peak, std::abs (sample));

        return peak;
    }

    /** Checks a preset's harmonics, 1st to 9th, have their textbook strengths. Missing ones should be exactly 0. */
    void checkPreset (waveform::Preset preset, const juce::String& description,
                      const std::function<double (int harmonic)>& expectedAmplitude)
    {
        const auto decomposition = waveform::decompose (waveform::makePreset (preset));
        auto amplitudesRight = true;

        for (const auto& harmonic : decomposition.harmonics)
        {
            const auto expected = expectedAmplitude (harmonic.harmonic);
            amplitudesRight = amplitudesRight && (expected == 0.0 ? harmonic.amplitude == 0.0f
                                                                  : isNear (harmonic.amplitude, expected, 0.01));
        }

        check (amplitudesRight, "a " + waveform::getName (preset).toLowerCase() + " " + description);
    }

    void testDecomposition()
    {
        std::cout << "Breaking waves down into sine waves" << std::endl;

        const auto isOdd = [] (int k) { return k % 2 == 1; };

        checkPreset (waveform::Preset::sine, "has only its 1st harmonic", [] (int k) { return k == 1 ? 1.0 : 0.0; });
        checkPreset (waveform::Preset::sawtooth, "has every harmonic, the kth at 2/(πk)", [] (int k) { return 2.0 / (pi * k); });
        checkPreset (waveform::Preset::square, "has odd harmonics at 4/(πk), and the even ones at 0",
                     [isOdd] (int k) { return isOdd (k) ? 4.0 / (pi * k) : 0.0; });
        checkPreset (waveform::Preset::triangle, "has odd harmonics at 8/(πk)², and the even ones at 0",
                     [isOdd] (int k) { return isOdd (k) ? 8.0 / (pi * pi * k * k) : 0.0; });

        const auto sine = waveform::decompose (waveform::makePreset (waveform::Preset::sine));
        check (std::abs (sine.harmonics[0].phase) < 1.0e-4, "a sine's harmonic starts at zero, rising");
        check (isNear (sine.peaks[1], 1.0, 0.001) && isNear (sine.peaks[9], 1.0, 0.001), "a sine peaks at 1");

        const auto square = waveform::decompose (waveform::makePreset (waveform::Preset::square));
        check (square.peaks[9] > 1.1f && square.peaks[9] < 1.25f, "a square's first nine harmonics overshoot its corners, as the Gibbs effect says");

        const auto isSilent = [] (const waveform::Decomposition& decomposition)
        {
            return std::all_of (decomposition.harmonics.begin(), decomposition.harmonics.end(),
                                [] (const waveform::Partial& harmonic) { return harmonic.amplitude == 0.0f; });
        };

        waveform::Shape flat {};
        check (isSilent (waveform::decompose (flat)), "a flat wave has no harmonics");

        waveform::Shape offset {};
        offset.fill (0.5f);
        check (isSilent (waveform::decompose (offset)), "an offset from zero isn't a harmonic");

        // A wave made mostly of its 5th harmonic, with a little of its 1st.
        waveform::Shape mostlyFifth {};
        waveform::addHarmonic (mostlyFifth, 5, 0.8f, 0.0f);
        waveform::addHarmonic (mostlyFifth, 1, 0.2f, 1.0f);

        const auto fifth = waveform::decompose (mostlyFifth);
        check (isNear (fifth.harmonics[4].amplitude, 0.8, 1.0e-4) && isNear (fifth.harmonics[0].amplitude, 0.2, 1.0e-4)
                 && fifth.harmonics[1].amplitude == 0.0f && fifth.harmonics[2].amplitude == 0.0f,
               "adding sine waves at the 1st and 5th harmonics gives a wave with just those harmonics");
        check (std::abs (fifth.harmonics[0].phase - 1.0) < 1.0e-3, "a harmonic's phase is found");
    }

    void testPlaying()
    {
        std::cout << "Playing notes" << std::endl;

        SynthProcessor synth;   // a sawtooth, played with 3 sine waves
        const auto signal = play (synth, 69, 0.5, 1.0);
        const auto steady = slice (signal, 0.1, 0.45);

        check (std::all_of (signal.begin(), signal.end(), [] (float sample) { return std::isfinite (sample); }),
               "the output is all numbers");
        check (peakOf (signal) > 0.05f && peakOf (signal) < 1.0f, "a note is clearly heard, without clipping");

        const auto first = amplitudeAt (steady, 440.0);
        check (isNear (amplitudeAt (steady, 880.0) / first, 1.0 / 2.0, 0.02), "A4's 2nd harmonic, 880 Hz, is half as strong as its 440 Hz fundamental");
        check (isNear (amplitudeAt (steady, 1320.0) / first, 1.0 / 3.0, 0.02), "its 3rd harmonic, 1320 Hz, is a third as strong");
        check (amplitudeAt (steady, 1760.0) / first < 1.0e-3, "it has no 4th harmonic, as only 3 were asked for");
        check (peakOf (slice (signal, 0.8, 1.0)) < 1.0e-4f, "the note fades away after its key is let go");

        setNumHarmonics (synth, 9);
        const auto nine = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (amplitudeAt (nine, 9 * 440.0) / amplitudeAt (nine, 440.0), 1.0 / 9.0, 0.02), "asking for 9 sine waves adds up to the 9th harmonic");

        setPreset (synth, 0, waveform::Preset::square);
        setNumHarmonics (synth, 3);
        const auto square = slice (play (synth, 57, 0.5, 0.8), 0.1, 0.45);   // A3, 220 Hz
        const auto squareFirst = amplitudeAt (square, 220.0);
        check (isNear (amplitudeAt (square, 660.0) / squareFirst, 1.0 / 3.0, 0.02),
               "a square played with 3 harmonics has its 1st and 3rd");
        check (amplitudeAt (square, 440.0) / squareFirst < 1.0e-3 && amplitudeAt (square, 1100.0) / squareFirst < 1.0e-3,
               "...and not its 2nd, which is 0, or its 5th, which is beyond the 3");

        setNumHarmonics (synth, 5);
        const auto squareFive = slice (play (synth, 57, 0.5, 0.8), 0.1, 0.45);
        check (isNear (amplitudeAt (squareFive, 1100.0) / amplitudeAt (squareFive, 220.0), 1.0 / 5.0, 0.02),
               "played with 5, it has its 5th too");

        // C8, 4186 Hz: a square's 5th harmonic is 20.9 kHz, too high to hear, and its 7th and 9th would alias
        // down to 18.7 kHz and 10.3 kHz.
        setNumHarmonics (synth, 9);
        const auto high = slice (play (synth, 108, 0.5, 0.8), 0.1, 0.45);
        const auto highFirst = amplitudeAt (high, 4186.0);
        check (amplitudeAt (high, 3 * 4186.0) / highFirst > 0.3, "a high note keeps the harmonics that can be heard");
        check (amplitudeAt (high, 5 * 4186.0) / highFirst < 1.0e-3, "...drops the ones too high to hear");
        check (amplitudeAt (high, sampleRate - 7 * 4186.0) / highFirst < 1.0e-3
                 && amplitudeAt (high, 9 * 4186.0 - sampleRate) / highFirst < 1.0e-3,
               "...and doesn't alias");
    }

    void testChangingTheWaveMidNote()
    {
        std::cout << "Changing the wave while a note plays" << std::endl;

        SynthProcessor synth;
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> output;

        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);

            if (block == 20)
                setPreset (synth, 0, waveform::Preset::square);

            synth.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }

        // A click shows up as a jump between samples bigger than the wave itself makes.
        auto largestJump = 0.0f;

        for (size_t i = 1; i < output.size(); ++i)
            if (i > 1000)
                largestJump = std::max (largestJump, std::abs (output[i] - output[i - 1]));

        check (largestJump < 0.05f, "the note glides to the new wave without a click");
    }

    /** Sends the grid a press, moves and a release of the mouse at positions through the wave, from 0 to 1, and
        levels, as a user drawing on it would.
    */
    void drawOnGrid (WaveGrid& grid, const std::vector<std::pair<double, float>>& stroke)
    {
        // Where the grid plots a position and a level, at its scale for a wave between -1 and 1.
        const auto plot = grid.getLocalBounds().toFloat().withTrimmedLeft (46.0f).withTrimmedRight (20.0f)
                                                         .withTrimmedTop (40.0f).withTrimmedBottom (32.0f);
        const auto at = [plot] (const std::pair<double, float>& point)
        {
            return juce::Point<float> (plot.getX() + (float) point.first * plot.getWidth(),
                                       plot.getCentreY() - point.second / 1.35f * plot.getHeight() / 2.0f);
        };

        const auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto now = juce::Time::getCurrentTime();
        const auto event = [&] (juce::Point<float> position)
        {
            return juce::MouseEvent (source, position, juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &grid, &grid, now, at (stroke.front()), now, 1, stroke.size() > 1);
        };

        grid.mouseDown (event (at (stroke.front())));

        for (size_t i = 1; i < stroke.size(); ++i)
            grid.mouseDrag (event (at (stroke[i])));

        grid.mouseUp (event (at (stroke.back())));
    }

    void testDrawingInSteps()
    {
        std::cout << "Drawing in steps" << std::endl;

        check (waveform::getStep (0.0, 8) == 0 && waveform::getStep (0.124, 8) == 0 && waveform::getStep (0.125, 8) == 1
                 && waveform::getStep (0.999, 8) == 7 && waveform::getStep (1.0, 8) == 7 && waveform::getStep (-0.1, 8) == 0,
               "the cycle is split into equal steps");

        waveform::Shape shape {};
        waveform::setSteps (shape, 8, 1, 0.5f, 1, 0.5f);
        const auto stepOnly = std::all_of (shape.begin(), shape.end(), [&shape] (const float& value)
        {
            const auto point = (int) (&value - shape.data());
            return juce::exactlyEqual (value, point >= 64 && point < 128 ? 0.5f : 0.0f);
        });
        check (stepOnly, "setting a step holds every point in it, and only those, at its level");

        waveform::setSteps (shape, 4, 0, -1.0f, 3, 1.0f);
        check (juce::exactlyEqual (shape[10], -1.0f) && isNear (shape[200], -1.0 / 3.0, 1.0e-5) && isNear (shape[300], 1.0 / 3.0, 1.0e-5)
                 && juce::exactlyEqual (shape[500], 1.0f),
               "a stroke across steps sets those between along the line from the first to the last");

        // Drawing on the grid with the mouse.
        WaveGrid grid;
        grid.setBounds (0, 0, 820, 520);
        waveform::Shape drawn {};
        grid.setShape (drawn);
        grid.onDraw = [&drawn] (const waveform::Shape& changed) { drawn = changed; };
        std::optional<waveform::Drawing> chosen;
        grid.onDrawingChange = [&chosen] (const waveform::Drawing& drawing) { chosen = drawing; };

        check (! grid.getDrawing().stepped && grid.getDrawing().numSteps == 8, "a wave starts being drawn smoothly, with 8 steps ready");

        grid.setDrawing ({ true, 4 });
        drawOnGrid (grid, { { 0.6, 0.5f } });
        const auto isLevel = [&drawn] (int from, int to, float level)
        {
            return std::all_of (drawn.begin() + from, drawn.begin() + to, [level] (float value) { return isNear (value, level, 0.02); });
        };

        check (isLevel (256, 384, 0.5f) && isLevel (0, 256, 0.0f) && isLevel (384, 512, 0.0f),
               "in steps, a click sets the step it's in to where the mouse is, and nothing else");

        drawOnGrid (grid, { { 0.1, 0.9f }, { 0.12, 0.6f }, { 0.95, -0.6f } });
        check (isLevel (0, 128, 0.6f) && isLevel (128, 256, 0.2f) && isLevel (256, 384, -0.2f) && isLevel (384, 512, -0.6f),
               "dragging across the steps sets each one the mouse passes through, the step it's in following it up and down");

        grid.setDrawing ({ false, 4 });
        drawOnGrid (grid, { { 0.5, 0.8f } });
        check (isNear (drawn[256], 0.8, 0.02) && isNear (drawn[250], 0.2, 0.02) && isNear (drawn[262], -0.2, 0.02),
               "drawing smoothly sets only the points under the mouse, as before");

        check (! chosen.has_value(), "showing a way of drawing doesn't count as the user choosing it");

        // Each oscillator and LFO keeps its own way of drawing, which is saved with the sound.
        SynthProcessor synth;
        check (! synth.getDrawing (0).stepped && ! synth.getDrawing (SynthProcessor::numOscillators).stepped,
               "every wave starts being drawn smoothly");

        synth.setDrawing (1, { true, 12 });
        synth.setDrawing (SynthProcessor::numOscillators + 2, { true, 5 });
        juce::MemoryBlock state;
        synth.getStateInformation (state);
        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getDrawing (1) == waveform::Drawing { true, 12 } && restored.getDrawing (SynthProcessor::numOscillators + 2) == waveform::Drawing { true, 5 }
                 && ! restored.getDrawing (0).stepped,
               "how each wave is drawn is saved and restored");

        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());

        for (auto* wave : xml->getChildWithTagNameIterator ("WAVE"))
        {
            wave->removeAttribute ("stepped");
            wave->removeAttribute ("steps");
        }

        juce::AudioProcessor::copyXmlToBinary (*xml, state);
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (! restored.getDrawing (1).stepped && restored.getDrawing (1).numSteps == 8,
               "a save from before waves could be drawn in steps has them drawn smoothly");

        check (synth.loadFactoryPreset ("Step Filter") && synth.getDrawing (SynthProcessor::numOscillators + 1) == waveform::Drawing { true, 8 },
               "the Step Filter preset's LFO opens in its 8 steps");
    }

    void testEditingHarmonics()
    {
        std::cout << "Raising and lowering harmonics" << std::endl;

        SynthProcessor synth;
        setPreset (synth, 0, waveform::Preset::sine);
        setNumHarmonics (synth, 3);
        const auto plainSine = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);

        synth.setHarmonicAmplitude (0, 3, 0.5f);
        const auto& raised = synth.getDecomposition (0);
        check (isNear (raised.harmonics[2].amplitude, 0.5, 1.0e-4), "a sine's 3rd harmonic can be raised from nothing");
        check (isNear (raised.harmonics[0].amplitude, 1.0, 1.0e-4) && raised.harmonics[1].amplitude == 0.0f,
               "...leaving its other harmonics as they were");
        check (! synth.getPreset (0).has_value(), "...after which it's no longer a sine");

        const auto played = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (amplitudeAt (played, 1320.0) / amplitudeAt (played, 440.0), 0.5, 0.02), "the raised harmonic is heard");

        synth.setHarmonicAmplitude (0, 1, 0.5f);
        synth.setHarmonicAmplitude (0, 3, 0.0f);
        const auto lowered = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (synth.getDecomposition (0).harmonics[2].amplitude == 0.0f, "a harmonic can be lowered to nothing");
        check (isNear (amplitudeAt (lowered, 440.0) / amplitudeAt (plainSine, 440.0), 0.5, 0.02),
               "lowering a sine's fundamental to half makes it half as loud");

        // A sawtooth's harmonics fall as a sine wave does at half a turn, upside down.
        setPreset (synth, 0, waveform::Preset::sawtooth);
        const auto sawtooth = synth.getShape (0);
        const auto secondAmplitude = synth.getDecomposition (0).harmonics[1].amplitude;
        synth.setHarmonicAmplitude (0, 2, 0.0f);
        synth.setHarmonicAmplitude (0, 2, secondAmplitude);

        auto largestDifference = 0.0f;

        for (size_t i = 0; i < sawtooth.size(); ++i)
            largestDifference = std::max (largestDifference, std::abs (synth.getShape (0)[i] - sawtooth[i]));

        check (largestDifference < 1.0e-4f, "a harmonic lowered to nothing and raised again comes back as it was");

        setPreset (synth, 0, waveform::Preset::sine);
        synth.setHarmonicAmplitude (0, 2, 0.5f);
        check (std::abs (synth.getDecomposition (0).harmonics[1].phase) < 1.0e-4,
               "a harmonic raised from nothing after choosing a preset starts like a sine, whatever the wave before had");

        setPreset (synth, 0, waveform::Preset::sawtooth);
        synth.setHarmonicAmplitude (0, 7, 0.3f);
        check (isNear (synth.getDecomposition (0).harmonics[6].amplitude, 0.3, 1.0e-4),
               "a harmonic beyond those played can be set too, and is kept for when it's played");

        synth.setHarmonicAmplitude (0, 1, waveform::maxAmplitude);
        synth.setHarmonicAmplitude (0, 2, waveform::maxAmplitude);
        synth.setHarmonicAmplitude (0, 3, waveform::maxAmplitude);
        const auto loud = play (synth, 69, 0.5, 0.8);
        setPreset (synth, 0, waveform::Preset::sine);
        const auto plain = play (synth, 69, 0.5, 0.8);
        check (peakOf (loud) <= peakOf (plain) * 1.001f, "a wave raised beyond full scale is turned down to fit");
    }

    void testOscillators()
    {
        std::cout << "Mixing oscillators" << std::endl;

        SynthProcessor synth;
        check (synth.isOn (0) && ! synth.isOn (1) && ! synth.isOn (2), "only the first oscillator is on to begin with");

        for (int i = 0; i < SynthProcessor::numOscillators; ++i)
            setPreset (synth, i, waveform::Preset::sine);

        const auto fundamental = [&synth] { return amplitudeAt (slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45), 440.0); };
        const auto one = fundamental();

        setParameter (synth, SynthProcessor::onId (1), 1.0f);
        setParameter (synth, SynthProcessor::levelId (1), 0.5f);
        check (isNear (fundamental() / one, 1.5, 0.01), "a second oscillator at half level adds half as much again");

        setParameter (synth, SynthProcessor::onId (2), 1.0f);
        check (isNear (fundamental() / one, 2.5, 0.01), "a third at full level adds as much again as the first");

        setParameter (synth, SynthProcessor::onId (2), 0.0f);
        setParameter (synth, SynthProcessor::onId (1), 0.0f);
        check (isNear (fundamental() / one, 1.0, 0.01), "switching them off leaves only the first");

        // The second oscillator plays a square at half level, alongside the first's sine.
        setPreset (synth, 1, waveform::Preset::square);
        setParameter (synth, SynthProcessor::onId (1), 1.0f);
        const auto mixed = slice (play (synth, 57, 0.5, 0.8), 0.1, 0.45);   // A3, 220 Hz

        // A wave whose harmonics add up to more than full scale is turned down to fit, as a square's do.
        const auto& square = synth.getDecomposition (1);
        const auto squareScale = 0.5 / std::max (1.0f, square.peaks[3]);
        const auto expectedFirst = 1.0 + squareScale * square.harmonics[0].amplitude;
        const auto expectedThird = squareScale * square.harmonics[2].amplitude;
        const auto mixedFirst = amplitudeAt (mixed, 220.0);
        check (isNear (amplitudeAt (mixed, 660.0) / mixedFirst, expectedThird / expectedFirst, 0.02),
               "a square in the second oscillator adds its 3rd harmonic to the first oscillator's sine");
        check (amplitudeAt (mixed, 440.0) / mixedFirst < 1.0e-3, "...and no 2nd harmonic, as neither wave has one");
        check (square.peaks[3] > 1.0f, "(a square's first 3 harmonics do add up to more than full scale)");

        setNumHarmonics (synth, 1, 1);
        const auto fewer = slice (play (synth, 57, 0.5, 0.8), 0.1, 0.45);
        check (amplitudeAt (fewer, 660.0) / amplitudeAt (fewer, 220.0) < 1.0e-3,
               "each oscillator has its own number of harmonics");

        setParameter (synth, SynthProcessor::onId (1), 0.0f);
        setParameter (synth, SynthProcessor::levelId (0), 0.0f);
        check (peakOf (play (synth, 69, 0.5, 0.8)) < 1.0e-6f, "an oscillator at zero level is silent");
    }

    void testFilters()
    {
        std::cout << "Filtering" << std::endl;

        SynthProcessor synth;   // a sawtooth
        setNumHarmonics (synth, 9);
        const auto open = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);   // A4: harmonics from 440 Hz to 3960 Hz
        const auto gainAt = [&open] (const std::vector<float>& filtered, double frequency)
        {
            return amplitudeAt (filtered, frequency) / amplitudeAt (open, frequency);
        };
        const auto flatQ = (double) FilterStage::resonanceToQ (0.0f);

        check (synth.parameters.getRawParameterValue (SynthProcessor::filterIds (0).on)->load() < 0.5f
                 && synth.parameters.getRawParameterValue (SynthProcessor::filterIds (1).on)->load() < 0.5f,
               "both filters are out to begin with");

        setFilter (synth, SynthProcessor::filterIds (0), true, 1000.0f);
        const auto lowPassed = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (lowPassed, 440.0), expectedGain (true, 440.0, 1000.0, flatQ), 0.02)
                 && isNear (gainAt (lowPassed, 3960.0), expectedGain (true, 3960.0, 1000.0, flatQ), 0.05),
               "a low pass at 1 kHz keeps the 1st harmonic and cuts the 9th by "
                 + juce::String (-juce::Decibels::gainToDecibels (expectedGain (true, 3960.0, 1000.0, flatQ)), 1) + " dB");

        setFilter (synth, SynthProcessor::filterIds (0), false, 1000.0f);
        const auto switchedOut = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (switchedOut, 3960.0), 1.0, 0.001), "switching it out restores the sound");

        setFilter (synth, SynthProcessor::filterIds (1), true, 1000.0f);
        const auto highPassed = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (highPassed, 440.0), expectedGain (false, 440.0, 1000.0, flatQ), 0.02)
                 && isNear (gainAt (highPassed, 3960.0), expectedGain (false, 3960.0, 1000.0, flatQ), 0.02),
               "a high pass at 1 kHz cuts the 1st harmonic by "
                 + juce::String (-juce::Decibels::gainToDecibels (expectedGain (false, 440.0, 1000.0, flatQ)), 1)
                 + " dB and keeps the 9th");

        setFilter (synth, SynthProcessor::filterIds (1), true, 500.0f);
        setFilter (synth, SynthProcessor::filterIds (0), true, 2000.0f);
        const auto band = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (band, 880.0), expectedGain (false, 880.0, 500.0, flatQ) * expectedGain (true, 880.0, 2000.0, flatQ), 0.02),
               "both filters at once shape the sound one after the other");

        setFilter (synth, SynthProcessor::filterIds (1), false, 500.0f);
        setFilter (synth, SynthProcessor::filterIds (0), true, 1320.0f, 1.0f);
        const auto resonant = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (resonant, 1320.0), FilterStage::resonanceToQ (1.0f), 0.03),
               "full resonance boosts the harmonic at the cutoff " + juce::String (FilterStage::resonanceToQ (1.0f), 1) + " times");

        // Switching a filter in during a note fades it in, rather than jumping.
        setFilter (synth, SynthProcessor::filterIds (0), false, 300.0f);
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> output;

        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);

            if (block == 20)
                setParameter (synth, SynthProcessor::filterIds (0).on, 1.0f);

            synth.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }

        const auto largestJump = [&output] (size_t from, size_t to)
        {
            auto largest = 0.0f;

            for (auto i = from; i < to; ++i)
                largest = std::max (largest, std::abs (output[i] - output[i - 1]));

            return largest;
        };

        const auto switchPoint = (size_t) (20 * blockSize);
        check (largestJump (switchPoint, output.size()) <= largestJump (1000, switchPoint) * 1.05f,
               "switching a filter in during a note doesn't click");

        // Either filter can be a mid pass, which lets through a band around its cutoff, as wide as it's set, in octaves
        // between the points where it lets through half the power, and lets the middle through as it is. Here it's
        // centred on A4's 2nd harmonic and 2 octaves wide, so it lets through half the power of the 1st and the 4th.
        setFilter (synth, SynthProcessor::filterIds (0), false, 1000.0f);
        setFilterType (synth, 1, FilterType::midPass);
        setParameter (synth, SynthProcessor::filterIds (1).width, 2.0f);
        setFilter (synth, SynthProcessor::filterIds (1), true, 880.0f);
        const auto midPassed = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        const auto halfPower = std::sqrt (0.5);
        check (isNear (gainAt (midPassed, 880.0), 1.0, 0.02) && isNear (gainAt (midPassed, 440.0), halfPower, 0.03)
                 && isNear (gainAt (midPassed, 1760.0), halfPower, 0.03),
               "a mid pass 2 octaves wide lets its middle through as it is, and half the power an octave either side");
        check (isNear (gainAt (midPassed, 3960.0), expectedMidPassGain (3960.0, 880.0, FilterStage::widthToQ (2.0f)), 0.05)
                 && gainAt (midPassed, 3960.0) < 0.5 * halfPower,
               "...and less further out, cutting the 9th harmonic by "
                 + juce::String (-juce::Decibels::gainToDecibels (expectedMidPassGain (3960.0, 880.0, FilterStage::widthToQ (2.0f))), 1) + " dB");

        setParameter (synth, SynthProcessor::filterIds (1).width, 0.5f);
        const auto narrow = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (narrow, 880.0), 1.0, 0.02)
                 && isNear (gainAt (narrow, 440.0), expectedMidPassGain (440.0, 880.0, FilterStage::widthToQ (0.5f)), 0.05)
                 && gainAt (narrow, 440.0) < 0.5 * gainAt (midPassed, 440.0),
               "a narrower mid pass lets through less either side, and its middle just as much");

        // A notch takes away a band around its cutoff. At full depth it takes away all but a thousandth of the cutoff
        // itself, and half the power at the edges of its bandwidth, which are its cutoff over its Q apart. Here it's at
        // A4's 2nd harmonic with a Q of 1.2, so its edges are 733 Hz apart, at 587 Hz and at 1320 Hz, the 3rd harmonic.
        setFilter (synth, SynthProcessor::filterIds (1), false, 880.0f);
        setFilterType (synth, 0, FilterType::notch);
        setParameter (synth, SynthProcessor::filterIds (0).q, 1.2f);
        setParameter (synth, SynthProcessor::filterIds (0).depth, 60.0f);
        setFilter (synth, SynthProcessor::filterIds (0), true, 880.0f);
        const auto notched = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (gainAt (notched, 880.0) < 0.003 && isNear (gainAt (notched, 1320.0), halfPower, 0.03)
                 && isNear (gainAt (notched, 440.0), expectedNotchGain (440.0, 880.0, 1.2, 1.0), 0.03)
                 && isNear (gainAt (notched, 3960.0), 1.0, 0.03),
               "a notch at full depth takes away its cutoff, and half the power at the edges of its bandwidth, its cutoff over its Q apart");

        // Its depth is how far it cuts its cutoff, in decibels: 6 dB halves it, and none leaves it as it is.
        setParameter (synth, SynthProcessor::filterIds (0).depth, 6.0f);
        const auto shallow = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        const auto halfCut = 1.0 - std::pow (10.0, -6.0 / 20.0);
        check (isNear (gainAt (shallow, 880.0), 0.501, 0.03) && isNear (gainAt (shallow, 1320.0), expectedNotchGain (1320.0, 880.0, 1.2, halfCut), 0.03),
               "a notch 6 dB deep halves its cutoff, and cuts less either side");

        setParameter (synth, SynthProcessor::filterIds (0).depth, 0.0f);
        const auto noDepth = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (noDepth, 880.0), 1.0, 0.01) && isNear (gainAt (noDepth, 1320.0), 1.0, 0.01),
               "a notch with no depth leaves the sound as it is");

        check (isNear (FilterStage::qToOctaves (juce::MathConstants<float>::sqrt2), 1.0, 1.0e-4)
                 && isNear (FilterStage::qToOctaves (FilterStage::widthToQ (2.5f)), 2.5, 1.0e-4),
               "a Q of 1.41 is a bandwidth of an octave, as the Audio EQ Cookbook has it, and agrees with the mid pass's widths");
        setFilter (synth, SynthProcessor::filterIds (0), false, 880.0f);

        // The first filter can be a high pass and the second a low pass, swapping them round.
        setFilterType (synth, 0, FilterType::highPass);
        setFilterType (synth, 1, FilterType::lowPass);
        setFilter (synth, SynthProcessor::filterIds (0), true, 500.0f);
        setFilter (synth, SynthProcessor::filterIds (1), true, 2000.0f);
        const auto swapped = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (swapped, 880.0), expectedGain (false, 880.0, 500.0, flatQ) * expectedGain (true, 880.0, 2000.0, flatQ), 0.02),
               "either filter can be any type: here the first is a high pass and the second a low pass");
        setFilter (synth, SynthProcessor::filterIds (1), false, 2000.0f);

        // Changing a filter's type while a note plays fades from one to the other, rather than clicking.
        setFilterType (synth, 0, FilterType::lowPass);
        setFilter (synth, SynthProcessor::filterIds (0), true, 1000.0f);
        synth.prepareToPlay (sampleRate, blockSize);
        output.clear();

        for (int block = 0; block < 60; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);

            if (block == 20)
                setFilterType (synth, 0, FilterType::highPass);

            synth.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }

        const auto settled = std::max (largestJump (1000, switchPoint), largestJump (switchPoint + 4800, output.size()));
        check (largestJump (switchPoint, switchPoint + 4800) <= settled * 1.05f,
               "changing a filter's type during a note doesn't click");

        juce::MidiBuffer letGo;
        letGo.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        synth.processBlock (buffer, letGo);
        setFilter (synth, SynthProcessor::filterIds (0), false, 1000.0f);

        // A save from before the filters could each be any type had a low pass and a high pass. They load as the
        // first filter, a low pass, and the second, a high pass, with what moved them still moving them.
        {
            juce::XmlElement older ("EZSynth");
            const auto addParameter = [&older] (const char* id, double value)
            {
                auto* parameter = older.createNewChildElement ("PARAM");
                parameter->setAttribute ("id", id);
                parameter->setAttribute ("value", value);
            };

            addParameter ("lowPassOn", 1.0);
            addParameter ("lowPassCutoff", 1500.0);
            addParameter ("lowPassResonance", 0.3);
            addParameter ("highPassOn", 1.0);
            addParameter ("highPassCutoff", 120.0);
            addParameter ("highPassResonance", 0.0);

            auto* lfoOnCutoff = older.createNewChildElement ("MODULATION");
            lfoOnCutoff->setAttribute ("target", "lowPassCutoff");
            lfoOnCutoff->setAttribute ("source", "LFO 1");
            lfoOnCutoff->setAttribute ("range", 0.4);
            auto* velocityOnCutoff = older.createNewChildElement ("VELOCITY")->createNewChildElement ("ASSIGNMENT");
            velocityOnCutoff->setAttribute ("target", "highPassCutoff");
            velocityOnCutoff->setAttribute ("range", 0.3);

            juce::MemoryBlock state;
            juce::AudioProcessor::copyXmlToBinary (older, state);

            SynthProcessor loaded;
            setFilterType (loaded, 0, FilterType::midPass);
            setFilterType (loaded, 1, FilterType::midPass);
            loaded.setStateInformation (state.getData(), (int) state.getSize());

            const auto value = [&loaded] (const juce::String& id) { return (double) loaded.parameters.getRawParameterValue (id)->load(); };
            const auto first = SynthProcessor::filterIds (0), second = SynthProcessor::filterIds (1);
            check (juce::roundToInt (value (first.type)) == (int) FilterType::lowPass && value (first.on) > 0.5
                     && isNear (value (first.cutoff), 1500.0, 1.0e-4) && isNear (value (first.resonance), 0.3, 1.0e-4)
                     && juce::roundToInt (value (second.type)) == (int) FilterType::highPass && value (second.on) > 0.5
                     && isNear (value (second.cutoff), 120.0, 1.0e-4),
                   "a save from before the filters could be any type loads its low pass and high pass as they were");
            check (loaded.getAssignment (modulation::filterCutoffTarget (0)).source == 0
                     && loaded.getVelocityAssignment (modulation::filterCutoffTarget (1)).isActive(),
                   "...with what moved them still moving them");
        }

        // The panel shows a resonance dial beside a low or high pass's cutoff, and a width dial beside a mid pass's.
        {
            SynthProcessor shown;
            FilterPanel panel (shown.parameters);
            const auto isShown = [&panel] (const juce::String& title)
            {
                return std::any_of (panel.getChildren().begin(), panel.getChildren().end(),
                                    [&title] (juce::Component* child) { return child->isVisible() && child->getTitle() == title; });
            };

            check (isShown ("Filter 1 resonance") && ! isShown ("Filter 1 width") && isShown ("Filter 2 resonance"),
                   "the filters start as a low pass and a high pass, with resonance dials");
            setFilterType (shown, 0, FilterType::midPass);
            check (isShown ("Filter 1 width") && ! isShown ("Filter 1 resonance") && isShown ("Filter 2 resonance"),
                   "a mid pass shows a width dial instead");
            setFilterType (shown, 0, FilterType::notch);
            check (isShown ("Filter 1 Q") && isShown ("Filter 1 depth") && ! isShown ("Filter 1 width") && ! isShown ("Filter 1 resonance"),
                   "a notch shows Q and depth dials beside its cutoff");
        }

        // At a low sample rate a 20 kHz cutoff is beyond the Nyquist frequency, where the filter can't work.
        SynthProcessor lowRate;
        setFilter (lowRate, SynthProcessor::filterIds (0), true, 20000.0f);
        lowRate.prepareToPlay (11025.0, blockSize);
        std::vector<float> lowRateOutput;

        for (int block = 0; block < 20; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);

            lowRate.processBlock (buffer, midi);
            lowRateOutput.insert (lowRateOutput.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }

        check (std::all_of (lowRateOutput.begin(), lowRateOutput.end(), [] (float sample) { return std::isfinite (sample); })
                 && peakOf (lowRateOutput) > 0.05f && peakOf (lowRateOutput) < 1.0f,
               "a cutoff too high for the sample rate is lowered to fit, rather than failing");
    }

    void setLfo (SynthProcessor& synth, int lfo, float periodSeconds, bool once, waveform::Preset preset)
    {
        setParameter (synth, SynthProcessor::lfoPeriodId (lfo), periodSeconds);
        setParameter (synth, SynthProcessor::lfoModeId (lfo), once ? 1.0f : 0.0f);
        synth.setLfoShape (lfo, waveform::makePreset (preset), preset);
    }

    /** How loud a stretch of a signal is: its root mean square. */
    double loudness (const std::vector<float>& signal, double fromSeconds, double toSeconds)
    {
        const auto stretch = slice (signal, fromSeconds, toSeconds);
        auto sum = 0.0;

        for (const auto sample : stretch)
            sum += sample * sample;

        return std::sqrt (sum / (double) stretch.size());
    }

    /** Plays notes starting at the given times and holds them until the end. Then it lets them go, and plays on
        until they've faded, so the next test starts in silence.
    */
    std::vector<float> playNotes (SynthProcessor& synth, const std::vector<std::pair<double, int>>& notes, double totalSeconds)
    {
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> output;
        const auto totalSamples = (int) (totalSeconds * sampleRate);
        const auto fadedSamples = totalSamples + (int) ((HarmonicVoice::releaseSeconds + 0.05) * sampleRate);

        for (int start = 0; start < fadedSamples; start += blockSize)
        {
            juce::MidiBuffer midi;

            for (const auto& [seconds, note] : notes)
            {
                if (const auto position = (int) (seconds * sampleRate) - start; position >= 0 && position < blockSize)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), position);

                if (const auto position = totalSamples - start; position >= 0 && position < blockSize)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), position);
            }

            synth.processBlock (buffer, midi);

            if (start < totalSamples)
                output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }

        return output;
    }

    void testLfos()
    {
        std::cout << "LFOs" << std::endl;

        // Every control an LFO can move is named, and has a key that finds it again. The parameters' keys are their IDs.
        {
            SynthProcessor synth;
            auto keysWork = true, parametersFound = true;
            juce::StringArray names;

            for (int target = 0; target < modulation::numTargets; ++target)
            {
                keysWork = keysWork && modulation::findTarget (modulation::getKey (target)) == target;
                parametersFound = parametersFound && (! modulation::isParameter (target)
                                                      || synth.parameters.getParameter (modulation::getKey (target)) != nullptr);
                names.addIfNotAlreadyThere (modulation::getName (target));
            }

            check (keysWork && names.size() == modulation::numTargets, "all " + juce::String (modulation::numTargets) + " controls an LFO can move have their own names");
            check (parametersFound, "the controls that are parameters are found by their parameter IDs");
        }

        const auto lfoOn = [] (SynthProcessor& synth, int lfo) { return synth.isLfoOn (lfo); };

        // A sine LFO with a half-second period swings the oscillator's level between nothing and full.
        SynthProcessor synth;
        setPreset (synth, 0, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::levelId (0), 0.5f);
        setLfo (synth, 0, 0.5f, false, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::lfoOnId (0), 0.0f);
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, false });
        check (lfoOn (synth, 0), "giving an LFO a control to move switches it on");

        // The sine rises to its peak a quarter of the way through its period, and falls to its trough at three quarters.
        auto signal = play (synth, 69, 1.6, 2.0);
        const auto peak = loudness (signal, 0.10, 0.15);
        const auto trough = loudness (signal, 0.35, 0.40);
        check (peak > 10.0 * trough, "a looping LFO swings an oscillator's level up and down");
        check (isNear (loudness (signal, 1.10, 1.15), peak, 0.05), "...over and over, once each period");

        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, true });
        signal = play (synth, 69, 1.6, 2.0);
        check (loudness (signal, 0.35, 0.40) > 10.0 * loudness (signal, 0.10, 0.15), "inverted, it falls as the LFO rises");

        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 0.5f, false });
        signal = play (synth, 69, 1.6, 2.0);
        check (isNear (loudness (signal, 0.10, 0.15) / loudness (signal, 0.35, 0.40), 3.0, 0.1),
               "a range of 50% swings a level set at 50% between 25% and 75%");

        setParameter (synth, SynthProcessor::lfoOnId (0), 0.0f);
        signal = play (synth, 69, 1.6, 2.0);
        check (isNear (loudness (signal, 0.10, 0.15), loudness (signal, 0.35, 0.40), 0.01), "an LFO that's off moves nothing");
        setParameter (synth, SynthProcessor::lfoOnId (0), 1.0f);

        // Shifted to the right by a phase, a control follows where the LFO was that much of a period ago.
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, false, 0.25f });
        signal = play (synth, 69, 1.6, 2.0);
        check (loudness (signal, 0.225, 0.275) > 10.0 * loudness (signal, 0.475, 0.525),
               "a phase of 90 degrees has an LFO move a control a quarter of a period later");

        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, false, 0.75f });
        signal = play (synth, 69, 1.6, 2.0);
        check (loudness (signal, 0.475, 0.525) > 10.0 * loudness (signal, 0.225, 0.275),
               "a phase of 270 degrees moves it a quarter of a period earlier, as the wave repeats");

        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, false, 2.0f });
        check (juce::exactlyEqual (synth.getAssignment (modulation::oscillatorLevelTarget (0)).phase, modulation::latestPhase),
               "a phase stops a degree short of a whole period");

        // Run once, a rising sawtooth over 0.2 seconds is an envelope that swells each note in, then holds.
        setLfo (synth, 0, 0.2f, true, waveform::Preset::sawtooth);
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, false });
        signal = playNotes (synth, { { 0.0, 69 }, { 0.5, 76 } }, 1.0);   // A4, then E5 half a second later
        const auto e5 = juce::MidiMessage::getMidiNoteInHertz (76);
        const auto a4Held = amplitudeAt (slice (signal, 0.30, 0.45), 440.0);

        check (amplitudeAt (slice (signal, 0.02, 0.06), 440.0) < 0.3 * a4Held,
               "an LFO that runs once starts each note at the start of its wave");
        check (isNear (amplitudeAt (slice (signal, 0.80, 0.95), 440.0), a4Held, 0.02), "...and holds its last value afterwards");
        check (amplitudeAt (slice (signal, 0.52, 0.56), e5) < 0.3 * amplitudeAt (slice (signal, 0.80, 0.95), e5),
               "a second note starts its own run from the start");
        check (isNear (amplitudeAt (slice (signal, 0.52, 0.56), 440.0), a4Held, 0.02), "...without starting the first note's again");

        // Shifted to the right by half its period, a sine that runs once over 0.4 seconds waits 0.2 seconds after the
        // key, holding the control where its wave starts. Then it peaks a quarter of a period later, and falls to its
        // trough a half after that.
        setLfo (synth, 0, 0.4f, true, waveform::Preset::sine);
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, false, 0.5f });
        signal = playNotes (synth, { { 0.0, 69 } }, 1.0);
        const auto waiting = loudness (signal, 0.05, 0.15);

        check (isNear (waiting, loudness (signal, 0.80, 0.95), 0.03),
               "a phase holds an LFO that runs once where its wave starts, until it starts late");
        check (loudness (signal, 0.28, 0.32) > 1.5 * waiting && loudness (signal, 0.48, 0.52) < 0.1 * waiting,
               "...then runs through its wave");

        // Through the last stretch of its period, an LFO that runs once heads for its wave's last point, rather than
        // back to its first as a looping one does.
        {
            const auto rising = waveform::makePreset (waveform::Preset::sawtooth);
            const auto secondToLast = rising[waveform::numPoints - 2];
            auto stays = true;

            for (int i = 0; i <= 100; ++i)
            {
                const auto periodsGone = (waveform::numPoints - 2 + 2.0 * i / 100) / waveform::numPoints;
                stays = stays && modulation::lfoValue (rising, periodsGone, true) >= secondToLast;
            }

            check (stays, "an LFO that runs once doesn't slide back towards its first value at the end of its period");

            const auto lastStretch = (waveform::numPoints - 0.5) / waveform::numPoints;
            check (modulation::lfoValue (rising, lastStretch, false) < rising.back() - 0.1f,
                   "...while a looping one does, as its wave repeats");
        }

        synth.setAssignment (modulation::oscillatorLevelTarget (0), {});
        setParameter (synth, SynthProcessor::levelId (0), 1.0f);

        // A square LFO switches between its two halves, so each half of its period can be measured on its own.
        setLfo (synth, 1, 1.0f, false, waveform::Preset::square);

        setPreset (synth, 0, waveform::Preset::sawtooth);
        setNumHarmonics (synth, 5);
        synth.setAssignment (modulation::oscillatorHarmonicsTarget (0), { 1, 1.0f, false });
        signal = play (synth, 69, 0.95, 1.3);
        check (amplitudeAt (slice (signal, 0.1, 0.4), 3 * 440.0) / amplitudeAt (slice (signal, 0.1, 0.4), 440.0) > 0.25
                 && amplitudeAt (slice (signal, 0.6, 0.9), 3 * 440.0) / amplitudeAt (slice (signal, 0.6, 0.9), 440.0) < 0.001,
               "an LFO on the number of harmonics plays all 9, then just the 1st");
        synth.setAssignment (modulation::oscillatorHarmonicsTarget (0), {});

        setPreset (synth, 0, waveform::Preset::sine);
        synth.setAssignment (modulation::harmonicTarget (0, 2), { 1, 1.0f, false });
        signal = play (synth, 69, 0.95, 1.3);
        const auto raisedTo = modulation::amplitudeRange().convertFrom0to1 (0.5f);
        check (isNear (amplitudeAt (slice (signal, 0.1, 0.4), 880.0) / amplitudeAt (slice (signal, 0.1, 0.4), 440.0), raisedTo, 0.03)
                 && amplitudeAt (slice (signal, 0.6, 0.9), 880.0) / amplitudeAt (slice (signal, 0.6, 0.9), 440.0) < 0.001,
               "an LFO on a sine's 2nd harmonic raises it to " + juce::String (raisedTo, 2) + ", then takes it away");
        synth.setAssignment (modulation::harmonicTarget (0, 2), {});

        setPreset (synth, 0, waveform::Preset::sawtooth);
        setNumHarmonics (synth, 9);
        setFilter (synth, SynthProcessor::filterIds (0), true, 1000.0f);
        synth.setAssignment (modulation::filterCutoffTarget (0), { 1, 0.4f, false });
        signal = play (synth, 69, 0.95, 1.3);
        check (amplitudeAt (slice (signal, 0.1, 0.4), 2200.0) > 10.0 * amplitudeAt (slice (signal, 0.6, 0.9), 2200.0),
               "an LFO on the low pass cutoff sweeps it up and down, letting the 5th harmonic through and then cutting it");
        synth.setAssignment (modulation::filterCutoffTarget (0), {});
        setFilter (synth, SynthProcessor::filterIds (0), false, 1000.0f);

        setParameter (synth, SynthProcessor::levelId (0), 0.0f);
        setParameter (synth, SynthProcessor::onId (1), 0.0f);
        synth.setAssignment (modulation::oscillatorOnTarget (1), { 1, 1.0f, false });
        signal = play (synth, 69, 0.95, 1.3);
        check (loudness (signal, 0.1, 0.4) > 0.01 && loudness (signal, 0.6, 0.9) < 1.0e-4,
               "an LFO on an oscillator's switch switches it on and off");

        // Saving and loading the LFOs and what they move.
        waveform::Shape drawn {};
        drawn.fill (0.5f);
        drawn[0] = -1.0f;
        synth.setLfoShape (2, drawn);
        setParameter (synth, SynthProcessor::lfoPeriodId (2), 7.5f);
        setParameter (synth, SynthProcessor::lfoModeId (2), 1.0f);
        synth.setAssignment (modulation::volumeTarget, { 2, 0.3f, true, 0.75f });

        juce::MemoryBlock state;
        synth.getStateInformation (state);
        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        check (restored.getLfoShape (2) == drawn && ! restored.getLfoPreset (2).has_value()
                 && restored.getLfoPreset (1) == waveform::Preset::square,
               "the LFOs' waves are restored");
        check (isNear (restored.getLfoPeriod (2), 7.5, 1.0e-4) && restored.isLfoOnce (2) && ! restored.isLfoOnce (1),
               "their periods, and whether they loop, are restored");
        check (restored.getAssignment (modulation::volumeTarget) == modulation::Assignment { 2, 0.3f, true, 0.75f }
                 && restored.getAssignment (modulation::oscillatorOnTarget (1)) == modulation::Assignment { 1, 1.0f, false },
               "what each LFO moves, and how, with what phase, is restored");

        // Everything at once: 16 notes, three oscillators of 9 harmonics, both filters, and LFOs moving much of it.
        SynthProcessor busy;

        for (int i = 0; i < SynthProcessor::numOscillators; ++i)
        {
            setParameter (busy, SynthProcessor::onId (i), 1.0f);
            setNumHarmonics (busy, 9, i);
            busy.setAssignment (modulation::oscillatorLevelTarget (i), { i, 0.5f, false });
            busy.setAssignment (modulation::harmonicTarget (i, 4), { (i + 1) % 3, 0.5f, false, 0.3f * (float) i });
            setParameter (busy, SynthProcessor::tuneId (i), 3.5f * (float) i);
        }

        setFilter (busy, SynthProcessor::filterIds (0), true, 3000.0f, 0.5f);
        setFilter (busy, SynthProcessor::filterIds (1), true, 100.0f);
        busy.setAssignment (modulation::filterCutoffTarget (0), { 0, 0.5f, false });
        busy.setAssignment (modulation::filterCutoffTarget (1), { 1, 0.2f, true });
        setParameter (busy, SynthProcessor::lfoModeId (2), 1.0f);

        std::vector<std::pair<double, int>> chord;

        for (int i = 0; i < 16; ++i)
            chord.push_back ({ 0.0, 40 + 3 * i });

        const auto started = juce::Time::getMillisecondCounterHiRes();
        signal = playNotes (busy, chord, 2.0);
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
        check (std::all_of (signal.begin(), signal.end(), [] (float sample) { return std::isfinite (sample); }) && seconds < 0.5,
               "playing 16 notes with everything on and moving takes " + juce::String (seconds * 1000.0 / 2.0, 1)
                 + " ms a second of sound, under a quarter of the time it lasts");
    }

    void setEnvelope (SynthProcessor& synth, int envelope, const EnvelopeSettings& settings)
    {
        setParameter (synth, SynthProcessor::attackId (envelope), settings.attack);
        setParameter (synth, SynthProcessor::decayId (envelope), settings.decay);
        setParameter (synth, SynthProcessor::sustainId (envelope), settings.sustain);
        setParameter (synth, SynthProcessor::releaseId (envelope), settings.release);
    }

    void testRandomLfos()
    {
        std::cout << "Random LFOs" << std::endl;

        constexpr std::uint32_t seed = 12345;
        const waveform::Drawing smooth { false, 8 }, eightSteps { true, 8 };

        // The smooth random wave.
        {
            auto withinRange = true, joined = true, deterministic = true;
            auto turns = 0;
            std::vector<double> stretches;   // how long each rise and fall lasts, in cycles
            constexpr int cycles = 200, pointsPerCycle = 2000;
            auto lastTurn = 0.0;
            auto previous = randomwave::valueAt (seed, 0.0, smooth);
            auto previousSlope = 0.0;

            for (int i = 1; i <= cycles * pointsPerCycle; ++i)
            {
                const auto t = (double) i / pointsPerCycle;
                const auto value = randomwave::valueAt (seed, t, smooth);
                const auto slope = (double) (value - previous) * pointsPerCycle;
                withinRange = withinRange && value >= -1.0f && value <= 1.0f;

                if (slope * previousSlope < 0.0)
                {
                    ++turns;
                    stretches.push_back (t - lastTurn);
                    lastTurn = t;
                }

                previous = value;
                previousSlope = juce::approximatelyEqual (slope, 0.0) ? previousSlope : slope;
            }

            for (int cycle = 1; cycle < 50; ++cycle)
            {
                // Where each cycle meets the next, the value and the slope carry straight on.
                const auto before = randomwave::valueAt (seed, cycle - 1.0e-5, smooth), at = randomwave::valueAt (seed, (double) cycle, smooth);
                const auto after = randomwave::valueAt (seed, cycle + 1.0e-5, smooth);
                joined = joined && std::abs (at - before) < 1.0e-3 && std::abs ((after - at) - (at - before)) < 1.0e-5;
                deterministic = deterministic && juce::exactlyEqual (at, randomwave::valueAt (seed, (double) cycle, smooth));
            }

            const auto [shortest, longest] = std::minmax_element (stretches.begin(), stretches.end());
            const auto turnsPerCycle = (double) turns / cycles;

            check (withinRange, "a smooth random wave stays between -1 and 1");
            check (joined, "...and each cycle starts where the last one ended, at the same slope, so it runs on smoothly");
            check (turnsPerCycle > 2.0 && turnsPerCycle < 8.0,
                   "...turning " + juce::String (turnsPerCycle, 1) + " times a cycle, so it rises and falls without being jagged");
            check (*shortest < 0.15 && *longest > 0.5,
                   "...over stretches of different lengths, from " + juce::String (*shortest, 2) + " to " + juce::String (*longest, 2) + " of a cycle");
            check (deterministic, "...and the same seed gives the same wave");

            const auto first = randomwave::getCycle (seed, 0, smooth), second = randomwave::getCycle (seed, 1, smooth);
            auto difference = 0.0;

            for (size_t i = 0; i < first.size(); ++i)
                difference += std::abs (first[i] - second[i]) / (double) first.size();

            check (difference > 0.2, "each cycle is a new wave");
        }

        // The random wave in steps.
        {
            std::array<int, 10> counts {};
            auto flat = true;
            auto lowest = 1.0f, highest = -1.0f;
            constexpr int numSteps = 40000;

            for (int step = 0; step < numSteps; ++step)
            {
                const auto start = randomwave::valueAt (seed, (step + 0.01) / 8.0, eightSteps);
                flat = flat && juce::exactlyEqual (start, randomwave::valueAt (seed, (step + 0.99) / 8.0, eightSteps));
                ++counts[(size_t) juce::jlimit (0, 9, (int) ((start + 1.0f) * 5.0f))];
                lowest = std::min (lowest, start);
                highest = std::max (highest, start);
            }

            check (flat, "a random wave in steps holds each step at one level");
            check (std::all_of (counts.begin(), counts.end(), [] (int count) { return count > 0.09 * numSteps && count < 0.11 * numSteps; })
                     && lowest < -0.999f && highest > 0.999f,
                   "...each level from -1 to 1 as likely as any other");

            // The last step of each cycle and the first of the next have nothing to do with each other.
            auto sumLast = 0.0, sumFirst = 0.0, sumProduct = 0.0, sumLastSquared = 0.0, sumFirstSquared = 0.0;
            constexpr int numCycles = 5000;

            for (int cycle = 0; cycle < numCycles; ++cycle)
            {
                const double last = randomwave::valueAt (seed, cycle + 0.95, eightSteps), next = randomwave::valueAt (seed, cycle + 1.05, eightSteps);
                sumLast += last;
                sumFirst += next;
                sumProduct += last * next;
                sumLastSquared += last * last;
                sumFirstSquared += next * next;
            }

            const auto n = (double) numCycles;
            const auto correlation = (sumProduct - sumLast * sumFirst / n)
                                   / std::sqrt ((sumLastSquared - sumLast * sumLast / n) * (sumFirstSquared - sumFirst * sumFirst / n));
            check (std::abs (correlation) < 0.05, "...the first and last steps as random as the others, even where the cycles meet");

            check (juce::exactlyEqual (randomwave::onceValueAt (seed, 1.5, eightSteps), randomwave::valueAt (seed, 0.95, eightSteps))
                     && juce::exactlyEqual (randomwave::onceValueAt (seed, -0.3, eightSteps), randomwave::valueAt (seed, 0.05, eightSteps)),
                   "an LFO that runs once holds its random wave's first step before it starts, and its last after it ends");
        }

        // Played: a random LFO swinging an oscillator's level.
        SynthProcessor synth;
        setPreset (synth, 0, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::levelId (0), 0.5f);
        setLfo (synth, 0, 0.25f, false, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::lfoRandomId (0), 1.0f);
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 1.0f, false });

        // How loud each eighth of a cycle is, through a stretch of cycles.
        const auto profile = [] (const std::vector<float>& signal, double from, int numCycles)
        {
            std::vector<double> levels;

            for (int i = 0; i < numCycles * 8; ++i)
                levels.push_back (loudness (signal, from + i * 0.25 / 8.0, from + (i + 1) * 0.25 / 8.0));

            return levels;
        };

        const auto differenceBetween = [] (const std::vector<double>& a, size_t aFrom, const std::vector<double>& b, size_t bFrom)
        {
            auto total = 0.0, size = 0.0;

            for (size_t i = 0; i < 8; ++i)
            {
                total += std::abs (a[aFrom + i] - b[bFrom + i]);
                size += std::abs (a[aFrom + i]);
            }

            return total / size;
        };

        auto signal = play (synth, 69, 1.4, 1.6);
        auto levels = profile (signal, 0.25, 4);
        check (differenceBetween (levels, 0, levels, 8) > 0.2 && differenceBetween (levels, 8, levels, 16) > 0.2,
               "a looping random LFO plays a new wave every cycle");

        auto largestJump = 0.0f;

        for (auto i = (size_t) (0.1 * sampleRate); i < (size_t) (1.3 * sampleRate); ++i)
            largestJump = std::max (largestJump, std::abs (signal[i] - signal[i - 1]));

        check (largestJump < 0.03f, "...and runs on smoothly from one cycle to the next, without a click");

        setParameter (synth, SynthProcessor::lfoRandomId (0), 0.0f);
        signal = play (synth, 69, 1.4, 1.6);
        levels = profile (signal, 0.25, 4);
        check (differenceBetween (levels, 0, levels, 8) < 0.02, "(switched off, it plays its drawn wave, the same every cycle)");

        // In steps, a looping random LFO holds the level through each step.
        setParameter (synth, SynthProcessor::lfoRandomId (0), 1.0f);
        synth.setDrawing (SynthProcessor::numOscillators, eightSteps);
        signal = play (synth, 69, 1.4, 1.6);
        auto steady = true;
        const auto loudest = loudness (signal, 0.25, 1.25) * 3.0;

        // Each step glides to its level over its first 20 ms or so, so as not to click, then holds it.
        for (int step = 8; step < 40; ++step)
        {
            const auto start = 0.25 * step / 8.0;
            const auto earlier = loudness (signal, start + 0.020, start + 0.025), later = loudness (signal, start + 0.025, start + 0.030);
            steady = steady && std::abs (later - earlier) < 0.02 * loudest;
        }

        check (steady, "in steps, a random LFO holds each step's level");
        synth.setDrawing (SynthProcessor::numOscillators, smooth);

        // Run once, a random LFO plays a new wave with each key.
        setParameter (synth, SynthProcessor::lfoModeId (0), 1.0f);
        const auto firstNote = profile (play (synth, 69, 0.4, 0.7), 0.0, 1);
        const auto secondNote = profile (play (synth, 69, 0.4, 0.7), 0.0, 1);
        check (differenceBetween (firstNote, 0, secondNote, 0) > 0.2, "an LFO that runs once plays a new random wave with each key");
        setParameter (synth, SynthProcessor::lfoModeId (0), 0.0f);

        // The window shows the cycle that's playing.
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int block = 0; block < 60; ++block)
        {
            juce::MidiBuffer nothing;
            synth.processBlock (buffer, nothing);
        }

        const auto playing = synth.getRandomCycle (0);
        const auto progress = synth.getLfoProgress (0).value_or (0.0);
        const auto shown = synth.getPlayedLfoShape (0);
        check (playing.cycle == (std::int64_t) (60.0 * blockSize / (0.25 * sampleRate))
                 && shown == randomwave::getCycle (playing.seed, playing.cycle, smooth)
                 && isNear (synth.getLfoValue (0), randomwave::valueAt (playing.seed, (double) playing.cycle + progress, smooth), 1.0e-6),
               "the window shows the random wave's cycle that's playing, and where it is in it");

        // Saved and restored, with its drawing.
        synth.setDrawing (SynthProcessor::numOscillators, eightSteps);
        juce::MemoryBlock state;
        synth.getStateInformation (state);
        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.isLfoRandom (0) && ! restored.isLfoRandom (1) && restored.getDrawing (SynthProcessor::numOscillators) == eightSteps,
               "whether each LFO plays a random wave is saved and restored");

        // The grid shows a switch for an LFO, and can't be drawn on while it plays a random wave.
        WaveGrid grid;
        grid.setBounds (0, 0, 820, 520);
        waveform::Shape drawn {};
        grid.setShape (drawn);
        auto drewSomething = false;
        grid.onDraw = [&drewSomething] (const waveform::Shape&) { drewSomething = true; };

        grid.setRandom (true);
        drawOnGrid (grid, { { 0.5, 0.5f }, { 0.7, -0.5f } });
        check (! drewSomething, "a random wave can't be drawn on");

        grid.setRandom (false);
        drawOnGrid (grid, { { 0.5, 0.5f } });
        check (drewSomething, "...but the LFO's own wave can, once random is off");

        const auto randomSwitchShown = [&grid]
        {
            return std::any_of (grid.getChildren().begin(), grid.getChildren().end(),
                                [] (juce::Component* child) { return child->getName() == "Random" && child->isVisible(); });
        };

        check (randomSwitchShown(), "an LFO's grid has a switch for its random wave");
        grid.setRandom (std::nullopt);
        check (! randomSwitchShown(), "...and an oscillator's has none");

        // In the window, choosing a wave for an LFO that's playing a random one has it play that wave instead.
        {
            SynthProcessor app;
            setParameter (app, SynthProcessor::lfoRandomId (1), 1.0f);
            app.editedSource = SynthProcessor::numOscillators + 1;
            std::unique_ptr<juce::AudioProcessorEditor> editor (app.createEditorAndMakeActive());

            for (auto* child : editor->getChildren())
                if (auto* button = dynamic_cast<juce::TextButton*> (child); button != nullptr && button->getButtonText() == "Square")
                    button->onClick();

            check (! app.isLfoRandom (1) && app.getLfoPreset (1) == waveform::Preset::square,
                   "choosing a wave for an LFO playing a random one plays that wave instead");
            app.editorBeingDeleted (editor.get());
        }
    }

    void testEnvelopes()
    {
        std::cout << "Envelopes" << std::endl;

        {
            const EnvelopeSettings settings { 1.0f, 1.0f, 0.5f, 2.0f };
            EnvelopeRun run;
            std::vector<float> levels;

            run.start();

            for (const auto seconds : { 0.5, 0.5, 0.5, 0.5, 10.0 })
            {
                run.advance (settings, seconds);
                levels.push_back (run.getLevel());
            }

            check (isNear (levels[0], 0.5, 1.0e-5) && isNear (levels[1], 1.0, 1.0e-5),
                   "an attack of 1 second rises to the peak in 1 second");
            check (isNear (levels[2], 0.75, 1.0e-5) && isNear (levels[3], 0.5, 1.0e-5),
                   "a decay of 1 second falls to the sustain level in 1 second");
            check (isNear (levels[4], 0.5, 1.0e-5) && run.getStage() == EnvelopeRun::Stage::sustain,
                   "the sustain level holds for as long as the key is held");

            run.release();
            run.advance (settings, 1.0);
            const auto halfReleased = run.getLevel();
            run.advance (settings, 1.0);
            check (isNear (halfReleased, 0.25, 1.0e-5) && run.getLevel() == 0.0f && run.isIdle(),
                   "a release of 2 seconds falls from the sustain level to nothing in 2 seconds");

            run.start();
            run.advance (settings, 0.4);
            run.release();
            run.advance (settings, 1.0);
            check (isNear (run.getLevel(), 0.2, 1.0e-5), "a key let go during the attack releases from where the attack got to");

            run.start();
            run.advance ({ 0.0f, 0.0f, 0.3f, 0.0f }, 0.0);
            check (isNear (run.getLevel(), 0.3, 1.0e-5) && run.getStage() == EnvelopeRun::Stage::sustain,
                   "an attack and decay of 0 go straight to the sustain level");

            run.advance ({ 0.0f, 0.0f, 0.8f, 0.0f }, 0.01);
            check (isNear (run.getLevel(), 0.8, 1.0e-5), "turning the sustain dial while a key is held moves the level");
        }

        // Env 1 shapes the loudness of OSC 1, whose level is turned down to 0, so the envelope is all of it.
        SynthProcessor synth;
        setPreset (synth, 0, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::levelId (0), 0.0f);
        setEnvelope (synth, 0, { 0.2f, 0.2f, 0.5f, 0.4f });
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { modulation::envelopeSource (0), 1.0f, false });

        auto signal = play (synth, 69, 1.0, 1.8);
        const auto peak = loudness (signal, 0.19, 0.21);
        check (isNear (loudness (signal, 0.09, 0.11) / peak, 0.5, 0.08), "an envelope's attack swells a note in");
        check (isNear (loudness (signal, 0.6, 0.9) / peak, 0.5, 0.05), "...its decay falls to the sustain level, where it holds");
        check (isNear (loudness (signal, 1.19, 1.21) / peak, 0.25, 0.1), "...and its release fades the note when the key is let go");
        check (loudness (signal, 1.33, 1.36) > 0.01 * peak && loudness (signal, 1.5, 1.8) < 1.0e-5,
               "a note goes on sounding through a long release on its loudness, and stops when it's over");
        check (isNear (synth.getTailLengthSeconds(), 0.4, 1.0e-4), "hosts are told the note can sound for the length of that release");

        // Each note runs its own envelope.
        setEnvelope (synth, 0, { 0.4f, 0.0f, 1.0f, 0.1f });
        signal = playNotes (synth, { { 0.0, 69 }, { 0.6, 76 } }, 1.2);
        const auto e5 = juce::MidiMessage::getMidiNoteInHertz (76);
        check (amplitudeAt (slice (signal, 0.62, 0.66), e5) < 0.3 * amplitudeAt (slice (signal, 1.05, 1.15), e5)
                 && isNear (amplitudeAt (slice (signal, 0.62, 0.66), 440.0), amplitudeAt (slice (signal, 0.45, 0.55), 440.0), 0.02),
               "each key starts its own envelope, leaving notes already held alone");

        // While a key is held, the window shows where the newest note is in each envelope.
        setEnvelope (synth, 0, { 0.0f, 0.1f, 0.5f, 0.4f });
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);

            synth.processBlock (buffer, midi);
        }

        const auto display = synth.getEnvelopeDisplay (0);
        check (display.has_value() && display->stage == EnvelopeRun::Stage::sustain && isNear (display->level, 0.5, 1.0e-4),
               "the window can show where the newest note is in an envelope");

        juce::MidiBuffer letGo;
        letGo.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        synth.processBlock (buffer, letGo);

        for (int block = 0; block < 60; ++block)
        {
            juce::MidiBuffer midi;
            synth.processBlock (buffer, midi);
        }

        synth.setAssignment (modulation::oscillatorLevelTarget (0), {});
        setParameter (synth, SynthProcessor::levelId (0), 1.0f);

        // An envelope on anything else doesn't make a note last longer.
        setEnvelope (synth, 1, { 0.0f, 0.0f, 1.0f, 5.0f });
        setFilter (synth, SynthProcessor::filterIds (0), true, 2000.0f);
        synth.setAssignment (modulation::filterCutoffTarget (0), { modulation::envelopeSource (1), 0.5f, true });
        signal = play (synth, 69, 0.5, 1.2);
        check (loudness (signal, 0.9, 1.2) < 1.0e-5, "a long release on the filter's envelope doesn't keep a note sounding");
        synth.setAssignment (modulation::filterCutoffTarget (0), {});
        setFilter (synth, SynthProcessor::filterIds (0), false, 2000.0f);

        // Inverted, an envelope moves a control down: the volume, set at 75%, down to 25%.
        const auto plain = loudness (play (synth, 69, 0.5, 0.8), 0.2, 0.4);
        setEnvelope (synth, 2, { 0.0f, 0.0f, 1.0f, 0.0f });
        synth.setAssignment (modulation::volumeTarget, { modulation::envelopeSource (2), 0.5f, true });
        const auto lowered = loudness (play (synth, 69, 0.5, 0.8), 0.2, 0.4);
        check (isNear (lowered / plain, Patch::volumeToGain (0.25f) / Patch::volumeToGain (0.75f), 0.02),
               "an inverted envelope moves a control down from where it's set");

        // Saving and loading.
        juce::MemoryBlock state;
        synth.getStateInformation (state);
        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        const auto settings = restored.getEnvelopeSettings (1);
        check (isNear (settings.release, 5.0, 1.0e-4) && isNear (settings.sustain, 1.0, 1.0e-4),
               "the envelopes' settings are restored");
        check (restored.getAssignment (modulation::volumeTarget) == modulation::Assignment { modulation::envelopeSource (2), 0.5f, true },
               "what the envelopes move is restored");

        juce::XmlElement older ("EZSynth");
        auto* lfoModulation = older.createNewChildElement ("MODULATION");
        lfoModulation->setAttribute ("target", "osc1Level");
        lfoModulation->setAttribute ("lfo", 1);
        lfoModulation->setAttribute ("range", 0.5);
        juce::AudioProcessor::copyXmlToBinary (older, state);
        SynthProcessor upgraded;
        upgraded.setStateInformation (state.getData(), (int) state.getSize());
        check (upgraded.getAssignment (modulation::oscillatorLevelTarget (0)).source == 1,
               "a save from before there were envelopes still has its LFOs moving what they moved");
    }

    /** An envelope's release brings what it moves steadily down from where it was when the key was let go to rest,
        which is the bottom of its travel, or in tune for an oscillator's tune.
    */
    void testEnvelopeReleases()
    {
        std::cout << "Envelope releases" << std::endl;

        // The release takes its whole time, even from 0.
        {
            const EnvelopeSettings settings { 0.0f, 0.1f, 0.0f, 1.0f };
            EnvelopeRun run;
            run.start();
            run.advance (settings, 0.5);
            run.release();
            run.advance (settings, 0.5);
            const auto halfway = run.isReleasing() && isNear (run.getReleaseLeft(), 0.5, 1.0e-5) && juce::exactlyEqual (run.getLevel(), 0.0f);
            run.advance (settings, 0.6);
            check (halfway && run.isIdle(), "an envelope's release takes its whole time, even when the envelope is already at 0");
        }

        // Where a control goes as the release runs.
        {
            using namespace modulation;
            const Assignment byEnvelope { envelopeSource (0), 0.4f, false };
            const EnvelopeState held { 0.5f, false, 0.0f, 1.0f }, halfReleased { 0.25f, true, 0.5f, 0.5f };
            const auto cutoff = filterCutoffTarget (0);
            const auto tune = oscillatorTuneTarget (0);

            check (isNear (applyEnvelope (cutoff, byEnvelope, 0.3f, held), 0.5, 1.0e-5)
                     && isNear (applyEnvelope (cutoff, byEnvelope, 0.3f, halfReleased), 0.25, 1.0e-5),
                   "halfway through the release, a control has come halfway down from where it was at key up to the bottom");
            check (isNear (applyEnvelope (tune, byEnvelope, tuning::toPosition (7.0f), halfReleased),
                           tuning::toPosition (0.5f * (7.0f + 0.4f * 0.5f * 24.0f)), 1.0e-5),
                   "an oscillator's tune comes back into tune as the release runs, rather than an octave down");
        }

        // An envelope on the volume, which is set to 60% and raised to the top by the envelope, fades the note out
        // over the release from wherever it was, rather than coming back to 60% and stopping there.
        SynthProcessor synth;
        setPreset (synth, 0, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::volumeId, 0.6f);
        setEnvelope (synth, 0, { 0.0f, 0.0f, 1.0f, 1.0f });
        synth.setAssignment (modulation::volumeTarget, { modulation::envelopeSource (0), 0.4f, false });

        // Held for half a second, then let go, with a second's release. The note gets as loud as the volume's square.
        auto signal = play (synth, 69, 0.5, 1.8);
        const auto held = loudness (signal, 0.3, 0.45);
        // Halfway through, the volume has come down halfway, so it's a quarter as loud, give or take the volume's own
        // smoothing.
        check (isNear (loudness (signal, 0.97, 1.03) / held, 0.25, 0.12) && loudness (signal, 1.4, 1.45) < 0.03 * held
                 && loudness (signal, 1.48, 1.8) < 0.01 * held,
               "an envelope on the volume fades the note steadily to nothing over its release, and it stays silent");

        auto largestJump = 0.0f;

        for (auto i = (size_t) (1.2 * sampleRate); i < (size_t) (1.6 * sampleRate); ++i)
            largestJump = std::max (largestJump, std::abs (signal[i] - signal[i - 1]));

        check (largestJump < 0.01f, "...without stopping suddenly as the release ends");

        // An envelope that's fallen to nothing by the time the key's let go leaves the volume where it's set, and the
        // release still fades the note from there.
        setEnvelope (synth, 0, { 0.0f, 0.1f, 0.0f, 1.0f });
        signal = play (synth, 69, 0.5, 1.8);
        const auto atSetting = loudness (signal, 0.3, 0.45);
        check (atSetting > 0.01 && isNear (loudness (signal, 0.97, 1.03) / atSetting, 0.25, 0.12)
                 && loudness (signal, 1.4, 1.45) < 0.03 * atSetting && loudness (signal, 1.48, 1.8) < 0.01 * atSetting,
               "...even from an envelope that had fallen to nothing, fading from the volume as it's set");
        synth.setAssignment (modulation::volumeTarget, {});

        // An envelope on one oscillator's level fades that oscillator over its release. Another oscillator, with no
        // envelope on it, fades as a note normally does, rather than playing on until the release ends.
        setPreset (synth, 1, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::onId (1), 1.0f);
        setParameter (synth, SynthProcessor::tuneId (1), 12.0f);
        setEnvelope (synth, 0, { 0.0f, 0.0f, 1.0f, 1.0f });
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { modulation::envelopeSource (0), 0.5f, false });
        setParameter (synth, SynthProcessor::levelId (0), 0.5f);

        signal = play (synth, 69, 0.5, 1.8);
        const auto firstHeld = amplitudeAt (slice (signal, 0.3, 0.45), 440.0);
        const auto secondHeld = amplitudeAt (slice (signal, 0.3, 0.45), 880.0);
        check (isNear (amplitudeAt (slice (signal, 0.95, 1.05), 440.0) / firstHeld, 0.5, 0.05)
                 && amplitudeAt (slice (signal, 1.4, 1.48), 440.0) < 0.1 * firstHeld,
               "an envelope on an oscillator's level fades it steadily to nothing over its release");
        check (amplitudeAt (slice (signal, 0.8, 0.9), 880.0) < 0.01 * secondHeld,
               "...while another oscillator, with no envelope, fades out as a note normally does");

        synth.setAssignment (modulation::oscillatorLevelTarget (0), {});
    }

    void testVelocity()
    {
        std::cout << "Velocity" << std::endl;

        // MIDI velocities are whole numbers up to 127, so these are ones it can send exactly.
        const auto soft = 32.0f / 127.0f, medium = 64.0f / 127.0f;
        const auto volumeAt = [] (float velocity) { return Patch::volumeToGain (0.75f - 0.35f * (1.0f - velocity)); };

        SynthProcessor synth;
        setPreset (synth, 0, waveform::Preset::sine);
        check (synth.getVelocityAssignment (modulation::volumeTarget) == modulation::startingVelocityOnVolume,
               "velocity starts out moving the volume");

        const auto hard = loudness (play (synth, 69, 0.5, 0.8), 0.2, 0.4);
        const auto mediumLoudness = loudness (play (synth, 69, 0.5, 0.8, medium), 0.2, 0.4);
        const auto softLoudness = loudness (play (synth, 69, 0.5, 0.8, soft), 0.2, 0.4);
        check (isNear (mediumLoudness / hard, volumeAt (medium) / volumeAt (1.0f), 0.01)
                 && isNear (softLoudness / hard, volumeAt (soft) / volumeAt (1.0f), 0.01),
               "softer notes are quieter, the softest by up to 35% of the volume's travel");

        synth.setVelocityAssignment (modulation::volumeTarget, {});
        check (isNear (loudness (play (synth, 69, 0.5, 0.8, soft), 0.2, 0.4), hard, 0.001),
               "taking velocity off the volume makes every note as loud as the hardest");

        // Velocity on the low pass cutoff: softer notes are darker.
        setPreset (synth, 0, waveform::Preset::sawtooth);
        setNumHarmonics (synth, 9);
        setFilter (synth, SynthProcessor::filterIds (0), true, 4000.0f);
        synth.setVelocityAssignment (modulation::filterCutoffTarget (0), { modulation::velocitySource, 0.5f, false });
        const auto bright = slice (play (synth, 69, 0.5, 0.8), 0.2, 0.4);
        const auto dark = slice (play (synth, 69, 0.5, 0.8, soft), 0.2, 0.4);
        check (amplitudeAt (dark, 5 * 440.0) / amplitudeAt (dark, 440.0) < 0.2 * amplitudeAt (bright, 5 * 440.0) / amplitudeAt (bright, 440.0),
               "velocity on the low pass cutoff makes softer notes darker");
        synth.setVelocityAssignment (modulation::filterCutoffTarget (0), {});
        setFilter (synth, SynthProcessor::filterIds (0), false, 4000.0f);

        // Velocity and an LFO on the same control. A square LFO swings OSC 1's level, set at 50%, by 20% either way,
        // and velocity lowers it by up to 40% for the softest notes.
        setPreset (synth, 0, waveform::Preset::sine);
        setNumHarmonics (synth, 3);
        setParameter (synth, SynthProcessor::levelId (0), 0.5f);
        setLfo (synth, 0, 1.0f, false, waveform::Preset::square);
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 0.4f, false });
        synth.setVelocityAssignment (modulation::oscillatorLevelTarget (0), { modulation::velocitySource, 0.4f, false });

        auto signal = play (synth, 69, 0.95, 1.3);
        check (isNear (loudness (signal, 0.1, 0.4) / loudness (signal, 0.6, 0.9), 0.7 / 0.3, 0.02),
               "played hard, an LFO alone moves a control that velocity also moves");

        signal = play (synth, 69, 0.95, 1.3, medium);   // velocity takes off about 20%, about cancelling the LFO's rise
        const auto velocityOffset = 0.4f * (1.0f - medium);
        check (isNear (loudness (signal, 0.1, 0.4) / loudness (signal, 0.6, 0.9), (0.7 - velocityOffset) / (0.3 - velocityOffset), 0.03),
               "played softly, velocity moves it as well as the LFO");

        synth.setAssignment (modulation::oscillatorLevelTarget (0), {});
        synth.setVelocityAssignment (modulation::oscillatorLevelTarget (0), { modulation::velocitySource, 0.4f, true });
        check (! synth.getVelocityAssignment (modulation::oscillatorLevelTarget (0)).inverted,
               "velocity has no polarity: it always moves controls down for softer notes");

        // The window can show the newest note's velocity.
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, medium), 0);
        synth.processBlock (buffer, midi);
        check (isNear (synth.getSourceValue (modulation::velocitySource).value_or (0.0f), medium, 1.0e-4),
               "the window can show how hard the newest note was played");

        juce::MidiBuffer letGo;
        letGo.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        synth.processBlock (buffer, letGo);

        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer nothing;
            synth.processBlock (buffer, nothing);
        }

        // Saving and loading, including a patch with velocity taken off everything.
        juce::MemoryBlock state;
        synth.getStateInformation (state);
        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getVelocityAssignment (modulation::oscillatorLevelTarget (0)) == modulation::Assignment { modulation::velocitySource, 0.4f, false }
                 && ! restored.getVelocityAssignment (modulation::volumeTarget).isActive(),
               "what velocity moves is restored, even when it moves nothing it started out moving");

        juce::XmlElement older ("EZSynth");
        juce::AudioProcessor::copyXmlToBinary (older, state);
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getVelocityAssignment (modulation::volumeTarget) == modulation::startingVelocityOnVolume,
               "a save from before velocity could be assigned still has softer notes play quieter");
    }

    /** Lets go of a held note and plays on until it's faded, so the next test starts in silence. */
    void letGo (SynthProcessor& synth, int note)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        synth.processBlock (buffer, midi);

        for (int block = 0; block < (int) (1.0 * sampleRate / blockSize); ++block)
        {
            juce::MidiBuffer nothing;
            synth.processBlock (buffer, nothing);
        }
    }

    void testShowingMovedHarmonics()
    {
        std::cout << "Showing moved harmonics" << std::endl;

        SynthProcessor synth;
        setPreset (synth, 0, waveform::Preset::sine);
        auto shown = synth.getLiveWave (0);
        check (! shown.moved && shown.harmonics.size() == 3 && juce::exactlyEqual (shown.harmonics[0].amplitude, 1.0f)
                 && juce::exactlyEqual (shown.harmonics[1].amplitude, 0.0f),
               "with nothing moving them, an oscillator's harmonics are shown as set");

        // A looping sine LFO, a quarter of a period behind, raises the 2nd harmonic from nothing: halfway along its
        // slider at the LFO's peak, which comes half a period in.
        setLfo (synth, 0, 1.0f, false, waveform::Preset::sine);
        synth.setAssignment (modulation::harmonicTarget (0, 2), { 0, 1.0f, false, 0.25f });
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int block = 0; block < juce::roundToInt (0.5 * sampleRate / blockSize); ++block)
        {
            juce::MidiBuffer nothing;
            synth.processBlock (buffer, nothing);
        }

        const auto lfoValue = std::sin (juce::MathConstants<double>::twoPi * (synth.getLfoProgress (0).value_or (0.0) - 0.25));
        shown = synth.getLiveWave (0);
        check (shown.moved && shown.harmonics.size() == 3
                 && isNear (shown.harmonics[1].amplitude, modulation::amplitudeRange().convertFrom0to1 ((float) (0.5 * lfoValue)), 0.01)
                 && juce::exactlyEqual (shown.harmonics[0].amplitude, 1.0f),
               "an LFO moving a harmonic shows it where the LFO has it now, leaving the others as set");

        setParameter (synth, SynthProcessor::lfoOnId (0), 0.0f);
        check (! synth.getLiveWave (0).moved, "...and as set once the LFO is switched off");
        synth.setAssignment (modulation::harmonicTarget (0, 2), {});

        // An envelope raising the 3rd harmonic holds it at its sustain, 70%, while the note is held. What's shown is
        // what's heard.
        synth.setAssignment (modulation::harmonicTarget (0, 3), { modulation::envelopeSource (0), 1.0f, false });
        check (! synth.getLiveWave (0).moved, "an envelope moves nothing before a note is played");

        const auto signal = play (synth, 69, 2.0, 1.0);
        shown = synth.getLiveWave (0);
        const auto expected = modulation::amplitudeRange().convertFrom0to1 (0.7f);
        const auto held = slice (signal, 0.6, 1.0);

        check (shown.moved && isNear (shown.harmonics[2].amplitude, expected, 1.0e-3),
               "an envelope moving a harmonic shows it where the newest note's envelope has it");
        check (isNear (amplitudeAt (held, 3 * 440.0) / amplitudeAt (held, 440.0),
                       shown.harmonics[2].amplitude / shown.harmonics[0].amplitude, 0.02),
               "...which is how strong it's heard");
        letGo (synth, 69);
        synth.setAssignment (modulation::harmonicTarget (0, 3), {});

        // Velocity taking away harmonics for softer notes.
        synth.setVelocityAssignment (modulation::volumeTarget, {});
        synth.setVelocityAssignment (modulation::oscillatorHarmonicsTarget (0), { modulation::velocitySource, 1.0f, false });
        play (synth, 69, 2.0, 0.2, 0.5f);
        shown = synth.getLiveWave (0);
        check (shown.moved && shown.harmonics.size() == 1, "velocity moving the number of harmonics shows a soft note's fewer");
        letGo (synth, 69);
    }

    void testTuning()
    {
        std::cout << "Tuning oscillators" << std::endl;

        const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));

        check (juce::exactlyEqual (tuning::snap (6.93, 0.1), 7.0) && juce::exactlyEqual (tuning::snap (6.85, 0.1), 6.85)
                 && juce::exactlyEqual (tuning::snap (-0.03, 0.05), 0.0),
               "dragging the tune snaps to a whole semitone within the distance given");
        check (tuning::getIntervalName (7) == "perfect 5th" && tuning::getIntervalName (4) == "major 3rd"
                 && tuning::getIntervalName (0) == "unison" && tuning::getIntervalName (12) == "octave"
                 && tuning::getIntervalName (-7) == "perfect 5th below" && tuning::getIntervalName (-12) == "octave below",
               "whole semitones are named as the intervals they make, above the note or below it");
        check (tuning::toText (7.0f) == "+7 st" && tuning::toText (0.15f) == "+0.15 st" && tuning::toText (0.0f) == "0 st"
                 && tuning::toText (-7.0f) == minus + "7 st" && tuning::toText (-0.15f) == minus + "0.15 st",
               "tunings are written in semitones, up or down");
        check (std::all_of (tuning::markedIntervals.begin(), tuning::markedIntervals.end(),
                            [] (const tuning::Interval& interval)
                            {
                                const auto name = tuning::getIntervalName (interval.semitones);
                                const auto suffix = interval.number == 2 ? "nd" : interval.number == 3 ? "rd" : "th";
                                return (interval.number == 8 ? name == "octave" : name.endsWith (juce::String (interval.number) + suffix))
                                    && ! name.startsWith ("minor");
                            }),
               "the slider marks the major and perfect intervals from the 2nd to the octave");

        // Zooming in on the tune slider's scale.
        const auto isWindow = [] (float tune, float reach, float low, float high)
        {
            const auto window = tuning::getWindow (tune, reach);
            return juce::exactlyEqual (window.low, low) && juce::exactlyEqual (window.high, high);
        };

        check (isNear (tuning::zoomToReach (0.0f), 12.0, 1.0e-6) && isNear (tuning::zoomToReach (1.0f), 1.0, 1.0e-6)
                 && isNear (tuning::zoomToReach (0.5f), std::sqrt (12.0), 1.0e-4),
               "zooming in goes from an octave either side to a semitone either side, by the same factor each step");
        check (isWindow (0.0f, 1.0f, -1.0f, 1.0f) && isWindow (0.3f, 1.0f, -1.0f, 1.0f) && isWindow (7.0f, 1.0f, 6.0f, 8.0f)
                 && isWindow (11.8f, 2.0f, 8.0f, 12.0f) && isWindow (-12.0f, 3.0f, -12.0f, -6.0f) && isWindow (5.0f, 12.0f, -12.0f, 12.0f),
               "zoomed in, the scale shows the part around the whole semitone nearest the tune, within the octave either way");

        SynthProcessor synth;
        check (juce::exactlyEqual (synth.getTune (0), 0.0f) && juce::exactlyEqual (synth.getTune (2), 0.0f),
               "the oscillators start in tune");

        // A sine on the first oscillator, in tune, and one on the second, tuned up.
        setPreset (synth, 0, waveform::Preset::sine);
        setPreset (synth, 1, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::onId (1), 1.0f);

        const auto playTuned = [&synth] (float tune)
        {
            setParameter (synth, SynthProcessor::tuneId (1), tune);
            return slice (play (synth, 69, 0.5, 0.8), 0.1, 0.5);
        };

        const auto inTune = playTuned (0.0f);
        const auto bothAt440 = amplitudeAt (inTune, 440.0);

        for (const auto& [tune, name] : { std::pair (7.0f, "up a 5th"), std::pair (12.0f, "up an octave"),
                                          std::pair (0.5f, "up a quarter tone"), std::pair (-7.0f, "down a 5th"),
                                          std::pair (-12.0f, "down an octave"), std::pair (-0.5f, "down a quarter tone") })
        {
            const auto signal = playTuned (tune);
            const auto tuned = 440.0 * std::pow (2.0, tune / 12.0);
            check (isNear (amplitudeAt (signal, tuned) / amplitudeAt (signal, 440.0), 1.0, 0.03)
                     && isNear (amplitudeAt (signal, 440.0) / bothAt440, 0.5, 0.03),
                   "an oscillator tuned " + juce::String (name) + " plays that far from the note, alongside one in tune");
        }

        auto& tuneParameter = *synth.parameters.getParameter (SynthProcessor::tuneId (1));
        check (isNear (tuneParameter.convertFrom0to1 (tuneParameter.getValueForText (minus + "7 st")), -7.0, 1.0e-5)
                 && isNear (tuneParameter.convertFrom0to1 (tuneParameter.getValueForText ("-2.5")), -2.5, 1.0e-5)
                 && tuneParameter.getText (tuneParameter.convertTo0to1 (-7.0f), 20) == minus + "7 st",
               "a host can show the tune, and have it typed in, below the note as well as above");

        // The harmonics too high to play cleanly are worked out at the oscillator's own pitch: a C8 tuned up an
        // octave plays its 2nd harmonic, at 16.7 kHz, partly faded, and leaves out its 3rd, at 25.1 kHz.
        setParameter (synth, SynthProcessor::onId (0), 0.0f);
        setPreset (synth, 1, waveform::Preset::sawtooth);
        setParameter (synth, SynthProcessor::tuneId (1), 12.0f);
        const auto high = slice (play (synth, 108, 0.5, 0.8), 0.1, 0.5);
        const auto c9 = 2.0 * juce::MidiMessage::getMidiNoteInHertz (108);
        check (amplitudeAt (high, c9) > 0.05 && amplitudeAt (high, sampleRate - 3.0 * c9) < 1.0e-3 * amplitudeAt (high, c9),
               "an oscillator tuned up leaves out the harmonics too high to play at its pitch");

        // An envelope can sweep the tune: here, straight up a 5th as each key is pressed.
        setPreset (synth, 1, waveform::Preset::sine);
        setParameter (synth, SynthProcessor::onId (0), 1.0f);
        setParameter (synth, SynthProcessor::tuneId (1), 0.0f);
        setEnvelope (synth, 0, { 0.0f, 0.0f, 1.0f, 0.1f });
        synth.setAssignment (modulation::oscillatorTuneTarget (1), { modulation::envelopeSource (0), 7.0f / 24.0f, false });
        auto signal = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.5);
        check (isNear (amplitudeAt (signal, 440.0 * std::pow (2.0, 7.0 / 12.0)) / amplitudeAt (signal, 440.0), 1.0, 0.03),
               "an envelope on an oscillator's tune moves its pitch");
        synth.setAssignment (modulation::oscillatorTuneTarget (1), {});

        // Retuning while a note plays glides there without a click. A sine's steepest step between samples grows with
        // its pitch, by half as much again for a 5th up.
        setParameter (synth, SynthProcessor::onId (0), 0.0f);
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> output;

        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);

            if (block == 20)
                setParameter (synth, SynthProcessor::tuneId (1), 7.0f);

            synth.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }

        const auto largestJump = [&output] (size_t from, size_t to)
        {
            auto largest = 0.0f;

            for (auto i = from; i < to; ++i)
                largest = std::max (largest, std::abs (output[i] - output[i - 1]));

            return largest;
        };

        const auto switchPoint = (size_t) (20 * blockSize);
        check (largestJump (switchPoint, output.size()) < 1.55f * largestJump (2000, switchPoint),
               "retuning a playing note glides to the new pitch without a click");
        letGo (synth, 69);

        // The window's tune slider shows whichever oscillator is chosen, and moving between them leaves each one's
        // tune as it was.
        {
            SynthProcessor shown;
            std::unique_ptr<juce::AudioProcessorEditor> editor (shown.createEditorAndMakeActive());
            auto& synthEditor = dynamic_cast<SynthEditor&> (*editor);
            setParameter (shown, SynthProcessor::tuneId (0), 7.0f);
            setParameter (shown, SynthProcessor::tuneId (2), 3.4f);

            for (const auto source : { 2, 0, SynthProcessor::numOscillators, 1, 2, 0 })
            {
                shown.editedSource = source;
                synthEditor.refresh();
            }

            check (isNear (shown.getTune (0), 7.0, 1.0e-6) && juce::exactlyEqual (shown.getTune (1), 0.0f)
                     && isNear (shown.getTune (2), 3.4, 1.0e-6),
                   "choosing one oscillator after another in the window leaves each one's tune as it was");
            shown.editorBeingDeleted (editor.get());
        }

        // Zooming in on the tune slider never changes the tune, even one beyond the part of the scale shown.
        {
            SynthProcessor zoomed;
            TuneSlider tuneSlider;
            tuneSlider.setBounds (0, 0, 340, TuneSlider::preferredHeight);
            tuneSlider.setOscillator (*zoomed.parameters.getParameter (SynthProcessor::tuneId (0)), 0);
            const auto shows = [&tuneSlider] (float from, float to)
            {
                return juce::exactlyEqual (tuneSlider.getWindow().low, from) && juce::exactlyEqual (tuneSlider.getWindow().high, to);
            };

            check (shows (-12.0f, 12.0f), "the tune slider starts zoomed right out, an octave either side");

            setParameter (zoomed, SynthProcessor::tuneId (0), 7.0f);
            tuneSlider.setZoom (1.0f);
            check (shows (6.0f, 8.0f) && isNear (zoomed.getTune (0), 7.0, 1.0e-6),
                   "zoomed right in, it shows the semitone either side of the tune, and leaves the tune as it was");

            setParameter (zoomed, SynthProcessor::tuneId (0), -0.2f);
            check (shows (-1.0f, 1.0f) && isNear (zoomed.getTune (0), -0.2, 1.0e-5),
                   "a tune set from elsewhere, beyond the part shown, moves the scale to it without changing it");

            tuneSlider.setZoom (0.0f);
            check (shows (-12.0f, 12.0f) && isNear (zoomed.getTune (0), -0.2, 1.0e-5), "zooming out shows the whole range again");
        }

        // Saving and loading.
        juce::MemoryBlock state;
        synth.getStateInformation (state);
        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (isNear (restored.getTune (1), 7.0, 1.0e-5) && juce::exactlyEqual (restored.getTune (0), 0.0f),
               "each oscillator's tune is restored");

        // Loading a save from before oscillators could be tuned puts them in tune.
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());

        for (int i = 0; i < SynthProcessor::numOscillators; ++i)
            if (auto* tune = xml->getChildByAttribute ("id", SynthProcessor::tuneId (i)))
                xml->removeChildElement (tune, true);

        juce::AudioProcessor::copyXmlToBinary (*xml, state);
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (juce::exactlyEqual (restored.getTune (1), 0.0f), "a save from before oscillators could be tuned loads them in tune");
    }

    /** Stands in for the standalone app's MIDI devices: those plugged in, and those switched on. */
    struct StandInMidiDevices
    {
        juce::Array<juce::MidiDeviceInfo> pluggedIn;
        juce::StringArray on;

        AutoMidiInputs::Devices get()
        {
            AutoMidiInputs::Devices devices;
            devices.list = [this] { return pluggedIn; };
            devices.isOn = [this] (const juce::String& identifier) { return on.contains (identifier); };
            devices.setOn = [this] (const juce::String& identifier, bool switchOn)
            {
                if (switchOn)
                    on.addIfNotAlreadyThere (identifier);
                else
                    on.removeString (identifier);
            };

            return devices;
        }
    };

    /** The standalone app's MIDI inputs, for the window to show, without any devices behind them. */
    struct ShownMidiInputs final : public MidiInputs
    {
        std::vector<Input> inputs;

        std::vector<Input> getInputs() const override               { return inputs; }
        void setOn (const juce::String&, bool) override             {}
        void showSettings() override                                {}
    };

    void testMidiInputs()
    {
        std::cout << "MIDI devices in the standalone app" << std::endl;

        StandInMidiDevices devices;
        const juce::MidiDeviceInfo keys ("KeyStep 37", "keystep"), pads ("MPD218", "pads");
        devices.pluggedIn = { keys };
        juce::PropertySet settings;

        {
            AutoMidiInputs inputs (devices.get(), settings);
            check (devices.on.contains ("keystep"), "a MIDI device plugged in when the app starts is played from");

            devices.pluggedIn.add (pads);
            inputs.update();
            check (devices.on.contains ("pads"), "...as is one plugged in while it runs, as soon as it's noticed");
            check (inputs.getInputs() == std::vector<MidiInputs::Input> { { "keystep", "KeyStep 37", true }, { "pads", "MPD218", true } },
                   "the devices plugged in are listed, with whether each is played from");

            inputs.setOn ("pads", false);
            devices.pluggedIn.removeFirstMatchingValue (pads);
            inputs.update();
            devices.pluggedIn.add (pads);
            inputs.update();
            check (! devices.on.contains ("pads") && devices.on.contains ("keystep"),
                   "a device switched off stays off when it's unplugged and plugged in again");

            devices.pluggedIn.removeFirstMatchingValue (keys);
            inputs.update();
            check (! devices.on.contains ("keystep"), "a device that's unplugged is let go");
            devices.pluggedIn.insert (0, keys);
            inputs.update();
            check (devices.on.contains ("keystep"), "...and played from again when it's plugged back in");

            // Switched off in the audio and MIDI settings.
            devices.on.removeString ("keystep");
            inputs.devicesSwitched();
        }

        // Starting again, as the app does each time it's opened.
        devices.on.clear();

        {
            AutoMidiInputs inputs (devices.get(), settings);
            check (devices.on.isEmpty(), "devices switched off, here or in the audio and MIDI settings, stay off when the app starts again");
            inputs.setOn ("keystep", true);
        }

        devices.on.clear();

        {
            AutoMidiInputs inputs (devices.get(), settings);
            check (devices.on == juce::StringArray { "keystep" }, "...until they're switched on again");
        }

        check (MidiPanel::describe ({}) == "No MIDI devices plugged in"
                 && MidiPanel::describe ({ { "keystep", "KeyStep 37", true }, { "pads", "MPD218", false } }) == "KeyStep 37"
                 && MidiPanel::describe ({ { "keystep", "KeyStep 37", true }, { "pads", "MPD218", true } }) == "2 devices"
                 && MidiPanel::describe ({ { "pads", "MPD218", false } }).startsWith ("Off"),
               "the window names the device played from, or says how many, or that none is");

        // Through the Mac's own MIDI system: a virtual MIDI keyboard appears, is played from, and its notes arrive.
        {
            struct Received final : public juce::MidiInputCallback
            {
                void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
                {
                    if (message.isNoteOn())
                        note = message.getNoteNumber();
                }

                std::atomic<int> note { -1 };
            };

            juce::AudioDeviceManager deviceManager;
            Received received;
            deviceManager.addMidiInputDeviceCallback ({}, &received);
            juce::PropertySet realSettings;
            AutoMidiInputs inputs (AutoMidiInputs::getDevices (deviceManager, {}), realSettings);

            const juce::String name ("EZ Synth Test Keyboard");
            // The Mac's MIDI system tells a program about devices coming and going through its main run loop, which
            // runs all the time in the app, but here only when it's asked to.
            const auto letMidiSystemCatchUp = [] { CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.5, false); };

            auto virtualKeyboard = juce::MidiOutput::createNewDevice (name);
            letMidiSystemCatchUp();
            inputs.update();

            const auto listed = [&inputs, &name]
            {
                const auto all = inputs.getInputs();
                const auto found = std::find_if (all.begin(), all.end(), [&name] (const MidiInputs::Input& input) { return input.name == name; });
                return found != all.end() ? std::optional (*found) : std::nullopt;
            };

            check (virtualKeyboard != nullptr && listed().has_value() && listed()->on,
                   "a MIDI keyboard plugged in is noticed and played from");

            if (virtualKeyboard != nullptr)
            {
                virtualKeyboard->sendMessageNow (juce::MidiMessage::noteOn (1, 64, 0.8f));

                for (int wait = 0; wait < 200 && received.note.load() < 0; ++wait)
                    juce::Thread::sleep (10);

                virtualKeyboard->sendMessageNow (juce::MidiMessage::noteOff (1, 64));
            }

            check (received.note.load() == 64, "a note played on it arrives");

            const auto wasListed = listed().has_value();
            virtualKeyboard = nullptr;
            letMidiSystemCatchUp();
            inputs.update();
            check (wasListed && ! listed().has_value(), "a keyboard unplugged leaves the list");
            deviceManager.removeMidiInputDeviceCallback ({}, &received);
        }

        // The processor counts MIDI arriving from a device or the host, so the window can show it, but not the
        // on-screen keys' notes, nor a device's clock.
        {
            SynthProcessor synth;
            synth.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);

            const auto playBlock = [&synth, &buffer] (std::initializer_list<juce::MidiMessage> messages)
            {
                juce::MidiBuffer midi;

                for (const auto& message : messages)
                    midi.addEvent (message, 0);

                synth.processBlock (buffer, midi);
                return synth.getMidiReceived();
            };

            const auto before = synth.getMidiReceived();
            const auto afterNote = playBlock ({ juce::MidiMessage::noteOn (1, 60, 0.5f) });
            const auto afterClock = playBlock ({ juce::MidiMessage::midiClock() });
            synth.keyboardState.noteOn (1, 64, 1.0f);
            const auto afterOnScreen = playBlock ({});
            const auto afterWheel = playBlock ({ juce::MidiMessage::pitchWheel (1, 9000) });

            check (afterNote == before + 1 && afterClock == afterNote && afterOnScreen == afterNote && afterWheel == afterNote + 1,
                   "MIDI played from a device counts as arriving, but not the on-screen keys or a device's clock");
            playBlock ({ juce::MidiMessage::allNotesOff (1) });
        }

        // A MIDI keyboard's sustain pedal holds notes after their keys are let go, until the pedal is.
        {
            SynthProcessor synth;
            synth.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);
            std::vector<float> output;
            const auto at = [] (double seconds) { return (int) (seconds * sampleRate); };

            for (int start = 0; start < at (1.2); start += blockSize)
            {
                juce::MidiBuffer midi;
                const auto add = [&midi, start] (int sample, const juce::MidiMessage& message)
                {
                    if (sample >= start && sample < start + blockSize)
                        midi.addEvent (message, sample - start);
                };

                add (0, juce::MidiMessage::noteOn (1, 69, 1.0f));
                add (at (0.1), juce::MidiMessage::controllerEvent (1, 64, 127));
                add (at (0.2), juce::MidiMessage::noteOff (1, 69));
                add (at (0.6), juce::MidiMessage::controllerEvent (1, 64, 0));

                synth.processBlock (buffer, midi);
                output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }

            check (isNear (loudness (output, 0.4, 0.55), loudness (output, 0.05, 0.1), 0.02) && loudness (output, 1.0, 1.2) < 1.0e-4,
                   "the sustain pedal holds a note after its key is let go, until the pedal is");
        }

        // The window shows the MIDI devices only in the standalone app.
        {
            SynthProcessor synth;
            const auto hasMidiPanel = [] (juce::Component& window)
            {
                return std::any_of (window.getChildren().begin(), window.getChildren().end(),
                                    [] (juce::Component* child) { return dynamic_cast<MidiPanel*> (child) != nullptr; });
            };

            std::unique_ptr<juce::AudioProcessorEditor> pluginWindow (synth.createEditorAndMakeActive());
            check (! hasMidiPanel (*pluginWindow), "a plugin's window, which takes MIDI from its host, has no MIDI devices");
            synth.editorBeingDeleted (pluginWindow.get());
            pluginWindow = nullptr;

            ShownMidiInputs shown;
            MidiInputs::set (&shown);
            std::unique_ptr<juce::AudioProcessorEditor> appWindow (synth.createEditorAndMakeActive());
            check (hasMidiPanel (*appWindow), "the standalone app's window shows its MIDI devices");
            synth.editorBeingDeleted (appWindow.get());
            appWindow = nullptr;
            MidiInputs::set (nullptr);
        }
    }

    void testPresets()
    {
        std::cout << "Presets" << std::endl;

        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("EZ Synth tests " + juce::String (juce::Time::currentTimeMillis()));
        folder.createDirectory();

        SynthProcessor synth;
        check (synth.getPresetName() == "Init" && ! synth.isPresetEdited(), "the synth starts on Init, unedited");

        setParameter (synth, SynthProcessor::levelId (0), 0.3f);
        check (synth.isPresetEdited(), "turning a dial edits the preset");

        waveform::Shape drawn {};
        drawn.fill (0.25f);
        drawn[100] = -0.75f;
        synth.setShape (1, drawn);
        setEnvelope (synth, 2, { 1.5f, 0.2f, 0.4f, 3.0f });
        synth.setAssignment (modulation::filterCutoffTarget (1), { modulation::envelopeSource (2), 0.6f, true });
        synth.setVelocityAssignment (modulation::volumeTarget, {});
        synth.setVelocityAssignment (modulation::harmonicTarget (0, 2), { modulation::velocitySource, 0.25f, false });

        // Saving adds the preset extension, names the preset after its file, and counts as unedited.
        check (synth.savePreset (folder.getChildFile ("Glassy Pad")), "a preset can be saved");
        const auto saved = folder.getChildFile (juce::String ("Glassy Pad") + SynthProcessor::presetExtension);
        check (saved.existsAsFile() && synth.getPresetName() == "Glassy Pad" && ! synth.isPresetEdited(),
               "a saved preset gets the preset extension and its file's name, and isn't edited");
        check (juce::XmlDocument::parse (saved) != nullptr, "a preset is a readable XML file");

        // Loading it into a fresh synth brings back the whole sound.
        SynthProcessor loaded;
        check (loaded.loadPreset (saved), "a preset can be loaded");
        check (loaded.getShape (1) == drawn && isNear (loaded.getEnvelopeSettings (2).release, 3.0, 1.0e-4)
                 && isNear (loaded.parameters.getRawParameterValue (SynthProcessor::levelId (0))->load(), 0.3, 1.0e-4),
               "loading a preset brings back its waves and settings");
        check (loaded.getAssignment (modulation::filterCutoffTarget (1)) == modulation::Assignment { modulation::envelopeSource (2), 0.6f, true }
                 && ! loaded.getVelocityAssignment (modulation::volumeTarget).isActive()
                 && loaded.getVelocityAssignment (modulation::harmonicTarget (0, 2)).isActive(),
               "...and what its LFOs, envelopes and velocity move");
        check (loaded.getPresetName() == "Glassy Pad" && loaded.getPresetFile() == saved && ! loaded.isPresetEdited(),
               "...under its name, unedited");

        // A host's project remembers the preset, and whether it was edited.
        loaded.setShape (0, drawn);
        juce::MemoryBlock state;
        loaded.getStateInformation (state);
        SynthProcessor reopened;
        reopened.setStateInformation (state.getData(), (int) state.getSize());
        check (reopened.getPresetName() == "Glassy Pad" && reopened.isPresetEdited(),
               "a host's project remembers the preset's name, and that it had been edited");

        // Init goes back to the sound the synth starts with.
        loaded.loadInitPreset();
        SynthProcessor fresh;
        check (loaded.getPresetName() == "Init" && ! loaded.isPresetEdited() && loaded.getShape (1) == fresh.getShape (1)
                 && loaded.getVelocityAssignment (modulation::volumeTarget) == modulation::startingVelocityOnVolume
                 && ! loaded.getAssignment (modulation::filterCutoffTarget (1)).isActive(),
               "Init goes back to the sound the synth starts with");

        // Presets are listed by name, with numbers counted as numbers.
        for (const auto* name : { "Bass 10", "Bass 2", "Arp" })
            synth.savePreset (folder.getChildFile (name));

        juce::StringArray names;

        for (const auto& file : SynthProcessor::findPresets (folder))
            names.add (file.getFileNameWithoutExtension());

        check (names.joinIntoString (", ") == "Arp, Bass 2, Bass 10, Glassy Pad", "presets are listed in order of name");

        const auto notPreset = folder.getChildFile ("notes.ezsynth");
        notPreset.replaceWithText ("not a preset");
        check (! loaded.loadPreset (notPreset) && loaded.getPresetName() == "Init", "a file that isn't a preset is refused, leaving the sound alone");

        folder.deleteRecursively();
    }

    juce::Button* findButton (juce::Component& parent, const juce::String& name)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* button = dynamic_cast<juce::Button*> (child); button != nullptr && button->getName() == name)
                return button;

            if (auto* found = findButton (*child, name))
                return found;
        }

        return nullptr;
    }

    void testFactoryPresets()
    {
        std::cout << "Factory presets" << std::endl;

        const auto names = SynthProcessor::getFactoryPresetNames();
        check (names.size() >= 6, "EZ Synth comes with factory presets");

        auto sorted = names;
        sorted.sortNatural();
        check (names == sorted, "...in order of name");

        for (const auto& name : names)
        {
            SynthProcessor synth;
            check (synth.loadFactoryPreset (name) && synth.getPresetName() == name && synth.isFactoryPreset()
                     && ! synth.isPresetEdited() && synth.getPresetFile() == juce::File(),
                   name + " loads, under its name, unedited");

            const auto signal = play (synth, 60, 2.5, 3.0);
            check (std::all_of (signal.begin(), signal.end(), [] (float sample) { return std::isfinite (sample); })
                     && peakOf (signal) > 0.05f && peakOf (signal) < 1.0f,
                   "...and plays a note that's clearly heard, without clipping");
        }

        SynthProcessor synth;
        check (! synth.loadFactoryPreset ("No such preset") && synth.getPresetName() == "Init",
               "a factory preset that doesn't exist isn't loaded, leaving the sound alone");

        // A host's project remembers that the sound came from a factory preset.
        synth.loadFactoryPreset (names[0]);
        juce::MemoryBlock state;
        synth.getStateInformation (state);
        SynthProcessor reopened;
        reopened.setStateInformation (state.getData(), (int) state.getSize());
        check (reopened.getPresetName() == names[0] && reopened.isFactoryPreset() && ! reopened.isPresetEdited(),
               "a host's project remembers the factory preset");

        // Saving a factory preset saves a copy of it, which is the user's own.
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("EZ Synth tests " + juce::String (juce::Time::currentTimeMillis()));
        check (synth.savePreset (folder.getChildFile (names[0])) && ! synth.isFactoryPreset()
                 && synth.getPresetFile() == folder.getChildFile (names[0] + SynthProcessor::presetExtension),
               "a factory preset saved is a copy of it, among the user's own");

        SynthProcessor copy;
        check (copy.loadPreset (synth.getPresetFile()) && ! copy.isFactoryPreset() && copy.getPresetName() == names[0],
               "...which loads as the user's own");
        folder.deleteRecursively();

        // The arrows in the window step through the factory presets, starting from Init.
        SynthProcessor stepped;
        std::unique_ptr<juce::AudioProcessorEditor> editor (stepped.createEditorAndMakeActive());
        auto* previous = findButton (*editor, "Previous preset");
        auto* next = findButton (*editor, "Next preset");

        if (previous != nullptr && next != nullptr && names.size() >= 2)
        {
            next->onClick();
            const auto first = stepped.getPresetName();
            next->onClick();
            const auto second = stepped.getPresetName();
            previous->onClick();

            check (first == names[0] && second == names[1] && stepped.getPresetName() == names[0] && stepped.isFactoryPreset(),
                   "the arrows step through the factory presets, from Init");
        }
        else
        {
            check (false, "the window has arrows to step through the presets");
        }
    }

    void testSavingAndLoading()
    {
        std::cout << "Saving and loading" << std::endl;

        SynthProcessor original;
        waveform::Shape drawn {};

        for (int i = 0; i < waveform::numPoints; ++i)
            drawn[(size_t) i] = i < waveform::numPoints / 8 ? 1.0f : -0.25f;

        original.setShape (1, drawn);
        original.setHarmonicAmplitude (1, 1, waveform::maxAmplitude);
        drawn = original.getShape (1);
        setPreset (original, 2, waveform::Preset::triangle);
        setNumHarmonics (original, 7, 1);
        setParameter (original, SynthProcessor::onId (1), 1.0f);
        setParameter (original, SynthProcessor::levelId (1), 0.4f);
        setFilter (original, SynthProcessor::filterIds (0), true, 3000.0f, 0.5f);

        juce::MemoryBlock state;
        original.getStateInformation (state);

        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        check (restored.getShape (1) == drawn, "a drawn wave is restored to its oscillator, even beyond full scale");
        check (! restored.getPreset (1).has_value(), "...and isn't taken for a preset");
        check (restored.getPreset (2) == waveform::Preset::triangle && restored.getPreset (0) == waveform::Preset::sawtooth,
               "the other oscillators' presets are restored as those presets");
        check (restored.getNumHarmonics (1) == 7 && restored.getNumHarmonics (0) == 3, "each oscillator's number of harmonics is restored");
        check (restored.isOn (0) && restored.isOn (1) && ! restored.isOn (2), "which oscillators are on is restored");
        check (isNear (restored.parameters.getRawParameterValue (SynthProcessor::levelId (1))->load(), 0.4, 1.0e-6),
               "oscillator levels are restored");
        check (restored.parameters.getRawParameterValue (SynthProcessor::filterIds (0).on)->load() > 0.5f
                 && isNear (restored.parameters.getRawParameterValue (SynthProcessor::filterIds (0).cutoff)->load(), 3000.0, 1.0e-4)
                 && isNear (restored.parameters.getRawParameterValue (SynthProcessor::filterIds (0).resonance)->load(), 0.5, 1.0e-4),
               "the filters' settings are restored");

        restored.setStateInformation ("not a saved state", 17);
        check (restored.getPreset (2) == waveform::Preset::triangle, "a corrupt state is ignored");

        // Before there were three oscillators, a save had one wave, with no oscillator number.
        juce::XmlElement older ("EZSynth");
        auto* wave = older.createNewChildElement ("WAVE");
        const auto square = waveform::makePreset (waveform::Preset::square);
        wave->setAttribute ("points", juce::MemoryBlock (square.data(), sizeof (square)).toBase64Encoding());
        wave->setAttribute ("preset", "Square");
        juce::AudioProcessor::copyXmlToBinary (older, state);

        SynthProcessor upgraded;
        upgraded.setStateInformation (state.getData(), (int) state.getSize());
        check (upgraded.getPreset (0) == waveform::Preset::square && upgraded.getShape (0) == square,
               "a save from before there were three oscillators loads into the first");
    }

    void saveSnapshots (const juce::File& folder)
    {
        std::cout << "Saving pictures of the window to " << folder.getFullPathName().toStdString() << std::endl;
        folder.createDirectory();

        SynthProcessor synth;
        std::unique_ptr<juce::AudioProcessorEditor> editor (synth.createEditorAndMakeActive());
        auto& synthEditor = dynamic_cast<SynthEditor&> (*editor);

        const auto save = [&] (const juce::String& name)
        {
            synthEditor.refresh();
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            const auto file = folder.getChildFile (name + ".png");
            file.deleteFile();

            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
        };

        // Holding a key sets the note shown, as playing it would.
        const auto hold = [&] (int note)
        {
            synth.keyboardState.allNotesOff (1);
            synth.keyboardState.noteOn (1, note, 1.0f);
            synth.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            synth.processBlock (buffer, midi);
        };

        save ("1-sawtooth");

        // A preset with a name, edited since it was saved, for the pictures that follow.
        const auto presetFolder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EZ Synth snapshots");
        synth.savePreset (presetFolder.getChildFile ("Glassy Pad"));
        presetFolder.deleteRecursively();

        hold (69);
        setPreset (synth, 0, waveform::Preset::square);
        setNumHarmonics (synth, 9);
        save ("2-square-9");

        setPreset (synth, 0, waveform::Preset::sine);
        setNumHarmonics (synth, 3);
        save ("3-sine");

        setPreset (synth, 0, waveform::Preset::square);
        save ("3a-square-3");

        setPreset (synth, 0, waveform::Preset::sine);
        synth.setHarmonicAmplitude (0, 2, 0.25f);
        synth.setHarmonicAmplitude (0, 3, 0.6f);
        save ("3b-sine-with-raised-harmonics");

        // Something a person might draw: a lopsided bump and a dip.
        waveform::Shape drawn {};

        for (int i = 0; i < waveform::numPoints; ++i)
        {
            const auto t = (double) i / waveform::numPoints;
            drawn[(size_t) i] = (float) (t < 0.3 ? std::sin (pi * t / 0.3) * 0.9
                                                 : (t < 0.45 ? 0.0 : -0.6 * std::sin (pi * (t - 0.45) / 0.55) + 0.15 * std::sin (twoPi * 6.0 * t)));
        }

        hold (48);
        synth.setShape (0, drawn);
        setNumHarmonics (synth, 5);
        save ("4-drawn-5");

        // All three on: a drawn wave, a square at 60% and a triangle at 35%, with the square being edited.
        setPreset (synth, 2, waveform::Preset::triangle);
        setParameter (synth, SynthProcessor::onId (1), 1.0f);
        setParameter (synth, SynthProcessor::onId (2), 1.0f);
        setParameter (synth, SynthProcessor::levelId (1), 0.6f);
        setParameter (synth, SynthProcessor::levelId (2), 0.35f);
        synth.editedSource = 1;
        setFilter (synth, SynthProcessor::filterIds (0), true, 1800.0f, 0.4f);
        setFilter (synth, SynthProcessor::filterIds (1), true, 90.0f);
        save ("6-three-oscillators");

        setParameter (synth, SynthProcessor::onId (2), 0.0f);
        synth.editedSource = 2;
        save ("7-editing-an-oscillator-that-is-off");

        // LFO 1 swinging OSC 1's level and, a quarter of a period behind, the low pass cutoff, and LFO 2 running once
        // on the 3rd harmonic.
        setParameter (synth, SynthProcessor::onId (2), 1.0f);
        synth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 0.6f, false });
        synth.setAssignment (modulation::filterCutoffTarget (0), { 0, 0.4f, true, 0.25f });
        synth.setAssignment (modulation::harmonicTarget (0, 3), { 1, 0.5f, false });
        synth.setAssignment (modulation::harmonicTarget (0, 2), { 0, 0.8f, false, 0.5f });
        setParameter (synth, SynthProcessor::tuneId (0), 7.0f);
        setParameter (synth, SynthProcessor::tuneId (2), 3.4f);
        synth.setAssignment (modulation::volumeTarget, { modulation::envelopeSource (0), 0.3f, false });
        synth.setAssignment (modulation::filterResonanceTarget (0), { modulation::envelopeSource (1), 0.6f, false });
        setParameter (synth, SynthProcessor::lfoModeId (1), 1.0f);
        synth.editedSource = SynthProcessor::numOscillators;

        // Plays on a while, so the LFOs are partway through their periods.
        for (int block = 0; block < 30; ++block)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            synth.processBlock (buffer, midi);
        }

        synthEditor.showModulation();
        save ("9-lfo");

        synth.editedSource = 0;
        synthEditor.refresh();
        synthEditor.showModulation();
        save ("10-modulated-controls");

        // The editor that opens on a right-click, on its own: for an LFO with a phase, and for an envelope, which has
        // none.
        const auto saveModulationEditor = [&folder, &synth] (int target, const juce::String& name)
        {
            SynthLookAndFeel lookAndFeel;
            ModulationEditor modulationEditor (synth, target);
            modulationEditor.setLookAndFeel (&lookAndFeel);

            juce::Image image (juce::Image::ARGB, modulationEditor.getWidth() * 2, modulationEditor.getHeight() * 2, true);
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (2.0f));
            g.fillAll (colours::panel.brighter (0.04f));
            modulationEditor.paintEntireComponent (g, false);

            const auto file = folder.getChildFile (name + ".png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            modulationEditor.setLookAndFeel (nullptr);
        };

        saveModulationEditor (modulation::filterCutoffTarget (0), "11-modulation-editor");
        saveModulationEditor (modulation::volumeTarget, "11a-modulation-editor-envelope");

        // Waves drawn in steps: the Step Filter preset's LFO, in its 8 steps, and an oscillator drawn in 12.
        {
            SynthProcessor stepped;
            stepped.loadFactoryPreset ("Step Filter");
            stepped.editedSource = SynthProcessor::numOscillators + 1;
            std::unique_ptr<juce::AudioProcessorEditor> steppedEditor (stepped.createEditorAndMakeActive());
            auto& steppedWindow = dynamic_cast<SynthEditor&> (*steppedEditor);

            const auto saveWindow = [&folder, &steppedEditor, &steppedWindow] (const juce::String& name)
            {
                steppedWindow.refresh();
                const auto file = folder.getChildFile (name + ".png");
                file.deleteFile();
                juce::FileOutputStream stream (file);
                juce::PNGImageFormat().writeImageToStream (steppedEditor->createComponentSnapshot (steppedEditor->getLocalBounds(), true, 2.0f), stream);
            };

            saveWindow ("14-lfo-in-steps");

            waveform::Shape staircase {};
            constexpr std::array levels { -0.9f, -0.6f, -0.3f, 0.0f, 0.3f, 0.6f, 0.9f, 0.6f, 0.3f, 0.0f, -0.3f, -0.6f };

            for (int step = 0; step < (int) levels.size(); ++step)
                waveform::setSteps (staircase, (int) levels.size(), step, levels[(size_t) step], step, levels[(size_t) step]);

            stepped.setShape (0, staircase);
            setNumHarmonics (stepped, 9);
            stepped.setDrawing (0, { true, (int) levels.size() });
            stepped.editedSource = 0;
            saveWindow ("14a-oscillator-in-steps");

            stepped.editorBeingDeleted (steppedEditor.get());
        }

        // A random LFO, drawn smoothly and then in 12 steps, a few cycles in.
        {
            SynthProcessor randomSynth;
            setParameter (randomSynth, SynthProcessor::lfoRandomId (0), 1.0f);
            randomSynth.setAssignment (modulation::oscillatorLevelTarget (0), { 0, 0.6f, false });
            randomSynth.editedSource = SynthProcessor::numOscillators;
            randomSynth.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);

            for (int block = 0; block < 250; ++block)
            {
                juce::MidiBuffer nothing;
                randomSynth.processBlock (buffer, nothing);
            }

            std::unique_ptr<juce::AudioProcessorEditor> randomEditor (randomSynth.createEditorAndMakeActive());
            auto& randomWindow = dynamic_cast<SynthEditor&> (*randomEditor);

            const auto saveWindow = [&folder, &randomEditor, &randomWindow] (const juce::String& name)
            {
                randomWindow.refresh();
                const auto file = folder.getChildFile (name + ".png");
                file.deleteFile();
                juce::FileOutputStream stream (file);
                juce::PNGImageFormat().writeImageToStream (randomEditor->createComponentSnapshot (randomEditor->getLocalBounds(), true, 2.0f), stream);
            };

            saveWindow ("15-random-lfo");
            randomSynth.setDrawing (SynthProcessor::numOscillators, { true, 12 });
            saveWindow ("15a-random-lfo-in-steps");
            randomSynth.editorBeingDeleted (randomEditor.get());
        }

        // The tune slider on its own: zoomed right out, a 5th below the note, then zoomed in on a slight detune, with
        // an LFO swinging it, and partway in on a 5th above.
        {
            SynthProcessor tuned;
            SynthLookAndFeel lookAndFeel;
            TuneSlider tuneSlider;
            tuneSlider.setLookAndFeel (&lookAndFeel);
            tuneSlider.setBounds (0, 0, 348, TuneSlider::preferredHeight);
            tuneSlider.setOscillator (*tuned.parameters.getParameter (SynthProcessor::tuneId (0)), 0);

            const auto saveTuneSlider = [&folder, &tuneSlider] (const juce::String& name)
            {
                juce::Image image (juce::Image::ARGB, tuneSlider.getWidth() * 2 + 64, tuneSlider.getHeight() * 2 + 48, true);
                juce::Graphics g (image);
                g.fillAll (colours::panel);
                g.addTransform (juce::AffineTransform::scale (2.0f).translated (32.0f, 24.0f));
                tuneSlider.paintEntireComponent (g, false);

                const auto file = folder.getChildFile (name + ".png");
                file.deleteFile();
                juce::FileOutputStream stream (file);
                juce::PNGImageFormat().writeImageToStream (image, stream);
            };

            setParameter (tuned, SynthProcessor::tuneId (0), -7.0f);
            saveTuneSlider ("13-tune-zoomed-out");

            setParameter (tuned, SynthProcessor::tuneId (0), 0.15f);
            tuneSlider.setZoom (1.0f);
            modulatable::setLivePosition (*tuneSlider.getChildComponent (0), tuning::toPosition (-0.32f));
            saveTuneSlider ("13a-tune-zoomed-in");

            setParameter (tuned, SynthProcessor::tuneId (0), 7.0f);
            tuneSlider.setZoom (0.5f);
            modulatable::setLivePosition (*tuneSlider.getChildComponent (0), std::nullopt);
            saveTuneSlider ("13b-tune-zoomed-partway");
            tuneSlider.setLookAndFeel (nullptr);
        }

        // The standalone app's window, playing from a MIDI keyboard, as a note arrives.
        {
            ShownMidiInputs shown;
            shown.inputs = { { "keystep", "Arturia KeyStep 37", true }, { "pads", "Akai MPD218", false } };
            MidiInputs::set (&shown);

            SynthProcessor app;
            std::unique_ptr<juce::AudioProcessorEditor> appEditor (app.createEditorAndMakeActive());
            auto& appWindow = dynamic_cast<SynthEditor&> (*appEditor);

            app.prepareToPlay (sampleRate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            app.processBlock (buffer, midi);
            appWindow.refresh();

            const auto file = folder.getChildFile ("12-standalone-midi.png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (appEditor->createComponentSnapshot (appEditor->getLocalBounds(), true, 2.0f), stream);

            app.editorBeingDeleted (appEditor.get());
            appEditor = nullptr;
            MidiInputs::set (nullptr);
        }

        // The smallest the window goes, with the most harmonic sliders.
        setNumHarmonics (synth, 9, 2);
        synth.editedSource = 2;
        // With the first filter a notch, and the second a mid pass.
        setFilterType (synth, 0, FilterType::notch);
        setParameter (synth, SynthProcessor::filterIds (0).depth, 24.0f);
        setFilterType (synth, 1, FilterType::midPass);
        setFilter (synth, SynthProcessor::filterIds (1), true, 1200.0f);
        editor->setSize (1240, 900);
        save ("8-smallest");
    }
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;

    testDecomposition();
    testPlaying();
    testChangingTheWaveMidNote();
    testEditingHarmonics();
    testDrawingInSteps();
    testOscillators();
    testFilters();
    testLfos();
    testRandomLfos();
    testEnvelopes();
    testEnvelopeReleases();
    testVelocity();
    testShowingMovedHarmonics();
    testTuning();
    testMidiInputs();
    testPresets();
    testFactoryPresets();
    testSavingAndLoading();

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshots")
            saveSnapshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]));

    std::cout << std::endl << (failures == 0 ? "All tests passed." : std::to_string (failures) + " failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
