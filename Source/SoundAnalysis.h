#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <optional>
#include <vector>

/** Measures a recorded note in the terms EZ Synth is set in: its pitch, how its loudness rises and falls, and how
    strong each harmonic is as it goes on, so an AI matching a sample with EZ Synth has numbers to go on, and can
    measure its own attempts the same way to compare them with the sample.
*/
namespace analysis
{
    /** The most harmonics measured, more than one oscillator's 16, as other oscillators tuned an octave or two up
        can play the harmonics above them, up to the 64th.
    */
    inline constexpr int numHarmonics = 64;

    struct Audio
    {
        std::vector<float> samples;   // mixed down to one channel
        double sampleRate = 48000.0;
    };

    /** Reads a WAV, AIFF, MP3 or any other file the system can read. Returns nothing, and says why, if it can't. */
    std::optional<Audio> readFile (const juce::File&, juce::String& error);

    /** Writes audio as a 24-bit WAV file. Returns false if it can't. */
    bool writeWav (const Audio&, const juce::File&);

    /** Plays one note on an instrument, holding it down for a while, then letting it go and playing on to the
        length given, as one channel. The note starts at the very beginning.
    */
    Audio renderNote (juce::AudioProcessor&, int note, float velocity, double holdSeconds, double lengthSeconds,
                      double sampleRate = 48000.0);

    /** The harmonics at one moment of the note. */
    struct Snapshot
    {
        double time = 0.0;                // seconds after the note starts
        float levelDb = -120.0f;          // how loud, against the loudest moment of the note
        double fundamental = 0.0;         // in hertz, as measured near this moment

        /** Each harmonic's amplitude, from the 1st up, against the strongest of them now, which is 1. */
        std::array<float, numHarmonics> amplitudes {};

        /** Each harmonic's frequency over the fundamental's: 2 for an exact 2nd harmonic, 2.1 for one that's sharp. */
        std::array<float, numHarmonics> ratios {};

        /** Where the harmonics' power is centred, as a harmonic number: higher is brighter. */
        float centroid = 0.0f;

        /** How much of the power is in the harmonics, rather than noise or partials between them, from 0 to 1. */
        float harmonicShare = 0.0f;

        /** How many harmonics fit below 20 kHz, or half the sample rate. */
        int harmonicsMeasured = 0;

        /** The strongest peaks in the spectrum, wherever they are, strongest first: each one's frequency over the
            fundamental's, and its amplitude against the strongest. Inharmonic partials, such as a bell's, show here.
        */
        std::vector<std::pair<float, float>> peaks;
    };

    struct Analysis
    {
        double duration = 0.0;     // of the whole file, in seconds
        double sampleRate = 48000.0;
        double onset = 0.0;        // when the note starts, in seconds from the start of the file
        double peakTime = 0.0;     // when it's loudest, after the onset
        double end = 0.0;          // when it's faded to 60 dB below its peak, after the onset
        float peakDb = -120.0f;    // its peak level, against full scale

        /** How loud it is at moments after the onset, in dB against its loudest. */
        std::vector<std::pair<double, float>> envelope;

        /** The fundamental, if the note has a pitch: its median, the MIDI note nearest it, and how far from that
            note it is, in cents.
        */
        std::optional<double> fundamental;
        int midiNote = 60;
        float centsOff = 0.0f;

        /** How far the pitch wavers either way, in cents, and about how fast, in hertz: a vibrato's depth and rate. */
        float vibratoCents = 0.0f;
        float vibratoRate = 0.0f;

        /** How the pitch moves from the start to the middle of the note, in cents: a fall into the note, if
            negative, or a rise.
        */
        float pitchDriftCents = 0.0f;

        std::vector<Snapshot> snapshots;
    };

    /** The moments after the onset the harmonics are measured at, unless others are given. */
    std::vector<double> getStandardSnapshotTimes();

    /** Measures a note. The snapshots are taken at the times given, from the onset, that fall within the note. */
    Analysis analyse (const Audio&, const std::vector<double>& snapshotTimes = getStandardSnapshotTimes());

    /** The measurements, as text for an AI or a person to read. */
    juce::String describe (const Analysis&, const juce::String& title);

    /** A sample's measurements beside a synth's attempt's, with how far apart they are. */
    juce::String compare (const Analysis& sample, const Analysis& attempt);

    /** How far apart two analyses are, as single numbers, lower being closer: the loudness envelopes' average
        difference in dB, the harmonics' in dB, and the pitch's in cents.
    */
    struct Distance
    {
        float envelopeDb = 0.0f;
        float harmonicsDb = 0.0f;
        float pitchCents = 0.0f;
    };

    Distance measureDistance (const Analysis& sample, const Analysis& attempt);
}
