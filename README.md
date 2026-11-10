# EZ Synth

A synthesizer you play by drawing. Draw one wavelength of any shape on the grid, and EZ Synth breaks it down
into the sine waves at its harmonics, shows each of them on the grid, and plays their sum at the pitch of
whatever key you press. It has three oscillators, each with its own drawn wave, and every one that's on plays
each note, through two filters, each a low pass, mid pass, high pass or notch. Three LFOs, each with a drawn wave of its own, three
envelopes, which run with each key pressed, and how hard each key is played can move almost any control.

It runs as a standalone Mac app, and as a VST3 or Audio Unit instrument in a DAW.

## Using it

- **The window** has the six cards across the top, the grid under them, and the volume, envelopes and filters
  under that. Down the right, from the cards to the filters, is a tall panel with the chosen oscillator's
  harmonics, with big dials, or what the chosen LFO moves. The window can be resized, down to 1420 by 900.
- **Choose an oscillator or LFO.** The six cards across the top are the three oscillators, then the three
  LFOs. Click one to show its wave on the grid; its card is outlined. The presets and the drawing work on
  whichever is shown. Only OSC 1 is on to begin with, playing a sine. The others start as a square and a
  sawtooth, so each sounds different once switched on, and each plays its first 3 harmonics. The LFOs start
  switched off, until you give one something to move.
- **Switch oscillators on and off** with the switch on each card. Every oscillator that's on plays every
  note, added together. You can still edit one that's off: its wave is drawn faintly, and the grid says it's
  off.
- **Set each oscillator's level** with the dial on its card, from 0 to 100% of its full amplitude.
  Double-click a dial to reset it to 100%.
- **Tune an oscillator** with the **Tune** slider at the foot of the panel beside the grid, anywhere from
  an octave below the note to an octave above. The middle of the slider, marked with a longer tick, is in
  tune. Ticks mark each semitone, and the major and perfect intervals are numbered either way, 2 to 8: a
  major 2nd, 3rd, 6th and 7th, a perfect 4th and 5th, and the octave. On a whole semitone the slider names
  the interval, such as *+7 st · perfect 5th* or *−7 st · perfect 5th below*, and the interval's number
  lights up. Double-click it to put it back in tune. An oscillator that's tuned shows how far on its card,
  and the harmonics' frequencies beside the sliders are its own.
- **Zoom in on the tune** with the **Zoom** slider just above it, to set small tunings. Drag it right to
  zoom in, from the whole two octaves down to a semitone either side, and left to zoom out; double-click it
  to zoom right out. The figure beside it says how far either side of the middle the scale reaches. Zoomed
  in, the scale shows the part around the whole semitone nearest the tune, so from in tune it shows a
  semitone either side of the note, and from a 5th up a semitone either side of the 5th, for detuning the
  5th slightly. Faint ticks mark tenths, quarters or halves of a semitone, whichever fit. The scale stays
  put while you drag the tune, and follows the tune if it's set from elsewhere, such as by a preset. Zooming
  never changes the tune. Each oscillator has its own zoom, saved with the sound, and it also sets how far
  modulation moves the tune (see below).
- **Snapping.** Dragging the tune snaps to the nearest whole semitone once you're within a few pixels of
  it, so the intervals are easy to land on exactly. As that's a few pixels however far you've zoomed in, a
  slight detune is easiest zoomed in. Hold Option while dragging to tune without snapping.
- **Move the tune** like the other controls, with an LFO, for vibrato either side of the note, an
  envelope, to sweep the pitch with each note, velocity or the mod wheel. The zoom sets how far they can
  move it: at their full range, they move the tune across as much of the scale as the slider shows. Zoomed
  right out, a full range spans the whole two octaves; zoomed right in, to a semitone either side, an LFO at
  100% swings the tune a semitone either side of where it's set, so a range of 10% is a gentle vibrato of a
  tenth of a semitone. Zooming in or out while a sound plays rescales its modulation to match.
- **Glide** into each note with the **Glide** row under the tune slider, separately for each oscillator.
  **Off** plays each note at its pitch straight away. **Glide** slides the oscillator into every note from
  the pitch of the note played before, even one already let go. **Legato** slides only into a note played
  while another key's still held, so playing smoothly glides and playing detached doesn't. A note played
  mid-glide slides on from wherever the glide had got to. The slider beside the buttons sets how long the
  slide takes, from 5 ms to 5 seconds; double-click it for 200 ms. Since each oscillator has its own, one
  can glide while another jumps straight to the note, or two can glide at different speeds.
- **Draw a wave.** Click and drag on the grid. It shows one wavelength, from 0° to 360°, and values from
  −1 to +1. Fast strokes are joined up, so there are no gaps.
- **Draw in steps.** **Draw** at the top right of the grid switches between **Smooth**, freehand drawing,
  and **Steps**, which splits the wave into a number of equal steps, each held at one level, for stepped
  LFOs like the Step Filter preset's, or stepped oscillator waves. Set how many steps, from 2 to 20, with
  the arrows beside it, or the mouse wheel over it. Faint lines mark the steps, and the one under the mouse
  is lit. A click in a step sets its level to where the mouse is, and holding the button down and dragging
  sets each step the mouse passes through, the one it's in following it up and down. A quick drag that
  skips steps sets them along the line between where it was and where it is. Switching to steps, or
  changing how many, leaves the wave as it is until you draw on it. Each oscillator and LFO keeps its own
  way of drawing and number of steps, which are saved with the sound, so the Step Filter preset's LFO opens
  in its 8 steps.
- **Start from a preset.** Sine, Sawtooth, Square or Triangle replace the wave. Draw over one to change it.
- **Pick how many harmonics to play**, 1 to 16, with the slider under *Harmonics* (3 to start with;
  double-click it to go back to 3). The number's shown beside it. The oscillator plays its wave's harmonics
  from the 1st up to that number, and each oscillator has its own number. Harmonics the wave doesn't have
  are still played, at nothing: a square played with 3 has its 1st and 3rd, and its 2nd at 0. Each
  harmonic's row of controls stays full size while they all fit; pick more than fit in the window, and the
  rows, sliders and dials shrink just enough for all of them to.
- **Switch harmonics on and off** with the numbered boxes under the slider, one for each harmonic. A box is
  solid in its harmonic's colour while the harmonic plays, fainter if the wave hasn't got it, and grey while
  it doesn't play. Click a box to switch its harmonic off, or on. A harmonic switched off stays off as the
  slider passes it, and one switched on above the slider stays on as the slider falls below it; switched
  back on within the slider, a harmonic follows the slider again. A harmonic that's off leaves the window:
  its row of controls, and its line on the grid, go, so with the 2nd off the 1st and 3rd are side by side.
  The oscillator's card counts the harmonics playing.
- **Switch a harmonic from a modulator.** Right-click, or Control-click, a box to have an LFO, envelope,
  velocity or the mod wheel switch its harmonic, as **Randomize** can be triggered: each time it crosses
  its **Threshold**, either way, it switches the harmonic off if it's on, and on if it's off. A square LFO
  at 50% switches it every half period. A box can be linked to a MIDI controller too, which switches it as
  it's turned past its threshold.
- **Randomize** at the top right of the harmonics panel gives the oscillator shown a new random wave:
  every one of its 16 harmonics gets a random amplitude and phase, even those it isn't playing yet, so
  raising the number of harmonics brings in more of the same random wave. Amplitudes are spread evenly along
  the sliders' travel, so weak harmonics turn up as often as they would dragging a slider at random, and a
  wave that would go past full scale is turned down to fit the grid. The harmonics' tunings, the number
  played and the other oscillators are left alone. Click it again for another; Cmd-Z undoes it.
- **Randomize from a modulator.** Right-click, or Control-click, **Randomize** to have an LFO, envelope,
  velocity or the mod wheel randomize the oscillator's harmonics for you, each time it crosses a
  threshold, its midline to start with, either way. An LFO crosses at 0, so a looping LFO randomizes twice
  a period, even with no note playing; its phase shifts when. An envelope crosses halfway, as the newest
  note's attack rises past it and again as its release falls; the mod wheel as it's pushed past halfway and
  pulled back. Velocity randomizes when a note's played on the other side of halfway from the note before:
  hard after soft, or soft after hard. In place of a range, each has a **Threshold**, from 0% to 100% of
  the source's values, 50% to start with: the midline. For an LFO that's from -1 to 1 on its wave, so 25% is -0.5; for the
  others it's from 0 to 1. Crossing the threshold either way triggers it, so there's no polarity. At the
  very ends it's reaching them that counts: at 0% an LFO randomizes once at the bottom of each cycle and an
  envelope as each note's release ends, and at 100% an LFO once at each peak and an envelope as each
  attack reaches the top. While something's set to trigger it, the button's text is light blue. The audio thread spots each crossing and the window's
  thread randomizes, a few dozen times a second at most, so an LFO faster than about 30 Hz won't randomize
  on every crossing. An LFO is looked at every 64 samples, and the very ends count from 1% inside them, so
  a smooth wave that only comes close to its lowest or highest still reaches them.
- **Raise or lower each harmonic** with its slider. There's one for every harmonic being played, with its
  amplitude next to it, and once you've played a note, its frequency. A slider changes the wave itself,
  adding or taking away that harmonic's sine wave and leaving the others alone, so the drawing and the
  sliders always agree. Raise a sine's 2nd and 3rd harmonics from 0, for instance, and the wave becomes
  Custom. A harmonic you lower to 0 and raise again comes back as it was. Raised harmonics can take a wave
  past −1 and +1, and the grid stretches to show it.
- **Tune a harmonic** with the first dial beside it, for inharmonic sounds, such as bells, metal and drums,
  whose overtones aren't whole multiples of the fundamental. Every harmonic above the fundamental has one;
  the fundamental can't be tuned. A harmonic tunes all the way from the harmonic below to the one above:
  the 3rd, for instance, from 2 to 4 times the fundamental. In tune is the middle, where
  the dial snaps, and its coloured arc shows which way and how far a harmonic is tuned. Double-click a dial
  to put its harmonic back in tune, and hover over it to see the multiple it plays at. Once you've played a
  note, the frequencies beside the sliders are the tuned ones; until then, a tuned harmonic shows its
  multiple, such as ×2.25. On the grid, a tuned harmonic's sine wave no longer fits the cycle a whole number
  of times, so it doesn't end where it began, and nor does the sum. Each oscillator keeps its harmonics'
  tunes when you choose or draw another wave, and they're saved with the sound. Like the other controls, a
  tune can be moved by an LFO, an envelope or velocity; an envelope's release brings it back into tune.
- **Set a harmonic's phase** with the ring-shaped dial beside its tune dial: where its sine wave starts in the
  cycle, from 0° at the top of the dial round to a whole turn. Every harmonic has one, the fundamental too,
  and the coloured ring fills round as far as the phase goes. The dial has no stops: drag up or right to turn
  it on, down or left to turn it back, and past 0° it carries on round, to 359° and below, or past a whole
  turn back to 0° and beyond. Dragging snaps to each quarter turn, and a
  double-click sets it to 0°; hover over it to see the phase. Turning a harmonic's phase turns its sine wave
  within the wave and leaves the others alone, so the drawing and the grid's lines change with it and the
  wave becomes Custom, though the harmonic is just as loud: a sawtooth with its 2nd harmonic turned a
  quarter turn has the same harmonics but a different shape. A harmonic that's at 0 keeps the phase you give
  it, and comes in at it when you raise it. The phase can be moved by an LFO, an envelope or velocity too,
  and goes round in the same way: an LFO swinging below 0° turns it back past 0° rather than holding it there,
  and an envelope's release brings it back to rest the shorter way round.
- **Read the grid.** The faint band is the wave as drawn, the key's **Target wave**. Each coloured line is one
  harmonic, at its true size and phase, coloured like its slider. The white line is their sum, the key's
  **Actual wave**, which is the wave that oscillator plays.
  Playing more harmonics makes the white line follow the wave more closely.
- **Filter the sound** with the Filters panel, at the right of the row above the keys. It has two filters,
  which shape all the oscillators together, the first and then the second. Each has a switch, a choice of
  type, and a cutoff dial from 20 Hz to 20 kHz. The four types are drawn as the shape of what they let
  through, with low frequencies at the left: a **low pass**, a **mid pass**, a **high pass** and a
  **notch**. Hover over one to see its name.
  - A **low pass** cuts the harmonics above its cutoff, and a **high pass** those below. Beside the cutoff
    is a **resonance** dial, which makes the filter peak at its cutoff, boosting the harmonics there up to 8
    times.
  - A **mid pass** lets through a band of harmonics around its cutoff, and cuts those above and below.
    Beside the cutoff is a **width** dial, from 0.2 to 6 octaves: how far apart the edges of the band are,
    where it lets through half the power. The harmonics in the middle come through as they are, however
    narrow the band. At 2 octaves wide, a mid pass at 880 Hz lets A4's 2nd harmonic through as it is, and
    half the power of its 1st and 4th harmonics, an octave either side.
  - A **notch** takes away a band around its cutoff and leaves the rest. Beside the cutoff are a **Q** dial
    and a **depth** dial.
    - **Q**, from 0.5 to 20, is the cutoff over the notch's bandwidth: the distance in hertz between the
      points either side where a notch at full depth lets through half the power. So a higher Q makes a
      narrower notch. A Q of 1.41 is a bandwidth of an octave, 2.0 is 0.71 octaves, and 4.0 is 0.36
      octaves, by the Audio EQ Cookbook's 2 / ln 2 × asinh (1 / 2Q). Hover over the Q dial to see the
      bandwidth in octaves.
    - **Depth**, from 0 to 60 dB, is how far the notch cuts its cutoff. At 60 dB, where it starts, it takes
      away all but a thousandth: the standard notch, (s² + 1) / (s² + s / Q + 1). At 6 dB it halves the
      cutoff, and at 0 it leaves the sound as it is.

    A notch at 880 Hz with a Q of 1.2 takes away A4's 2nd harmonic, and half the power of its 3rd, at the
    notch's upper edge.

  To begin with, the first filter is a low pass and the second a high pass, both switched out, with cutoffs
  of 1 kHz and 250 Hz that change the sound clearly when you switch them in. Changing a filter's type keeps
  its settings, and while a note plays it fades from the old type to the new over 20 ms, so it doesn't
  click. Each filter keeps its resonance, width, Q and depth, so switching to another type and back leaves
  them as they were. Double-click a dial to reset it. Presets saved before the filters had types, which
  had a low pass and a high pass, load with the first filter as that low pass and the second as that high
  pass, so they sound as they did.
- **Play.** Each key plays the wave with the key's pitch as its fundamental frequency. The header shows the
  last note and its frequency. Click the on-screen keys, or in the standalone app type on the computer
  keyboard: A to ; play the white keys from middle C, and W E T Y U O P play the black keys. Or play a
  MIDI keyboard: see below. The pitch wheel bends two semitones either way, and the sustain pedal holds
  notes. How hard you play can move any control: see *Giving an LFO, envelope, velocity or the mod wheel something to
  move*.
- **Play from a MIDI keyboard in the standalone app.** Plug it in, before or after opening EZ Synth, and
  it plays straight away, with no setting up: the app plays from every MIDI device plugged in, noticing a
  new one within half a second. The **MIDI In** panel beside the keys names the device it's playing from,
  or says how many, and its light flashes as notes, controllers and the pitch wheel arrive. Notes played
  on a MIDI keyboard light up the on-screen keys too. Click the panel to switch each device plugged in on
  or off, or to open the *Audio/MIDI Settings*, which the *Options* button at the top of the window opens
  too. A device you switch off, here or in the settings, stays off, even unplugged and plugged in again or
  after the app quits, until you switch it back on. As a plugin, EZ Synth plays whatever MIDI its host
  sends it, so it has no MIDI In panel.
- **Volume** is the knob at the left of the row above the keys. Double-click it to reset it.

### LFOs

- **An LFO is an oscillator mostly too slow to hear**, whose wave moves controls instead. Its card has a switch,
  a **Rate** dial for how many cycles it runs a second, from 0.1 Hz, a cycle every 10 seconds, to 100 Hz,
  fast enough to roughen a sound or growl, and **Loop** and **Once**, which is a menu. Even at 100 Hz an LFO swings the volume,
  a level or a harmonic as fully as a slow one does, within 1 dB, and a filter's cutoff nearly so, as the
  filter itself takes a moment to respond. Click
  the card to draw its wave on the grid, or pick a preset for it. An LFO follows its drawing exactly, rather
  than rebuilding it from harmonics, so a square LFO switches cleanly between its two values. The grid marks
  the LFO's period in time, and a line shows where the LFO has got to.
- **Sync rate to tempo.** Click an LFO's card, and at the top of the panel beside the grid, a switch syncs its
  rate to your DAW's tempo. The **Rate** dial then sets how many cycles the LFO runs a beat rather than a
  second, snapping to **1/8**, **1/6**, **1/4**, **1/3**, **1/2**, **1**, **2**, **3**, **4**, **6** and
  **8 per beat**, as it shows under the dial. At 1 per beat, a cycle lasts a quarter note in 4/4; at 1/4 per
  beat, a whole bar; at 1/3 and 1/6 per beat, 3 and 6 beats, for bars of 3/4 and 6/8.
  While the DAW plays, a looping LFO follows its beat, so its cycles line up with the beats: at 1/4 per beat
  each cycle starts on a bar. When it's stopped, the LFO runs on at the tempo. The panel shows the tempo it's
  following; the standalone app has no DAW to give one, so it follows 120 BPM. The rate in hertz is kept for
  when it's unsynced. Nothing moves a synced LFO's rate: an LFO, envelope, velocity or mod wheel given its rate
  leaves it alone until it's unsynced.
- **Loop** runs the LFO all the time, the same for every note. **Once** runs it through one period each
  time a key is pressed, separately for each note, then holds its last value: an envelope. Draw a wave that
  rises from −1 to +1 and holds there, for instance, to swell each note in.
- Clicking **Once** opens a menu to run the LFO **Once**, **Twice**, **3 times**, **4 times** or **5 times**
  from each key, or **∞**, forever. The button shows the one chosen. Each starts from the start of the wave
  with each key, as Once does, and runs through it that many times, heading back to the start between times as
  a looping LFO does, then holds the last value. **∞** runs like Loop, except that it starts again with each
  key, separately for each note. With Loop chosen, the button keeps showing the last number chosen, so
  clicking it goes back to that.
- An LFO that's switched off moves nothing. Giving one something to move switches it on.
- **Random** plays a new random wave instead of the LFO's drawn one: every cycle when it loops, and with
  every key when it starts with each key, separately for each note, a new wave each time it runs through. Switch it on with **Random** at the top of the
  grid, beside **Draw**, which sets what kind of random wave it plays:
  - **Smooth** is a curve that rises and falls through random levels between −1 and +1, turning smoothly
    at each, as a curve drawn by hand might. The levels come at random times, from 0.08 to 0.32 of a cycle
    apart, so the rises and falls last different lengths, and the curve turns about three times a cycle.
    When the LFO loops, each cycle carries straight on from where the last one ended, at the same slope, so
    there's no jump between them.
  - **Steps** holds each step at a random level from −1 to +1, every level as likely as any other, the
    first and last steps as much as the others. Set how many steps with the number beside it.

  The grid and the LFO's card show the random wave as it plays, a new one each cycle, or with each key.
  A random wave can't be drawn on, but the LFO keeps its drawn wave, which it plays again when Random is
  switched off, or when you choose a wave such as Sine for it. Phase works as usual: a control shifted
  behind the LFO follows the random wave that much later, even into the cycle before.

### Envelopes

- **The three envelopes**, Env 1 to Env 3, sit between the volume and the filters. Each runs separately for
  every key pressed, with the usual four stages:
  - **Attack**, 0 to 20 seconds: how long it takes to rise from nothing to its peak.
  - **Decay**, 0 to 20 seconds: how long it then takes to fall to the sustain level.
  - **Sustain**, 0 to 100%: the level it holds while the key stays down.
  - **Release**, 0 to 20 seconds: how long it takes to fall to nothing once the key is let go, from wherever
    it was. It takes that long even if the envelope had already fallen to nothing, as what it moves still
    has to come down.

  Each stage is a straight line, and takes as long as it's set to. The picture above the dials shows the
  envelope's shape, with a light blue dot marking where the newest note is in it. Its title line says what
  it moves.
- **While the key is held, an envelope moves a control one way from where it's set**, up by its range at
  the peak (or down, if inverted), and back as it falls to the sustain level.
- **Once the key is let go, the release brings the control steadily down to nothing**, from wherever it was
  at that moment, reaching the bottom of its travel just as the release ends, and staying there. So an
  envelope on the volume fades the note out over its release, whatever the volume knob is set to: with the
  volume at 75% and an envelope on it, the note doesn't fall back to 75% and then stop, it fades all the
  way. The same goes for every control: a filter's cutoff closes, a harmonic fades away, and an
  oscillator's level fades out. An oscillator's tune is the one exception, as the bottom of its travel is an
  octave down: its release brings it back into tune instead.
- **An envelope on the loudness sets how long a note lasts.** While an envelope moves the volume, or an
  oscillator's switch or level, a note goes on sounding until that envelope's release is over, however long.
  Otherwise a note fades over a quarter of a second once its key is let go. With an envelope on one
  oscillator's level, the others, which no envelope shapes, fade over that quarter of a second too, rather
  than playing on until the envelope's release ends. An envelope on anything else, such as a filter's
  cutoff, is cut off when the note ends.

### Giving an LFO, envelope, velocity or the mod wheel something to move

- **Right-click, or Control-click,** almost any control: the volume, an oscillator's switch, level or tune,
  the number of harmonics, a harmonic's slider, tune or phase, or a filter's switch, cutoff, resonance, width, Q or depth, or an LFO's rate. A small window opens,
  in three parts. In the top part you pick an LFO or envelope to move the control; in the middle part you
  switch on **Velocity** to have how hard each key is played move it too; and in the bottom part you switch
  on **Mod wheel** to have a MIDI controller's mod wheel move it. Each part has its own **range**, the top
  part a **phase** and **polarity** too, and the mod wheel a **polarity**. All three can move the same
  control at once, their movements adding up.
  - **Moved by** None, LFO 1 to 3, or Env 1 to 3. A control can be moved by one of these at a time, as well
    as by velocity and the mod wheel.
  - **Range** is how far the control moves, from one end of the source to the other, as a share of its
    travel. An LFO's wave swings the control either side of where it's set: at 0 the control is where you
    set it, and at +1 and −1 it's at either end of its range. An envelope moves it one way, by its whole
    range at the peak. Nothing moves a control past the ends of its travel.
  - **Velocity** leaves the control where it's set for the hardest notes, and moves it down for softer ones,
    by its whole range for the softest. So what you hear when you click the on-screen keys, which always play
    as hard as they can, is what the controls say. To make softer notes darker, for instance, set the low
    pass cutoff where you want the hardest notes, and give it velocity with a range of 30% or so.
  - **Mod wheel** leaves the control where it's set with the wheel at the bottom, and moves it up as the wheel
    is pushed up, by its whole range at the top, or down if its polarity is **Inverted**. It follows the
    wheel as you move it, during notes as well as between them, and a new note starts with the wheel where
    it was left. It's MIDI controller 1, which most keyboards' mod wheels send, so in a DAW the wheel can be
    recorded and drawn like any other MIDI. To open a filter with the wheel, for instance, set its cutoff
    low and give it the mod wheel with a range of 60% or so.
  - **An LFO's rate** can be moved too: right-click the **Rate** dial on its card. Another LFO, an envelope,
    velocity and the mod wheel can each move it, faster as they rise, or slower if **Inverted**. The dial runs
    from 0.1 to 100 Hz, so each 10% of range is about twice as fast: give the mod wheel 10% for it to double
    the rate at the top, for a vibrato or tremolo that quickens as you push it. An envelope or velocity moves
    each note's LFO on its own, so a vibrato can speed up as each note goes on, or run faster for harder notes;
    a looping LFO then carries on from where it was when the note started, at that note's rate. An envelope's
    release brings the rate back to where it's set. An LFO speeds up and slows down from wherever it's got to,
    so it never jumps. The dial shows the rate it's been moved to in light blue, as the newest note has it, and
    the rate under it, in Hz, changes to match.
  - **No LFO loops.** An LFO can't move its own rate, nor the rate of an LFO that moves its rate, directly or
    through a third: if LFO 2 moves LFO 1's rate, LFO 1 can't move LFO 2's. Those LFOs are greyed out in the
    window, which says why.
  - **Phase**, for an LFO, shifts its wave to the right for this control alone, from 0°, in step with the
    LFO, up to 359°, a degree short of a whole period. The control follows where the LFO was that much of a
    period ago, so at 90° it peaks a quarter of a period after the LFO does. As the wave repeats, a phase
    just short of 360° is the same as shifting it slightly to the left: at 270° the control peaks a quarter
    of a period before the LFO does. Give two controls on the same LFO different phases and they move out of
    step. For an LFO that starts with each key, a phase is a delay: after each key, the control waits where the
    wave starts for that share of the period, then runs through the wave. Double-click the slider to put it
    back to 0°. Envelopes, velocity and the mod wheel have no phase.
  - **Normal** polarity raises the control as the LFO or envelope rises. **Inverted** lowers it. Velocity has
    no polarity: softer notes always move a control down.
  - Choose **None**, or switch velocity or the mod wheel **Off**, to stop it being moved.
- **Velocity starts out on the volume**, with a range of 35%, so softer notes are quieter, as on most
  instruments. Switch it off there for every note to play as loud as the hardest, or move it elsewhere.
- **See them at work.** A control that's being moved shows where it's been moved to, in light blue, as a
  dot and a stretch from where it's set, following the newest note and how hard it was played. An LFO's card lists everything it
  moves, with each one's range, phase and polarity. Click one to change it, or its cross to stop the LFO
  moving it.
- **Watch the wave change.** While anything moves an oscillator's harmonics, or how many it has, the grid
  draws its sine waves, and their sum, where they've been moved to, many times a second, and says
  **As played** in light blue. The drawn wave stays as you drew it, so you can see how far the sound has
  moved from it. The oscillator's card above follows along too. The grid stretches to fit a harmonic
  raised beyond it, and holds that scale while the harmonics swing, so the picture doesn't jump.

### Modulation overview

- **See everything that moves a control** on one screen: click **Modulation** in the header, beside
  **Save**, or in the standalone app, choose **Modulation Overview...** from the **Options** menu at the top
  left of its window. It lists every link, grouped by
  what it comes from: each LFO and envelope, velocity, the mod wheel, and the MIDI controllers learned.
  Each row names the control and says what's done to it, such as *±20% of its travel, inverted, 90°
  behind* or *CC 74, across its whole travel*.
- **Click a row** to open the control's modulation editor beside it, as right-clicking the control does,
  or its **×** to take that link off, leaving anything else on the same control. **Done**, or Escape, or
  choosing the menu item again, goes back to the synth. The list follows changes as they're made.

### MIDI learn

- **Link a control to a knob or slider** on a MIDI controller: right-click, or Control-click, the control,
  click **Learn** at the foot of the window that opens, and move the knob. The control follows it from then
  on, across its whole travel, from 0 at one end to 127 at the other. The window shows which controller it
  is, such as *CC 74*; click **Learn** again before moving anything to cancel, or **Forget** to unlink it.
- **Almost anything can be linked**: every control that an LFO can move, from the volume and the filters to
  each harmonic's amplitude, tune and phase. A synced LFO's rate dial steps through its cycles a beat.
  Linked to **Randomize**, or a harmonic's switch, a knob triggers it each time it's turned past its
  **Threshold**, either way. The threshold's slider is under the MIDI CC row. It starts at 100%, where only
  turning the knob right up counts, and at 0% only turning it right down does. It's kept with the link.
- **See what's linked** in the **Modulation** overview, which lists every control linked to a MIDI
  controller, with its number, such as *CC 74*, as well as in each control's own right-click window.
- **One control for each controller.** Linking a controller to a second control unlinks it from the first.
  Controllers 120 and up, such as All Notes Off, can't be learned. The mod wheel, controller 1, can, though
  it also goes on moving whatever it's set to move as a modulator.
- **The links belong to this copy of EZ Synth**, rather than the sound: a host saves them with its project,
  and they stay as presets are loaded and changes are undone. A controller sets its control a few dozen
  times a second, from the window's thread, and its moves aren't steps of their own to undo.

### Undo

**Command-Z** undoes the last change to the sound, whatever it was: a knob, a slider, a switch, a drawn wave,
what an LFO, envelope or velocity moves, or loading a preset. Press it again to go further back.
**Shift-Command-Z** redoes what was undone. A drag or a scroll is one step, however far it went. Making a
change after undoing replaces what was undone. EZ Synth remembers the last 200 steps. Changes made by your
DAW's automation aren't steps of their own, so undoing goes back to the last change you made.

In a plugin, click in EZ Synth's window first so the keys come to it rather than to your DAW.

### Presets

- **Save** in the header saves the whole sound as a preset: every oscillator's and LFO's wave and settings,
  the harmonics, the envelopes, the filters, the volume, and what the LFOs, envelopes and velocity move. It
  suggests the current preset's name, so saving again over it is quick.
- **The preset's name** opens a menu of the presets, to load one: **Init**, the sound EZ Synth starts with,
  then the factory presets, then your own. Init puts every control back as it starts: a sine, a square and a
  sawtooth with 3 harmonics each, only the sine on, every oscillator and harmonic in tune and every
  harmonic's phase as its wave has it, each wave drawn smoothly, the filters a low pass and a high pass, both
  off, the LFOs off and not random, the envelopes as they start, and nothing moved by an LFO or envelope.
  Velocity still makes softer notes quieter, as it starts out doing. It also has a way to open a preset file from anywhere, and a way
  to show the presets folder in Finder.
- **The arrows** either side of the name step back and forth through the factory presets, then your own,
  each in order of name.
- A dot after the name means the sound has changed since the preset was loaded or saved.
- Presets are kept in `~/Documents/EZ Synth/Presets`, as `.ezsynth` files. They're readable XML, so they
  can be copied, shared or renamed in Finder; a preset is named after its file.

#### Factory presets

EZ Synth comes with eleven presets, built into the app and the plugins. Each one shows off something different:

| Preset | What it shows |
| --- | --- |
| Bell | A wave made from its harmonics, an envelope on the 9th harmonic that lights it up for a moment as each note strikes, and an envelope on the volume for a long ring |
| Brass | An envelope opening the low pass filter with each note, and velocity on the cutoff, so softer notes are darker |
| Electric Keys | Velocity alongside envelopes: softer notes are quieter, darker and lose the triangle's bark, while the volume dies away and the 4th harmonic pings |
| Grand Piano | As near a grand piano as EZ Synth gets: two strings 2 cents apart that beat gently, a bright strike of upper harmonics that fades within a second, a low pass that mellows the note as it rings, a ring drawn on an LFO run once, falling quickly and then slowly, and velocity on the strings' levels and the cutoff, so soft notes are quieter and darker but ring as long |
| Harmonic Sweep | One LFO moving two controls: the number of harmonics of a sawtooth, drawn falling, and of a square, inverted, so one brightens as the other darkens |
| Organ | A sine with raised harmonics, like drawbars, a tremolo, a click on each key, and velocity taken off the volume |
| Pluck | An oscillator at no level that an envelope plucks, with a low pass filter that snaps shut after it |
| Soft Pad | All three oscillators swelling in through a low pass filter that one LFO slowly opens and closes, while another makes one shimmer |
| Square Lead | Both filters, and an LFO run once per note, drawn to wait and then rise, bringing a sawtooth in under held notes |
| Step Filter | An LFO drawn as eight steps, moving a resonant low pass filter like a step sequencer |
| Wobble Bass | A square and a sine through a resonant low pass filter that an LFO wobbles |

A factory preset can't be saved over. **Save** suggests saving a copy, under the same name, among your own
presets, where you can change it as you like.

The factory presets are made by `Tools/MakeFactoryPresets.cpp`, which builds each one up from Init with the
same calls the window makes, and saves it into `Presets/`. To change one, change its recipe, run the tool
from the repository's folder, and build again:

```bash
./build/EZSynthMakeFactoryPresets
```

To add a preset, add its recipe, and its file to the list in `CMakeLists.txt`. As the tool is built with the
presets in it, make an empty file for the new one first, so the tool can be built to fill it in:

```bash
touch "Presets/New Preset.ezsynth"
```

To hear a preset without opening the app, `Tools/RenderPreset.cpp` plays it through a short phrase, an
arpeggio, a held chord, a note played softly and then hard, and a high note and a low one, and saves it as a
WAV file. Name a factory preset, or a preset's file:

```bash
./build/EZSynthRenderPreset "Grand Piano" piano.wav
```

Everything is also saved with your DAW project, including which preset the sound came from and whether it's
been edited since. Everything except the waves and what moves what can be automated.

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

The app's icon is three sine waves of the same height, the 1st, 2nd and 3rd harmonics over one wavelength,
in red, yellow and cyan. `Tools/MakeIcon.cpp` draws it into `Resources/Icon.png`, which the build turns into the
app's icon, and the plugins' too. To change it, edit the drawing, run `./build/EZSynthMakeIcon` from the
repository's folder, and build again: a new icon has the build configure again, as JUCE makes the icon then,
and sign and install the app and plugins with it. If the Finder or the Dock still shows the old icon, quit
the app and open it again.

## Testing

```bash
./build/EZSynthTests
```

This checks that the presets break down into their textbook harmonics, that notes come out at the right
pitch with the right harmonics, that raising and lowering a harmonic changes it alone, that a tuned harmonic
plays at its own multiple of the fundamental, glides there without a click and is saved, that turning a
harmonic's phase turns only its sine wave, as heard, that oscillators add
together at their levels, that a tuned oscillator plays as far above or below the note as it's tuned, glides there
without a click and leaves out the harmonics too high for its pitch, that the filters cut and boost each harmonic by as much as theory says, as low, mid and high passes and notches,
that a filter's type can change during a note without a click, that saves from before the filters had types
load as they were, that LFOs
move each kind of control by the range and in the direction they're set to, looping or once per note, and
as far behind as their phase says, that
envelopes run each stage for as long as it's set, separately for each note, that a note lasts through
the release of an envelope on its loudness, that a release fades what it moves steadily to nothing from
wherever it was, without a sudden stop, while oscillators no envelope shapes fade as usual, that velocity moves controls by its range, alongside an LFO, that
the grid shows harmonics where LFOs, envelopes and velocity have moved them, as strong as they're heard, that
the standalone app plays from MIDI devices as they're plugged in and remembers those switched off, with a
virtual MIDI keyboard's notes arriving through the Mac's own MIDI system, that
presets save and load the whole sound, that every factory preset loads and plays a note that's heard without
clipping, that the arrows step through the factory presets, that the Grand Piano strikes, rings, mellows, damps and
answers velocity as a piano does, that random LFOs stay smooth, joined up and natural,
or spread their steps evenly, and play a new wave every cycle or key, that a click or a drag on the grid sets steps, and
that each wave's way of drawing is saved, that high notes leave out harmonics too high to play cleanly, that changing the
wave during a note doesn't click, and that saved settings load back. Add `--snapshots <folder>` to also
save pictures of the window showing several waves.

## How it works

- **Breaking the wave down.** A wave is 512 evenly spaced points. A discrete Fourier transform finds the
  amplitude and phase of its harmonics 1 to 16 (`Source/Waveform.cpp`). Any offset from zero is left out, as
  it can't be heard, and anything 100 dB below full scale counts as 0.
- **Changing a harmonic.** Harmonics on 512 points are independent, so adding a sine wave at harmonic k to
  the wave changes harmonic k and nothing else. A slider adds the difference between the amplitude it's
  set to and the one the wave has.
- **Playing it.** Each voice adds up the chosen harmonics as sine waves (`Source/HarmonicVoice.cpp`).
  An oscillator plays its harmonics at their amplitudes times its level. If they add up to more than full
  scale, as a square's do, the oscillator turns them down to fit. Each oscillator keeps its own phase and
  plays at the note's frequency times 2 to the power of its tune in octaves, so oscillators tuned apart
  drift in and out of step as they should. A tuned harmonic turns at its own speed, its number plus its tune
  times the fundamental's, from where its whole number's phase would be plus how far it's drifted from it, so
  one in tune stays exactly in its place in the wave. Only the harmonics that are sounding are worked out. Harmonics
  fade out between 16 and 19 kHz, worked out at each oscillator's own pitch, so high notes don't alias.
  When a wave, level, switch, tune, harmonic's tune or number of harmonics changes during a note, the voice glides to the
  new sound, most of the way in 1.5 ms and all of it in about 6, rather than clicking, quickly enough to follow
  an LFO at 100 Hz. The volume glides over 1.5 ms. Notes
  start in 8 ms and fade out over 250 ms when let go.
- **Filtering.** Each note goes through its own two filters, the first and then the second
  (`Source/FilterStage.cpp`), so an LFO that runs once can sweep each note's filter separately. Each is a
  state variable filter, 12 dB an octave, made with the topology-preserving transform, which works out the
  low pass, band pass and high pass at once, so changing type fades from one to the other over 20 ms. A mid
  pass is the band pass, scaled down by its Q so its middle comes through as it is; its width of N octaves
  sets its Q to √(2^N) / (2^N − 1). A notch is the sound less the mid pass's band, times its depth's share,
  1 − 10^(−depth / 20), so at full depth it's the low pass and high pass added together. Switching a filter
  fades it in or out over 20 ms, and its cutoff, resonance, width, Q and depth glide over 1.5 ms, so nothing
  clicks. A filter that's switched out is skipped
  entirely. Cutoffs too near the Nyquist frequency, as 20 kHz is at low sample rates, are lowered to 45% of
  the sample rate.
- **Random waves.** A random LFO wave is worked out from a seed and the time, in cycles, so any moment of it
  can be found at once, the same each time (`Source/RandomWave.cpp`). Its levels come from the SplitMix64
  hash of the seed and their number. Smooth, level k is at a random time in the middle 60% of the k-th fifth
  of a cycle, and the curve joins the levels with Fritsch and Carlson's monotone cubic, which turns flat at
  each peak and trough and never overshoots, so it stays between −1 and 1. A looping LFO counts its cycles
  and plays its own seed's wave at the cycle and phase it's at, so each cycle is new and runs on from the
  last. One that starts with each key gives each note a new seed, and plays its cycles from the first, holding
  the end of the last if it plays a number of times.
- **Modulation.** Every 64 samples, each note works out where the LFOs and its envelopes are, and where
  they've moved its controls (`Source/HarmonicVoice.cpp`). A looping LFO carries on from where it was at
  the start of the block; one that runs once, and each envelope (`Source/Envelope.cpp`), counts from the
  note's start. A control given a phase takes the LFO's value that share of a period earlier; an LFO that
  runs once holds its first value until then. A control's position along its travel, from 0 to 1, moves by
  its range times half an LFO's value, or times an envelope's whole value, by its velocity range times how far short of full velocity
  the note was played, and by its mod wheel range times how far up the wheel is. The movements add up before the control is kept within its travel. Once the key is
  let go, a control an envelope moves goes from where it was then to its rest, the bottom of its travel or in
  tune for a tune, in proportion to how much of the release is left. Cutoffs travel by octave, and harmonic sliders give weak
  harmonics more of their travel, just as their dials and sliders do (`Source/Modulation.cpp`). The note
  then glides its harmonics, filters and volume there, so even a square LFO doesn't click. The window
  shows the same positions for the latest note, on the controls and in the grid's sine waves.
- **Between the threads.** The window analyses the drawing on the message thread. The analysis reaches the
  audio thread through a lock that the audio thread only ever tries, so audio never waits on drawing.

| File | What's in it |
| --- | --- |
| `Source/Waveform.*` | The drawn shape, the presets and the Fourier analysis |
| `Source/HarmonicVoice.*` | A voice playing a note: its harmonics, filters and LFOs |
| `Source/SynthProcessor.*` | The plugin: the oscillators, parameters, MIDI, saving and loading |
| `Source/SynthEditor.*` | The window and its layout |
| `Source/SourceCard.*` | The oscillators' and LFOs' cards |
| `Source/Tuning.*` | Semitones, the intervals they make, snapping to them, and the part of the scale zoomed in on |
| `Source/TuneSlider.*` | The tune slider, with its intervals marked, and the zoom slider above it |
| `Source/Modulation.*` | The controls LFOs can move, and how an LFO moves one |
| `Source/Patch.h` | Everything the notes need to play a block |
| `Source/ModulatableControls.*` | Dials, sliders and switches that open the modulation editor on a right-click |
| `Source/Envelope.*` | One note's run through an envelope's attack, decay, sustain and release |
| `Source/EnvelopePanel.*` | An envelope's dials and the picture of its shape |
| `Source/ModulationOverview.*` | The screen listing everything that moves a control, or is linked to one |
| `Source/ModulationEditor.*` | The window for choosing an LFO or envelope, velocity and the mod wheel, with their ranges, phases and polarities |
| `Source/LfoPanel.*` | The list of what an LFO moves |
| `Source/ChoiceButtons.*` | Rows of joined buttons, such as Loop and Once |
| `Source/PresetBar.*` | Loading, stepping through and saving presets |
| `Presets/` | The factory presets, which the build turns into code with `juce_add_binary_data` |
| `Tools/MakeFactoryPresets.cpp` | The factory presets' recipes, and the tool that saves them |
| `Tools/RenderPreset.cpp` | The tool that plays a preset through a phrase into a WAV file |
| `Tools/MakeIcon.cpp`, `Resources/Icon.png` | The tool that draws the icon, and the icon it draws |
| `Source/FilterPanel.*` | The filters' switches, types, cutoff dials, and resonance, width, or Q and depth dials |
| `Source/FilterStage.*` | A low, mid or high pass or notch filter that switches, changes type and sweeps without clicks |
| `Source/WaveGrid.*` | The grid you draw on, smoothly or in steps |
| `Source/NumberStepper.*` | A number with arrows to step it, for the number of steps |
| `Source/RandomWave.*` | Random LFO waves, smooth or in steps, worked out from a seed and the time |
| `Source/HarmonicSelector.*` | The slider for how many harmonics to play, with their numbers under it |
| `Source/HarmonicSliders.*` | A slider and tune and phase dials for each harmonic, and the key to the grid's lines |
| `Source/Keyboard.*` | The on-screen piano |
| `Source/StandaloneApp.cpp` | The standalone app, which only the standalone build includes, playing from every MIDI device plugged in |
| `Source/MidiInputs.*` | The standalone app's MIDI devices: switching them on as they're plugged in, and remembering those switched off |
| `Source/MidiPanel.*` | The standalone app's MIDI In panel, beside the keys |
| `Source/SynthLookAndFeel.*` | Colours, fonts, the knobs, sliders, switches and buttons |
| `Tests/Tests.cpp` | The tests and the window pictures |
