// Helps an AI, or a person, match a recorded sample with EZ Synth: it measures a sample, plays a sound description
// as one note and saves it, and measures that against the sample, to see how close it is. Run it with no arguments
// to see how.

#include "../Source/SoundAnalysis.h"
#include "../Source/SoundDescription.h"
#include "../Source/SoundPrompt.h"
#include "../Source/SynthProcessor.h"

#include <iostream>

namespace
{
    const char* usage = R"(EZ Synth's sound tool, for matching a recorded sample with EZ Synth.

  EZSynthSoundTool analyze <sample>
      Measures a sample (WAV, AIFF, MP3 and more): its pitch, its loudness over time, and its harmonics' amplitudes
      at moments through the note.

  EZSynthSoundTool render <sound> <out.wav> [--note N] [--velocity V] [--hold S] [--length S]
      Plays a sound as one note and saves it. The sound is a sound description (.json), a preset (.ezsynth), or a
      factory preset's name. The note is a MIDI note number or a name, such as C4, 60 to start with; velocity is from 0 to 1, 1 to start
      with; hold is how long the key's held, 1 second to start with; length is how long to record, 2 seconds to
      start with.

  EZSynthSoundTool compare <sound> <sample> [--note N] [--velocity V] [--hold S] [--length S] [--out file.wav]
      Plays the sound as the sample's note, as long as the sample, measures both, and shows them side by side with
      how far apart they are. To start with, the note is the sample's nearest MIDI note, the key's held until the
      sample has faded 60 dB, and the recording is as long as the sample. It saves what it played beside the sound,
      as a WAV file, unless told where.

  EZSynthSoundTool describe <sound>
      Prints a sound, such as a factory preset, as a sound description, to see how one's written.

  EZSynthSoundTool prompt
      Prints the prompt that tells an AI how EZ Synth works and how to write a sound description.
)";

    struct Options
    {
        std::optional<int> note;
        float velocity = 1.0f;
        std::optional<double> hold, length;
        juce::File out;
    };

    Options readOptions (const juce::StringArray& arguments, int from)
    {
        Options options;

        for (int i = from; i + 1 < arguments.size(); i += 2)
        {
            const auto& name = arguments[i];
            const auto& value = arguments[i + 1];

            if (name == "--note")
                options.note = sound::findNote (value.containsOnly ("0123456789") ? juce::var (value.getIntValue()) : juce::var (value));
            else if (name == "--velocity")
                options.velocity = value.getFloatValue();
            else if (name == "--hold")
                options.hold = value.getDoubleValue();
            else if (name == "--length")
                options.length = value.getDoubleValue();
            else if (name == "--out")
                options.out = juce::File::getCurrentWorkingDirectory().getChildFile (value);
        }

        return options;
    }

    juce::File findFile (const juce::String& path)
    {
        return juce::File::getCurrentWorkingDirectory().getChildFile (path.replace ("~", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName()));
    }

    /** Loads a sound description, a preset file or a factory preset. Says what went wrong, if anything did. */
    bool loadSound (SynthProcessor& synth, const juce::String& name)
    {
        const auto file = findFile (name);

        if (file.hasFileExtension ("json"))
        {
            const auto result = sound::loadFile (synth, file);

            for (const auto& problem : result.problems)
                std::cout << "Problem in the sound description: " << problem << std::endl;

            return result.loaded;
        }

        if (file.existsAsFile() ? synth.loadPreset (file) : synth.loadFactoryPreset (name))
            return true;

        std::cout << "There's no sound description, preset or factory preset called " << name << std::endl;
        return false;
    }

    int analyze (const juce::StringArray& arguments)
    {
        juce::String error;
        const auto audio = analysis::readFile (findFile (arguments[2]), error);

        if (! audio.has_value())
        {
            std::cout << error << std::endl;
            return 1;
        }

        std::cout << analysis::describe (analysis::analyse (*audio), findFile (arguments[2]).getFileName());
        return 0;
    }

    int render (const juce::StringArray& arguments)
    {
        SynthProcessor synth;

        if (! loadSound (synth, arguments[2]))
            return 1;

        const auto options = readOptions (arguments, 4);
        const auto audio = analysis::renderNote (synth, options.note.value_or (60), options.velocity, options.hold.value_or (1.0),
                                                 options.length.value_or (2.0));
        const auto file = findFile (arguments[3]);

        if (! analysis::writeWav (audio, file))
        {
            std::cout << "Couldn't write " << file.getFullPathName() << std::endl;
            return 1;
        }

        std::cout << "Saved " << file.getFullPathName() << std::endl;
        return 0;
    }

    int compare (const juce::StringArray& arguments)
    {
        juce::String error;
        const auto sampleFile = findFile (arguments[3]);
        const auto sample = analysis::readFile (sampleFile, error);

        if (! sample.has_value())
        {
            std::cout << error << std::endl;
            return 1;
        }

        SynthProcessor synth;

        if (! loadSound (synth, arguments[2]))
            return 1;

        const auto measured = analysis::analyse (*sample);
        const auto options = readOptions (arguments, 4);
        const auto note = options.note.value_or (measured.midiNote);
        const auto length = options.length.value_or (juce::jmax (0.2, measured.duration - measured.onset));
        const auto hold = options.hold.value_or (juce::jmin (length, juce::jmax (0.05, measured.end)));
        const auto played = analysis::renderNote (synth, note, options.velocity, hold, length);

        const auto out = options.out != juce::File() ? options.out
                                                     : findFile (arguments[2]).getSiblingFile (findFile (arguments[2]).getFileNameWithoutExtension()
                                                                                               + " (EZ Synth).wav");

        if (out.getParentDirectory().isDirectory() && analysis::writeWav (played, out))
            std::cout << "Played MIDI note " << note << " (" << juce::MidiMessage::getMidiNoteName (note, true, true, 4)
                      << ", the pitch set " << synth.getPitchSetName (synth.getPitchSetForNote (note)) << ")"
                      << " at velocity " << options.velocity << ", held " << juce::String (hold, 2)
                      << " s, " << juce::String (length, 2) << " s long, saved as " << out.getFullPathName() << "\n\n";

        // The attempt is measured at the same moments as the sample.
        std::vector<double> times;

        for (const auto& snapshot : measured.snapshots)
            times.push_back (snapshot.time);

        std::cout << analysis::compare (measured, analysis::analyse (played, times));
        return 0;
    }

    int describe (const juce::StringArray& arguments)
    {
        SynthProcessor synth;

        if (! loadSound (synth, arguments[2]))
            return 1;

        std::cout << sound::toJson (synth, true) << std::endl;
        return 0;
    }
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;
    juce::StringArray arguments;

    for (int i = 0; i < argc; ++i)
        arguments.add (juce::CharPointer_UTF8 (argv[i]));

    const auto command = arguments[1];

    if (command == "analyze" && arguments.size() >= 3)
        return analyze (arguments);

    if (command == "render" && arguments.size() >= 4)
        return render (arguments);

    if (command == "compare" && arguments.size() >= 4)
        return compare (arguments);

    if (command == "describe" && arguments.size() >= 3)
        return describe (arguments);

    if (command == "prompt")
    {
        std::cout << prompt::make() << std::endl;
        return 0;
    }

    std::cout << usage;
    return command.isEmpty() ? 0 : 1;
}
