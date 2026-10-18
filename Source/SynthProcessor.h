#pragma once

#include "HarmonicVoice.h"
#include "Modulation.h"
#include "Patch.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The synth. Up to three oscillators each play a drawn wave, rebuilt from its first harmonics, at the pitch of
    each note. Their sounds are added together, then go through a high pass and a low pass filter. Three LFOs, each
    with a drawn wave of its own, and three envelopes, which run with each note, can move any of the controls.
*/
class SynthProcessor final : public juce::AudioProcessor
{
public:
    SynthProcessor();

    static constexpr int numOscillators = modulation::numOscillators;
    static constexpr int numLfos = modulation::numLfos;
    static constexpr int numEnvelopes = modulation::numEnvelopes;

    /** The oscillators and LFOs, numbered with the oscillators first, are the sources of the sound and its movement.
        The editor shows one of them at a time.
    */
    static constexpr int numSources = numOscillators + numLfos;
    static constexpr bool isLfo (int source)   { return source >= numOscillators; }

    static constexpr auto volumeId = "volume";

    /** The IDs of each oscillator's parameters: whether it plays, how loud, and with how many harmonics. */
    static juce::String onId (int oscillator);
    static juce::String levelId (int oscillator);
    static juce::String harmonicsId (int oscillator);

    /** The IDs of a filter's parameters: whether it's on, its cutoff frequency in hertz, and its resonance. */
    struct FilterIds
    {
        const char* on;
        const char* cutoff;
        const char* resonance;
    };

    static constexpr FilterIds lowPassIds { "lowPassOn", "lowPassCutoff", "lowPassResonance" };
    static constexpr FilterIds highPassIds { "highPassOn", "highPassCutoff", "highPassResonance" };

    /** The IDs of each LFO's parameters: whether it's on, its period in seconds, and whether it loops or runs once. */
    static juce::String lfoOnId (int lfo);
    static juce::String lfoPeriodId (int lfo);
    static juce::String lfoModeId (int lfo);

    /** The IDs of each envelope's parameters: its attack, decay and release in seconds, and its sustain level. */
    static juce::String attackId (int envelope);
    static juce::String decayId (int envelope);
    static juce::String sustainId (int envelope);
    static juce::String releaseId (int envelope);

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
    double getTailLengthSeconds() const override;

    int getNumPrograms() override   { return 1; }
    int getCurrentProgram() override   { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override   { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // The oscillators' waves. These, and everything below, are for the message thread.

    const waveform::Shape& getShape (int oscillator) const noexcept;
    const waveform::Decomposition& getDecomposition (int oscillator) const noexcept;

    /** The preset the oscillator's wave is, or nothing if it was drawn. */
    std::optional<waveform::Preset> getPreset (int oscillator) const noexcept;

    void setShape (int oscillator, const waveform::Shape&, std::optional<waveform::Preset> fromPreset = std::nullopt);

    /** Raises or lowers one harmonic of an oscillator's wave, numbered from 1, leaving the others as they are. */
    void setHarmonicAmplitude (int oscillator, int harmonic, float amplitude);

    /** Goes up whenever any oscillator's or LFO's wave changes, so the editor can tell when to redraw. */
    int getShapeVersion() const noexcept   { return shapeVersion; }

    bool isOn (int oscillator) const noexcept;

    /** How many harmonics the oscillator plays its wave with, from the 1st up. */
    int getNumHarmonics (int oscillator) const noexcept;

    //==============================================================================
    // The LFOs. An LFO plays its wave just as it's drawn, rather than rebuilt from harmonics.

    const waveform::Shape& getLfoShape (int lfo) const noexcept;
    std::optional<waveform::Preset> getLfoPreset (int lfo) const noexcept;
    void setLfoShape (int lfo, const waveform::Shape&, std::optional<waveform::Preset> fromPreset = std::nullopt);

    bool isLfoOn (int lfo) const noexcept;
    float getLfoPeriod (int lfo) const noexcept;
    bool isLfoOnce (int lfo) const noexcept;

    /** How far through its periods an LFO is, as last played. One that runs once counts from the last key pressed,
        and has no progress before any key has been.
    */
    std::optional<double> getLfoProgress (int lfo) const noexcept;

    /** Where the LFO's wave is now, from -1 to 1. */
    float getLfoValue (int lfo) const;

    //==============================================================================
    // The envelopes, which run with each note.

    EnvelopeSettings getEnvelopeSettings (int envelope) const noexcept;

    /** Where the newest note is in an envelope. */
    struct EnvelopeDisplay
    {
        EnvelopeRun::Stage stage = EnvelopeRun::Stage::idle;
        float progress = 0.0f;   // how far through the stage, from 0 to 1
        float level = 0.0f;
    };

    /** Where the newest note that's still sounding is in an envelope, or nothing if no note is. */
    std::optional<EnvelopeDisplay> getEnvelopeDisplay (int envelope) const noexcept;

    //==============================================================================
    // What the LFOs and envelopes move.

    const modulation::Assignment& getAssignment (int target) const noexcept;

    /** Has an LFO or envelope move a control, or stops one moving it. Assigning an LFO that's off switches it on. */
    void setAssignment (int target, const modulation::Assignment&);

    /** An LFO's or envelope's value now, as the newest note has it, or nothing if it isn't moving anything: an
        LFO that's off, or that runs once before any key has been pressed, or an envelope when no note is sounding.
    */
    std::optional<float> getSourceValue (int source) const;

    /** Goes up whenever an assignment changes. */
    int getModulationVersion() const noexcept   { return modulationVersion; }

    /** Where a control is set, from 0 to 1 along its travel, before any LFO moves it. */
    float getPosition (int target) const;

    //==============================================================================
    /** The note most recently started, or -1 if none has been. */
    int getLastNote() const noexcept   { return lastNote.load(); }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    /** The oscillator or LFO shown on the editor's grid. It's kept here so it's still shown when the editor reopens. */
    int editedSource = 0;

private:
    juce::AudioProcessorEditor* createEditor() override;

    struct Oscillator
    {
        waveform::Shape shape {};
        std::optional<waveform::Preset> preset;
        waveform::Decomposition decomposition;

        /** Each harmonic's phase the last time it was heard, for when it's raised again from nothing. */
        std::array<float, waveform::maxHarmonics> phases {};
    };

    struct Lfo
    {
        waveform::Shape shape {};
        std::optional<waveform::Preset> preset;
    };

    /** What the message thread hands to the audio thread: everything the voices need that isn't a parameter. */
    struct Shared
    {
        std::array<waveform::Decomposition, numOscillators> decompositions;
        std::array<std::array<float, waveform::maxHarmonics>, numOscillators> phases {};
        std::array<waveform::Shape, numLfos> lfoShapes {};
        modulation::Assignments assignments;
    };

    struct OscillatorParameters
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* numHarmonics = nullptr;
    };

    struct FilterParameters
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
    };

    struct LfoParameters
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* period = nullptr;
        std::atomic<float>* mode = nullptr;
    };

    struct EnvelopeParameters
    {
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
    };

    /** Where the newest note is in each envelope, for the window to show. */
    struct SharedEnvelopeDisplay
    {
        std::atomic<int> stage { 0 };
        std::atomic<float> progress { 0.0f };
        std::atomic<float> level { 0.0f };
    };

    void shareEnvelopeDisplay();

    /** Further from zero than drawing and raising harmonics can take a wave. Loading a saved wave keeps it within
        this, in case the save is corrupt.
    */
    static constexpr float maxValue = 32.0f;

    /** Sets an oscillator's wave, remembering the phase of each harmonic it has. */
    void replaceShape (int oscillator, const waveform::Shape&, std::optional<waveform::Preset>);

    /** Hands the waves and assignments to the audio thread. */
    void share();

    /** On the audio thread: takes what the message thread shared, if it's changed, and reads the parameters. */
    void updatePatch();

    std::array<Oscillator, numOscillators> oscillators;
    std::array<Lfo, numLfos> lfos;
    modulation::Assignments assignments;
    int shapeVersion = 0;
    int modulationVersion = 0;

    // The audio thread only ever tries the lock, so it never waits.
    juce::SpinLock sharedLock;
    Shared shared;
    std::atomic<int> sharedVersion { 0 };

    // The audio thread's own copies. The sample rate is kept from prepareToPlay, as not every host sets the one
    // getSampleRate() returns.
    double playingSampleRate = 44100.0;
    Shared received;
    int receivedVersion = -1;
    Patch patch;

    std::array<OscillatorParameters, numOscillators> oscillatorParameters;
    std::array<FilterParameters, modulation::numFilters> filterParameters;
    std::array<LfoParameters, numLfos> lfoParameters;
    std::array<EnvelopeParameters, numEnvelopes> envelopeParameters;
    std::atomic<float>* volume = nullptr;

    std::array<SharedEnvelopeDisplay, numEnvelopes> envelopeDisplays;
    std::atomic<bool> noteSounding { false };
    std::vector<HarmonicVoice*> voices;

    // Where each looping LFO is, which the audio thread moves on with every block, and how far each LFO has got,
    // for the editor to show.
    std::array<double, numLfos> lfoPhases {};
    std::array<std::atomic<double>, numLfos> lfoProgress {};
    juce::int64 samplesPlayed = 0;
    juce::int64 lastNoteOnSample = -1;

    juce::Synthesiser synth;
    std::atomic<int> lastNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthProcessor)
};
