# EZ Synth

A synthesizer you play by drawing. Draw one wavelength of any shape on the grid, and EZ Synth breaks it down
into the sine waves at its harmonics, shows each of them on the grid, and plays their sum at the pitch of
whatever key you press. It has three oscillators, each with its own drawn wave, and every one that's on plays
each note.

It runs as a standalone Mac app, and as a VST3 or Audio Unit instrument in a DAW.

## Using it

- **Choose an oscillator.** The three cards across the top are the oscillators. Click one to show its wave
  on the grid. The presets, the drawing, the harmonic buttons and the harmonic sliders all work on that
  oscillator, and its card is outlined. Only OSC 1 is on to begin with. The others start as a square and a sine, so each sounds
  different once switched on.
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
- **Play.** Each key plays the wave with the key's pitch as its fundamental frequency. The header shows the
  last note and its frequency. Click the on-screen keys, or in the standalone app type on the computer
  keyboard: A to ; play the white keys from middle C, and W E T Y U O P play the black keys. A MIDI
  keyboard works too. In the standalone app, choose it under *Options › Audio/MIDI Settings*. The pitch
  wheel bends two semitones either way.
- **Volume** is the knob left of the keys. Double-click it to reset it.

Each oscillator's wave, switch, level and number of harmonics are saved with your DAW project. All of
them except the waves can be automated, as can the volume.

## Building

You need CMake 3.22 or later and Xcode or its command line tools. JUCE 9.0.2 is downloaded during the
first configure.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 10
```

This builds:

- `build/EZSynth_artefacts/Release/Standalone/EZ Synth.app`, the standalone app
- `EZ Synth.vst3`, copied into `~/Library/Audio/Plug-Ins/VST3`
- `EZ Synth.component` (Audio Unit), copied into `~/Library/Audio/Plug-Ins/Components`

Rescan plugins in your DAW to find it under instruments, made by EZ Synth. To check the Audio Unit with
Apple's validator, run `auval -v aumu Ezs1 Ezsy`.

## Testing

```bash
./build/EZSynthTests
```

This checks that the presets break down into their textbook harmonics, that notes come out at the right
pitch with the right harmonics, that raising and lowering a harmonic changes it alone, that oscillators add
together at their levels, that high notes leave out harmonics too high to play cleanly, that changing the
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
  harmonics changes during a note, the voice glides to the new sound over 10 ms rather than clicking. Notes
  start in 8 ms and fade out over 250 ms when let go.
- **Between the threads.** The window analyses the drawing on the message thread. The analysis reaches the
  audio thread through a lock that the audio thread only ever tries, so audio never waits on drawing.

| File | What's in it |
| --- | --- |
| `Source/Waveform.*` | The drawn shape, the presets and the Fourier analysis |
| `Source/HarmonicVoice.*` | A voice playing a note as a sum of sine waves |
| `Source/SynthProcessor.*` | The plugin: the oscillators, parameters, MIDI, saving and loading |
| `Source/SynthEditor.*` | The window and its layout |
| `Source/OscillatorCard.*` | An oscillator's switch, wave picture and level dial |
| `Source/WaveGrid.*` | The grid you draw on |
| `Source/HarmonicSelector.*` | The 1 to 9 buttons |
| `Source/HarmonicSliders.*` | A slider for each harmonic, and the key to the grid's lines |
| `Source/Keyboard.*` | The on-screen piano |
| `Source/SynthLookAndFeel.*` | Colours, fonts, the knobs, sliders, switches and buttons |
| `Tests/Tests.cpp` | The tests and the window pictures |
