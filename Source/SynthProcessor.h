#pragma once

#include "HarmonicVoice.h"
#include "Modulation.h"
#include "Patch.h"
#include "Tuning.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** The synth. Up to three oscillators each play a drawn wave, rebuilt from its first harmonics, at the pitch of
    each note. Their sounds are added together, then go through a high pass and a low pass filter. Three LFOs, each
    with a drawn wave of its own, and three envelopes, which run with each note, can move any of the controls.
*/
class SynthProcessor final : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::Timer
{
public:
    SynthProcessor();
    ~SynthProcessor() override;

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
    static juce::String tuneId (int oscillator);

    /** The ID of how far an oscillator's tune slider is zoomed in, which sets how far LFOs, envelopes, velocity and the
        mod wheel move its tune: at their full range, across the part of the scale it shows.
    */
    static juce::String tuneZoomId (int oscillator);

    /** The IDs of whether an oscillator glides into each note from the one before, always or only legato, and how
        long its glide takes, in seconds.
    */
    static juce::String glideId (int oscillator);
    static juce::String glideTimeId (int oscillator);
    static juce::StringArray getGlideNames();
    static juce::NormalisableRange<float> glideTimeRange();

    /** How long a glide takes, such as "40 ms" or "1.50 s". */
    static juce::String glideTimeToText (float seconds);

    /** The IDs of a filter's parameters: whether it's on, its type, its cutoff frequency in hertz, its resonance, as a
        low or high pass, its width in octaves, as a mid pass, and its Q and depth in decibels, as a notch.
    */
    struct FilterIds
    {
        juce::String on, type, cutoff, resonance, width, q, depth;
    };

    /** The IDs of a filter's parameters, numbered from 0. The first starts as a low pass, the second a high pass. */
    static FilterIds filterIds (int filter);

    /** The IDs of each LFO's parameters: whether it's on, its period in seconds, which is shown as its rate, in
        hertz, whether it loops or starts with each key, and how many times it plays then, and whether it's synced
        to the host's tempo, and if so, how many cycles it runs a beat.
    */
    static juce::String lfoOnId (int lfo);
    static juce::String lfoPeriodId (int lfo);
    static juce::String lfoModeId (int lfo);
    static juce::String lfoRandomId (int lfo);
    static juce::String lfoSyncId (int lfo);
    static juce::String lfoBeatRateId (int lfo);

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

    /** How far a harmonic, numbered from 2, is tuned from its whole number, in harmonics, from the one below to
        the one above. The fundamental can't be tuned. Tuning harmonics makes the sound
        inharmonic, as a bell's or a drum's is. Changing the wave keeps them tuned.
    */
    float getHarmonicTune (int oscillator, int harmonic) const noexcept;
    void setHarmonicTune (int oscillator, int harmonic, float tune);

    /** A harmonic's phase, in radians from 0 up to a whole turn: where its sine wave starts in the cycle. Setting it
        turns that harmonic's sine wave within the wave, leaving the others as they are, so the wave becomes Custom.
        A harmonic that's silent keeps the phase it's given, and comes in at it when it's raised.
    */
    float getHarmonicPhase (int oscillator, int harmonic) const noexcept;
    void setHarmonicPhase (int oscillator, int harmonic, float phase);

    /** Whether a harmonic, numbered from 1, plays as the number of harmonics says, or is switched off, so it stays
        off as the number goes past it, or switched on above the number, so it stays on as the number falls below it.
    */
    waveform::HarmonicSwitch getHarmonicSwitch (int oscillator, int harmonic) const noexcept;
    void setHarmonicSwitch (int oscillator, int harmonic, waveform::HarmonicSwitch);

    /** Whether a harmonic plays, with the number of harmonics as it's set. */
    bool isHarmonicPlayed (int oscillator, int harmonic) const noexcept;

    /** Switches a harmonic off if it plays, and on if it doesn't: back to following the number of harmonics if it's
        within it, and switched on if it's above it.
    */
    void toggleHarmonic (int oscillator, int harmonic);

    /** What clicking a harmonic's box does, whatever the number of harmonics: a harmonic following the number is
        switched off, one switched off is switched on, and one switched on goes back to following the number.
    */
    void cycleHarmonicSwitch (int oscillator, int harmonic);

    /** Gives every one of an oscillator's 16 harmonics a random amplitude and phase, making a new Custom wave. The
        amplitudes are spread evenly along their sliders' travel, so weak harmonics are as likely as along the
        sliders. A wave that would reach beyond full scale is turned down to fit the grid. The harmonics' tunings
        are left as they are.
    */
    void randomizeHarmonics (int oscillator, juce::Random&);

    /** Randomizes the harmonics of each oscillator whose randomizing has been triggered, by its LFO, envelope,
        velocity or the mod wheel crossing its threshold, since this was last called, and switches each harmonic
        triggered on or off. The audio thread asks, and the message thread does it, a few dozen times a second.
    */
    void randomizeAsTriggered();

    /** How many times an oscillator's randomizing has been triggered and carried out. */
    int getRandomizeCount (int oscillator) const noexcept   { return randomizeCounts[(size_t) oscillator]; }

    //==============================================================================
    // MIDI learn: linking a control to a knob or slider on a MIDI controller, which then sets it, across its whole
    // travel from 0 to 127. Each MIDI controller number sets one control. The links belong to this copy of the synth,
    // saved with a host's project but not with presets, so they stay as presets are loaded and changes undone.

    /** The MIDI controller numbers that can be learned: all but the channel mode messages, 120 up. */
    static constexpr int numLearnableControllers = 120;

    /** Waits for the next MIDI controller moved, and links it to a control, in place of any other it set. */
    void startMidiLearn (int target);
    void stopMidiLearn();
    std::optional<int> getMidiLearnTarget() const noexcept;

    /** The MIDI controller number linked to a control, if any. */
    std::optional<int> getMidiController (int target) const noexcept;
    void setMidiController (int target, std::optional<int> controller);

    /** For a control that's triggered, such as randomizing or a harmonic's switch, where across the controller's
        travel, from 0 to 1, turning it past triggers the control, either way: right up to start with. As with a
        modulator's threshold, 0 counts only turning it right down, and 1 only turning it right up.
    */
    float getMidiThreshold (int target) const noexcept   { return midiThresholds[(size_t) target]; }
    void setMidiThreshold (int target, float threshold);

    /** Goes up whenever a link is learned or forgotten, or learning starts or stops, for the window to show it. */
    int getMidiLearnVersion() const noexcept   { return midiLearnVersion; }

    /** Sets each linked control from where its MIDI controller has been moved to, since this was last called, and
        links the control being learned to the first controller moved. The message thread does this a few dozen
        times a second.
    */
    void applyMidiControllers();

    /** Goes up whenever any oscillator's or LFO's wave changes, so the editor can tell when to redraw. */
    int getShapeVersion() const noexcept   { return shapeVersion; }

    bool isOn (int oscillator) const noexcept;

    /** How many harmonics the oscillator plays its wave with, from the 1st up. */
    int getNumHarmonics (int oscillator) const noexcept;

    /** How many semitones away from the note the oscillator plays, from an octave down to an octave up. */
    float getTune (int oscillator) const noexcept;

    //==============================================================================
    // The LFOs. An LFO plays its wave just as it's drawn, rather than rebuilt from harmonics.

    const waveform::Shape& getLfoShape (int lfo) const noexcept;
    std::optional<waveform::Preset> getLfoPreset (int lfo) const noexcept;
    void setLfoShape (int lfo, const waveform::Shape&, std::optional<waveform::Preset> fromPreset = std::nullopt);

    bool isLfoOn (int lfo) const noexcept;
    float getLfoPeriod (int lfo) const noexcept;
    /** Whether an LFO's rate is synced to the host's tempo, running a number of cycles a beat rather than as its rate
        dial says in hertz. A looping LFO synced to the tempo follows the host's beat while it plays, so its periods
        start on the beat. Nothing moves a synced LFO's rate.
    */
    bool isLfoSynced (int lfo) const noexcept;

    /** How many cycles a beat a synced LFO runs, and so how many beats each of its periods lasts. */
    double getLfoCyclesPerBeat (int lfo) const noexcept;
    double getLfoSyncedBeats (int lfo) const noexcept   { return 1.0 / getLfoCyclesPerBeat (lfo); }

    /** The tempo synced LFOs follow, in beats a minute: the host's, or 120 if it doesn't give one, as last played. */
    double getTempo() const noexcept   { return hostTempo.load(); }
    bool isTempoFromHost() const noexcept   { return hostGivesTempo.load(); }

    /** How the LFO runs: modulation::lfoLoops, a number of times from each key, or modulation::lfoForever. */
    int getLfoMode (int lfo) const noexcept;

    /** Whether the LFO starts with each key, rather than looping all the time. */
    bool isLfoFromKey (int lfo) const noexcept;

    /** How many times an LFO that starts with each key plays its wave, or 0 for forever. */
    int getLfoTimes (int lfo) const noexcept;

    /** Whether an LFO that plays its wave a number of times from each key is on its last, as last played, so its
        wave holds its end rather than heading back to its start.
    */
    bool isLfoOnLastTime (int lfo) const noexcept;

    /** How far through its periods an LFO is, as last played. One that starts with each key counts from the last key
        pressed, and has no progress before any key has been.
    */
    std::optional<double> getLfoProgress (int lfo) const noexcept;

    /** Where the LFO's wave is now, from -1 to 1, or where it was the given proportion of a period ago, for a control
        it's moving with a phase.
    */
    float getLfoValue (int lfo, float phase = 0.0f) const;

    /** Whether the LFO plays a new random wave every cycle, starting anew with each key if it starts with each
        key, rather than its drawn wave.
    */
    bool isLfoRandom (int lfo) const noexcept;

    /** The random wave an LFO is playing: its seed, and the cycle it's in. A looping LFO's is its own seed, and the
        cycle it's in now. One that starts with each key plays the newest note's seed, from its first cycle.
    */
    struct RandomCycle
    {
        std::uint32_t seed = 0;
        std::int64_t cycle = 0;

        bool operator== (const RandomCycle&) const = default;
    };

    RandomCycle getRandomCycle (int lfo) const noexcept;

    /** The wave an LFO is playing: the one drawn, or this cycle of its random wave. */
    waveform::Shape getPlayedLfoShape (int lfo) const;

    /** Where a looping LFO's random wave starts its next cycle, which is where this one ends, or nothing for a wave
        that ends where it began.
    */
    std::optional<float> getPlayedLfoEnd (int lfo) const;

    //==============================================================================
    // The envelopes, which run with each note.

    EnvelopeSettings getEnvelopeSettings (int envelope) const noexcept;

    /** Where the newest note is in an envelope. */
    struct EnvelopeDisplay
    {
        EnvelopeRun::Stage stage = EnvelopeRun::Stage::idle;
        float progress = 0.0f;   // how far through the stage, from 0 to 1
        float level = 0.0f;
        float releaseFrom = 0.0f;   // the level it was at when the key was let go

        /** Where the envelope is, for working out where it moves a control. */
        modulation::EnvelopeState getState() const noexcept
        {
            const auto releasing = stage == EnvelopeRun::Stage::release;
            return { level, releasing, releaseFrom, releasing ? 1.0f - progress : 1.0f };
        }
    };

    /** Where the newest note that's still sounding is in an envelope, or nothing if no note is. */
    std::optional<EnvelopeDisplay> getEnvelopeDisplay (int envelope) const noexcept;

    //==============================================================================
    // What the LFOs and envelopes move.

    const modulation::Assignment& getAssignment (int target) const noexcept;

    /** Has an LFO or envelope move a control, or stops one moving it. Assigning an LFO that's off switches it on.
        Only an LFO's phase counts. An assignment that would have an LFO move its own rate, even through other LFOs,
        is refused.
    */
    void setAssignment (int target, const modulation::Assignment&);

    /** Whether a source moving a control would have an LFO move its own rate, directly or through other LFOs. */
    bool wouldMakeLoop (int target, int source) const;

    /** What the key velocity moves. Velocity can move a control as well as an LFO or envelope does. It always moves
        a control down for softer notes, so an assignment's polarity is ignored.
    */
    const modulation::Assignment& getVelocityAssignment (int target) const noexcept;
    void setVelocityAssignment (int target, const modulation::Assignment&);

    /** What the mod wheel moves. At the bottom it leaves a control where it's set, and as it's pushed up it moves
        the control up, or down if inverted, by as much as its range at the top. It has no phase. It can also move
        each LFO's rate, faster as it's pushed up, or slower if inverted, for every note alike.
    */
    const modulation::Assignment& getModWheelAssignment (int target) const noexcept;
    void setModWheelAssignment (int target, const modulation::Assignment&);

    /** Where the mod wheel is, from 0 to 1, as last played. */
    float getModWheel() const noexcept   { return modWheelShown.load(); }

    /** An LFO's, envelope's or velocity's value now, as the newest note has it, or nothing if it isn't moving
        anything: an LFO that's off, or that starts with each key before any key has been pressed, or an envelope or velocity
        when no note is sounding. An LFO's value is taken the given phase behind, as a control assigned it with that
        phase has it.
    */
    std::optional<float> getSourceValue (int source, float phase = 0.0f) const;

    /** Where the LFO, envelope, velocity and mod wheel moving a control have moved it, from 0 to 1 along its
        travel, as the newest note has it, or nothing if none of them is moving it now.
    */
    std::optional<float> getLivePosition (int target) const;

    /** An oscillator's harmonics as the newest note plays them, from the 1st up to as many as it's playing, with
        whatever the LFOs, envelopes and velocity have done to them and to how many there are.
    */
    struct LiveWave
    {
        std::vector<waveform::Partial> harmonics;
        bool moved = false;   // whether anything is moving them, rather than them being as set

        bool operator== (const LiveWave&) const noexcept;
    };

    LiveWave getLiveWave (int oscillator) const;

    /** Goes up whenever an assignment changes. */
    int getModulationVersion() const noexcept   { return modulationVersion; }

    /** Where a control is set, from 0 to 1 along its travel, before any LFO moves it. */
    float getPosition (int target) const;

    //==============================================================================
    // Presets: the whole sound, saved in a file. These are for the message thread.

    static constexpr auto presetExtension = ".ezsynth";
    static constexpr auto initPresetName = "Init";

    /** Where presets are kept: ~/Documents/EZ Synth/Presets. */
    static juce::File getPresetFolder();

    /** The presets in a folder, in order of name, numbers counted as numbers. */
    static juce::Array<juce::File> findPresets (const juce::File& folder = getPresetFolder());

    /** Saves the whole sound as a preset named after the file, which is given the preset extension if it hasn't
        got it. Returns false if the file couldn't be written.
    */
    bool savePreset (const juce::File&);

    /** Loads a preset. Returns false, and leaves the sound alone, if the file isn't one. */
    bool loadPreset (const juce::File&);

    /** Goes back to the sound the synth starts with. */
    void loadInitPreset();

    /** The presets that come with EZ Synth, which are built into it, in order of name. */
    static juce::StringArray getFactoryPresetNames();

    /** Loads one of the factory presets. Returns false, and leaves the sound alone, if there's none by that name. */
    bool loadFactoryPreset (const juce::String& name);

    /** Whether the preset last loaded is a factory preset. A factory preset has no file, so it can't be saved over. */
    bool isFactoryPreset() const noexcept   { return factoryPreset; }

    /** The name of the preset last loaded or saved, or Init. */
    juce::String getPresetName() const   { return presetName; }

    /** The file of the preset last loaded or saved, or none for Init or a factory preset. */
    juce::File getPresetFile() const   { return presetFile; }

    /** Whether the sound has been changed since its preset was loaded or saved. */
    bool isPresetEdited() const noexcept   { return presetEdited; }

    /** Goes up whenever a preset is loaded or saved. */
    int getPresetVersion() const noexcept   { return presetVersion; }

    //==============================================================================
    // Undo and redo, of the whole sound.

    /** Goes up with every change to the sound, from the window, a preset or the host. */
    int getChangeCount() const noexcept   { return changeCount.load(); }

    /** Remembers the sound as it is now as the next step that can be undone, after whatever was undone, if it's
        changed since the last. The window calls this after each change the user makes.
    */
    void rememberForUndo();

    /** Takes the sound as it is now as the step it's on, without adding a step: for changes the user didn't make,
        such as a host's automation, so undoing doesn't step through them.
    */
    void updateUndoStep();

    /** Goes back to the sound before the last change, or on to the next one undone, if there is one. */
    bool undo();
    bool redo();
    bool canUndo() const noexcept   { return undoStep > 0; }
    bool canRedo() const noexcept   { return undoStep + 1 < undoHistory.size(); }

    /** How many steps the history keeps. */
    static constexpr size_t mostUndoSteps = 200;

    //==============================================================================
    /** The note most recently started, or -1 if none has been. */
    int getLastNote() const noexcept   { return lastNote.load(); }

    /** Goes up with each block that brings notes, controllers or the pitch wheel from a MIDI device or the host, but
        not from the on-screen keyboard, so the window can show MIDI arriving.
    */
    int getMidiReceived() const noexcept   { return midiReceived.load(); }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

    /** The oscillator or LFO shown on the editor's grid. It's kept here so it's still shown when the editor reopens. */
    int editedSource = 0;

    /** How each source's wave is drawn on the grid, smoothly or in steps, numbered as the sources are: the
        oscillators, then the LFOs. It doesn't change the sound, but it's saved with it, so a wave drawn in steps
        opens in steps.
    */
    const waveform::Drawing& getDrawing (int source) const noexcept   { return drawings[(size_t) source]; }
    void setDrawing (int source, const waveform::Drawing&);

private:
    juce::AudioProcessorEditor* createEditor() override;
    void parameterChanged (const juce::String& parameterId, float newValue) override;

    /** The whole sound, and the preset it came from, as saved with a host's project or in a preset file. */
    juce::ValueTree createState();
    void loadState (juce::ValueTree);

    /** Loads a preset's XML under a name. Returns false, and leaves the sound alone, if it isn't a preset. */
    bool loadPresetXml (const juce::XmlElement*, const juce::String& name, bool isFactory);

    struct Oscillator
    {
        waveform::Shape shape {};
        std::optional<waveform::Preset> preset;
        waveform::Decomposition decomposition;

        /** Each harmonic's phase the last time it was heard, for when it's raised again from nothing. */
        std::array<float, waveform::maxHarmonics> phases {};

        /** How far each harmonic, from the 1st up, is tuned from its whole number. The 1st never is. */
        std::array<float, waveform::maxHarmonics> tunes {};

        /** Whether each harmonic plays as the number of harmonics says, or is switched off, or on. */
        waveform::HarmonicSwitches switches {};
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
        std::array<std::array<float, waveform::maxHarmonics>, numOscillators> phases {}, tunes {};
        std::array<waveform::HarmonicSet, numOscillators> switches {};
        std::array<std::array<float, waveform::maxHarmonics + 1>, numOscillators> peaks {};
        std::array<waveform::Shape, numLfos> lfoShapes {};
        std::array<waveform::Drawing, numLfos> lfoDrawings {};
        modulation::Assignments assignments, velocityAssignments, modWheelAssignments;
    };

    struct OscillatorParameters
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* numHarmonics = nullptr;
        std::atomic<float>* tune = nullptr;
        std::atomic<float>* tuneZoom = nullptr;
        std::atomic<float>* glide = nullptr;
        std::atomic<float>* glideTime = nullptr;
    };

    struct FilterParameters
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* width = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* depth = nullptr;
    };

    struct LfoParameters
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* period = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* random = nullptr;
        std::atomic<float>* sync = nullptr;
        std::atomic<float>* beatRate = nullptr;
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
        std::atomic<float> releaseFrom { 0.0f };
    };

    /** Shares where the newest note still sounding is in each envelope, and how hard it was played. */
    void shareNewestNote();

    /** On the audio thread, after each block: watches what's given each oscillator's randomizing, and asks for it
        whenever one crosses its threshold.
    */
    void watchRandomizeTriggers (int numSamples);

    /** A source's value at the end of the block, on the audio thread: an LFO's from where it's got to, shifted
        back by the phase given, or as it was the periods given earlier in the block, the newest note's envelope, if
        a note's sounding, and the last note's velocity.
    */
    std::optional<float> getSourceValueNow (int source, double phase, double periodsEarlier = 0.0) const;

    void timerCallback() override;

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
    modulation::Assignments assignments, velocityAssignments, modWheelAssignments;
    int shapeVersion = 0;
    int modulationVersion = 0;

    juce::String presetName { initPresetName };
    juce::File presetFile;
    bool factoryPreset = false;
    std::atomic<bool> presetEdited { false };   // set from whichever thread changes a parameter
    std::atomic<int> changeCount { 0 };

    /** Notes that the sound has been changed, so it's no longer the preset's, and there's something to undo. */
    void markEdited() noexcept
    {
        presetEdited = true;
        ++changeCount;
    }

    std::vector<juce::ValueTree> undoHistory;
    size_t undoStep = 0;
    int presetVersion = 0;
    juce::ValueTree initState;

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

    // The controls that are triggered: each oscillator's randomizing, then each of its harmonics' switches.
    static constexpr int firstTrigger = modulation::randomizeTarget (0);
    static constexpr int numTriggers = modulation::numTargets - firstTrigger;

    // For each trigger, what's given it, as moving, by velocity and by the mod wheel, each watched for crossing its
    // threshold. The audio thread counts up the times it's asked for, the message thread how many it's carried out.
    std::array<std::array<modulation::ThresholdCrossing, 3>, numTriggers> triggerCrossings {};
    std::array<std::atomic<int>, numTriggers> triggerRequests {};
    std::array<int, numTriggers> triggerRequestsDone {};
    std::array<int, numOscillators> randomizeCounts {};
    juce::Random randomizer;

    /** Does what a trigger does: randomizes an oscillator's harmonics, or switches a harmonic on or off. */
    void carryOutTrigger (int target);

    // MIDI learn. The audio thread keeps where each controller was last put and counts its moves, and notes the
    // first moved while a control's being learned; the message thread links and sets the controls.
    std::array<std::atomic<int>, numLearnableControllers> controllerValues {}, controllerMoves {};
    std::atomic<int> learningTarget { -1 }, learnedController { -1 };
    std::array<int, modulation::numTargets> targetControllers {}, appliedMoves {};
    std::array<float, modulation::numTargets> midiThresholds {};
    std::array<modulation::ThresholdCrossing, numTriggers> controllerTriggerCrossings {};
    int midiLearnVersion = 0;

    /** Sets a control to a position along its travel, from 0 to 1, as a MIDI controller does. */
    void setTargetPosition (int target, float position);
    std::atomic<bool> noteSounding { false };
    std::atomic<float> newestVelocity { 1.0f };
    std::atomic<float> modWheelShown { 0.0f };

    // The tempo synced LFOs follow, and where the host is, in beats, while it plays.
    double playingTempo = modulation::defaultTempo;
    std::optional<double> playingBeat;
    std::atomic<double> hostTempo { modulation::defaultTempo };
    std::atomic<bool> hostGivesTempo { false };
    std::vector<HarmonicVoice*> voices;

    // Where each looping LFO is, which the audio thread moves on with every block, and how far each LFO has got,
    // for the editor to show.
    std::array<double, numLfos> lfoPhases {};

    // How many periods each looping LFO has been through, which picks each cycle of a random wave, the seed each
    // plays its random wave from, and for the window, the cycle each is in and the newest note's seed for those
    // that run once.
    std::array<std::int64_t, numLfos> lfoCycles {};
    std::array<std::uint32_t, numLfos> randomSeeds {};
    std::array<std::atomic<std::int64_t>, numLfos> lfoCyclesShown {};
    std::array<std::atomic<std::uint32_t>, numLfos> newestOnceSeeds {};
    std::array<std::atomic<double>, numLfos> lfoProgress {};
    std::array<double, numLfos> periodsSinceLastNote {};
    std::array<double, numLfos> newestLfoPeriods {};
    juce::int64 samplesPlayed = 0;
    juce::int64 lastNoteOnSample = -1;

    /** A synthesiser that keeps where the mod wheel is in the patch, as each message comes, for the voices. */
    struct ModWheelSynthesiser final : public juce::Synthesiser
    {
        explicit ModWheelSynthesiser (Patch& patchToMove) : patch (patchToMove) {}

        void handleController (int midiChannel, int controllerNumber, int controllerValue) override
        {
            if (controllerNumber == modulation::modWheelController)
                patch.modWheel = (float) controllerValue / 127.0f;

            juce::Synthesiser::handleController (midiChannel, controllerNumber, controllerValue);
        }

        /** Tells the note starting where each oscillator of the note played before had glided to, and whether another
            key's held, for its oscillators that glide, then notes which voice plays it, for the next note.
        */
        void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
        {
            patch.noteStart.legato = false;

            for (auto* voice : voices)
                if (voice->isKeyDown() && voice->getCurrentlyPlayingNote() != midiNoteNumber)
                    patch.noteStart.legato = true;

            for (size_t i = 0; i < patch.noteStart.glideFrom.size(); ++i)
                patch.noteStart.glideFrom[i] = lastVoice != nullptr ? std::optional (lastVoice->getGlidingPitch ((int) i)) : std::nullopt;

            juce::Synthesiser::noteOn (midiChannel, midiNoteNumber, velocity);

            for (auto* voice : voices)
                if (auto* harmonicVoice = dynamic_cast<HarmonicVoice*> (voice);
                    harmonicVoice != nullptr && harmonicVoice->isKeyDown() && harmonicVoice->getCurrentlyPlayingNote() == midiNoteNumber
                    && harmonicVoice->getSamplesSinceNoteOn() == 0)
                    lastVoice = harmonicVoice;
        }

        Patch& patch;
        const HarmonicVoice* lastVoice = nullptr;
    };

    ModWheelSynthesiser synth { patch };
    std::atomic<int> lastNote { -1 };
    std::atomic<int> midiReceived { 0 };
    std::array<waveform::Drawing, numSources> drawings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthProcessor)
};
