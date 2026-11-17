#include "SoundAnalysis.h"

#include <juce_dsp/juce_dsp.h>

namespace analysis
{
    namespace
    {
        constexpr float floorDb = -120.0f;

        float toDb (double gain)
        {
            return gain > 0.0 ? (float) juce::jmax ((double) floorDb, 20.0 * std::log10 (gain)) : floorDb;
        }

        /** The RMS level of a stretch of the samples. */
        double rms (const std::vector<float>& samples, int start, int length)
        {
            start = juce::jlimit (0, (int) samples.size(), start);
            const auto stop = juce::jlimit (start, (int) samples.size(), start + length);

            if (stop <= start)
                return 0.0;

            auto sum = 0.0;

            for (int i = start; i < stop; ++i)
                sum += (double) samples[(size_t) i] * samples[(size_t) i];

            return std::sqrt (sum / (stop - start));
        }

        /** The period of the stretch from start, by YIN, in samples, if it has one. */
        std::optional<double> findPeriod (const std::vector<float>& samples, int start, double sampleRate)
        {
            const auto shortest = (int) (sampleRate / 2000.0);
            const auto longest = (int) (sampleRate / 35.0);
            const auto window = longest;

            if (start < 0 || start + window + longest >= (int) samples.size())
                return std::nullopt;

            std::vector<double> difference ((size_t) longest + 2, 0.0);

            for (int lag = 1; lag <= longest + 1; ++lag)
            {
                auto sum = 0.0;

                for (int j = 0; j < window; ++j)
                {
                    const auto delta = (double) samples[(size_t) (start + j)] - samples[(size_t) (start + j + lag)];
                    sum += delta * delta;
                }

                difference[(size_t) lag] = sum;
            }

            // The cumulative mean normalised difference.
            std::vector<double> normalised (difference.size(), 1.0);
            auto running = 0.0;

            for (size_t lag = 1; lag < difference.size(); ++lag)
            {
                running += difference[lag];
                normalised[lag] = running > 0.0 ? difference[lag] * (double) lag / running : 1.0;
            }

            auto chosen = -1;

            for (int lag = shortest; lag <= longest; ++lag)
            {
                if (normalised[(size_t) lag] < 0.15)
                {
                    while (lag + 1 <= longest && normalised[(size_t) lag + 1] < normalised[(size_t) lag])
                        ++lag;

                    chosen = lag;
                    break;
                }
            }

            if (chosen < 0)
            {
                auto best = shortest;

                for (int lag = shortest; lag <= longest; ++lag)
                    if (normalised[(size_t) lag] < normalised[(size_t) best])
                        best = lag;

                if (normalised[(size_t) best] > 0.35)
                    return std::nullopt;

                chosen = best;
            }

            // Between samples, by fitting a parabola through the dip.
            const auto a = normalised[(size_t) chosen - 1], b = normalised[(size_t) chosen], c = normalised[(size_t) chosen + 1];
            const auto denominator = a - 2.0 * b + c;
            const auto shift = std::abs (denominator) > 1.0e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denominator) : 0.0;
            return chosen + shift;
        }

        double median (std::vector<double> values)
        {
            std::sort (values.begin(), values.end());
            return values[values.size() / 2];
        }

        Snapshot measureHarmonics (const Audio& audio, double time, double onset, double fundamental, double peakRms)
        {
            Snapshot snapshot;
            snapshot.time = time;
            snapshot.fundamental = fundamental;

            const auto sampleRate = audio.sampleRate;
            const auto length = juce::jlimit (1024, (int) (0.2 * sampleRate), (int) (8.0 * sampleRate / fundamental));
            const auto centre = (int) ((onset + time) * sampleRate);
            const auto start = juce::jlimit (0, juce::jmax (0, (int) audio.samples.size() - length), centre - length / 2);
            snapshot.levelDb = toDb (rms (audio.samples, start, length) / juce::jmax (1.0e-12, peakRms));

            auto order = 1;

            while ((1 << order) < 4 * length)
                ++order;

            juce::dsp::FFT fft (order);
            const auto size = (size_t) fft.getSize();
            std::vector<float> data (2 * size, 0.0f);
            auto windowSum = 0.0;

            for (int i = 0; i < length && start + i < (int) audio.samples.size(); ++i)
            {
                const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (length - 1));
                data[(size_t) i] = (float) (audio.samples[(size_t) (start + i)] * window);
                windowSum += window;
            }

            fft.performFrequencyOnlyForwardTransform (data.data(), true);
            const auto binHz = sampleRate / (double) size;
            const auto magnitude = [&data, size] (int bin) { return (double) data[(size_t) juce::jlimit (0, (int) size / 2, bin)]; };

            const auto top = juce::jmin (20000.0, 0.48 * sampleRate);
            snapshot.harmonicsMeasured = juce::jlimit (0, numHarmonics, (int) std::floor (top / fundamental));

            std::array<double, numHarmonics> found {};
            auto harmonicPower = 0.0;

            for (int harmonic = 1; harmonic <= snapshot.harmonicsMeasured; ++harmonic)
            {
                const auto low = (int) std::ceil ((harmonic - 0.4) * fundamental / binHz);
                const auto high = (int) std::floor ((harmonic + 0.4) * fundamental / binHz);
                auto best = low;

                for (int bin = low; bin <= high; ++bin)
                    if (magnitude (bin) > magnitude (best))
                        best = bin;

                // The peak's frequency between bins, from a parabola through the log magnitudes around it.
                const auto a = std::log (magnitude (best - 1) + 1.0e-12), b = std::log (magnitude (best) + 1.0e-12),
                           c = std::log (magnitude (best + 1) + 1.0e-12);
                const auto denominator = a - 2.0 * b + c;
                const auto shift = std::abs (denominator) > 1.0e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denominator) : 0.0;

                found[(size_t) harmonic - 1] = 2.0 * magnitude (best) / juce::jmax (1.0e-12, windowSum);
                snapshot.ratios[(size_t) harmonic - 1] = (float) ((best + shift) * binHz / fundamental);

                // The power around the peak, within its window's main lobe.
                const auto lobe = juce::jmax (2, (int) std::ceil (4.0 * sampleRate / length / binHz / 2.0));

                for (int bin = best - lobe; bin <= best + lobe; ++bin)
                    harmonicPower += magnitude (bin) * magnitude (bin);
            }

            auto totalPower = 0.0;

            for (int bin = (int) (0.5 * fundamental / binHz); bin <= (int) (top / binHz); ++bin)
                totalPower += magnitude (bin) * magnitude (bin);

            snapshot.harmonicShare = totalPower > 0.0 ? (float) juce::jlimit (0.0, 1.0, harmonicPower / totalPower) : 0.0f;

            const auto strongest = *std::max_element (found.begin(), found.end());

            // Frequencies are against the 1st harmonic's own peak, where it's strong enough to find, which is more
            // exact than the fundamental found from the period.
            if (snapshot.harmonicsMeasured > 0 && found[0] > 0.05 * strongest && snapshot.ratios[0] > 0.0f)
            {
                const auto first = snapshot.ratios[0];

                for (int harmonic = 1; harmonic <= snapshot.harmonicsMeasured; ++harmonic)
                    snapshot.ratios[(size_t) harmonic - 1] /= first;

                snapshot.fundamental *= first;
            }
            auto weighted = 0.0, power = 0.0;

            for (int harmonic = 1; harmonic <= snapshot.harmonicsMeasured; ++harmonic)
            {
                const auto amplitude = strongest > 0.0 ? found[(size_t) harmonic - 1] / strongest : 0.0;
                snapshot.amplitudes[(size_t) harmonic - 1] = (float) amplitude;
                weighted += harmonic * amplitude * amplitude;
                power += amplitude * amplitude;
            }

            snapshot.centroid = power > 0.0 ? (float) (weighted / power) : 0.0f;

            // The strongest peaks anywhere, at least 40 dB above the floor of the strongest, and a third of the
            // fundamental apart, so each partial counts once.
            std::vector<std::pair<double, int>> candidates;
            const auto from = juce::jmax (2, (int) (0.3 * fundamental / binHz));

            for (int bin = from; bin < (int) (top / binHz); ++bin)
                if (magnitude (bin) > magnitude (bin - 1) && magnitude (bin) >= magnitude (bin + 1))
                    candidates.push_back ({ magnitude (bin), bin });

            std::sort (candidates.begin(), candidates.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
            const auto loudest = candidates.empty() ? 0.0 : candidates.front().first;
            std::vector<int> taken;

            for (const auto& [level, bin] : candidates)
            {
                if (snapshot.peaks.size() >= 12 || level < loudest * 0.01)
                    break;

                if (std::any_of (taken.begin(), taken.end(), [candidate = bin, &fundamental, binHz] (int other)
                                 { return std::abs (other - candidate) * binHz < fundamental / 3.0; }))
                    continue;

                taken.push_back (bin);
                snapshot.peaks.push_back ({ (float) (bin * binHz / snapshot.fundamental), (float) (level / loudest) });
            }

            return snapshot;
        }

        juce::String pad (const juce::String& text, int width)
        {
            return text.paddedLeft (' ', width);
        }

        juce::String formatTime (double seconds)
        {
            return seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms" : juce::String (seconds, 2) + " s";
        }

        juce::String formatAmplitude (float amplitude)
        {
            return amplitude >= 0.9995f ? juce::String ("1") : juce::String (amplitude, 3).substring (1);
        }

        /** The snapshot nearest a moment, if there's one within a few milliseconds. */
        const Snapshot* findSnapshot (const Analysis& analysis, double time)
        {
            for (const auto& snapshot : analysis.snapshots)
                if (std::abs (snapshot.time - time) < 0.005)
                    return &snapshot;

            return nullptr;
        }

        float envelopeAt (const Analysis& analysis, double time)
        {
            for (const auto& [at, level] : analysis.envelope)
                if (std::abs (at - time) < 0.0005)
                    return level;

            return floorDb;
        }

        juce::String describeVibrato (const Analysis& analysis)
        {
            return juce::String (analysis.vibratoCents, 1) + " cents"
                 + (analysis.vibratoCents > 4.0f ? " at " + juce::String (analysis.vibratoRate, 1) + " Hz" : juce::String());
        }

        /** How many harmonics to show: up to the highest that's anywhere above 0.002, at least 8, as far as measured. */
        int countAudible (const std::vector<Snapshot>& snapshots)
        {
            auto measured = 0, audible = 8;

            for (const auto& snapshot : snapshots)
            {
                measured = juce::jmax (measured, snapshot.harmonicsMeasured);

                for (int harmonic = 1; harmonic <= snapshot.harmonicsMeasured; ++harmonic)
                    if (snapshot.amplitudes[(size_t) harmonic - 1] >= 0.002f)
                        audible = juce::jmax (audible, harmonic);
            }

            return juce::jmin (measured, audible);
        }

        juce::String describeHarmonicRows (const std::vector<const Snapshot*>& rows, const juce::StringArray& labels, int count)
        {
            juce::String text;
            text << pad ("", 18);

            for (int harmonic = 1; harmonic <= count; ++harmonic)
                text << pad (juce::String (harmonic), 5);

            text << "   level  centroid  harmonic\n";

            for (size_t row = 0; row < rows.size(); ++row)
            {
                const auto& snapshot = *rows[row];
                text << pad (labels[(int) row], 18);

                for (int harmonic = 1; harmonic <= count; ++harmonic)
                    text << pad (harmonic <= snapshot.harmonicsMeasured ? formatAmplitude (snapshot.amplitudes[(size_t) harmonic - 1]) : "-", 5);

                text << pad (juce::String (juce::roundToInt (snapshot.levelDb)) + " dB", 8)
                     << pad (juce::String (snapshot.centroid, 1), 10)
                     << pad (juce::String (juce::roundToInt (snapshot.harmonicShare * 100.0f)) + "%", 10) << "\n";
            }

            return text;
        }
    }

    std::optional<Audio> readFile (const juce::File& file, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
        {
            error = file.existsAsFile() ? "Couldn't read " + file.getFullPathName() + " as audio"
                                        : "There's no file " + file.getFullPathName();
            return std::nullopt;
        }

        const auto length = (int) juce::jmin ((juce::int64) (60.0 * reader->sampleRate), reader->lengthInSamples);
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
        reader->read (&buffer, 0, length, 0, true, true);

        Audio audio;
        audio.sampleRate = reader->sampleRate;
        audio.samples.assign ((size_t) length, 0.0f);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < length; ++i)
                audio.samples[(size_t) i] += buffer.getSample (channel, i) / (float) buffer.getNumChannels();

        return audio;
    }

    bool writeWav (const Audio& audio, const juce::File& file)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());

        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        const auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (audio.sampleRate)
                                                                                       .withNumChannels (1)
                                                                                       .withBitsPerSample (24));

        if (writer == nullptr)
            return false;

        const float* channels[] = { audio.samples.data() };
        return writer->writeFromFloatArrays (channels, 1, (int) audio.samples.size());
    }

    Audio renderNote (juce::AudioProcessor& instrument, int note, float velocity, double holdSeconds, double lengthSeconds,
                      double sampleRate)
    {
        constexpr int blockSize = 256;
        instrument.setRateAndBufferSizeDetails (sampleRate, blockSize);
        instrument.prepareToPlay (sampleRate, blockSize);

        Audio audio;
        audio.sampleRate = sampleRate;
        const auto total = (int) (lengthSeconds * sampleRate);
        const auto release = (int) (holdSeconds * sampleRate);
        audio.samples.assign ((size_t) total, 0.0f);
        juce::AudioBuffer<float> block (juce::jmax (2, instrument.getTotalNumOutputChannels()), blockSize);

        for (int start = 0; start < total; start += blockSize)
        {
            juce::MidiBuffer midi;

            if (start == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.01f, 1.0f, velocity)), 0);

            if (release >= start && release < start + blockSize)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), release - start);

            block.clear();
            instrument.processBlock (block, midi);

            // Both sides together, as a sample's are, so a panned sound is heard whole. In the middle, both sides
            // are the same.
            const auto channels = juce::jmin (2, block.getNumChannels());

            for (int i = 0; i < blockSize && start + i < total; ++i)
            {
                auto sum = 0.0f;

                for (int channel = 0; channel < channels; ++channel)
                    sum += block.getSample (channel, i);

                audio.samples[(size_t) (start + i)] = sum / (float) channels;
            }
        }

        instrument.releaseResources();
        return audio;
    }

    std::vector<double> getStandardSnapshotTimes()
    {
        return { 0.02, 0.05, 0.1, 0.2, 0.35, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0 };
    }

    Analysis analyse (const Audio& audio, const std::vector<double>& snapshotTimes)
    {
        Analysis analysis;
        const auto sampleRate = audio.sampleRate;
        const auto& samples = audio.samples;
        analysis.sampleRate = sampleRate;
        analysis.duration = (double) samples.size() / sampleRate;

        // The level, every 2.5 ms over 10 ms.
        const auto hop = juce::jmax (1, (int) (0.0025 * sampleRate));
        const auto frame = (int) (0.01 * sampleRate);
        std::vector<double> levels;

        for (int start = 0; start + frame <= (int) samples.size(); start += hop)
            levels.push_back (rms (samples, start, frame));

        if (levels.empty())
            return analysis;

        const auto peakFrame = (size_t) std::distance (levels.begin(), std::max_element (levels.begin(), levels.end()));
        const auto peakRms = levels[peakFrame];
        analysis.peakDb = toDb (peakRms);

        if (peakRms <= 0.0)
            return analysis;

        // The note starts when it first comes within 40 dB of its peak, and ends when it's last within 60 dB.
        size_t onsetFrame = 0, endFrame = levels.size() - 1;

        while (onsetFrame < peakFrame && levels[onsetFrame] < peakRms * 0.01)
            ++onsetFrame;

        while (endFrame > peakFrame && levels[endFrame] < peakRms * 0.001)
            --endFrame;

        analysis.onset = (double) onsetFrame * hop / sampleRate;
        analysis.peakTime = (double) peakFrame * hop / sampleRate - analysis.onset;
        analysis.end = ((double) endFrame * hop + frame) / sampleRate - analysis.onset;

        for (const auto time : { 0.0, 0.005, 0.01, 0.02, 0.03, 0.05, 0.075, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.75, 1.0, 1.25,
                                 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 15.0, 20.0, 30.0 })
        {
            if (time > analysis.end + 0.25 || analysis.onset + time + 0.01 > analysis.duration)
                break;

            const auto start = (int) ((analysis.onset + time) * sampleRate);
            analysis.envelope.push_back ({ time, toDb (rms (samples, start, frame) / peakRms) });
        }

        // The pitch, every 25 ms, where the note's within 30 dB of its peak.
        std::vector<std::pair<double, double>> pitches;   // time from the onset, and frequency

        for (auto time = 0.0; time < analysis.end && pitches.size() < 160; time += 0.025)
        {
            const auto start = (int) ((analysis.onset + time) * sampleRate);

            if (rms (samples, start, frame) < peakRms * 0.03)
                continue;

            if (const auto period = findPeriod (samples, start, sampleRate))
                pitches.push_back ({ time, sampleRate / *period });
        }

        if (pitches.size() >= 2)
        {
            std::vector<double> frequencies;

            for (const auto& [time, frequency] : pitches)
                frequencies.push_back (frequency);

            const auto fundamental = median (frequencies);
            analysis.fundamental = fundamental;
            const auto note = 69.0 + 12.0 * std::log2 (fundamental / 440.0);
            analysis.midiNote = juce::roundToInt (note);
            analysis.centsOff = (float) ((note - analysis.midiNote) * 100.0);

            // The vibrato from the pitch's spread, leaving out the start, where it may be settling, and octave slips.
            std::vector<double> cents;

            for (const auto& [time, frequency] : pitches)
                if (time >= 0.1 && std::abs (1200.0 * std::log2 (frequency / fundamental)) < 300.0)
                    cents.push_back (1200.0 * std::log2 (frequency / fundamental));

            if (cents.size() >= 6)
            {
                auto sorted = cents;
                std::sort (sorted.begin(), sorted.end());
                analysis.vibratoCents = (float) ((sorted[sorted.size() * 9 / 10] - sorted[sorted.size() / 10]) / 2.0);

                const auto middle = median (cents);
                auto crossings = 0;

                for (size_t i = 1; i < cents.size(); ++i)
                    if ((cents[i - 1] - middle) * (cents[i] - middle) < 0.0)
                        ++crossings;

                analysis.vibratoRate = (float) (crossings / 2.0 / ((double) cents.size() * 0.025));
            }

            // How far the start is from the middle.
            std::vector<double> early, settled;

            for (const auto& [time, frequency] : pitches)
                if (std::abs (1200.0 * std::log2 (frequency / fundamental)) < 300.0)
                    (time < 0.08 ? early : settled).push_back (frequency);

            if (! early.empty() && ! settled.empty())
                analysis.pitchDriftCents = (float) (1200.0 * std::log2 (median (early) / median (settled)));
        }

        const auto fundamentalNear = [&pitches, &analysis] (double time)
        {
            auto best = analysis.fundamental.value_or (0.0);
            auto bestDistance = 0.06;

            for (const auto& [at, frequency] : pitches)
                if (std::abs (at - time) < bestDistance
                     && std::abs (1200.0 * std::log2 (frequency / analysis.fundamental.value_or (frequency))) < 300.0)
                {
                    best = frequency;
                    bestDistance = std::abs (at - time);
                }

            return best;
        };

        if (analysis.fundamental.has_value())
            for (const auto time : snapshotTimes)
                if (time <= analysis.end && analysis.onset + time < analysis.duration - 0.01)
                    analysis.snapshots.push_back (measureHarmonics (audio, time, analysis.onset, fundamentalNear (time), peakRms));

        return analysis;
    }

    juce::String describe (const Analysis& analysis, const juce::String& title)
    {
        juce::String text;
        text << "== " << title << " ==\n";
        text << "Length " << juce::String (analysis.duration, 2) << " s at " << juce::roundToInt (analysis.sampleRate) << " Hz. "
             << "The note starts at " << formatTime (analysis.onset) << ", peaks " << formatTime (analysis.peakTime)
             << " later at " << juce::String (analysis.peakDb, 1) << " dBFS, and fades 60 dB below its peak "
             << formatTime (analysis.end) << " after it starts.\n";

        if (analysis.fundamental.has_value())
        {
            text << "Pitch: " << juce::String (*analysis.fundamental, 2) << " Hz, MIDI note " << analysis.midiNote << " ("
                 << juce::MidiMessage::getMidiNoteName (analysis.midiNote, true, true, 4) << ") "
                 << (analysis.centsOff >= 0.0f ? "+" : "") << juce::String (analysis.centsOff, 1) << " cents. "
                 << "Wavers by about " << juce::String (analysis.vibratoCents, 1) << " cents either way"
                 << (analysis.vibratoCents > 4.0f ? ", about " + juce::String (analysis.vibratoRate, 1) + " times a second" : juce::String())
                 << ". The first 80 ms are " << juce::String (analysis.pitchDriftCents, 1) << " cents from the rest.\n";
        }
        else
        {
            text << "Pitch: none found. It may be noise, a drum, or too short.\n";
        }

        text << "\nLoudness after the start, in dB against its peak:\n";

        for (const auto& [time, level] : analysis.envelope)
            text << pad (formatTime (time), 9) << pad (juce::String (level, 1), 8) << "\n";

        if (! analysis.snapshots.empty())
        {
            const auto count = countAudible (analysis.snapshots);

            text << "\nHarmonic amplitudes (linear, against the strongest at that moment, which is 1), the level at that "
                    "moment, where the power is centred (as a harmonic number), and the share of power in the harmonics "
                    "rather than noise:\n";

            std::vector<const Snapshot*> rows;
            juce::StringArray labels;

            for (const auto& snapshot : analysis.snapshots)
            {
                rows.push_back (&snapshot);
                labels.add (formatTime (snapshot.time));
            }

            text << describeHarmonicRows (rows, labels, count);

            // Harmonics that aren't whole multiples, from the snapshot after the attack.
            const auto& settled = analysis.snapshots[juce::jmin ((size_t) 3, analysis.snapshots.size() - 1)];
            juce::StringArray detuned;

            for (int harmonic = 2; harmonic <= settled.harmonicsMeasured; ++harmonic)
            {
                const auto ratio = settled.ratios[(size_t) harmonic - 1];

                if (settled.amplitudes[(size_t) harmonic - 1] > 0.01f && std::abs (ratio / (float) harmonic - 1.0f) > 0.01f)
                    detuned.add (juce::String (harmonic) + ": x" + juce::String (ratio, 3));
            }

            text << "\nHarmonics more than 1% off their whole multiple at " << formatTime (settled.time) << ": "
                 << (detuned.isEmpty() ? juce::String ("none") : detuned.joinIntoString (", ")) << "\n";

            // The strongest peaks, near the start and once it's settled, to show partials between the harmonics.
            for (const auto* snapshot : { &analysis.snapshots.front(), &settled })
            {
                juce::StringArray peaks;

                for (const auto& [ratio, amplitude] : snapshot->peaks)
                    peaks.add ("x" + juce::String (ratio, 3) + " (" + formatAmplitude (amplitude) + ")");

                text << "Strongest peaks at " << formatTime (snapshot->time) << ", as multiples of the fundamental (amplitude): "
                     << peaks.joinIntoString (", ") << "\n";
            }
        }

        return text;
    }

    Distance measureDistance (const Analysis& sample, const Analysis& attempt)
    {
        Distance distance;
        auto envelopeSum = 0.0;
        auto envelopeCount = 0;

        for (const auto& [time, level] : sample.envelope)
        {
            if (level < -60.0f)
                continue;

            envelopeSum += std::abs (juce::jmax (-60.0f, level) - juce::jmax (-60.0f, envelopeAt (attempt, time)));
            ++envelopeCount;
        }

        distance.envelopeDb = envelopeCount > 0 ? (float) (envelopeSum / envelopeCount) : 0.0f;

        auto harmonicSum = 0.0;
        auto harmonicCount = 0;

        for (const auto& snapshot : sample.snapshots)
        {
            const auto* other = findSnapshot (attempt, snapshot.time);

            if (other == nullptr || snapshot.levelDb < -50.0f)
                continue;

            for (int harmonic = 1; harmonic <= juce::jmin (snapshot.harmonicsMeasured, other->harmonicsMeasured, 24); ++harmonic)
            {
                const auto a = juce::jmax (-50.0f, toDb (snapshot.amplitudes[(size_t) harmonic - 1]));
                const auto b = juce::jmax (-50.0f, toDb (other->amplitudes[(size_t) harmonic - 1]));
                harmonicSum += std::abs (a - b);
                ++harmonicCount;
            }
        }

        distance.harmonicsDb = harmonicCount > 0 ? (float) (harmonicSum / harmonicCount) : 0.0f;

        if (sample.fundamental.has_value() && attempt.fundamental.has_value())
            distance.pitchCents = (float) (1200.0 * std::log2 (*attempt.fundamental / *sample.fundamental));

        return distance;
    }

    juce::String compare (const Analysis& sample, const Analysis& attempt)
    {
        juce::String text;
        const auto distance = measureDistance (sample, attempt);

        text << "== Sample against EZ Synth ==\n";
        text << "Average loudness difference: " << juce::String (distance.envelopeDb, 1) << " dB.  Average harmonic difference: "
             << juce::String (distance.harmonicsDb, 1) << " dB.  Pitch: EZ Synth is " << juce::String (distance.pitchCents, 1)
             << " cents " << (distance.pitchCents >= 0.0f ? "above" : "below") << " the sample. Lower is closer for each.\n";

        if (sample.fundamental.has_value() && attempt.fundamental.has_value())
            text << "Vibrato: sample " << describeVibrato (sample) << ", EZ Synth " << describeVibrato (attempt) << ". Start of note: sample " << juce::String (sample.pitchDriftCents, 1) << " cents, EZ Synth "
                 << juce::String (attempt.pitchDriftCents, 1) << " cents.\n";

        text << "Peak: sample " << formatTime (sample.peakTime) << " after the start, EZ Synth " << formatTime (attempt.peakTime)
             << ". Fades 60 dB down: sample at " << formatTime (sample.end) << ", EZ Synth at " << formatTime (attempt.end) << ".\n";

        text << "\nLoudness after the start, in dB against each one's own peak:\n" << pad ("", 9) << pad ("sample", 9)
             << pad ("synth", 9) << pad ("diff", 9) << "\n";

        for (const auto& [time, level] : sample.envelope)
        {
            const auto other = envelopeAt (attempt, time);
            text << pad (formatTime (time), 9) << pad (juce::String (level, 1), 9) << pad (juce::String (other, 1), 9)
                 << pad (juce::String (other - level, 1), 9) << "\n";
        }

        auto both = sample.snapshots;
        both.insert (both.end(), attempt.snapshots.begin(), attempt.snapshots.end());
        const auto count = countAudible (both);

        text << "\nHarmonic amplitudes, linear against the strongest at each moment, the sample's row above EZ Synth's:\n";
        std::vector<const Snapshot*> rows;
        juce::StringArray labels;

        for (const auto& snapshot : sample.snapshots)
        {
            if (const auto* other = findSnapshot (attempt, snapshot.time))
            {
                rows.push_back (&snapshot);
                labels.add (formatTime (snapshot.time) + " sample");
                rows.push_back (other);
                labels.add ("synth");
            }
        }

        text << describeHarmonicRows (rows, labels, count);
        return text;
    }
}
