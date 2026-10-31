# Beads manual errata

Differences between the [Mutable Instruments Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/)
and vanilla firmware. Drizzlet keeps these unless noted; its additions are in
[README](../README.md).


## Corrections

- **Sunny tape buffer:** holds about 10.67 s in stereo or 21.33 s in mono, rather than 10 or 20 s.
- **Restarting automatic gain** (Recording quality): hold [A] for about **1.5 s**, not one second; releasing earlier changes the recording quality. With Drizzlet's wavetable synth Manual setting, both inputs unpatched and FREEZE off, this hold starts or stops wavetable synth instead.
- **Synth quality table** (Beads as a granular wavetable synth): the amber setting is **16-bit dry, 6-bit grains**, not 4-bit.
- **Update error** (Firmware update procedure): the [A] and input-level LEDs keep showing the input level; the other LEDs blink.

## Changed by the April 2021 update

- **Unfreezing:** recording resumes with a short crossfade into the current input. It does not replay the old buffer tail.
- **Synth banks:** FEEDBACK (and its assigned CV) selects **12 banks** of 16 waveforms, not 8; four FM banks were added.
- **Shortest grains:** grains near the reverse/forward point of SIZE (about 10:30) can be much shorter than 30 ms: about 8 ms requested, 10–20 ms heard depending on SHAPE.
- **Autosave toggle:** hold the CV-assign button [M] and press FREEZE to turn buffer autosave on or off. The FREEZE LED shows dim for off, brighter for on. With it off, a saved buffer is not loaded at power-up.
- **Delay bandwidth:** in delay and slice mode, the SIZE attenurandomizer is a filter: counterclockwise darker, centre unfiltered, clockwise thinner. SIZE CV also moves it.
- **Overwrite indicator:** while recording, the quality LED fades as the record head approaches the point where FREEZE was last released, then brightens past it. Not in wavetable synth mode.
- **Manual gain display:** while setting manual gain ([A] + FEEDBACK), the quality LED becomes a green-to-red gain indicator.
- **Manual gain clipping:** with manual gain, overloads hard-clip in the digital modes, soft-clip in Scorched cassette, and use a blend in Sunny tape.
- **Granular/delay switch:** turning SIZE into or out of delay fades the wet signal out and back in, taking about 1.5 s.
- **Reverb at minimum:** turning REVERB fully counterclockwise stops sending signal into the reverb; the tail already in it fades out instead of being cut.
- **Clocked probability:** with a clock in SEED, DENSITY from 12 o'clock to fully clockwise sets the trigger probability on a quadratic curve, so sparse grains are easy to get even from a fast clock.

## Not in the manual

- **Power-on reset:** holding SEED at power-on resets saved settings to defaults; calibration is kept. In Drizzlet this also resets every option, including monophonic grains, to its default: stock in the standard firmware, or the Hygrostat profile's. On a module showing the lost-calibration warning (all LEDs blinking for 3 s at power-up), it repairs the settings flash instead; see the README.
- **Erase chord:** FREEZE and [M] held at power-on erase the firmware and wait for an update. FREEZE alone enters the updater without erasing. Use only for recovery.
- **Calibration:** pressing [A] and [M] together starts CV calibration, which needs precise reference voltages and overwrites the stored calibration. Avoid it unless recalibrating; see [CALIBRATION.md](CALIBRATION.md).
- **Saved choices:** recording quality, [M] routing and amounts, the R trigger-output mode and autosave are remembered across power cycles, not just input gain.
- **Buffer save takes time:** 10 s of FREEZE starts the save, and the FREEZE LED dims while it runs. Keep FREEZE on until it finishes, or the buffer is not restored. In Drizzlet, a save cut short makes the next wait longer, up to 320 s (see README.md).
- **Quality changes while frozen** are remembered and shown, and take effect when you unfreeze.
- **Knobs after hidden edits:** after setting gain or an [M] amount, the knob catches up gradually with its normal parameter instead of jumping.
- **Synth controls:** in wavetable synth mode TIME scans the bank's 16 waveforms, SIZE fully clockwise gives long grains (not delay), and counterclockwise SIZE reverses nothing.
- **Slow clocks in delay mode:** clock or tap intervals of 4 s or more are ignored; the last accepted tempo (initially 0.25 s) stays.
- **Fast clocks:** with SEED clocked above about 48 Hz and DENSITY fully accepting, grains follow a smoothed internal rhythm rather than each edge.
- **Randomness with CV:** with a cable patched and the attenurandomizer counterclockwise, only positive CV adds randomness, but grain panning stays random.
- **R trigger output** is a gate whose length depends on how grains are generated, not a fixed pulse. In wavetable synth mode it keeps running while FREEZE stops new grains.
- **SEED input delay:** external SEED triggers are delayed about 2.5 ms on purpose; the 0.25 ms latency on the module page is not the trigger response.
- **Pitch CV range:** PITCH tracks ±5 V (±60 semitones); the ±8 V on the module page is the input range, not the tracking range.
- **Output polarity:** OUT is inverted relative to IN, by the hardware's output op-amp stage, not the firmware. Invert downstream if phase alignment with the dry source matters.
- **Updater start-up:** the updater ignores the first 2 s or so of audio, so start playback a moment after entering it.

## Clarifications

- **Scorched cassette resolution:** the 8-bit recording uses µ-law compression, which preserves quiet sounds better than linear 8-bit recording.
- **Automatic gain** waits about 1 s, then listens for 5 s, and keeps adapting afterwards; silence postpones it. Restarting with [A] skips the wait.
- **Hiss:** automatic gain raises the input by up to 40 times (about +32 dB), and the input's own noise with it. For quiet sources set the gain by hand ([A]+FEEDBACK). Silent desktop input gave silence; Clouds-style carried 2–7 dB more input
  noise than stock. Module noise is unmeasured.
- **Input limiter:** the limiter for big level changes is the automatic gain itself. A sudden jump can briefly pass above full scale; the gain then falls in about 0.2 s (0.1 s while still listening).
- **mix LEDs** (Mixing and audio output): the LEDs under FEEDBACK, DRY/WET and REVERB show the [M] setting: half lit for a single destination, each LED's brightness for individual amounts. They do not follow the incoming CV.
- **DENSITY extremes:** fully counterclockwise gives a steady C3 rate (about 131 grains/s); fully clockwise is random with a lower average, not a steady C3.
- **Gated SEED at noon:** the button and the SEED input are combined, so a trigger while the button is held does not start another grain.
- **Entering wavetable synth mode:** the 10 s countdown only runs while the output is silent and FREEZE is off; lingering feedback or reverb postpones it. Drizzlet's Wavetable synth Manual setting turns the countdown off for an [A] hold.
- **Feedback schemes:** Cold digital and Sunny tape share the same feedback saturation; they still sound different through sample rate and dry path.
- **"Gaussian" randomness** is bell-shaped but bounded.
