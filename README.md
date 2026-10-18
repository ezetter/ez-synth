# EZ Synth

A synthesizer you play by drawing. Draw one wavelength of any shape on the grid, and EZ Synth breaks it down
into the sine waves at its harmonics, shows each of them on the grid, and plays their sum at the pitch of
whatever key you press. It has three oscillators, each with its own drawn wave, and every one that's on plays
each note, through a low pass and a high pass filter. Three LFOs, each with a drawn wave of its own, three
envelopes, which run with each key pressed, and how hard each key is played can move almost any control.

It runs as a standalone Mac app, and as a VST3 or Audio Unit instrument in a DAW.

## Using it

- **Choose an oscillator or LFO.** The six cards across the top are the three oscillators, then the three
  LFOs. Click one to show its wave on the grid; its card is outlined. The presets and the drawing work on
  whichever is shown. Only OSC 1 is on to begin with. The others start as a square and a sine, so each
  sounds different once switched on.
- **Switch oscillators on and off** with the switch on each card. Every oscillator that's on plays every
  note, added together. You can still edit one that's off: its wave is drawn faintly, and the grid says it's
  off.
- **Set each oscillator's level** with the dial on its card, from 0 to 100% of its full amplitude.
  Double-click a dial to reset it to 100%.
- **Draw a wave.** Click and drag on the grid. It shows one wavelength, from 0° to 360°, and values from
  −1 to +1. Fast strokes are joined up, so there are no gaps.
- **Start from a preset.** Sine, Sawtooth, Square or Triangle replace the wave. Draw over one to change it.
- **Pick how many harmonics to play**, 1 to 9, with the numbered buttons under *Harmonics* (3 to start
  with). The oscillator plays its wave's harmonics from the 1st up to that number, and each oscillator has
  its own number. Harmonics the wave doesn't have are still played, at nothing: a square played with 3 has
  its 1st and 3rd, and its 2nd at 0, so the 2nd button is only outlined.
- **Raise or lower each harmonic** with its slider. There's one for every harmonic being played, with its
  amplitude next to it, and once you've played a note, its frequency. A slider changes the wave itself,
  adding or taking away that harmonic's sine wave and leaving the others alone, so the drawing and the
  sliders always agree. Raise a sine's 2nd and 3rd harmonics from 0, for instance, and the wave becomes
  Custom. A harmonic you lower to 0 and raise again comes back as it was. Raised harmonics can take a wave
  past −1 and +1, and the grid stretches to show it.
- **Read the grid.** The faint band is the wave. Each coloured line is one harmonic, at its true size and
  phase, coloured like its slider. The white line is their sum, which is the wave that oscillator plays.
  Playing more harmonics makes the white line follow the wave more closely.
- **Filter the sound** with the Filters panel, at the right of the row above the keys. It shapes all the
  oscillators together.
  The low pass filter cuts harmonics above its cutoff, and the high pass filter cuts those below. Each has
  a switch, a cutoff dial from 20 Hz to 20 kHz, and a resonance dial that makes the filter peak at its
  cutoff, boosting the harmonics there up to 8 times. Both start switched out, with cutoffs of 1 kHz and
  250 Hz that change the sound clearly when you switch them in. Double-click a dial to reset it.
- **Play.** Each key plays the wave with the key's pitch as its fundamental frequency. The header shows the
  last note and its frequency. Click the on-screen keys, or in the standalone app type on the computer
  keyboard: A to ; play the white keys from middle C, and W E T Y U O P play the black keys. A MIDI
  keyboard works too. In the standalone app, choose it under *Options › Audio/MIDI Settings*. The pitch
  wheel bends two semitones either way.
- **Volume** is the knob at the left of the row above the keys. Double-click it to reset it.

### LFOs

- **An LFO is an oscillator too slow to hear**, whose wave moves controls instead. Its card has a switch, a
  **Period** dial for how long one cycle takes, from 0.1 to 10 seconds, and **Loop** and **Once**. Click
  the card to draw its wave on the grid, or pick a preset for it. An LFO follows its drawing exactly, rather
  than rebuilding it from harmonics, so a square LFO switches cleanly between its two values. The grid marks
  the LFO's period in time, and a line shows where the LFO has got to.
- **Loop** runs the LFO all the time, the same for every note. **Once** runs it through one period each
  time a key is pressed, separately for each note, then holds its last value: an envelope. Draw a wave that
  rises from −1 to +1 and holds there, for instance, to swell each note in.
- An LFO that's switched off moves nothing. Giving one something to move switches it on.

### Envelopes

- **The three envelopes**, Env 1 to Env 3, sit between the volume and the filters. Each runs separately for
  every key pressed, with the usual four stages:
  - **Attack**, 0 to 20 seconds: how long it takes to rise from nothing to its peak.
  - **Decay**, 0 to 20 seconds: how long it then takes to fall to the sustain level.
  - **Sustain**, 0 to 100%: the level it holds while the key stays down.
  - **Release**, 0 to 20 seconds: how long it takes to fall to nothing once the key is let go, from wherever
    it was.

  Each stage is a straight line, and takes as long as it's set to. The picture above the dials shows the
  envelope's shape, with a light blue dot marking where the newest note is in it. Its title line says what
  it moves.
- **An envelope moves a control one way from where it's set**, up by its range at the peak (or down, if
  inverted), and back as it falls. To shape a note's loudness, for instance, turn an oscillator's level, or
  the volume, down to 0 and give it an envelope with a range of 100%.
- **An envelope on the loudness sets how long a note lasts.** While an envelope moves the volume, or an
  oscillator's switch or level, a note goes on sounding until that envelope's release is over, however long.
  Otherwise a note fades over a quarter of a second once its key is let go. An envelope on anything else,
  such as a filter's cutoff, is cut off when the note ends.

### Giving an LFO, envelope or velocity something to move

- **Right-click, or Control-click,** almost any control: the volume, an oscillator's switch or level, the
  harmonic buttons, a harmonic's slider, or a filter's switch, cutoff or resonance. A small window opens,
  in two parts. In the top part you pick an LFO or envelope to move the control; in the bottom part you
  switch on **Velocity** to have how hard each key is played move it too. Each part has its own **range**,
  the top part a **polarity** too, and both can move the same control at once, their movements adding up.
  - **Moved by** None, LFO 1 to 3, or Env 1 to 3. A control can be moved by one of these at a time, as well
    as by velocity.
  - **Range** is how far the control moves, from one end of the source to the other, as a share of its
    travel. An LFO's wave swings the control either side of where it's set: at 0 the control is where you
    set it, and at +1 and −1 it's at either end of its range. An envelope moves it one way, by its whole
    range at the peak. Nothing moves a control past the ends of its travel.
  - **Velocity** leaves the control where it's set for the hardest notes, and moves it down for softer ones,
    by its whole range for the softest. So what you hear when you click the on-screen keys, which always play
    as hard as they can, is what the controls say. To make softer notes darker, for instance, set the low
    pass cutoff where you want the hardest notes, and give it velocity with a range of 30% or so.
  - **Normal** polarity raises the control as the LFO or envelope rises. **Inverted** lowers it. Velocity has
    no polarity: softer notes always move a control down.
  - Choose **None**, or switch velocity **Off**, to stop it being moved.
- **Velocity starts out on the volume**, with a range of 35%, so softer notes are quieter, as on most
  instruments. Switch it off there for every note to play as loud as the hardest, or move it elsewhere.
- **See them at work.** A control that's being moved shows where it's been moved to, in light blue, as a
  dot and a stretch from where it's set, following the newest note and how hard it was played. An LFO's card lists everything it
  moves, with each one's range and polarity. Click one to change it, or its cross to stop the LFO moving it.

Everything is saved with your DAW project: each oscillator's and LFO's wave and settings, the envelopes, the
filters, and what the LFOs, envelopes and velocity move. Everything except the waves and what moves what can be
automated.

## Building

You need CMake 3.22 or later and Xcode or its command line tools. JUCE 9.0.2 is downloaded during the
first configure.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 10
```

This builds:

- `build/EZSynth_artefacts/Release/Standalone/EZ Synth.app`, the standalone app
- `EZ Synth.vst3`, installed into the system-wide `/Library/Audio/Plug-Ins/VST3`, with other VST3 plugins
- `EZ Synth.component` (Audio Unit), installed into your own `~/Library/Audio/Plug-Ins/Components`

Only an administrator can add to `/Library/Audio/Plug-Ins/VST3`, so the first time, the build says it
couldn't install the VST3 there. Run this once, with your password, to make the plugin's folder there and
give it to your account. Every build after that installs the VST3 without asking:

```bash
sudo mkdir "/Library/Audio/Plug-Ins/VST3/EZ Synth.vst3" && sudo chown "$USER" "/Library/Audio/Plug-Ins/VST3/EZ Synth.vst3"
```

To install somewhere else, set `EZSYNTH_VST3_FOLDER` or `EZSYNTH_AU_FOLDER` when configuring, such as
`-DEZSYNTH_VST3_FOLDER="$HOME/Library/Audio/Plug-Ins/VST3"`.

Restart your DAW, or rescan its plugins, to find EZ Synth among its instruments, made by EZ Synth. GarageBand
and Logic use the Audio Unit; Cubase uses the VST3. To check the Audio Unit with Apple's validator, run
`auval -v aumu Ezs1 Ezsy`.

## Testing

```bash
./build/EZSynthTests
```

This checks that the presets break down into their textbook harmonics, that notes come out at the right
pitch with the right harmonics, that raising and lowering a harmonic changes it alone, that oscillators add
together at their levels, that the filters cut and boost each harmonic by as much as theory says, that LFOs
move each kind of control by the range and in the direction they're set to, looping or once per note, that
envelopes run each stage for as long as it's set, separately for each note, that a note lasts through
the release of an envelope on its loudness, that velocity moves controls by its range, alongside an LFO, that high notes leave out harmonics too high to play cleanly, that changing the
wave during a note doesn't click, and that saved settings load back. Add `--snapshots <folder>` to also
save pictures of the window showing several waves.

## How it works

- **Breaking the wave down.** A wave is 512 evenly spaced points. A discrete Fourier transform finds the
  amplitude and phase of its harmonics 1 to 9 (`Source/Waveform.cpp`). Any offset from zero is left out, as
  it can't be heard, and anything 100 dB below full scale counts as 0.
- **Changing a harmonic.** Harmonics on 512 points are independent, so adding a sine wave at harmonic k to
  the wave changes harmonic k and nothing else. A slider adds the difference between the amplitude it's
  set to and the one the wave has.
- **Playing it.** Each voice adds up the chosen harmonics as sine waves (`Source/HarmonicVoice.cpp`).
  An oscillator plays its harmonics at their amplitudes times its level. If they add up to more than full
  scale, as a square's do, the oscillator turns them down to fit. As every oscillator plays the same note, the oscillators' sine waves at each harmonic add up to a
  single sine wave, so a voice plays all three oscillators for little more than the cost of one. Harmonics
  fade out between 16 and 19 kHz, so high notes don't alias. When a wave, level, switch or number of
  harmonics changes during a note, the voice glides to the new sound, most of the way in 5 ms and all of it
  in about 20, rather than clicking. Notes
  start in 8 ms and fade out over 250 ms when let go.
- **Filtering.** Each note goes through its own high pass filter, then low pass filter
  (`Source/FilterStage.cpp`), so an LFO that runs once can sweep each note's filter separately. Both are
  JUCE's state variable filters, 12 dB an octave. Switching a filter fades it in or out over 20 ms, and its
  cutoff and resonance glide over 10 ms, so nothing clicks. A filter that's switched out is skipped
  entirely. Cutoffs too near the Nyquist frequency, as 20 kHz is at low sample rates, are lowered to 45% of
  the sample rate.
- **Modulation.** Every 64 samples, each note works out where the LFOs and its envelopes are, and where
  they've moved its controls (`Source/HarmonicVoice.cpp`). A looping LFO carries on from where it was at
  the start of the block; one that runs once, and each envelope (`Source/Envelope.cpp`), counts from the
  note's start. A control's position along its travel, from 0 to 1, moves by its range times half an LFO's
  value, or times an envelope's whole value, and by its velocity range times how far short of full velocity
  the note was played. The two movements add up before the control is kept within its travel. Cutoffs travel by octave, and harmonic sliders give weak
  harmonics more of their travel, just as their dials and sliders do (`Source/Modulation.cpp`). The note
  then glides its harmonics, filters and volume there, so even a square LFO doesn't click. The window
  shows the same positions for the latest note.
- **Between the threads.** The window analyses the drawing on the message thread. The analysis reaches the
  audio thread through a lock that the audio thread only ever tries, so audio never waits on drawing.

| File | What's in it |
| --- | --- |
| `Source/Waveform.*` | The drawn shape, the presets and the Fourier analysis |
| `Source/HarmonicVoice.*` | A voice playing a note: its harmonics, filters and LFOs |
| `Source/SynthProcessor.*` | The plugin: the oscillators, parameters, MIDI, saving and loading |
| `Source/SynthEditor.*` | The window and its layout |
| `Source/SourceCard.*` | The oscillators' and LFOs' cards |
| `Source/Modulation.*` | The controls LFOs can move, and how an LFO moves one |
| `Source/Patch.h` | Everything the notes need to play a block |
| `Source/ModulatableControls.*` | Dials, sliders and switches that open the modulation editor on a right-click |
| `Source/Envelope.*` | One note's run through an envelope's attack, decay, sustain and release |
| `Source/EnvelopePanel.*` | An envelope's dials and the picture of its shape |
| `Source/ModulationEditor.*` | The window for choosing an LFO or envelope, and velocity, with their ranges and polarities |
| `Source/LfoPanel.*` | The list of what an LFO moves |
| `Source/ChoiceButtons.*` | Rows of joined buttons, such as Loop and Once |
| `Source/FilterPanel.*` | The filters' switches, cutoff dials and resonance dials |
| `Source/FilterStage.*` | A low or high pass filter that switches and sweeps without clicks |
| `Source/WaveGrid.*` | The grid you draw on |
| `Source/HarmonicSelector.*` | The 1 to 9 buttons |
| `Source/HarmonicSliders.*` | A slider for each harmonic, and the key to the grid's lines |
| `Source/Keyboard.*` | The on-screen piano |
| `Source/SynthLookAndFeel.*` | Colours, fonts, the knobs, sliders, switches and buttons |
| `Tests/Tests.cpp` | The tests and the window pictures |
