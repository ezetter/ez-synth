// Plays a preset through a short phrase and saves it as a WAV file, to hear a sound outside the app, such as while
// making a factory preset. Name a factory preset, or a saved preset's file, and the file to save:
//
//     ./build/EZSynthRenderPreset "Grand Piano" piano.wav
//
// The phrase climbs an arpeggio, holds a chord to let it ring, plays the same note softly and then hard, and then a
// high note and a low one.

#include "../Source/SynthProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    struct Note
    {
        double start, length;
        int number;
        float velocity;
    };

    const std::vector<Note> phrase {
        { 0.00, 0.6, 48, 0.75f }, { 0.35, 0.6, 55, 0.75f }, { 0.70, 0.6, 60, 0.75f },
        { 1.05, 0.6, 64, 0.75f }, { 1.40, 0.6, 67, 0.75f }, { 1.75, 0.8, 72, 0.75f },
        { 2.90, 3.0, 48, 0.8f }, { 2.90, 3.0, 60, 0.8f }, { 2.92, 3.0, 64, 0.75f }, { 2.94, 3.0, 67, 0.75f },
        { 6.60, 1.0, 69, 0.3f }, { 8.00, 1.0, 69, 1.0f },
        { 9.50, 2.0, 84, 0.8f }, { 9.50, 2.0, 33, 0.8f },
    };

    constexpr double length = 12.5;
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI initialiser;

    if (argc < 3)
    {
        std::cout << "Usage: EZSynthRenderPreset <factory preset name or .ezsynth file> <file.wav>" << std::endl;
        return 1;
    }

    SynthProcessor synth;
    const juce::String preset (argv[1]);
    const auto presetFile = juce::File::getCurrentWorkingDirectory().getChildFile (preset);

    if (! (presetFile.existsAsFile() ? synth.loadPreset (presetFile) : synth.loadFactoryPreset (preset)))
    {
        std::cout << "There's no preset called " << preset << std::endl;
        return 1;
    }

    synth.prepareToPlay (sampleRate, blockSize);
    const auto totalSamples = (int) (length * sampleRate);
    juce::AudioBuffer<float> sound (2, totalSamples);
    juce::AudioBuffer<float> block (2, blockSize);

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        juce::MidiBuffer midi;

        for (const auto& note : phrase)
        {
            if (const auto on = (int) (note.start * sampleRate) - start; on >= 0 && on < blockSize)
                midi.addEvent (juce::MidiMessage::noteOn (1, note.number, note.velocity), on);

            if (const auto off = (int) ((note.start + note.length) * sampleRate) - start; off >= 0 && off < blockSize)
                midi.addEvent (juce::MidiMessage::noteOff (1, note.number), off);
        }

        synth.processBlock (block, midi);
        const auto count = std::min (blockSize, totalSamples - start);

        for (int channel = 0; channel < 2; ++channel)
            sound.copyFrom (channel, start, block, channel, 0, count);
    }

    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
    file.deleteFile();
    auto stream = file.createOutputStream();

    if (stream == nullptr)
    {
        std::cout << "Couldn't write " << file.getFullPathName() << std::endl;
        return 1;
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> output (stream.release());
    const auto writer = wav.createWriterFor (output, juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                                                   .withNumChannels (2)
                                                                                   .withBitsPerSample (24));

    if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (sound, 0, totalSamples))
    {
        std::cout << "Couldn't write " << file.getFullPathName() << std::endl;
        return 1;
    }

    std::cout << "Saved " << file.getFullPathName() << ", peaking at "
              << juce::Decibels::gainToDecibels (sound.getMagnitude (0, totalSamples)) << " dB" << std::endl;
    return 0;
}
