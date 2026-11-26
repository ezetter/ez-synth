#include "SoundPrompt.h"

#include "SoundDescription.h"
#include "SynthProcessor.h"

namespace prompt
{
    namespace
    {
        juce::String number (double value)
        {
            if (std::abs (value) >= 100.0 || juce::exactlyEqual (value, std::round (value)))
                return juce::String (juce::roundToInt (value));

            return juce::String (value, std::abs (value) >= 1.0 ? 3 : 4).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        }

        /** Every parameter, with its range, its default, and its value at points along its travel, which is what
            modulation ranges are shares of.
        */
        juce::String describeParameters (SynthProcessor& synth)
        {
            juce::String text;

            for (auto* parameter : synth.getParameters())
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);

                if (ranged == nullptr)
                    continue;

                const auto defaultValue = ranged->convertFrom0to1 (ranged->getDefaultValue());
                text << "- `" << ranged->paramID << "` (" << ranged->getName (64) << "): ";

                if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
                {
                    text << "true or false; default " << (defaultValue >= 0.5f ? "true" : "false") << "\n";
                    continue;
                }

                if (const auto* choice = dynamic_cast<juce::AudioParameterChoice*> (ranged))
                {
                    text << "one of \"" << choice->choices.joinIntoString ("\", \"") << "\"; default \""
                         << choice->choices[juce::roundToInt (defaultValue)] << "\"\n";
                    continue;
                }

                const auto& range = ranged->getNormalisableRange();
                text << number (range.start) << " to " << number (range.end) << "; default " << number (defaultValue)
                     << "; shown as " << ranged->getText (0.0f, 24) << " to " << ranged->getText (1.0f, 24)
                     << "; along its travel, 25% is " << number (range.convertFrom0to1 (0.25f)) << ", 50% is "
                     << number (range.convertFrom0to1 (0.5f)) << ", 75% is " << number (range.convertFrom0to1 (0.75f)) << "\n";
            }

            return text;
        }

        /** Every control that can be moved, by its key: the parameters one by one, the harmonics as a pattern. */
        juce::String describeTargets()
        {
            juce::StringArray keys;

            for (int target = 0; target < modulation::numTargets; ++target)
            {
                const auto kind = modulation::describe (target).kind;

                if (kind == modulation::Kind::harmonic || kind == modulation::Kind::harmonicTune
                     || kind == modulation::Kind::harmonicPhase || kind == modulation::Kind::harmonicSwitch)
                    continue;

                keys.add ("`" + modulation::getKey (target) + "`" + (modulation::isTrigger (target) ? " (triggered)" : ""));
            }

            // The harmonics' keys follow a pattern, as the tests check.
            return keys.joinIntoString (", ") + ".\n\nAnd each harmonic's controls, for oscillator N from 1 to 6 and harmonic H "
                   "from 1 to 16: its amplitude, `oscNHarmonicH` (such as `osc2Harmonic3`), its tune, `oscNHarmonicHTune` "
                   "(H from 2), its phase, `oscNHarmonicHPhase`, and its switch, `oscNHarmonicHSwitch` (triggered).\n";
        }

        juce::String example (const juce::String& preset)
        {
            SynthProcessor synth;

            if (! synth.loadFactoryPreset (preset))
                return {};

            return "### " + preset + "\n\n```json\n" + sound::toJson (synth, true) + "\n```\n\n";
        }

        const char* guide = R"(# Making EZ Synth sound like a sample

You're going to listen, with measurements, to a recorded sample, and write settings for EZ Synth, a software
synthesizer, that make it sound as much like the sample as it can. Everything you need to know about EZ Synth is
below. The person you're working with will give you the sample, as a WAV, MP3 or other audio file, or several
samples of one instrument at different notes, such as C2, C3 and C4. This is experimental: do your best, say how
close you got, and what EZ Synth couldn't match.

## What to do

1. Measure the sample. If the sound tool below is there, run `analyze` on the sample. Otherwise measure it yourself,
   with Python and NumPy if they're there (convert an MP3 with `afconvert -f WAVE -d LEI16 in.mp3 out.wav` on a Mac,
   or ffmpeg): find its pitch, its loudness over time, and each harmonic's amplitude at moments through the note.
2. Work out a first sound description (the format is below): the harmonics of the steadiest part of the note on
   oscillator 1, the loudness's rise and fall with an envelope or an LFO run once, and how the harmonics change
   over the note, with envelopes, LFOs or a filter.
3. Save it, and if the sound tool is there, run `compare` with it and the sample. It plays your sound as the
   sample's note, measures it the same way, and shows both side by side, with average differences: loudness in dB,
   harmonics in dB, and pitch in cents. Lower is closer.
4. Change what's furthest off and compare again. Keep going while it gets closer, up to ten rounds or so. Look at
   the loudness first, then the harmonics after the attack, then the attack itself, then how they change.
5. Save the final description, as UTF-8 JSON with the `.json` extension, in the folder EZ Synth looks in, named after
   the sample, and say how to load it: in EZ Synth, click the preset's name, and choose it under From Claude.
   Say how close it got, and what it couldn't match.

### Several samples at different notes

Given several samples of one instrument, such as at C2, C3 and C4, match each with a pitch set of its own (see
Pitch sets below), all in one description, so each range of keys sounds like its nearest sample:

1. Measure every sample, and note each one's MIDI note.
2. Choose the ranges: each sample's pitch set takes the keys nearer its note than any other sample's, splitting
   halfway between neighbouring samples, and the lowest and highest reach to the ends of the keyboard. For samples
   at C2, C3 and C4 (MIDI 36, 48 and 60): the default pitch set is C3's, for G2 to F#3 and anything else; one pitch
   set for C2's, from 0 to F#2 (42); and one for C4's, from G3 (55) to 127. Make the sample in the middle the
   default, so it's the one any key left over plays.
3. Match each sample as in the steps above, its pitch set's sound the one you change. `compare` plays a sample as
   its own note, so it plays the pitch set whose range takes that note in: compare each sample with the whole
   description in turn.
4. Keep the pitch sets' sounds alike where the samples allow: the same oscillators, envelopes and modulation doing
   the same jobs, with only their amounts changed. A key either side of a boundary plays a different pitch set,
   and the more their sounds differ, the more the instrument jumps there. Say where it jumps most.

## How EZ Synth makes sound

Each note played has its own voice. A voice adds up its six oscillators, sends the sum through four filters, one
after another, and turns it up or down by the volume. Eight LFOs, eight envelopes, how hard the key was played
(velocity), and a MIDI mod wheel can each move almost any control, as described below.

### Oscillators

There are six, numbered 1 to 6 (`osc1` to `osc6` in parameter IDs). Each plays one wave, made of up to 16
harmonics: sine waves at whole multiples of the note's frequency. Harmonic H of an oscillator plays
`amplitude × sin(2π × (H + tune) × f × t + phase)`, where f is the note's frequency, raised or lowered by the
oscillator's tune.

- `oscNOn` switches it on. Only oscillator 1 is on to start with.
- `oscNLevel`, from 0 to 1, is its loudness, a straight multiplier.
- `oscNHarmonics`, from 1 to 16, is how many harmonics it plays, from the 1st up. Harmonics above the number are
  silent, unless switched on (see below). If your description gives an oscillator's harmonics and leaves this out, it's
  set to the highest harmonic you gave an amplitude, so you rarely need it.
- Each harmonic has an amplitude, from 0 to 1.5 (a square wave's fundamental is 4/π, 1.27), a phase, in degrees,
  where its sine wave starts, and, from the 2nd up, a tune, from -1 to 1, how far it's moved towards the harmonic
  below or above: harmonic 3 with a tune of 0.25 plays at 3.25 times the fundamental. Tunes make inharmonic sounds,
  such as bells, metal, struck bars and stiff piano strings, whose partials aren't whole multiples.
- Phases change the wave's shape but not how it sounds, except where oscillators or harmonics are at the same
  frequency and add or cancel; leave them at 0 unless that matters.
- If an oscillator's harmonics add up to a wave that peaks above 1, the whole oscillator is turned down until it
  doesn't. So only the harmonics' amplitudes against each other matter within an oscillator; set the strongest
  near 1, and balance oscillators against each other with their levels.
- `oscNTune` moves the whole oscillator from 24 semitones down to 24 up, in fractions of a semitone too. Use it to
  reach partials above the 16th: an oscillator at +12 plays twice oscillator 1's frequencies, so its harmonics 1 to
  16 are the note's harmonics 2, 4, ... 32, and one at +24 four times, so its harmonics are the note's 4, 8, ... 64.
  Give oscillator 2 at +12 amplitudes only from its 9th up to add the note's even harmonics from 18 to 32 without
  doubling oscillator 1's. An oscillator at +12 with its harmonics' tunes at +0.5 plays odd harmonics, as harmonic K
  tuned +0.5 at twice the frequency is 2K + 1 times the fundamental: its 9th to 16th give the note's 19th to 33rd.
  (Its 1st can't be tuned, so give it no amplitude.) At +24, a harmonic K tuned by +0.25, +0.5 or +0.75 is the
  note's 4K + 1, 4K + 2 or 4K + 3, so oscillators at +24 can fill in harmonics from 33 to 64 the same way, four
  harmonics of the note between each pair of theirs, a tune for each oscillator.
  With six oscillators, you can also give several their own strong partials, each with its own level and its own
  envelope, so different parts of the sound rise and fade at their own speeds, as a piano's or a bell's do.
  An oscillator at -12 or -24 plays below the note, for a sub-octave.
  An oscillator a few cents off another makes them beat, as a piano's strings or a chorus do.
- `oscNTuneZoom`, from 0 to 1, sets how far modulation moves that oscillator's tune: it's shown as how many
  semitones either side its slider reaches, from 24 at 0 down to 1 at 1, as 24 × (1/24)^zoom. An LFO with a range
  of R swings the tune R × reach semitones either way, so at a zoom of 1 an LFO range of 0.1 is a vibrato of 10
  cents either way. An envelope with a range of R raises it by up to 2 × R × reach semitones.
- `glide` ("Off", "Glide" or "Legato") and `glideTime` slide every oscillator into each note from the one before,
  while `glideAll` is true, as it is to begin with; with it false, each oscillator glides as its own `oscNGlide`
  and `oscNGlideTime` are set (giving any of those sets it false). They don't matter for a single note.
- `oscNNoise`, true or false, makes the oscillator play white noise, evenly from -1 to 1 at its level, instead of
  its harmonics; its wave, harmonics, tune and glide then don't matter. Use it for breath, bow and hammer noise,
  wind, cymbals and the attack of drums, shaped with the filters (a mid pass for a band of noise) and with an
  envelope on its level. Noise has no harmonics, so the compare command's harmonic measurements won't show it,
  except as a lower harmonic share.
- `oscNPan`, from 0 (all the way left) to 1 (all the way right), 0.5 to begin with, pans the oscillator. In the
  middle it plays at its full level on both sides; panned away from it, the far side fades out. The sound tool
  measures both sides together, as it does a stereo sample, so leave pans in the middle to match a sample. Like
  any control, it can be moved, as `osc1Pan` in the modulation: an LFO on it swings the oscillator side to side.
- `oscNUnison`, from 1 (off) to 8, stacks that many voices of the oscillator, detuned `oscNUnisonDetune` cents
  either way at the outermost (0 to 100, 20 to begin with) and spread `oscNUnisonSpread` either side of its pan
  (0 to 1, 0.5 to begin with), together about as loud as one voice. Use it for supersaws, string ensembles,
  choirs and thick pads; a few voices and a few cents chorus a sound gently. Like any control, each can be moved
  in the modulation by its key. Noise has no unison.
- `oscNToFilterM`, true or false, true to begin with, sends oscillator N through filter M (1 to 4). The filters run
  in a chain, 1 to 4; an oscillator that isn't sent through one skips it, going straight on to the next. Use it to
  filter one part of a sound and not another, such as a low-passed body under an unfiltered noise attack.
- `oscNToDelay` and `oscNToReverb`, true or false, true to begin with, send oscillator N through the delay and
  the reverb, after its filters, in that order. One sent through neither plays straight out. See the effects
  below; each starts switched off, so it plays dry.
- A harmonic can be switched off, so it stays silent whatever the number of harmonics, or on, so it plays even
  above the number. Leave these as they follow the number unless you need them.
- The wave can be given as one of the presets, `"wave": "Sine"`, `"Sawtooth"`, `"Square"` or `"Triangle"`, with its
  harmonics as that wave has them, and harmonics listed after it change those. Without a wave, the harmonics listed
  are all it has; any not listed are silent.

### Loudness

- `volume`, from 0 to 1, sets the loudness as volume², over every oscillator. Each note is played at 0.3 of full scale
  before the volume, so chords have room.
- With nothing shaping its loudness, a note starts over 8 ms and fades over 0.25 s once its key is let go.
- Velocity is on the volume to start with, with a range of 0.35, so softer notes are quieter. The compare command
  plays at full velocity, where velocity changes nothing, so leave it as it is.
- Shape the loudness with an envelope or an LFO run once on `volume` or on each oscillator's `oscNLevel`. While an
  envelope moves the volume or a level, a note goes on sounding until that envelope's release is over, however long.

### Filters

Four, numbered 1 to 4, one after another, each 12 dB an octave, each switched off to start with (`filterNOn`).
`filterNType` is "Low pass", "Mid pass", "High pass" or "Notch". `filterNCutoff` is from 20 Hz to 20 kHz, spread by
octave along its travel, about a tenth of the travel an octave.

- A low or high pass cuts above or below its cutoff, and `filterNResonance`, from 0 to 1, makes it peak at the
  cutoff, by up to 8 times.
- A mid pass lets through a band around its cutoff, `filterNWidth` octaves wide (0.2 to 6) between the points where
  it lets through half the power; the middle comes through as it is.
- A notch takes away a band around its cutoff: `filterNQ` (0.5 to 20) is the cutoff over the bandwidth, and
  `filterNDepth` (0 to 60 dB) how far it cuts.

Filters suit sounds whose brightness changes over the note, such as a pluck or brass, and resonant bodies. For a
steady tone, set the harmonics directly instead: it's more exact.

### Delay

One stereo delay, for the whole synth, before the reverb, that the oscillators sent through it (`oscNToDelay`)
play through. `delaySync` false (to begin with) sets its time by `delayTime`, from 1 to 2000 ms (375 to begin
with); true sets it by `delayDivision`, a note at the tempo: "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
"1/8D", "1/4T", "1/4" (to begin with), "1/4D", "1/2T", "1/2", "1/2D", "1 bar" or "2 bars", T for triplet and D
for dotted; the sound tool plays at 120 BPM. `delayOn`, false to begin with, switches it on; off, what's sent
through it plays dry. `delayFeedback`, from 0 to 1 (0.35 to begin with), is how much of
each repeat comes back again, 1 repeating for ever. `delayMix`, from 0 to 1 (0.5 to begin with), is how much of
what comes out is the repeats, the rest dry. Like the reverb, its repeats go on after the note: leave `delayOn`
false when matching a dry sample.

### Reverb

One stereo reverb, for the whole synth, that the oscillators sent through it (`oscNToReverb`) play out through,
once `reverbOn` switches it on (false to begin with, so what's sent through it plays dry):
`reverbDelay`, the pre-delay before the first reflection, 0 to 1000 ms (20 to begin with); `reverbSize`, the room,
from 1 (a small box) to 100 (a large hall), 50 to begin with; `reverbDecay`, the seconds its tail takes to fall
60 dB, 0.1 to 20 (2 to begin with), its highs fading twice as fast; and `reverbMix`, how much of what comes out of
it is the reverb, 0 to 1 (0.5 to begin with), the rest being the dry sound. Held, a sound through it at full mix is
about as loud as it is dry. It goes on ringing after the note ends, so the sound tool's measurements of a note's
fade include it: leave `reverbOn` false when matching a dry sample, and switch it on for pads, plucks in a space,
and anything a sample shows ringing in a room; set an oscillator's `oscNToReverb` false to keep it dry.

### Envelopes

Eight, Env 1 to Env 8, each running separately for every note: `envNAttack`, `envNDecay` and `envNRelease` in seconds
(0 to 20), and `envNSustain` from 0 to 1. Each stage is a straight line: up from 0 to 1 over the attack, down to the
sustain over the decay, held while the key is down, then down to 0 over the release once it's let go, from wherever
it was.

An envelope moves a control up from where it's set by range × its level (down if inverted), as a share of the
control's travel. Once the key is let go, its release brings the control down to the very bottom of its travel,
wherever it's set, except an oscillator's tune, which comes back to where it's set. So for an envelope on the
loudness, set the level or volume to 0 and the envelope's range to the level you want at its peak.

As the stages are straight lines, an envelope on a level makes a straight-line fade in amplitude, unlike the
exponential fade, straight in dB, of most real sounds. For a long natural decay, use an LFO run once instead (below),
drawn as the curve you want, or put the envelope on the volume, whose square law curves it partway.

### LFOs

Eight, LFO 1 to LFO 8. Each has a wave, from -1 to 1, and plays it over its period:

- `lfoNOn` switches it on; giving it something to move switches it on too.
- `lfoNPeriod` is its period in seconds, from 0.01 (100 Hz) to 20 (0.05 Hz), shown as a rate in hertz.
- `lfoNMode`: "Loop" runs it all the time, the same for every note. "Once" starts it with each note, separately for
  each, runs through its wave once over its period, then holds its last value: an envelope of any shape you draw.
  "Twice" up to "5 times" run through it that many times from each note, and "∞" runs it forever from each note.
- `lfoNRandom` plays a new random wave every cycle instead of its own, smooth, or in steps if the LFO is stepped.
  A fast random LFO on a harmonic's amplitude or the tune can rough a sound up; for real noise, use `oscNNoise`.
- `lfoNSync`, `lfoNBeatRate` and `lfoNStartOnPlay` tie it to a host's tempo; leave them for samples.

An LFO swings a control either side of where it's set: its wave's value v moves the control by range × v / 2 of its
travel, so a range of 1 swings it half its travel either way. A phase (degrees) delays the wave for that one control;
for an LFO run once, a phase is a wait after the key before it starts.

To fade a level from L down to 0 along a drawn curve with an LFO run once: set the level to L / 2, give the LFO a
range of L, and draw its wave from 1 down to -1 along the curve. Its period is how long the fade takes.

An LFO's wave is a preset, `"wave": "Sine"`, `"Triangle"`, `"Sawtooth"` (rising from -1 to 1 then dropping back) or
`"Square"`, or drawn as `"points"`: values from -1 to 1, evenly spaced over one period, the first at its start, joined
by straight lines, the last joined back to the first. Use as many points as the shape needs, such as 64 for a
smooth curve. With `"stepped": true` each point is held as a step instead, and there can be 2 to 20 of them.

### What moves what

Each control can be moved by one LFO or envelope (with a range, a polarity, and for an LFO a phase), by velocity
(with a range), and by the mod wheel (with a range and polarity), all at once, their movements adding up. Ranges are
shares, from 0 to 1, of the control's travel along its dial or slider, which is spread as its "along its travel"
values say in the list of parameters below. A harmonic's amplitude travels from 0 to 1.5 with weak amplitudes given
more of its length: at a share p of its travel the amplitude is 1.5 × p^2.1, so 0.5 of the way is 0.35. A harmonic's
tune travels from -1 to 1, and its phase round a whole turn. The number of harmonics travels from 1 to 16 in equal
steps. An LFO's rate travels from 0.05 Hz to 100 Hz, each tenth of its travel about twice as fast.

Controls marked triggered below, a switch or randomizing, aren't moved: each time what's given them crosses its
threshold, from 0 to 1, they're switched or randomized. They're rarely useful for matching a sample.

An LFO can't move its own rate, even through other LFOs.

### Pitch sets

Every setting above can be different for a range of keys. A sound has a default pitch set, which plays every key no
other takes in, and up to 15 more, each with a range of keys, its lowest and highest, and a whole sound of its own:
its own parameters, oscillators, LFOs and modulation. A key plays the pitch set whose range takes it in, the
narrowest if more than one does, or the default if none does. A note plays its pitch set's sound from start to end.
Looping LFOs run separately in each pitch set, at its own rates.

They're for an instrument whose tone changes across its range, such as a piano, brighter and longer in the bass, or
for matching several samples at different notes, one pitch set each.

## The sound description

A JSON object. Everything is optional, and whatever's left out is as EZ Synth's Init sound has it: oscillator 1 on, a
sine with 3 harmonics, the other oscillators off, filters off, LFOs off, nothing moving anything but velocity on the
volume. Its sound is the default pitch set's; `pitchSets` lists any others.

```json
{
  "format": "EZ Synth sound",
  "version": 3,
  "name": "Short name for the sound",
  "notes": "Anything worth saying about it, such as what it couldn't match",
  "parameters": { "parameter ID": number, true or false, or a choice's name },
  "oscillators": [
    { "wave": "Sawtooth", "harmonics": [ { "harmonic": 1, "amplitude": 1.0, "phase": 0, "tune": 0, "switch": "follows" } ],
      "stepped": false, "steps": 8 },
    { ... oscillator 2 ... },
    ... up to 6
  ],
  "lfos": [ { "wave": "Sine" }, { "points": [1, 0.5, 0.2, 0, -0.3, -1], "stepped": false }, ... up to 8 ],
  "modulation": [ { "target": "control key", "source": "LFO 1 or Env 1", "range": 0.5, "inverted": false, "phase": 0,
                    "threshold": 0.5 } ],
  "velocity": [ { "target": "volume", "range": 0.35 } ],
  "modWheel": [ { "target": "control key", "range": 0.5, "inverted": false } ],
  "pitchSets": [
    { "lowest": 0, "highest": "F#2", "parameters": { ... }, "oscillators": [ ... ], "lfos": [ ... ], "modulation": [ ... ] },
    { "lowest": "G3", "highest": 127, ... },
    ... up to 15
  ]
}
```

- The oscillators and LFOs are listed in order, from 1; give `{}` for one you leave as Init has it.
- In a harmonic, give what you need: `amplitude`, `phase` (degrees), `tune` (-1 to 1, not for the 1st), and `switch`
  (`"follows"`, `"off"` or `"on"`).
- `modulation` gives each control's LFO or envelope. `velocity`, if given, replaces Init's velocity on the volume, so
  leave it out to keep that. `modWheel` doesn't matter for matching a sample.
- Each oscillator's `oscNOn` must be true in `parameters` for it to play, apart from oscillator 1.
- Each pitch set in `pitchSets` has `lowest` and `highest`, its range's ends, taken in, each a MIDI note number from 0
  to 127 or a note name with middle C as C4, such as `"C2"`, `"F#3"` or `"Bb4"`; and `parameters`, `oscillators`,
  `lfos`, `modulation`, `velocity` and `modWheel`, just as above. Its sound starts from Init, as the default's does,
  not from the default's sound: give it everything it needs, even what's the same as the default's.
- Save it as plain JSON, with no comments.

)";
    }

    juce::File getSoundTool()
    {
       #ifdef EZSYNTH_SOUND_TOOL
        if (const juce::File tool (EZSYNTH_SOUND_TOOL); tool.existsAsFile())
            return tool;
       #endif

        return {};
    }

    juce::String make()
    {
        SynthProcessor synth;
        auto text = juce::String (juce::CharPointer_UTF8 (guide));

        text << "## Where to save it\n\nSave the description in `" << sound::getClaudeFolder().getFullPathName()
             << "`, making the folder if it isn't there. EZ Synth lists the sounds there under From Claude, in the menu "
                "that opens from the preset's name.\n\n";

        text << "## The sound tool\n\n";

        if (const auto tool = getSoundTool(); tool != juce::File())
        {
            const auto quoted = "\"" + tool.getFullPathName() + "\"";
            text << "EZ Synth's sound tool is at `" << tool.getFullPathName() << "`. Run it with no arguments for its help. "
                    "Paths can be relative to where you run it.\n\n"
                    "```\n"
                 << quoted << " analyze sample.wav\n"
                 << quoted << " compare sound.json sample.wav\n"
                 << quoted << " compare sound.json sample.wav --hold 0.8   # if the sample's key is let go before it ends\n"
                 << quoted << " compare sound.json C2.wav --note C2   # as a key of your choosing\n"
                 << quoted << " render sound.json out.wav --note 60 --hold 1 --length 2\n"
                 << quoted << " describe \"Grand Piano\"   # a factory preset, as a sound description\n"
                    "```\n\n"
                    "`analyze` reads WAV, AIFF, MP3 and the other formats macOS reads. Its harmonic amplitudes are linear, "
                    "against the strongest harmonic at that moment, as EZ Synth's are; 64 harmonics are measured, where "
                    "they fit below 20 kHz. `compare` plays the sound as the sample's nearest MIDI note, holding the key "
                    "until the sample has faded 60 dB below its peak unless you give `--hold`, and saves what it played "
                    "beside the description, so the person can listen to both, and says which pitch set played. The factory presets are Acid Bass, Bowed Cello, Brass Section, Breath Choir, Church Bell, Drawbar "
                    "Organ, Drum Kit, Electric Piano, Fifth Lead, Flute, Glass Pad, Grand Piano, Helicopter, Kick Drum, "
                    "Marimba, Music Box, Noise Riser, Nylon Guitar, Ocean Surf, Pan Flute, Pipe Organ, Rain, Reese Bass, "
                    "Sample and Hold, Shaker Groove, Shifting Sands, Square Lead, Step Filter, Stepped Arp, String "
                    "Ensemble, Thunder, Vox Pad, Warm Pad, Wind and Wobble Bass. Breath Choir, Drum Kit, Helicopter, "
                    "Noise Riser, Ocean Surf, Pan Flute, Rain, Shaker Groove, Thunder and Wind use noise.\n\n";
        }
        else
        {
            text << "EZ Synth's sound tool, which measures samples and plays sound descriptions, isn't installed here, "
                    "so measure the sample yourself, as step 1 says, and skip the compare steps. Check your description "
                    "carefully against this prompt instead.\n\n";
        }

        text << "## Every parameter\n\nBy ID, with its range in the units the description uses, its default, how it's "
                "shown, and its value at points along its travel, which modulation ranges are shares of.\n\n"
             << describeParameters (synth)
             << "\n## Every control that can be moved\n\nBy the key a modulation's `target` uses: "
             << describeTargets()
             << "\n## Examples\n\nTwo of EZ Synth's factory presets as sound descriptions, giving only what differs from "
                "Init. Church Bell tunes harmonics to a bell's partials, fades each group with its own envelope, and draws "
                "its ring on an LFO run once; Electric Piano has a pitch set each for its low and high keys.\n\n"
             << example ("Church Bell") << example ("Electric Piano");

        return text;
    }
}
