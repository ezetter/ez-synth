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

    void setNumPartials (SynthProcessor& synth, int count, int oscillator = 0)
    {
        setParameter (synth, SynthProcessor::harmonicsId (oscillator), (float) count);
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

    /** Checks a preset's strongest harmonics are the expected ones, at the expected strengths. */
    void checkPreset (waveform::Preset preset, const std::vector<int>& expectedHarmonics,
                      const std::function<double (int harmonic)>& expectedAmplitude)
    {
        const auto decomposition = waveform::decompose (waveform::makePreset (preset));
        const auto name = waveform::getName (preset).toLowerCase();

        std::vector<int> harmonics;
        auto amplitudesRight = true;

        for (int i = 0; i < decomposition.numStrongest; ++i)
        {
            const auto& partial = decomposition.strongest[(size_t) i];
            harmonics.push_back (partial.harmonic);
            amplitudesRight = amplitudesRight && isNear (partial.amplitude, expectedAmplitude (partial.harmonic), 0.01);
        }

        juce::StringArray expected;

        for (const auto harmonic : expectedHarmonics)
            expected.add (juce::String (harmonic));

        check (harmonics == expectedHarmonics, "a " + name + "'s strongest harmonics are " + expected.joinIntoString (", "));
        check (amplitudesRight, "a " + name + "'s harmonics have the textbook strengths");
    }

    void testDecomposition()
    {
        std::cout << "Breaking waves down into sine waves" << std::endl;

        checkPreset (waveform::Preset::sine, { 1 }, [] (int) { return 1.0; });
        checkPreset (waveform::Preset::sawtooth, { 1, 2, 3, 4, 5, 6, 7, 8, 9 }, [] (int k) { return 2.0 / (pi * k); });
        checkPreset (waveform::Preset::square, { 1, 3, 5, 7, 9, 11, 13, 15, 17 }, [] (int k) { return 4.0 / (pi * k); });
        checkPreset (waveform::Preset::triangle, { 1, 3, 5, 7, 9, 11, 13, 15, 17 }, [] (int k) { return 8.0 / (pi * pi * k * k); });

        const auto sine = waveform::decompose (waveform::makePreset (waveform::Preset::sine));
        check (std::abs (sine.strongest[0].phase) < 1.0e-4, "a sine's harmonic starts at zero, rising");
        check (isNear (sine.peaks[1], 1.0, 0.001), "a sine peaks at 1");

        const auto square = waveform::decompose (waveform::makePreset (waveform::Preset::square));
        check (square.peaks[9] > 1.1f && square.peaks[9] < 1.25f, "nine of a square's sine waves overshoot its corners, as the Gibbs effect says");

        waveform::Shape flat {};
        check (waveform::decompose (flat).numStrongest == 0, "a flat wave has no harmonics");

        waveform::Shape offset {};
        offset.fill (0.5f);
        check (waveform::decompose (offset).numStrongest == 0, "an offset from zero isn't a harmonic");

        // A wave made mostly of its fifth harmonic, with a little of its first.
        waveform::Shape mostlyFifth {};

        for (int i = 0; i < waveform::numPoints; ++i)
        {
            const auto t = (double) i / waveform::numPoints;
            mostlyFifth[(size_t) i] = (float) (0.8 * std::sin (twoPi * 5.0 * t) + 0.2 * std::sin (twoPi * t + 1.0));
        }

        const auto fifth = waveform::decompose (mostlyFifth);
        check (fifth.numStrongest == 2 && fifth.strongest[0].harmonic == 5 && fifth.strongest[1].harmonic == 1,
               "the strongest harmonic is picked first, even when it isn't the first harmonic");
        check (std::abs (fifth.strongest[1].phase - 1.0) < 1.0e-3, "a harmonic's phase is found");
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

        setNumPartials (synth, 9);
        const auto nine = slice (play (synth, 69, 0.5, 0.8), 0.1, 0.45);
        check (isNear (amplitudeAt (nine, 9 * 440.0) / amplitudeAt (nine, 440.0), 1.0 / 9.0, 0.02), "asking for 9 sine waves adds up to the 9th harmonic");

        setPreset (synth, 0, waveform::Preset::square);
        setNumPartials (synth, 3);
        const auto square = slice (play (synth, 57, 0.5, 0.8), 0.1, 0.45);   // A3, 220 Hz
        const auto squareFirst = amplitudeAt (square, 220.0);
        check (isNear (amplitudeAt (square, 660.0) / squareFirst, 1.0 / 3.0, 0.02)
                 && isNear (amplitudeAt (square, 1100.0) / squareFirst, 1.0 / 5.0, 0.02),
               "a square played with 3 sine waves has its 1st, 3rd and 5th harmonics");
        check (amplitudeAt (square, 440.0) / squareFirst < 1.0e-3 && amplitudeAt (square, 1540.0) / squareFirst < 1.0e-3,
               "...and not its 2nd or 7th");

        // C8, 4186 Hz: a square's 5th harmonic is 20.9 kHz, too high to hear, and its 7th and 9th would alias
        // down to 18.7 kHz and 10.3 kHz.
        setNumPartials (synth, 9);
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

        const auto& square = synth.getDecomposition (1);
        const auto squareScale = 0.5 / square.peaks[3];
        const auto expectedFirst = 1.0 + squareScale * square.strongest[0].amplitude;
        const auto expectedThird = squareScale * square.strongest[1].amplitude;
        const auto mixedFirst = amplitudeAt (mixed, 220.0);
        check (isNear (amplitudeAt (mixed, 660.0) / mixedFirst, expectedThird / expectedFirst, 0.02),
               "a square in the second oscillator adds its 3rd harmonic to the first oscillator's sine");
        check (amplitudeAt (mixed, 440.0) / mixedFirst < 1.0e-3, "...and no 2nd harmonic, as neither wave has one");

        setNumPartials (synth, 1, 1);
        const auto fewer = slice (play (synth, 57, 0.5, 0.8), 0.1, 0.45);
        check (amplitudeAt (fewer, 660.0) / amplitudeAt (fewer, 220.0) < 1.0e-3,
               "each oscillator has its own number of harmonics");

        setParameter (synth, SynthProcessor::onId (1), 0.0f);
        setParameter (synth, SynthProcessor::levelId (0), 0.0f);
        check (peakOf (play (synth, 69, 0.5, 0.8)) < 1.0e-6f, "an oscillator at zero level is silent");
    }

    void testSavingAndLoading()
    {
        std::cout << "Saving and loading" << std::endl;

        SynthProcessor original;
        waveform::Shape drawn {};

        for (int i = 0; i < waveform::numPoints; ++i)
            drawn[(size_t) i] = i < waveform::numPoints / 8 ? 1.0f : -0.25f;

        original.setShape (1, drawn);
        setPreset (original, 2, waveform::Preset::triangle);
        setNumPartials (original, 7, 1);
        setParameter (original, SynthProcessor::onId (1), 1.0f);
        setParameter (original, SynthProcessor::levelId (1), 0.4f);

        juce::MemoryBlock state;
        original.getStateInformation (state);

        SynthProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        check (restored.getShape (1) == drawn, "a drawn wave is restored to its oscillator");
        check (! restored.getPreset (1).has_value(), "...and isn't taken for a preset");
        check (restored.getPreset (2) == waveform::Preset::triangle && restored.getPreset (0) == waveform::Preset::sawtooth,
               "the other oscillators' presets are restored as those presets");
        check (restored.getNumPartials (1) == 7 && restored.getNumPartials (0) == 3, "each oscillator's number of harmonics is restored");
        check (restored.isOn (0) && restored.isOn (1) && ! restored.isOn (2), "which oscillators are on is restored");
        check (isNear (restored.parameters.getRawParameterValue (SynthProcessor::levelId (1))->load(), 0.4, 1.0e-6),
               "oscillator levels are restored");

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
        setNumPartials (synth, 9);
        save ("2-square-9");

        setPreset (synth, 0, waveform::Preset::sine);
        setNumPartials (synth, 3);
        save ("3-sine");

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
        setNumPartials (synth, 5);
        save ("4-drawn-5");

        // All three on: a drawn wave, a square at 60% and a triangle at 35%, with the square being edited.
        setPreset (synth, 2, waveform::Preset::triangle);
        setParameter (synth, SynthProcessor::onId (1), 1.0f);
        setParameter (synth, SynthProcessor::onId (2), 1.0f);
        setParameter (synth, SynthProcessor::levelId (1), 0.6f);
        setParameter (synth, SynthProcessor::levelId (2), 0.35f);
        synth.editedOscillator = 1;
        save ("6-three-oscillators");

        setParameter (synth, SynthProcessor::onId (2), 0.0f);
        synth.editedOscillator = 2;
        save ("7-editing-an-oscillator-that-is-off");

        editor->setSize (960, 700);
        save ("8-smallest");
    }
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;

    testDecomposition();
    testPlaying();
    testChangingTheWaveMidNote();
    testOscillators();
    testSavingAndLoading();

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshots")
            saveSnapshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]));

    std::cout << std::endl << (failures == 0 ? "All tests passed." : std::to_string (failures) + " failed.") << std::endl;
    return failures == 0 ? 0 : 1;
}
