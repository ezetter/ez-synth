#pragma once

#include "HarmonicVoice.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The synth: plays the drawn wave, rebuilt from its strongest harmonics, at the pitch of each note. */
class SynthProcessor final : public juce::AudioProcessor
{
public:
    SynthProcessor();

    static constexpr auto volumeId = "volume";
    static constexpr auto harmonicsId = "harmonics";

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    bool hasEditor() const override   { return true; }
    const juce::String getName() const override;
    bool acceptsMidi() const override   { return true; }
    bool producesMidi() const override   { return false; }
    bool isMidiEffect() const override   { return false; }
    double getTailLengthSeconds() const override   { return HarmonicVoice::releaseSeconds; }

    int getNumPrograms() override   { return 1; }
    int getCurrentProgram() override   { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override   { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // The drawn wave. These are for the message thread.

    const waveform::Shape& getShape() const noexcept   { return shape; }
    const waveform::Decomposition& getDecomposition() const noexcept   { return decomposition; }

    /** The preset the wave is, or nothing if it was drawn. */
    std::optional<waveform::Preset> getPreset() const noexcept   { return preset; }

    void setShape (const waveform::Shape&, std::optional<waveform::Preset> fromPreset = std::nullopt);

    /** Goes up whenever the wave changes, so the editor can tell when to redraw. */
    int getShapeVersion() const noexcept   { return shapeVersion; }

    /** How many sine waves the user asked to play the wave with. */
    int getNumPartials() const noexcept;

    /** The note most recently started, or -1 if none has been. */
    int getLastNote() const noexcept   { return lastNote.load(); }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

private:
    juce::AudioProcessorEditor* createEditor() override;
    void updateTargets();

    waveform::Shape shape {};
    std::optional<waveform::Preset> preset;
    waveform::Decomposition decomposition;
    int shapeVersion = 0;

    // The decomposition passed to the audio thread. The audio thread only ever tries the lock, so it never waits.
    juce::SpinLock sharedLock;
    waveform::Decomposition sharedDecomposition;
    std::atomic<int> sharedVersion { 0 };

    // The audio thread's own copies.
    waveform::Decomposition playingDecomposition;
    int playingVersion = -1;
    int playingNumPartials = -1;
    HarmonicTargets targets;

    std::atomic<float>* volume = nullptr;
    std::atomic<float>* numPartials = nullptr;
    juce::SmoothedValue<float> gain;

    juce::Synthesiser synth;
    std::atomic<int> lastNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthProcessor)
};
