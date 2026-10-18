// Checks that waves break down into the right harmonics and that the synth plays them, then, if asked with
// --snapshots <folder>, saves pictures of the window showing a few waves.

#include "../Source/SynthEditor.h"
#include "../Source/SynthProcessor.h"
#include "../Source/Waveform.h"

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
    std::vector<float> play (SynthProcessor& synth, int note, double holdSeconds, double totalSeconds)
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
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);

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

        check (synth.parameters.getRawParameterValue (SynthProcessor::lowPassIds.on)->load() < 0.5f
                 && synth.parameters.getRawParameterValue (SynthProcessor::highPassIds.on)->load() < 0.5f,
               "both filters are out to begin with");

        setFilter (synth, SynthProcessor::lowPassIds, true, 1000.0f);
        const auto lowPassed = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (lowPassed, 440.0), expectedGain (true, 440.0, 1000.0, flatQ), 0.02)
                 && isNear (gainAt (lowPassed, 3960.0), expectedGain (true, 3960.0, 1000.0, flatQ), 0.05),
               "a low pass at 1 kHz keeps the 1st harmonic and cuts the 9th by "
                 + juce::String (-juce::Decibels::gainToDecibels (expectedGain (true, 3960.0, 1000.0, flatQ)), 1) + " dB");

        setFilter (synth, SynthProcessor::lowPassIds, false, 1000.0f);
        const auto switchedOut = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (switchedOut, 3960.0), 1.0, 0.001), "switching it out restores the sound");

        setFilter (synth, SynthProcessor::highPassIds, true, 1000.0f);
        const auto highPassed = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (highPassed, 440.0), expectedGain (false, 440.0, 1000.0, flatQ), 0.02)
                 && isNear (gainAt (highPassed, 3960.0), expectedGain (false, 3960.0, 1000.0, flatQ), 0.02),
               "a high pass at 1 kHz cuts the 1st harmonic by "
                 + juce::String (-juce::Decibels::gainToDecibels (expectedGain (false, 440.0, 1000.0, flatQ)), 1)
                 + " dB and keeps the 9th");

        setFilter (synth, SynthProcessor::highPassIds, true, 500.0f);
        setFilter (synth, SynthProcessor::lowPassIds, true, 2000.0f);
        const auto band = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (band, 880.0), expectedGain (false, 880.0, 500.0, flatQ) * expectedGain (true, 880.0, 2000.0, flatQ), 0.02),
               "both filters at once shape the sound one after the other");

        setFilter (synth, SynthProcessor::highPassIds, false, 500.0f);
        setFilter (synth, SynthProcessor::lowPassIds, true, 1320.0f, 1.0f);
        const auto resonant = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (gainAt (resonant, 1320.0), FilterStage::resonanceToQ (1.0f), 0.03),
               "full resonance boosts the harmonic at the cutoff " + juce::String (FilterStage::resonanceToQ (1.0f), 1) + " times");

        // Switching a filter in during a note fades it in, rather than jumping.
        setFilter (synth, SynthProcessor::lowPassIds, false, 300.0f);
        synth.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        std::vector<float> output;

        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);

            if (block == 20)
                setParameter (synth, SynthProcessor::lowPassIds.on, 1.0f);

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

        // At a low sample rate a 20 kHz cutoff is beyond the Nyquist frequency, where the filter can't work.
        SynthProcessor lowRate;
        setFilter (lowRate, SynthProcessor::lowPassIds, true, 20000.0f);
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
        setFilter (original, SynthProcessor::lowPassIds, true, 3000.0f, 0.5f);

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
        check (restored.parameters.getRawParameterValue (SynthProcessor::lowPassIds.on)->load() > 0.5f
                 && isNear (restored.parameters.getRawParameterValue (SynthProcessor::lowPassIds.cutoff)->load(), 3000.0, 1.0e-4)
                 && isNear (restored.parameters.getRawParameterValue (SynthProcessor::lowPassIds.resonance)->load(), 0.5, 1.0e-4),
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
        synth.editedOscillator = 1;
        setFilter (synth, SynthProcessor::lowPassIds, true, 1800.0f, 0.4f);
        setFilter (synth, SynthProcessor::highPassIds, true, 90.0f);
        save ("6-three-oscillators");

        setParameter (synth, SynthProcessor::onId (2), 0.0f);
        synth.editedOscillator = 2;
        save ("7-editing-an-oscillator-that-is-off");

        // The smallest the window goes, with the most harmonic sliders.
        setNumHarmonics (synth, 9, 2);
        editor->setSize (1060, 820);
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
    testOscillators();
    testFilters();
    testSavingAndLoading();

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshots")
            saveSnapshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]));

    std::cout << std::endl << (failures == 0 ? "All tests passed." : std::to_string (failures) + " failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
