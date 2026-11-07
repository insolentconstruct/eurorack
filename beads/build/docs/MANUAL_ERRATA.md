# Beads manual errata

**TL;DR:** places where the Beads manual differs from the firmware, such as the 1.5 s hold of the quality button [A] for automatic gain and 12 synth banks rather than 8.

Where the [Mutable Instruments Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/) and module page differ from the vanilla firmware source. This branch keeps all of it; its own changes are in [README-BASE.md](../../../README-BASE.md). Several items come from the April 2021 update, which the manual predates ([release notes](https://forum.mutable-instruments.net/t/beads-firmware-update-release-candidate/18572)).
<!-- Source: Mutable Instruments eurorack d8f4739 (stmlib e3bd7c9); manual.md and index.md from the mutable-instruments-documentation repository. Each item cites manual line and source. -->

## Corrections

- **Sunny tape buffer:** holds about 10.67 s in stereo or 21.33 s in mono, rather than 10 or 20 s. <!-- manual.md:34, images/quality_table.png; beads/dsp/processor.h:50-51,179-182; beads/dsp/processor.cc:49; beads/dsp/granular_processor.h:384-385 -->
- **Restarting automatic gain** (Recording quality): hold [A] for about **1.5 s**, not one second; releasing earlier changes the recording quality. <!-- manual.md:44; beads/ui.cc:48-50,402-425 -->
- **Synth quality table** (Beads as a granular wavetable synth): the amber setting is **16-bit dry, 6-bit grains**, not 4-bit. <!-- manual.md:190, images/wavetable_quality_table.png; beads/dsp/synth.cc:83-84,112-118; beads/dsp/synth.h:50-54; beads/dsp/wavetable_grain.h:142 -->
- **Update error** (Firmware update procedure): the [A] and input-level LEDs keep showing the input level; the other LEDs blink. <!-- manual.md:253; beads/bootloader/bootloader.cc:118-145 -->

## Changed by the April 2021 update

The update's [release notes](https://forum.mutable-instruments.net/t/beads-firmware-update-release-candidate/18572) are Mutable Instruments' list of changes from the factory firmware (the [beads-build-kit changelog](https://github.com/insolentconstruct/eurorack/blob/beads-build-kit/beads/build/CHANGELOG.md) has a summary). These are the ones the manual contradicts or leaves out.

- **Unfreezing:** recording resumes with a short crossfade into the current input. It does not replay the old buffer tail. <!-- manual.md:47; beads/dsp/delay_line.h:180,296-322; release notes post 1 -->
- **Synth banks:** FEEDBACK (and its assigned CV) selects **12 banks** of 16 waveforms, not 8; four FM banks were added. <!-- manual.md:182; beads/dsp/synth.h:43; beads/dsp/synth.cc:82; release notes post 1 -->
- **Shortest grains:** grains near the reverse/forward point of SIZE (about 10:30) can be much shorter than 30 ms: about 8 ms requested, 10–20 ms heard depending on SHAPE. <!-- manual.md:91; index.md:51; beads/dsp/granular_player.h:97-104; beads/dsp/grain_envelope.h -->
- **Autosave toggle:** hold the CV-assign button [M] and press FREEZE to turn buffer autosave on or off. The FREEZE LED shows dim for off, brighter for on. With it off, a saved buffer is not loaded at power-up. <!-- manual.md:49; beads/ui.cc:195-196,276-285; beads/background_buffer_backup.cc:54-56,154-158 -->
- **Delay bandwidth:** in delay and slice mode, the SIZE attenurandomizer is a filter: counterclockwise darker, centre unfiltered, clockwise thinner. SIZE CV also moves it. <!-- manual.md:93,102,174; beads/dsp/granular_processor.h:280-282,329-351; beads/dsp/multitap_delay.h:38; release notes post 1 -->
- **Overwrite indicator:** while recording, the quality LED fades as the record head approaches the point where FREEZE was last released, then brightens past it. Not in wavetable synth mode. <!-- manual.md:32; beads/beads.cc:156; beads/dsp/processor.h:116-119; beads/ui.cc:189-192 -->
- **Manual gain display:** while setting manual gain ([A] + FEEDBACK), the quality LED becomes a green-to-red gain indicator. <!-- manual.md:44; beads/ui.cc:152-153,186-192 -->
- **Manual gain clipping:** with manual gain, overloads hard-clip in the digital modes, soft-clip in Scorched cassette, and use a blend in Sunny tape. <!-- manual.md:44; beads/beads.cc:281-287; beads/gain_control.cc:133-181 -->
- **Granular/delay switch:** turning SIZE into or out of delay fades the wet signal out and back in, taking about 1.5 s. <!-- manual.md:93,144; beads/dsp/processor.cc:37-38,112-119,205-222 -->
- **Reverb at minimum:** turning REVERB fully counterclockwise stops sending signal into the reverb; the tail already in it fades out instead of being cut. <!-- manual.md:127; beads/dsp/fx/reverb.h:98-101 -->
- **Clocked probability:** with a clock in SEED, DENSITY from 12 o'clock to fully clockwise sets the trigger probability on a quadratic curve, so sparse grains are easy to get even from a fast clock. <!-- manual.md:69; beads/dsp/trigger_generator.cc:135-138 -->

## Not in the manual

- **Power-on reset:** holding SEED at power-on resets saved settings to defaults; calibration is kept. On this branch, on a module showing the damaged-settings warning (all LEDs blinking for 3 s at power-up), it repairs the settings flash instead; see [README-BASE.md](../../../README-BASE.md). <!-- beads/beads.cc:327-328; beads/settings.cc:43-75; stmlib/system/sector_storage_h7xx.h:126-130 -->
- **Erase chord:** FREEZE and [M] held at power-on erase the firmware and wait for an update. FREEZE alone enters the updater without erasing. Use only for recovery. <!-- beads/bootloader/bootloader.cc:28,266-281,315-322 -->
- **Calibration:** pressing [A] and [M] together starts CV calibration, which needs precise reference voltages and overwrites the stored calibration. Avoid it unless recalibrating. <!-- beads/ui.cc:320-328,358-384; beads/calibrator.h:91-118 -->
- **Saved choices:** recording quality, [M] routing and amounts, the R trigger-output mode and autosave are remembered across power cycles, not just input gain. <!-- beads/ui.cc:261-264,276-279,416-418,437-444; beads/settings.cc -->
- **Buffer save takes time:** 10 s of FREEZE starts the save, and the FREEZE LED dims while it runs. Keep FREEZE on until it finishes, or the buffer is not restored. On this branch, a save cut short makes the next wait longer, up to 320 s (see README-BASE.md). <!-- manual.md:49; beads/background_buffer_backup.cc:42,54-81,136-158; beads/ui.cc -->
- **Quality changes while frozen** are remembered and shown, and take effect when you unfreeze. <!-- manual.md:51; beads/ui.cc:416-418; beads/beads.cc:351-355,382-391 -->
- **Knobs after hidden edits:** after setting gain or an [M] amount, the knob catches up gradually with its normal parameter instead of jumping. <!-- beads/pot_controller.h:87-92,112-146; beads/ui.cc:425,431-444 -->
- **Synth controls:** in wavetable synth mode TIME scans the bank's 16 waveforms, SIZE fully clockwise gives long grains (not delay), and counterclockwise SIZE reverses nothing. <!-- manual.md:87,144,180; beads/dsp/wavetable_grain.h:124-146; beads/dsp/wavetable_voice.h:89-94; beads/beads.cc:244-259; beads/dsp/synth.cc:72-80 -->
- **Slow clocks in delay mode:** clock or tap intervals of 4 s or more are ignored; the last accepted tempo (initially 0.25 s) stays. <!-- manual.md:158; beads/dsp/delay_clock.cc:38-40,59-85 -->
- **Fast clocks:** with SEED clocked above about 48 Hz and DENSITY fully accepting, grains follow a smoothed internal rhythm rather than each edge. <!-- manual.md:69; beads/dsp/trigger_generator.cc:43-54,149-217 -->
- **Randomness with CV:** with a cable patched and the attenurandomizer counterclockwise, only positive CV adds randomness, but grain panning stays random. <!-- manual.md:102; beads/dsp/randomized_parameter.h:96-101,128-133; beads/dsp/granular_player.h:85-94,124-126 -->
- **R trigger output** is a gate whose length depends on how grains are generated, not a fixed pulse. In wavetable synth mode it keeps running while FREEZE stops new grains. <!-- manual.md:138,205; beads/dsp/trigger_generator.cc:115-118,215-224,305-307; beads/dsp/processor.cc:145-159 -->
- **SEED input delay:** external SEED triggers are delayed about 2.5 ms on purpose; the 0.25 ms latency on the module page is not the trigger response. <!-- index.md:84; beads/beads.cc:50,133-148,293; stmlib/dsp/delay_line.h -->
- **Pitch CV range:** PITCH tracks ±5 V (±60 semitones); the ±8 V on the module page is the input range, not the tracking range. <!-- index.md:85; beads/cv_reader.cc:67-85 -->
- **Output polarity:** OUT is inverted relative to IN, by the hardware's output op-amp stage, not the firmware. Invert downstream if phase alignment with the dry source matters (measured on one module: -0.996 correlation). <!-- beads/hardware_design/pcb/beads_v51.sch (NJM2068 differential stage, R52-R54, R60); beads/beads.cc -->
- **Updater start-up:** the updater ignores the first 2 s or so of audio, so start playback a moment after entering it. <!-- manual.md:251; beads/bootloader/bootloader.cc:151-176,308 -->

## Clarifications

- **Scorched cassette resolution:** the 8-bit recording uses µ-law compression, which preserves quiet sounds better than linear 8-bit recording. <!-- manual.md:34, images/quality_table.png; index.md:36; beads/dsp/processor.h:183-192; beads/dsp/delay_line.h:124-153; beads/dsp/mu_law.h:46-78 -->
- **Automatic gain** waits about 1 s, then listens for 5 s, and keeps adapting afterwards; silence postpones it. Restarting with [A] skips the wait. <!-- manual.md:42; beads/gain_control.cc:44-45,61-65,92-119 -->
- **Hiss:** automatic gain raises the input by up to 40 times (about +32 dB), and the input's own noise with it. For quiet sources set the gain by hand ([A]+FEEDBACK). In desktop tests, silent input gave silent output; the module's own noise is not yet measured. <!-- beads/gain_control.cc kMaxGain; desktop study for #59 -->
- **Input limiter:** the limiter for big level changes is the automatic gain itself. A sudden jump can briefly pass above full scale; the gain then falls in about 0.2 s (0.1 s while still listening). <!-- manual.md:42; beads/gain_control.cc:96-111,161-166 -->
- **mix LEDs** (Mixing and audio output): the LEDs under FEEDBACK, DRY/WET and REVERB show the [M] setting: half lit for a single destination, each LED's brightness for individual amounts. They do not follow the incoming CV. <!-- manual.md:129-130; beads/ui.cc:263-271 -->
- **DENSITY extremes:** fully counterclockwise gives a steady C3 rate (about 131 grains/s); fully clockwise is random with a lower average, not a steady C3. <!-- manual.md:65; beads/dsp/trigger_generator.cc:42,73-74,88-110,265-279 -->
- **Gated SEED at noon:** the button and the SEED input are combined, so a trigger while the button is held does not start another grain. <!-- manual.md:75; beads/dsp/trigger_generator.cc:255-263,289-307 -->
- **Entering wavetable synth mode:** the 10 s countdown only runs while the output is silent and FREEZE is off; lingering feedback or reverb postpones it. <!-- manual.md:180; beads/beads.cc:268-269,351-369 -->
- **Feedback schemes:** Cold digital and Sunny tape share the same feedback saturation; they still sound different through sample rate and dry path. <!-- manual.md:123; beads/dsp/processor.h:171-178; beads/dsp/granular_processor.h:287-312 -->
- **"Gaussian" randomness** is bell-shaped but bounded. <!-- index.md:60; beads/dsp/randomized_parameter.h:45-47,128-131 -->
