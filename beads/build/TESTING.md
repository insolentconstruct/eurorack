# Testing

How Drizzlet is tested: the automated checks on the `drizzlet` branch, and the checks its developers ran before releases 2026.10.14 and 2026.10.16 (alphas). Hardware testing used one module.

## TL;DR

- **In numbers:** 147 tests in 13 host test programs (916 assertion sites) covering 110 of 117 manual behaviours (3 only in part), 150 deliberate-fault mutations, 531 injected power-loss and flash-error points, byte-reproducible builds, emulator comparisons, and checks on a real module.
- **Automated:** every build is reproducible to pinned hashes, and host tests run on every push. Most run the real firmware on a computer, from start-up and the main loop down to the audio processor and synth. They cover 110 of the 117 behaviours listed from the manual, 3 of them only in part.
- **Before release:**
  - sanitizer runs show the two out-of-bounds reads (#33, #35) before their fixes and not after;
  - emulator comparisons found identical audio in 44 of 44 scenarios against the previous release (2026.10.14), and in 20 of 20 between GCC 9 and GCC 4.9.3 builds (2026.10.16).
- **On a module:** seven checks for 2026.10.14, from installing the release file to a stock firmware loading a Drizzlet save; six for 2026.10.16, including the crackle fix, flash wear from a modulated FREEZE, monophonic grains and self-play, using oscilloscope captures and the debug probe.
- **Not covered:** other modules, long sessions, and listening tests of the limiter modes.

Counts come from the test calls in each program’s `main()`, `CHECK` sites
in those source files, and the `cases` list in `mutation_check.py`. Options
also runs an advanced build; it is counted once. Assertions inside loops
execute more than once. The manual-coverage totals below are unchanged.

## Automated (every push)

| Check | What it covers |
|---|---|
| Reproducible build | App, bootloader, combo and update file must match pinned SHA-256 hashes; the update file is decoded back to the app with the bootloader's own decoder |
| `beads/test/options.mk` | Options page entry, categories, knob zones, idle save, cancel and settings format; stock ASSIGN routing, amounts and their LEDs, the autosave chord and display, SEED latch gestures and start-up latch, quality cycling, the MEDIA gain hold, manual gain and its meter, the input-level blink while learning, and the FREEZE LED while saving; recovery mode keeps calibration; input patch detection, the quality lock while frozen, the trigger-output chord, the overwrite indicator, and a corrupted calibration rewritten once; limiter arithmetic, stereo linking and mode switching. Normal and advanced builds; the monophonic grain gesture, its LED cue and saving; the self-play setting, its storage and the MEDIA hold; pitch and reverb option storage, zones, scale cycling and cancellation; trim gesture and FREEZE gate |
| `beads/test/pitch_quantizer.mk` | Major and Minor pentatonic grids and nearest-note boundaries, negative pitches, octave wrapping and ties; randomized pitch on scale notes, including microtonal scales |
| `beads/test/clouds.mk` | Stock output hashes, Clouds-style output and switching, diffuser impulse energy and stereo channels |
| `beads/test/gain.mk` | Input gain with sanitizers: the learning window, headroom after a level rise, digital and tape clipping, and gain on quiet inputs |
| `beads/test/trigger.mk` | Grain triggering with sanitizers: free-running rates at both ends of DENSITY, silence at noon (free and clocked), clock probability and divisions, SEED and gate enabling, one grain per press or gate at noon, and density FM |
| `beads/test/granular.mk` | Grain playback and attenurandomizers with sanitizers: TIME, direction, SIZE, SHAPE, the PITCH notches, patched and unpatched knob laws, random distributions and the shared random stream; through the processor, parameters fixed at grain start, envelopes, voice stealing and the grain count, PITCH CV response, random pan, and CV channel separation; monophonic grains fading earlier grains; quantized grain pitch and trimmed-window reads |
| `beads/test/delay.mk` | Frozen delay with sanitizers: clocked delay with short and start-up clock intervals, free-running delay with FM from −0.7 to +0.5, slice changes only at slice boundaries, every clock divider ratio, and the clock's idle counter stopping at its limit; every tap reads a valid delay. Also DENSITY and its CV, the secondary tap, SEED clocking and its crossfade, TIME multipliers, SHAPE envelopes and TIME's random modulation |
| `beads/test/backup.mk` | Frozen-buffer save format byte by byte; no save before 10 s of FREEZE and a clean cancel; stock saves load; out-of-range headers refused when loading; corrupted audio or header refused on saves carrying the CRC tag; power lost at 428 points (including half-erased sectors) never restores a partial save; flash errors at 103 points abandon cleanly; trimmed and empty buffer saves, older saves, invalid lengths and trim under a FREEZE gate; settings saved during a save; the backoff after cut-short saves, its cap and resets, across the clock wrapping and after a restored frozen buffer |
| `beads/test/wavetable.mk` | Synth mode with sanitizers: last-bank, last-wave reads stay inside the table, and the guard row matches the last wave; the FM bank's four frequency ratios; FEEDBACK's bank selection and its hysteresis; dry/wet, FREEZE, the output resolution of each quality, 1 V/octave and pitch spread, and the dry sine level at octaves from C3 to C9; quantized synth grains |
| `beads/test/processor.mk` | The whole audio processor with sanitizers: each quality's sample rate, bit depth and buffer length, mono and stereo; FREEZE from the button and the gate; frozen grains keep playing; the crossfade when recording resumes; trim dispatch for every quality, mono and stereo |
| `beads/test/mix.mk` | Mixing with sanitizers: feedback into the recording, feedback saturation per quality, dry/wet, reverb after dry/wet and its tail when the send is closed, stereo output from a mono recording, feedback echoes in delay mode |
| `beads/test/firmware.mk` | The firmware's own start-up, main loop and audio callback (`beads.cc`, unmodified) with hardware stubs and sanitizers: self-play after 10 s and on request, mono/stereo switching, the quality lock while frozen, the overwrite indicator, the trigger output, CV routing, the power-on reset and corrupted-calibration branches, the pitch chain from CV input, and leaving self-play with a frozen buffer; pitch-scale CV routing and reverb switching. A check keeps the test's copy of the main loop identical to `main()`'s |
| `beads/test/delay_mode.mk` | Delay mode through the processor with sanitizers: continuous playback with SIZE fully clockwise, the fade when switching modes, the pitch shifter and its bypass at noon, the delay bandwidth control and SIZE CV |

| Program | Tests | Assertion sites |
|---|---:|---:|
| backup | 16 | 106 |
| clouds | 3 | 9 |
| delay | 10 | 45 |
| delay_mode | 6 | 33 |
| feedback_diffuser | 2 | 8 |
| firmware | 14 | 102 |
| gain | 5 | 21 |
| granular | 22 | 99 |
| mix | 7 | 28 |
| options | 38 | 390 |
| pitch_quantizer | 2 | 23 |
| processor | 10 | 36 |
| trigger | 10 | 27 |
| wavetable | 10 | 42 |
| **Total** | **155** | **969** |

The table lists the tests on the `drizzlet` branch now; several were added after release 2026.10.14. Run any of them from the repository root with `make -f beads/test/<name>.mk test`. Where the firmware and the manual differ, the tests check the firmware: the 7.8 ms shortest requested grain, reset settings keeping calibration, the ASSIGN LED levels, and the input gain falling after an overload. These differences are in [MANUAL_ERRATA.md](MANUAL_ERRATA.md). The backup test was checked by removing each safeguard in turn, and `beads/test/mutation_check.py` breaks the firmware in a recorded way for each newer test: every change makes its test fail. It runs weekly on GitHub Actions and on demand; each test program also runs as its own job on every push.

Saves without the CRC tag, including every stock save, load after the range checks alone, so stock and Drizzlet can read each other's saves. A save whose CRC tag itself is damaged is treated the same way.

Stereo width adds host checks for the unchanged Stock calculation and RNG
sequence, zero displacement, full spread with zero randomness, CV clamping,
stock/enabled ASSIGN cycles, every saved width byte, preview and saving,
and unchanged bytes 12/13. Round 2 evidence is recorded in the lane REPORT.md.

Delay mode adds host checks for all stored byte values, preview, cancellation
and saving, Never at SIZE fully clockwise in every quality and channel mode,
returning from delay to granular, detent boundaries and neutral playback within 1e-7 FS
with SIZE CV inside the detent. Auto remains the default.

The compile-time frozen-feedback experiment has its own sanitizer target,
`beads/test/feedback_diffuser.mk`: impulse tail, channel isolation, finite
bounded output, zero-feedback bypass, and unchanged frozen recording banks
at maximum feedback in every quality, mono and stereo.

Default regression for stereo width and delay mode: GCC 9.3.1 builds of
`fcbfe38` and the additions matched in all 20 settled-panel emulator scenarios
(four qualities × granular, mono, delay, synth and self-play): 420,000 stereo
frames, maximum error 0. The prototype flag was undefined. A separate q3
stereo stress32 fixture, Clouds-style stress32 and frozen delay also match
at defaults. No hardware was accessed for these additions; performance
numbers below are instruction counts, not DWT cycles.

## Coverage of the manual

The developers listed 117 behaviours from the Beads manual, the module page and the April 2021 update notes. Host tests check 110 of them.

| Area | Behaviours | Host tests | Not covered by host tests |
|---|---:|---:|---|
| Recording quality, mono/stereo | 9 | 8 | 1 needs a module |
| Input gain | 10 | 10 | |
| FREEZE, buffer save, autosave | 9 | 9 | |
| Grain generation: SEED, DENSITY | 16 | 16 | |
| Grain playback: TIME, PITCH, SIZE, SHAPE | 13 | 12 | 1 untestable |
| Attenurandomizers | 9 | 9 | |
| Mixing, output, ASSIGN | 14 | 11 | 2 need a module, 1 untestable |
| Delay and slicer | 19 | 19 | |
| Synth mode | 15 | 14 | 1 untestable |
| Calibration and recovery | 3 | 2 | 1 untestable |
| **Total** | **117** | **110** | **3 module, 4 untestable** |

- **Only in part (3):** audio-rate FM and 1 V/octave pitch (granular and synth) are checked through the real CV input path with the default calibration. How closely a given module tracks depends on its own stored calibration.
- **Module:** the left-channel crackle report (checked on a module since, below), and audio and triggers summed into OUT L when OUT R is unpatched.
- **Untestable:** three compare with the original factory firmware, which the developers do not have; one needs a corrupted-storage sample, and none has been identified.

## Before release

Run by the developers; these scripts and results are not in this repository.

| Check | Result |
|---|---|
| Frozen delay with negative FM (#33), host build with AddressSanitizer and UBSan | Previous code: out-of-bounds read. Fixed code: clean; output at FM 0 and +0.5 identical |
| Wavetable interpolation past the last wave (#35) | Previous code: out-of-bounds read. Fixed code: clean; normal output identical |
| Emulator comparison, GCC 9 against GCC 4.9.3 builds of the released source (2026.10.16) | 20 of 20 scenarios bit-identical |
| Emulator comparison against the previous release (2026.10.14) | 44 of 44 scenarios bit-identical: all four qualities, extreme knobs, self-play (synth), freeze, save/restore, clipping, input gain, 16 grain-scheduler cases |
| Instruction counts | No audio-path function grew |
| Code review | Five rounds by a second AI model; the first four led to fixes (three of them real bugs in the frozen-buffer save) |

## On a module

**Setup:** one Beads, driven remotely and recorded:
- a Bela Gem Multi sets control voltages and gates;
- an ST-LINK debug probe reads the processor's state without stopping it;
- an oscilloscope captures both outputs;
- a laptop plays audio into the inputs.

### 2026.10.14

| # | Check | Result |
|---|---|---|
| 1 | Install with the release update file, over the stock 2021 firmware | Decoded first time; new firmware running |
| 2 | Settings from an earlier Drizzlet survive the update | Yes |
| 3 | Options page: change the limiter, idle save, power cycle | Saved and kept |
| 4 | Frozen-buffer save and restore across a power cycle | Restored audio plays, stereo |
| 5 | A setting saved while the buffer is being saved | Restore still complete (the recorded tone plays back) |
| 6 | Frozen delay, FM swept negative with SEED low, held and pulsed | No faults in 1,200 samples of the program counter |
| 7 | Stock firmware (2021 release) loads a Drizzlet save | Yes: restored audio plays |

### 2026.10.16

Control voltages and gates from the Bela; OUT L on the oscilloscope; flash activity from samples of the program counter.

| # | Check | Result |
|---|---|---|
| 1 | Install with the release update file, over the 2021 firmware | Decoded; new firmware running; the settings read back (trigger output, FREEZE, mode) unchanged |
| 2 | Crackle recipe (#1), below | No clicks in 83 s (2021 firmware: 145 in 21 s) |
| 3 | 20 s LFO into FREEZE, autosave on (#56) | Flash activity in the first cycle only (11–15 s), none in the next 77 s; the 2021 firmware showed it every cycle |
| 4 | Frozen-buffer save from the FREEZE gate, then the same image re-flashed by the debug probe while still frozen | Flash activity for 7 s, ending before the restart; after it FREEZE was back on and the restored audio played (1.4 V RMS, inputs silent, grains from a DENSITY CV) |
| 5 | Monophonic grains (#31): SEED pulsed 10 times a second into long grains | MEDIA LED at half brightness. Between triggers the envelope dips to 23 % of its peak (81 % with monophonic grains off); 48 % of its power is at 10 Hz and harmonics (5 % off) |
| 6 | Self-play, Automatic (the default): both inputs unplugged, SEED pulsed 10 times a second | Plays (1.4 V RMS); silent without the pulses, since a patched, unlatched SEED jack is the only grain trigger |

Self-play On request, the delay clock fix (#57) and the frozen-delay slice fix (#33) were checked by host tests only.

**Crackle (#1).** Settings: DENSITY 0.98, PITCH +24 semitones, SIZE 0.92 (just below delay) and SHAPE 0.5, set by CV; DRY/WET fully clockwise, FEEDBACK and REVERB fully counter-clockwise; Bright digital; music into both inputs. OUT L went to a scope with OUT R unplugged, so it carried L+R. A detector counted clicks on the scope captures:

| Firmware | Clicks | Output observed |
|---|---:|---:|
| 2021 forum release | 145 | 21 s |
| 2024 recovery firmware | 164 | 20.6 s |
| Released source, same GCC 4.9.3 compiler | 0 | 14 s |
| Drizzlet 2026.10.16 | 0 | 83 s (four runs) |

Reverse, square-envelope, low-pitch and no-grain variants gave none on any firmware. See CHANGELOG.md for the source difference.

## Not tested

Beta 1 preparation still needs module CPU headroom for the complete audio
callback, the full bench suite, trim/freeze/power-cycle and stock downgrade
with a trimmed save, pitch listening and LED checks, and Clouds-style listening
and switching checks. Outside testers on other units are still needed.
No new module results are claimed here.

- More than one module, or different hardware revisions.
- Long sessions and temperature.
- Listening tests of the limiter modes. Their code is unchanged since the limiter's own measurements.
- The full bench suite was not rerun on 2026.10.16.

Report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).
