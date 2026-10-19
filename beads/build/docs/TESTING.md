# Testing

**TL;DR:** 165 host tests in 13 programs cover 110 of 117 manual behaviours (3 only in part), backed by 154 deliberate-fault checks, 538 injected power-loss and flash-error points, reproducible builds, emulator comparisons and runs on one real module. Not covered: other modules, long sessions, limiter listening tests.

- **Every push:** builds match pinned hashes; host tests mostly run the real firmware on a computer, from start-up and main loop down to the audio processor and synth.
- **Before release:** sanitizer runs show the two out-of-bounds reads (#33, #35) before their fixes and not after. Emulator audio is identical in 44 of 44 scenarios against 2026.10.14, and 20 of 20 between GCC 9 and GCC 4.9.3 builds.
- **On a module:** seven checks for 2026.10.14 and six for 2026.10.16 (installing, the crackle recipe, flash wear from a modulated FREEZE, monophonic grains, wavetable synth), using oscilloscope captures and the debug probe. For Beta 1, 28 scenarios on release candidate 3 against the released source (22 shared), a shorter run on release candidate 5, every pitch scale on scale, and CPU load of 60–90.5% of the audio budget with no underruns.
- **Totals:** 48 firmware installs on the test module, none failed to boot; about 13 hours of automated runs.

Counts come from the test calls in each program's `main()`, `CHECK` sites in those files, and the `cases` list in `mutation_check.py`. Options also runs an advanced build, counted once. Assertions inside loops run more than once.

## Automated (every push)

| Check | What it covers |
|---|---|
| Reproducible build | App, bootloader, combo and update file must match pinned SHA-256 hashes; the update file is decoded back to the app with the bootloader's own decoder |
| `beads/test/options.mk` | Options page: entry, pages, knob zones, one-second save-on-settle, no idle close, close gestures, settings format. Stock UI: [M] routing, amounts and LEDs, the autosave chord and display, SEED latch gestures, quality cycling, the [A] gain hold, manual gain and meter, the input-level blink, the FREEZE LED while saving, recovery mode keeping calibration, input patch detection, the quality lock while frozen, the trigger-output chord, the overwrite indicator, a corrupted calibration rewritten once. Limiter arithmetic and mode switching. Normal and advanced builds: monophonic grain gesture, LED cue and saving; wavetable synth setting, storage and [A] hold; pitch and reverb option storage, zones and scale cycling; trim gesture and FREEZE gate. Settings saves until the flash sector fills and rolls over, with power cut at each of the six rewrite steps: options and calibration survive a clean rollover, and the windows where the stock storage loses them are pinned |
| `beads/test/pitch_quantizer.mk` | Major and Minor pentatonic grids and nearest-note boundaries, negative pitches, octave wrapping and ties; randomized pitch on scale notes, including microtonal scales |
| `beads/test/clouds.mk` | Stock output hashes, Clouds-style output and switching, diffuser impulse energy and stereo channels |
| `beads/test/gain.mk` | Input gain with sanitizers: the learning window, headroom after a level rise, digital and tape clipping, and gain on quiet inputs |
| `beads/test/trigger.mk` | Grain triggering with sanitizers: free-running rates at both ends of DENSITY, silence at noon (free and clocked), clock probability and divisions, SEED and gate enabling, one grain per press or gate at noon, and density FM |
| `beads/test/granular.mk` | Grain playback and attenurandomizers with sanitizers: TIME, direction, SIZE, SHAPE, the PITCH notches, patched and unpatched knob laws, random distributions and the shared random stream; through the processor, parameters fixed at grain start, envelopes, voice stealing and the grain count, PITCH CV response, random pan, and CV channel separation; monophonic grains fading earlier grains; quantized grain pitch and trimmed-window reads |
| `beads/test/delay.mk` | Frozen delay with sanitizers: clocked delay with short and start-up clock intervals, free-running delay with FM from −0.7 to +0.5, slice changes only at slice boundaries, every clock divider ratio, and the clock's idle counter stopping at its limit; every tap reads a valid delay. Also DENSITY and its CV, the secondary tap, SEED clocking and its crossfade, TIME multipliers, SHAPE envelopes and TIME's random modulation |
| `beads/test/backup.mk` | Frozen-buffer save format byte by byte; no save before 10 s of FREEZE and a clean cancel; stock saves load; out-of-range headers refused when loading; corrupted audio or header refused on saves carrying the CRC tag; power lost at 428 points (including half-erased sectors) never restores a partial save; flash errors at 103 points abandon cleanly; trimmed and empty buffer saves, older saves, invalid lengths and trim under a FREEZE gate; settings saved during a save; the backoff after cut-short saves, its cap and resets, across the clock wrapping and after a restored frozen buffer |
| `beads/test/wavetable.mk` | Synth mode with sanitizers: last-bank, last-wave reads stay inside the table, and the guard row matches the last wave; the FM bank's four frequency ratios; FEEDBACK's bank selection and its hysteresis; dry/wet, FREEZE, the output resolution of each quality, 1 V/octave and pitch spread, and the dry sine level at octaves from C3 to C9; quantized synth grains; stereo width in rendered grains (Stock, zero, small and full displacement, CV clamped at both ends, grains keep the width they started with) |
| `beads/test/processor.mk` | The whole audio processor with sanitizers: each quality's sample rate, bit depth and buffer length, mono and stereo; FREEZE from the button and the gate; frozen grains keep playing; the crossfade when recording resumes; trim dispatch for every quality, mono and stereo |
| `beads/test/mix.mk` | Mixing with sanitizers: feedback into the recording, feedback saturation per quality, dry/wet, reverb after dry/wet and its tail when the send is closed, stereo output from a mono recording, feedback echoes in delay mode |
| `beads/test/firmware.mk` | The firmware's own start-up, main loop and audio callback (`beads.cc`, unmodified) with hardware stubs and sanitizers: the wavetable synth after 10 s and on a [A] hold, mono/stereo switching, the quality lock while frozen, the overwrite indicator, the trigger output, CV routing, the power-on reset and corrupted-calibration branches, the pitch chain from CV input, and leaving the wavetable synth with a frozen buffer; pitch-scale CV routing and reverb switching; a saved pitch scale reaching wavetable synth grains (Automatic and Manual, random and CV, across a reboot) while the continuous oscillator stays unquantized. A check keeps the test's copy of the main loop identical to `main()`'s |
| `beads/test/delay_mode.mk` | Delay mode through the processor with sanitizers: continuous playback with SIZE fully clockwise, the fade when switching modes, the pitch shifter and its bypass at noon, the delay bandwidth control and SIZE CV |

| Program | Tests | Assertion sites |
|---|---:|---:|
| backup | 16 | 106 |
| clouds | 4 | 12 |
| delay | 11 | 46 |
| delay_mode | 9 | 40 |
| firmware | 15 | 124 |
| gain | 5 | 21 |
| granular | 23 | 113 |
| mix | 7 | 28 |
| options | 42 | 386 |
| pitch_quantizer | 2 | 23 |
| processor | 10 | 36 |
| trigger | 10 | 27 |
| wavetable | 11 | 60 |
| **Total** | **165** | **1022** |

The separate TSV/documentation check is not included in the C++ assertion counts.

Run any test from the repository root with `make -f beads/test/<name>.mk test`. The TSV/documentation check is not in the C++ counts. Where the firmware and the manual differ, the tests check the firmware (7.8 ms shortest requested grain, reset keeping calibration, [M] LED levels, input gain falling after an overload); see [MANUAL_ERRATA.md](MANUAL_ERRATA.md). The backup test was checked by removing each safeguard in turn, and `beads/test/mutation_check.py` breaks the firmware in a recorded way for each newer test: every change makes its test fail. It runs weekly on GitHub Actions and on demand.

Saves without the CRC tag (every stock save, or a damaged tag) load after the range checks alone, so stock and Drizzlet read each other's saves.

Feature checks beyond the table:
- **Stereo width:** the unchanged Stock calculation and RNG sequence, zero displacement, full spread with zero randomness, CV clamping, stock and enabled [M] cycles, every saved width byte, preview and saving, unchanged bytes 12/13, WIDTH CV routing, processor and synth propagation, rendered grain pan retention, saved assignments through the stock destination clamp.
- **Delay mode:** every stored byte value, preview and settled saving, Disabled at SIZE fully clockwise in every quality and channel mode, returning from delay to granular, detent boundaries, and neutral playback within 1e-7 FS with SIZE CV inside the detent.

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
| Mixing, output, CV assignment | 14 | 11 | 2 need a module, 1 untestable |
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
| Emulator comparison against the previous release (2026.10.14) | 44 of 44 scenarios bit-identical: all four qualities, extreme knobs, wavetable synth, freeze, save/restore, clipping, input gain, 16 grain-scheduler cases |
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
| 3 | Options page: change the limiter, wait one second, power cycle | Saved and kept |
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
| 5 | Monophonic grains (#31): SEED pulsed 10 times a second into long grains | Quality LED [A] at half brightness. Between triggers the envelope dips to 23 % of its peak (81 % with monophonic grains off); 48 % of its power is at 10 Hz and harmonics (5 % off) |
| 6 | Wavetable synth, Automatic (Stock) (the default): both inputs unplugged, SEED pulsed 10 times a second | Plays (1.4 V RMS); silent without the pulses, since a patched, unlatched SEED jack is the only grain trigger |

Wavetable synth Manual, the delay clock fix (#57) and the frozen-delay slice fix (#33) were checked by host tests only.

**Crackle (#1).** Settings: DENSITY 0.98, PITCH +24 semitones, SIZE 0.92 (just below delay) and SHAPE 0.5, set by CV; DRY/WET fully clockwise, FEEDBACK and REVERB fully counter-clockwise; Bright digital; music into both inputs. OUT L went to a scope with OUT R unplugged, so it carried L+R. A detector counted clicks on the scope captures:

| Firmware | Clicks | Output observed |
|---|---:|---:|
| 2021 forum release | 145 | 21 s |
| 2024 recovery firmware | 164 | 20.6 s |
| Released source, same GCC 4.9.3 compiler | 0 | 14 s |
| Drizzlet 2026.10.16 | 0 | 83 s (four runs) |

Reverse, square-envelope, low-pitch and no-grain variants gave no clicks on any firmware. A desktop run of both grain schedulers produced none either, so the DSP logic is not the cause; CPU timing is the working hypothesis (the 2024 scheduler costs more in that range). Drizzlet peaks at 105,346 cycles (87.8%) on the recipe; the 2024 firmware was not timed on the module. The source difference is in CHANGELOG.md.

### Beta 1

Recorded with a Focusrite Scarlett 18i8: both outputs, a copy of the input
signal and a timecode from the Bela for alignment. CPU load comes from a
calibration build: the same code with timing counters added.

**Release candidate 3 against the released source** (2026-10-17), same panel
settings. *Measured* scenarios record values for comparison and have no pass
threshold.

| Scenarios | Released source | Drizzlet |
|---|---|---|
| Dry path; PITCH, TIME, SIZE and SHAPE CV; SEED; FREEZE gate; gated DENSITY; latched clock; monophonic grains; dry and wet output in the qualities checked | 15 pass | 15 pass |
| Dry crosstalk | Inconclusive (the laptop's playback skipped) | Pass |
| Dry output, first quality; three delay scenarios | Inconclusive (measurement limits) | Inconclusive, same reasons |
| Input gain and limiter, delay echoes | Measured | Measured |
| Random amounts for PITCH, TIME, SIZE and SHAPE | Measured | Measured |
| Wavetable synth, inputs unplugged | Pass | Pass |
| Drizzlet only: limiter Fast and Off, three [M] destinations | | Measured |
| Drizzlet only: Clouds-style switching | | Inconclusive here, pass on release candidate 5 |
| Drizzlet only: all nine pitch scales | | Measured, see below |

Measured values were compared with the released source. Every difference
traced back to the test rather than the firmware, and those tests were fixed.
No recording had dropouts or digital clipping. The click candidates found came
from the audio itself (music on the dry path, delay echoes, wavetable synth), at
similar counts on the released source.

**Pitch scales.** With PITCH randomness turned up, 222 grains were detected
across the nine scale settings. Every one landed on a note of its scale, the
worst within 4.6 cents.

**CPU load on the module.** Peak cycles for the whole audio callback, including
codec and control input handling; the budget is 120,000 cycles per block. The
shared settings are DENSITY 0.95, SIZE 0.85, PITCH +24 semitones, Bright
digital, with inputs playing.

| Settings | Peak cycles | Budget |
|---|---:|---:|
| Delay mode | 72,472 | 60% |
| Sunny tape | 76,746 | 64% |
| Default options (heaviest grain case) | 102,009–103,223 | 85–86% |
| Crackle recipe, Clouds-style reverb | 105,346 | 87.8% |
| Clouds-style reverb | 105,720 | 88.1% |
| Clouds-style reverb and Raag Shri scale | 105,761 | 88.1% |
| Synth mode, inputs unplugged | 108,590 | 90.5% |

No audio underruns in any capture. The released source has no timing counters,
so it was not timed the same way.

**Release candidate 5** (2026-10-18), the Beta 1 code. Only the Clouds-style
reverb's modulation rate, explicit rounding that keeps GCC 9's results, and
code tidying changed since release candidate 3; the emulator gives identical
audio in every scenario with default options. A short run checked them on the module:

| Check | Result |
|---|---|
| Clouds-style switching, A/B/A | Pass |
| PITCH CV, gated DENSITY | Pass |
| Clouds-style CPU | 105,764 cycles (88.1%), no underruns, same as release candidate 3 |
| Wavetable synth | Inconclusive: the noise rule expects a quiet start, which wavetable synth never has |
| Dry path | Inconclusive: weak alignment, as before |
| Delay | Inconclusive at first: the alignment check fell 0.13 s short. Reduced again with the check corrected: loop period pass, pitch shift and echo levels measured |
| Dropouts, clipping | None |
| Click candidates | None in Clouds-style, PITCH, DENSITY or CPU recordings; dry path, delay and wavetable synth at the counts seen before |

**Test module totals** (2026-10-11 to 2026-10-18). Firmware installs:

| Firmware | Debug probe | Audio file |
|---|---:|---:|
| 2021 forum release | 2 | 4 |
| 2024 recovery firmware | 2 | 2 |
| Released source | 9 | 2 |
| Drizzlet (all builds) | 22 | 4 |
| Reverse-engineered port | 0 | 1 |
| **Total** | **35** | **13** |

Every probe install was read back and checked; the module never failed to boot. Once, an update file played too quietly stopped partway; the bootloader was untouched and replaying at full volume fixed it.

About 13 hours of automated runs on the module (roughly 9.5 before the Beta 1 sessions), counted from run logs, excluding waits for a control to be moved; a run recorded by several instruments counts once.

## Not tested

Everything above used one module. Also done on it for release candidate 3 (2026-10-17): trim, freeze, power cycle and stock downgrade with a trimmed save. Options page LED colours were checked by eye for all pitch scales, the limiter and a few other settings, not yet for every value.

- More than one module, or different hardware revisions.
- Long sessions and temperature.
- Power cuts while the settings memory is rewritten. About once every 4,000 settings saves, Mutable Instruments' storage code erases the settings sector and rewrites calibration and settings. A power cut during that rewrite can reset calibration, settings or both to defaults; the host tests check this window and that the full rewrite keeps everything.
- Listening tests of the limiter modes. Their code is unchanged since the limiter's own measurements.
- The full bench suite was not rerun on 2026.10.16.

Report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).

## Options checks

- `make -f beads/build/tools/options_tables.mk test` builds the TSV exporter and compares README.txt's Options reference with the firmware tables. `./build/options_tables` prints page, LED, knob, setting, value, colour and pulse.
- The UI host checks all nine page/knob slots, cycling, RAM page memory, inert Not-used knobs, takeover, a sweep with one settled save, idle pages, close gestures, pending saves after closing, width hysteresis, SEED scale cycling, steady-green stock values, and distinct colour/pattern combinations within each setting.
- Renderer checks cover the full 2.048 s pattern and all 255 enabled width amounts against Beads' meter. Entry, close and save confirmations last 100 ms; idle controls do not repeat the save flash.
- Regressions: 10 s of ±0.002 width noise at an encoded step boundary (one save, one confirmation); short SEED taps and long holds during a pending save; volatile commit publication; a second FREEZE press before the pending save settles. Closing restores the normal [A] indication immediately.
- The delay mode test compares the maximum wet-output step when switching Auto (Stock) to Wide with an identical no-switch render at the clockwise filter endpoint.
- Physical LED colour separation and audio during flash writes still need module checks.

Generate the options guides (Python 3, standard library only):

```sh
make -f beads/build/tools/options_tables.mk build/options_tables
./build/options_tables | python3 beads/build/tools/options_guide.py
make -f beads/build/tools/options_tables.mk test
```

The test regenerates all three guide files in a temporary directory and compares bytes, checks the Markdown values against the firmware TSV and README.txt, mutates each exported zone boundary to check that both SVGs change, and exercises release ZIP packaging with placeholder assets. The printable SVG is 190 mm wide, within A4 and Letter; print at actual size, centred, without browser headers and footers.

## Host CI groups

- `beads/test/ci-jobs.json` lists the host jobs and their commands; the workflow's `list-host-tests` job reads it into the matrix. Short suites share three jobs; delay mode uses three and Clouds two.
- Each make call uses `-j$(nproc)` and keeps its sanitizer flags. Native tests use `-O1` rather than `-Os`, which made sanitized DSP renders slow on the Ubuntu 24.04 compiler. Firmware flags are unchanged.
- The quantizer delay comparison warms each quality/channel processor once and restores that state, keeping all eight quality/channel cases, the 16000-block warmup and 1200 comparison blocks per output.
- Selection and local commands: [host test README](../../test/README.md).
