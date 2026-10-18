#pragma once

#include "HarmonicVoice.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The synth. Up to three oscillators each play a drawn wave, rebuilt from its strongest harmonics, at the pitch
    of each note, and their sounds are added together.
*/
class SynthProcessor final : public juce::AudioProcessor
{
public:
    SynthProcessor();

    static constexpr int numOscillators = 3;

    static constexpr auto volumeId = "volume";

    /** The IDs of each oscillator's parameters: whether it plays, how loud, and with how many sine waves. */
    static juce::String onId (int oscillator);
    static juce::String levelId (int oscillator);
    static juce::String harmonicsId (int oscillator);

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
    // The oscillators' waves. These are for the message thread.

    const waveform::Shape& getShape (int oscillator) const noexcept;
    const waveform::Decomposition& getDecomposition (int oscillator) const noexcept;

    /** The preset the oscillator's wave is, or nothing if it was drawn. */
    std::optional<waveform::Preset> getPreset (int oscillator) const noexcept;

    void setShape (int oscillator, const waveform::Shape&, std::optional<waveform::Preset> fromPreset = std::nullopt);

    /** Goes up whenever any oscillator's wave changes, so the editor can tell when to redraw. */
    int getShapeVersion() const noexcept   { return shapeVersion; }

    bool isOn (int oscillator) const noexcept;

    /** How many sine waves the user asked to play the oscillator's wave with. */
    int getNumPartials (int oscillator) const noexcept;

    /** The note most recently started, or -1 if none has been. */
    int getLastNote() const noexcept   { return lastNote.load(); }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    /** The oscillator shown on the editor's grid. It's kept here so it's still shown when the editor reopens. */
    int editedOscillator = 0;

private:
    juce::AudioProcessorEditor* createEditor() override;
    void updateTargets();

    struct Oscillator
    {
        waveform::Shape shape {};
        std::optional<waveform::Preset> preset;
        waveform::Decomposition decomposition;
    };

    /** What an oscillator's parameters were set to, as the audio thread last read them. */
    struct Settings
    {
        bool on = false;
        float level = 0.0f;
        int numPartials = 0;

        // Any change at all, however small, means the targets need working out again.
        bool operator== (const Settings& other) const noexcept
        {
            return on == other.on && juce::exactlyEqual (level, other.level) && numPartials == other.numPartials;
        }
    };

    struct OscillatorParameters
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* numPartials = nullptr;
    };

    Settings readSettings (int oscillator) const noexcept;

    std::array<Oscillator, numOscillators> oscillators;
    int shapeVersion = 0;

    // The decompositions passed to the audio thread. The audio thread only ever tries the lock, so it never waits.
    juce::SpinLock sharedLock;
    std::array<waveform::Decomposition, numOscillators> sharedDecompositions;
    std::atomic<int> sharedVersion { 0 };

    // The audio thread's own copies, and the settings the voices' targets were worked out from.
    std::array<waveform::Decomposition, numOscillators> playingDecompositions;
    std::array<Settings, numOscillators> playingSettings;
    int playingVersion = -1;
    HarmonicTargets targets;

    std::array<OscillatorParameters, numOscillators> oscillatorParameters;
    std::atomic<float>* volume = nullptr;
    juce::SmoothedValue<float> gain;

    juce::Synthesiser synth;
    std::atomic<int> lastNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthProcessor)
};
