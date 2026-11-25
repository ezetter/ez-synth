#pragma once

#include "HarmonicVoice.h"
#include "Modulation.h"
#include "Patch.h"
#include "Reverb.h"
#include "Tuning.h"
#include "Waveform.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

/** The synth. Up to six oscillators each play a drawn wave, rebuilt from its first harmonics, at the pitch of
    each note. Their sounds are added together, then go through the filters. Eight LFOs, each with a drawn wave of
    its own, and eight envelopes, which run with each note, can move any of the controls. Every setting can be set
    differently for ranges of keys, in pitch sets.
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

    /** The ID of whether an oscillator plays white noise rather than its harmonics. */
    static juce::String noiseId (int oscillator);

    /** The ID of where an oscillator is panned, from 0, all the way left, to 1, all the way right. In the middle,
        where it starts, it's 50% left and 50% right, and plays at its full level on both sides.
    */
    static juce::String panId (int oscillator);

    /** The ID of whether an oscillator's sent through a filter, both numbered from 0, as they all are to begin with.
        One that isn't skips that filter, going straight on to the next in the chain.
    */
    static juce::String filterSendId (int oscillator, int filter);

    /** The ID of whether an oscillator's sent through an effect, numbered from 0, once it's filtered, which none is
        to begin with. One sent through none plays straight out. For now the only effect is the reverb.
    */
    static juce::String effectSendId (int oscillator, int effect);

    /** The IDs of the reverb's settings: its pre-delay, from 0 to 200 ms, the room's size, from 1 to 100, how long
        its tail takes to fall 60 dB, from 0.1 to 20 seconds, and how much of what comes out of it is the reverb,
        the rest being what was sent through it. The reverb is one for the whole synth, so every pitch set plays
        through it as it's set.
    */
    static constexpr auto reverbDelayId = "reverbDelay";
    static constexpr auto reverbSizeId = "reverbSize";
    static constexpr auto reverbDecayId = "reverbDecay";
    static constexpr auto reverbMixId = "reverbMix";
    static juce::StringArray getReverbIds()   { return { reverbDelayId, reverbSizeId, reverbDecayId, reverbMixId }; }

    /** The names of the effects, for the routing screen and the window. */
    static juce::String getEffectName (int effect);

    /** Where a pan sends the sound, as its shares either way, such as "L 50% · R 50%", "L 25% · R 75%" or "L 100%". */
    static juce::String panToText (float pan);

    /** The IDs of an oscillator's unison: how many voices of it play, stacked, from 1, as it starts, to 8, how far the
        outermost are detuned from its pitch, either way, in cents, and how widely they're spread from side to side,
        from 0, all where the oscillator's panned, to 1, from all the way left of it to all the way right.
    */
    static juce::String unisonVoicesId (int oscillator);
    static juce::String unisonDetuneId (int oscillator);
    static juce::String unisonSpreadId (int oscillator);

    /** How the unison's voices, detune and spread are shown, such as "4 voices", "± 20 cents" and "50%". */
    static juce::String unisonVoicesToText (int voices);
    static juce::String unisonDetuneToText (float cents);
    static juce::String unisonSpreadToText (float spread);

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
    static juce::String lfoStartOnPlayId (int lfo);

    /** The IDs of each envelope's parameters: its attack, decay and release in seconds, and its sustain level. */
    static juce::String attackId (int envelope);
    static juce::String decayId (int envelope);
    static juce::String sustainId (int envelope);
    static juce::String releaseId (int envelope);

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    /** Sends the looping LFOs back to the start of their cycles. Nothing the host does calls this, so they run on
        smoothly through its loops and restarts; it's for starting from a known point, as the tests do.
    */
    void restartLfos();
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

    /** Whether the oscillator plays white noise, every frequency at once, rather than its harmonics. Its level, and
        whether it's on, still count, but its wave, harmonics, tune and glide don't.
    */
    bool isNoise (int oscillator) const noexcept;

    /** The white noise a noise oscillator has just played, from -1 to 1 before its level, as the newest note played
        it: the last noiseSamples samples, oldest first, or nothing if no note sounding plays it as noise. The count
        goes up with each block played, so the window can tell when there's more.
    */
    static constexpr int noiseSamples = 512;
    std::vector<float> getPlayedNoise (int oscillator) const;
    int getNoiseCount() const noexcept   { return noiseCount.load(); }

    /** The sample rate playing, or the usual 48 kHz before the host has said. */
    double getPlayingSampleRate() const noexcept   { return getSampleRate() > 0.0 ? getSampleRate() : 48000.0; }

    /** How many harmonics the oscillator plays its wave with, from the 1st up. */
    int getNumHarmonics (int oscillator) const noexcept;

    /** How many semitones away from the note the oscillator plays, from two octaves down to two octaves up. */
    float getTune (int oscillator) const noexcept;

    //==============================================================================
    // The LFOs. An LFO plays its wave just as it's drawn, rather than rebuilt from harmonics.

    const waveform::Shape& getLfoShape (int lfo) const noexcept;
    std::optional<waveform::Preset> getLfoPreset (int lfo) const noexcept;
    void setLfoShape (int lfo, const waveform::Shape&, std::optional<waveform::Preset> fromPreset = std::nullopt);

    bool isLfoOn (int lfo) const noexcept;
    float getLfoPeriod (int lfo) const noexcept;
    /** Whether an LFO's rate is synced to the host's tempo, running a number of cycles a beat rather than as its rate
        dial says in hertz. Nothing moves a synced LFO's rate.
    */
    bool isLfoSynced (int lfo) const noexcept;

    /** Whether a looping LFO starts when the host starts playing: synced, lined up with the bar, so a cycle of 1/4 a
        beat starts on each bar, and otherwise from the start of its cycle. Only starting counts: while the host plays
        on, its loops and jumps leave the LFO running smoothly. On to start with.
    */
    bool isLfoStartedOnPlay (int lfo) const noexcept;

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
        got it, in a category, one of getPresetCategories(), or the preset's own if none is given, and with a label
        the menu shows after its name, or Custom if none is given. Returns false if the file couldn't be written.
    */
    bool savePreset (const juce::File&, const juce::String& category = {}, const juce::String& label = {});

    /** Loads a preset. Returns false, and leaves the sound alone, if the file isn't one. */
    bool loadPreset (const juce::File&);

    /** Goes back to the sound the synth starts with. */
    void loadInitPreset();

    /** Names the sound as a preset that hasn't been saved, such as one loaded from a sound description, so it's shown
        under that name, unedited, until it's saved or changed.
    */
    void nameSound (const juce::String& name);

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

    /** The categories presets are put in, in the order they're listed, Other last. A preset with none, such as one
        saved before there were categories, Init, or a sound from Claude, is in Other.
    */
    static juce::StringArray getPresetCategories();
    static constexpr auto otherCategory = "Other";

    /** The category of the preset last loaded or saved, which is saved with it. */
    juce::String getPresetCategory() const   { return presetCategory; }

    /** Puts the preset in a category, one of getPresetCategories(). A preset of the user's own that's been saved is
        moved in its file straight away, without saving any other changes to the sound; otherwise the category is
        saved with the preset when it's saved. A factory preset's category can't be changed. Returns false if it
        couldn't be.
    */
    bool setPresetCategory (const juce::String& category);

    /** The category a preset's file, or a factory preset, is in, read from its file without loading it. */
    static juce::String getPresetCategory (const juce::File&);
    static juce::String getFactoryPresetCategory (const juce::String& name);

    /** The labels the menu shows after a preset's name: Factory for a factory preset, From AI for a sound from
        Claude, and for one of the user's own, the label it was saved with, Custom if none.
    */
    static constexpr auto factoryLabel = "Factory";
    static constexpr auto customLabel = "Custom";
    static constexpr auto aiLabel = "From AI";

    /** The label a preset's file was saved with, read from its file without loading it, or Custom. */
    static juce::String getPresetLabel (const juce::File&);

    //==============================================================================
    // Pitch sets: every setting of the sound, set differently for a range of keys. There's always the default pitch
    // set, which plays every key no other's range takes in. The window shows and edits one pitch set at a time, the
    // active one: every knob, wave and modulation is that pitch set's, and it's what the host's parameters move. Each
    // note plays its key's pitch set as the note starts. Where ranges overlap, the narrower one plays the key.

    static constexpr int maxPitchSets = ::maxPitchSets;

    /** The pitch sets, the default first, then the others in order of their lowest key. */
    int getNumPitchSets() const noexcept   { return (int) pitchSets.size(); }

    /** A pitch set's range of keys, as MIDI note numbers, lowest and highest together, or nothing for the default. */
    struct KeyRange
    {
        int lowest = 0, highest = 127;

        bool contains (int note) const noexcept   { return note >= lowest && note <= highest; }
        bool operator== (const KeyRange&) const = default;
    };

    std::optional<KeyRange> getPitchSetRange (int index) const;

    /** What a pitch set is called: "Default", or its range, such as "C2 to B2", or "C4" for a single key. */
    juce::String getPitchSetName (int index) const;
    static juce::String getKeyRangeName (KeyRange);

    /** The pitch set the window shows and edits. */
    int getActivePitchSet() const noexcept   { return activePitchSet; }

    /** Shows and edits another pitch set, leaving the one before as it is. */
    void setActivePitchSet (int index);

    /** Adds a pitch set for a range of keys, the lowest and highest given either way round, sounding as the active
        one does to start with, and returns where it is among them. It doesn't become active. Returns -1 if there are
        already as many pitch sets as there can be.
    */
    int addPitchSet (KeyRange);

    /** Gives a pitch set other than the default another range, and returns where it is now among them. */
    int setPitchSetRange (int index, KeyRange);

    /** Takes away a pitch set other than the default. If it was active, the default becomes active. */
    void removePitchSet (int index);

    /** The pitch set that plays a key: the narrowest whose range takes it in, or the default. */
    int getPitchSetForNote (int note) const;

    /** Puts the active pitch set's sound back to Init's, leaving the pitch sets and their ranges as they are. */
    void loadInitSound();

    /** Goes up whenever a pitch set is added, taken away, given a range or made active. */
    int getPitchSetVersion() const noexcept   { return pitchSetVersion; }

    /** The pitch set the moving displays show: the newest note's still sounding, or the active one if none is. Where the
        LFOs and envelopes are, the waves LFOs play, where controls are moved to and the harmonics as played are all
        that pitch set's, so playing keys of one pitch set shows how it plays, whichever is active.
    */
    int getShownPitchSet() const;

    /** Goes up with every key pressed, from anywhere, so the window can take the keys pressed to set a range. */
    int getNoteOnCount() const noexcept   { return noteOnCount.load(); }

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

    /** The whole sound, and the preset it came from, as saved with a host's project or in a preset file: the active
        pitch set's sound, with each other pitch set's sound in a child of its own.
    */
    juce::ValueTree createState();
    void loadState (juce::ValueTree);

    /** The active pitch set's sound: its parameters, waves and modulation. */
    juce::ValueTree createSoundState();
    void loadSoundState (juce::ValueTree);

    /** Makes a pitch set active, its sound going into the parameters, the waves and the modulation, after keeping
        the sound of the one active before.
    */
    void activate (int index);

    /** Works out which pitch set plays each key, for the audio thread. */
    void updateKeyMap();
    void sortPitchSets();

    struct PitchSet
    {
        std::optional<KeyRange> range;   // none for the default

        /** Where its patch, parameters and shared waves are kept for the audio thread. */
        int slot = 0;

        /** Its sound, kept while another pitch set is active. */
        juce::ValueTree sound;
    };

    std::vector<PitchSet> pitchSets;
    int activePitchSet = 0;
    int pitchSetVersion = 0;
    std::atomic<int> noteOnCount { 0 };

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
        std::atomic<float>* noise = nullptr;
        std::atomic<float>* pan = nullptr;
        std::array<std::atomic<float>*, modulation::numFilters> filterSends {};
        std::atomic<float>* unisonVoices = nullptr;
        std::atomic<float>* unisonDetune = nullptr;
        std::atomic<float>* unisonSpread = nullptr;
        std::array<std::atomic<float>*, Patch::numEffects> effectSends {};
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
        std::atomic<float>* startOnPlay = nullptr;
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

    /** A source's value at the end of the block, on the audio thread, for triggers: the default pitch set's LFO
        from where it's got to, shifted back by the phase given, or as it was the periods given earlier in the block,
        the newest note's envelope, if a note's sounding, and the last note's velocity.
    */
    std::optional<float> getSourceValueNow (int source, double phase, double periodsEarlier, int numSamples) const;

    void timerCallback() override;

    /** Further from zero than drawing and raising harmonics can take a wave. Loading a saved wave keeps it within
        this, in case the save is corrupt.
    */
    static constexpr float maxValue = 32.0f;

    /** Sets an oscillator's wave, remembering the phase of each harmonic it has. */
    void replaceShape (int oscillator, const waveform::Shape&, std::optional<waveform::Preset>);

    /** Hands the waves and assignments of the active pitch set to the audio thread. */
    void share();

    /** On the audio thread: takes what the message thread shared for a pitch set, if it's changed, and reads its
        parameters, into its patch.
    */
    void updatePatch (int slot);

    std::array<Oscillator, numOscillators> oscillators;

    /** Each pitch set's oscillators, by slot, kept while another pitch set is active, so triggers can change them. */
    std::vector<std::array<Oscillator, numOscillators>> slotOscillators = std::vector<std::array<Oscillator, numOscillators>> ((size_t) maxPitchSets);

    /** Sets an oscillator's wave, remembering the phase of each harmonic it has, and gives it random harmonics. */
    static void applyShape (Oscillator&, const waveform::Shape&, std::optional<waveform::Preset>);
    static void randomizeOscillator (Oscillator&, juce::Random&);

    /** Puts an oscillator's wave, and its harmonics' tunes and switches, into its WAVE in a saved sound. */
    static void writeOscillator (juce::ValueTree& wave, const Oscillator&);

    /** Hands a slot's oscillators to the audio thread, leaving the rest of what it shares as it is. */
    void shareOscillators (size_t slot, const std::array<Oscillator, numOscillators>&);
    std::array<Lfo, numLfos> lfos;
    modulation::Assignments assignments, velocityAssignments, modWheelAssignments;
    int shapeVersion = 0;
    int modulationVersion = 0;

    juce::String presetName { initPresetName };
    juce::File presetFile;
    juce::String presetCategory { otherCategory };
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

    // The audio thread only ever tries the lock, so it never waits. Each pitch set's slot has its own.
    std::array<juce::SpinLock, maxPitchSets> sharedLocks;
    std::vector<Shared> shared = std::vector<Shared> ((size_t) maxPitchSets);
    std::array<std::atomic<int>, maxPitchSets> sharedVersions {};

    // The audio thread's own copies. The sample rate is kept from prepareToPlay, as not every host sets the one
    // getSampleRate() returns.
    double playingSampleRate = 44100.0;
    std::vector<Shared> received = std::vector<Shared> ((size_t) maxPitchSets);
    std::array<int, maxPitchSets> receivedVersions {};

    // Kept apart from the processor, as they're too big to keep in a processor made on the stack.
    std::unique_ptr<Patches> patchStorage = std::make_unique<Patches>();
    Patches& patches = *patchStorage;

    /** The patch of the active pitch set, which the window shows. */
    Patch& activePatch() noexcept               { return patches.sets[(size_t) activeSlot.load()]; }
    const Patch& activePatch() const noexcept   { return patches.sets[(size_t) activeSlot.load()]; }

    /** Every parameter, by where it's kept: the host's, or a pitch set's own copy. */
    struct ParameterSet
    {
        std::array<OscillatorParameters, numOscillators> oscillators;
        std::array<FilterParameters, modulation::numFilters> filters;
        std::array<LfoParameters, numLfos> lfos;
        std::array<EnvelopeParameters, numEnvelopes> envelopes;
        std::atomic<float>* volume = nullptr;
    };

    ParameterSet makeParameterSet (const std::function<std::atomic<float>* (const juce::String&)>&);

    // The parameters as the host has them, which are the active pitch set's.
    std::array<OscillatorParameters, numOscillators> oscillatorParameters;
    std::array<FilterParameters, modulation::numFilters> filterParameters;
    std::array<LfoParameters, numLfos> lfoParameters;
    std::array<EnvelopeParameters, numEnvelopes> envelopeParameters;
    std::atomic<float>* volume = nullptr;

    // Each slot's own copy of every parameter, which the audio thread plays. The active pitch set's slot follows the
    // host's parameters as they change.
    std::vector<juce::RangedAudioParameter*> allParameters;
    std::vector<std::atomic<float>*> liveValues;
    std::map<juce::String, int> parameterIndices;
    std::array<std::unique_ptr<std::atomic<float>[]>, maxPitchSets> slotValues;
    std::array<ParameterSet, maxPitchSets> slotParameters;
    std::array<std::atomic<bool>, maxPitchSets> slotUsed {};
    std::atomic<int> activeSlot { 0 };

    /** Copies every parameter, as the host has it, into the active pitch set's slot. */
    void copyParametersToSlot();

    // The slot of the pitch set the moving displays show, which the audio thread sets after each block, and its
    // parameters, waves and modulation, as the message thread reads them for the displays.
    std::atomic<int> shownSlot { 0 };
    size_t getShownSlot() const noexcept   { return (size_t) shownSlot.load(); }
    const Shared& getShownShared() const noexcept   { return shared[getShownSlot()]; }
    const ParameterSet& getShownParameters() const noexcept   { return slotParameters[getShownSlot()]; }
    bool isShownActive() const noexcept   { return shownSlot.load() == activeSlot.load(); }
    int getShownLfoMode (int lfo) const noexcept;
    bool isShownLfoRandom (int lfo) const noexcept;

    /** Where a control is set in the pitch set shown, from 0 to 1 along its travel. */
    float getShownPosition (int target) const;

    std::array<SharedEnvelopeDisplay, numEnvelopes> envelopeDisplays;

    // The noise the newest note played, for each oscillator playing noise, or none, for the window.
    std::array<std::array<float, (size_t) noiseSamples>, numOscillators> playedNoise {};
    std::array<bool, numOscillators> noisePlayed {};
    mutable juce::SpinLock noiseLock;
    std::atomic<int> noiseCount { 0 };

    // The controls that are triggered rather than moved: the oscillators' and filters' switches, each oscillator's randomizing, and
    // each of its harmonics' switches.
    std::vector<int> triggerTargets;

    // For each control, if it's triggered, what's given it, as moving, by velocity and by the mod wheel, each watched
    // for crossing its threshold. The audio thread counts up the times it's asked for, the message thread how many
    // it's carried out.
    std::array<std::array<modulation::ThresholdCrossing, 3>, modulation::numTargets> triggerCrossings {};
    std::array<std::atomic<int>, modulation::numTargets> triggerRequests {};
    std::array<int, modulation::numTargets> triggerRequestsDone {};
    std::array<int, numOscillators> randomizeCounts {};
    juce::Random randomizer;

    /** Does what a trigger does: randomizes an oscillator's harmonics, or switches an oscillator, filter or harmonic on
        or off, in the pitch set of the slot given, which asked for it: the active one's controls, or another's sound
        as it's kept.
    */
    void carryOutTrigger (int target, int slot);

    /** The default pitch set's slot, which it always has, whose modulation alone triggers. */
    static constexpr int defaultSlot = 0;

    // MIDI learn. The audio thread keeps where each controller was last put and counts its moves, and notes the
    // first moved while a control's being learned; the message thread links and sets the controls.
    std::array<std::atomic<int>, numLearnableControllers> controllerValues {}, controllerMoves {};
    std::atomic<int> learningTarget { -1 }, learnedController { -1 };
    std::array<int, modulation::numTargets> targetControllers {}, appliedMoves {};
    std::array<float, modulation::numTargets> midiThresholds {};
    std::array<modulation::ThresholdCrossing, modulation::numTargets> controllerTriggerCrossings {};
    int midiLearnVersion = 0;

    /** Sets a control to a position along its travel, from 0 to 1, as a MIDI controller does. */
    void setTargetPosition (int target, float position);
    std::atomic<bool> noteSounding { false };
    std::atomic<float> newestVelocity { 1.0f };
    std::atomic<float> modWheelShown { 0.0f };

    // The tempo synced LFOs follow, and whether the host was playing last block, to catch it starting.
    double playingTempo = modulation::defaultTempo;
    bool hostWasPlaying = false;

    /** Starts the LFOs that start when the host does, from the beat it starts at, if it says. */
    void startLfosOnPlay (std::optional<double> beat);
    std::atomic<double> hostTempo { modulation::defaultTempo };
    std::atomic<bool> hostGivesTempo { false };
    std::vector<HarmonicVoice*> voices;

    // Where each looping LFO is, which the audio thread moves on with every block, and how far each LFO has got,
    // for the editor to show.
    std::array<std::array<double, numLfos>, maxPitchSets> lfoPhases {};

    // How many periods each looping LFO has been through, which picks each cycle of a random wave, the seed each
    // plays its random wave from, and for the window, the cycle each is in and the newest note's seed for those
    // that run once.
    std::array<std::array<std::int64_t, numLfos>, maxPitchSets> lfoCycles {};
    std::array<std::uint32_t, numLfos> randomSeeds {};
    std::array<std::atomic<std::int64_t>, numLfos> lfoCyclesShown {};
    std::array<std::atomic<std::uint32_t>, numLfos> newestOnceSeeds {};
    std::array<std::atomic<double>, numLfos> lfoProgress {};
    std::array<double, numLfos> periodsSinceLastNote {};
    std::array<double, numLfos> newestLfoPeriods {};
    juce::int64 samplesPlayed = 0;
    juce::int64 lastNoteOnSample = -1;

    /** A synthesiser that keeps where the mod wheel is in the patches, as each message comes, for the voices. */
    struct ModWheelSynthesiser final : public juce::Synthesiser
    {
        explicit ModWheelSynthesiser (Patches& patchesToMove) : patches (patchesToMove) {}

        void handleController (int midiChannel, int controllerNumber, int controllerValue) override
        {
            if (controllerNumber == modulation::modWheelController)
                for (auto& patch : patches.sets)
                    patch.modWheel = (float) controllerValue / 127.0f;

            juce::Synthesiser::handleController (midiChannel, controllerNumber, controllerValue);
        }

        /** Tells the note starting where each oscillator of the note played before had glided to, and whether another
            key's held, for its oscillators that glide, then notes which voice plays it, for the next note.
        */
        void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
        {
            Patch::NoteStart start;

            for (auto* voice : voices)
                if (voice->isKeyDown() && voice->getCurrentlyPlayingNote() != midiNoteNumber)
                    start.legato = true;

            for (size_t i = 0; i < start.glideFrom.size(); ++i)
                start.glideFrom[i] = lastVoice != nullptr ? std::optional (lastVoice->getGlidingPitch ((int) i)) : std::nullopt;

            for (auto& patch : patches.sets)
                patch.noteStart = start;

            juce::Synthesiser::noteOn (midiChannel, midiNoteNumber, velocity);

            for (auto* voice : voices)
                if (auto* harmonicVoice = dynamic_cast<HarmonicVoice*> (voice);
                    harmonicVoice != nullptr && harmonicVoice->isKeyDown() && harmonicVoice->getCurrentlyPlayingNote() == midiNoteNumber
                    && harmonicVoice->getSamplesSinceNoteOn() == 0)
                    lastVoice = harmonicVoice;
        }

        Patches& patches;
        const HarmonicVoice* lastVoice = nullptr;
    };

    ModWheelSynthesiser synth { patches };

    // The voices' sound, split by where it goes, straight out and through each effect, and the reverb, played
    // through as it's set, before everything's added up.
    juce::AudioBuffer<float> routedBuffer;
    Reverb reverb;
    std::atomic<float>* reverbDelay = nullptr;
    std::atomic<float>* reverbSize = nullptr;
    std::atomic<float>* reverbDecay = nullptr;
    std::atomic<float>* reverbMix = nullptr;

    /** Puts the reverb's settings as they are into a pitch set's sound, as the reverb is the same for them all. */
    void keepReverbIn (juce::ValueTree& sound) const;
    std::atomic<int> lastNote { -1 };
    std::atomic<int> midiReceived { 0 };
    std::array<waveform::Drawing, numSources> drawings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthProcessor)
};
