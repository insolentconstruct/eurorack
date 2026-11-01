# Testing

The expanded Beta 2 source contains 19 host programs, 265 named test sections,
2,060 assertion sites and 392 deliberate fault patterns. These are source
inventory counts, not a claim that final-candidate CI has passed. Previous
candidate CI passed all 19 programs and detected all 372 then-defined faults;
new focused checks do not replace the final full run.

TODO: record final-candidate host and mutation CI, reproducible app/WAV hashes,
module CPU and underruns, profiles/options/scales/tail results, install totals
and automated run hours. Earlier measurements below remain historical.

Tests are separated into desktop/CI checks, STM32H7 development-board checks,
unattended module measurements and attended module checks. The development
board supports functional checks and relative cycle measurements; it cannot
qualify Beads audio/CV, supply behaviour, physical LED colours or final timing.
Group related module checks by exact firmware/profile image and keep physical
setup together. No shorter final end-to-end duration is claimed here.

Count named `test_checker.Start` sections and `CHECK` call sites in test
sources; spectral counts once despite its alternate FFT build. Loop assertions
execute repeatedly.

## Automated (every push)

Host programs use sanitizers. After README edits, keep README.md and README.txt in step. After guide-generator edits, run
`ASAN_OPTIONS=detect_leaks=0 ./build/options_tables | python3 beads/build/tools/options_guide.py`.
`make -f beads/build/tools/options_tables.mk test` checks both formats,
72-column text, firmware tables, guides and release packaging.

| Check | What it covers |
|---|---|
| Reproducible build | App, bootloader, combo and update file must match pinned SHA-256 hashes; the update file is decoded back to the app with the bootloader's own decoder |
| `beads/test/options.mk` | Options page: entry, pages, knob zones, one-second save-on-settle, no idle close, close gestures, settings format. Stock UI: [M] routing, amounts and LEDs, the autosave chord and display, SEED latch gestures, quality cycling, the [A] gain hold, manual gain and meter, the input-level blink, the FREEZE LED while saving, recovery mode keeping calibration, input patch detection, the quality lock while frozen, the trigger-output chord, the overwrite indicator, a corrupted calibration rewritten once. Limiter arithmetic and mode switching. Monophonic grain gesture, LED cue and saving; engine setting (zones, LED, applied when the page closes), storage and the [A] hold; pitch and reverb option storage, zones and scale cycling; trim gesture and FREEZE gate. Settings saves until the flash sector fills and rolls over, with power cut at each of the six rewrite steps: options and calibration survive a clean rollover, and the windows where the stock storage loses them are pinned. Settings storage (#149): a stale flash error flag, refused or stuck erases, refused programs and a non-blank free slot never write over old records, and normal saves match stmlib's byte for byte |
| `beads/test/pitch_quantizer.mk` | Major and Minor pentatonic grids and nearest-note boundaries, negative pitches, octave wrapping and ties; randomized pitch on scale notes, including microtonal scales; custom bank boundaries, repeat intervals and finite pitch ratios at the full synth input range |
| `beads/test/clouds.mk` | Stock output hashes, Clouds-style output and switching, diffuser impulse energy and stereo channels |
| `beads/test/gain.mk` | Input gain with sanitizers: the learning window, headroom after a level rise, digital and tape clipping, and gain on quiet inputs |
| `beads/test/trigger.mk` | Grain triggering with sanitizers: free-running rates at both ends of DENSITY, silence at noon (free and clocked), clock probability and divisions, SEED and gate enabling, one grain per press or gate at noon, and density FM |
| `beads/test/granular.mk` | Grain playback and attenurandomizers with sanitizers: TIME, direction, SIZE, SHAPE, the PITCH notches, patched and unpatched knob laws, random distributions and the shared random stream; through the processor, parameters fixed at grain start, envelopes, voice stealing and the grain count, PITCH CV response, random pan, and CV channel separation; monophonic grains fading earlier grains; quantized grain pitch and trimmed-window reads |
| `beads/test/delay.mk` | Frozen delay with sanitizers: clocked delay with short and start-up clock intervals, free-running delay with FM from −0.7 to +0.5, slice changes only at slice boundaries, every clock divider ratio, and the clock's idle counter stopping at its limit; every tap reads a valid delay. Also DENSITY and its CV, the secondary tap, SEED clocking and its crossfade, TIME multipliers, SHAPE envelopes and TIME's random modulation |
| `beads/test/backup.mk` | Frozen-buffer save format byte by byte; no save before 10 s of FREEZE and a clean cancel; stock saves load; out-of-range headers refused when loading; corrupted audio or header refused on saves carrying the CRC tag; power lost at 428 points (including half-erased sectors) never restores a partial save; flash errors at 103 points abandon cleanly; trimmed and empty buffer saves, older saves, invalid lengths and trim under a FREEZE gate; settings saved during a save; the backoff after cut-short saves, its cap and resets, across the clock wrapping and after a restored frozen buffer |
| `beads/test/wavetable.mk` | Synth mode with sanitizers: last-bank, last-wave reads stay inside the table, and the guard row matches the last wave; the FM bank's four frequency ratios; FEEDBACK's bank selection and its hysteresis; dry/wet, FREEZE, the output resolution of each quality, 1 V/octave and pitch spread, and the dry sine level at octaves from C3 to C9; quantized synth grains; stereo width in rendered grains (Stock, zero, small and full displacement, CV clamped at both ends, grains keep the width they started with) |
| `beads/test/processor.mk` | The whole audio processor with sanitizers: each quality's sample rate, bit depth and buffer length, mono and stereo; FREEZE from the button and the gate; frozen grains keep playing; the crossfade when recording resumes; trim dispatch for every quality, mono and stereo |
| `beads/test/mix.mk` | Mixing with sanitizers: feedback into the recording, feedback saturation per quality, dry/wet, reverb after dry/wet and its tail when the send is closed, stereo output from a mono recording, feedback echoes in delay mode |
| `beads/test/firmware.mk` | The firmware's own start-up, main loop and audio callback (`beads.cc`, unmodified) with hardware stubs and sanitizers: the wavetable synth after 10 s and on a [A] hold, mono/stereo switching, the quality lock while frozen, the overwrite indicator, the trigger output, CV routing, the power-on reset and corrupted-calibration branches, the pitch chain from CV input, and leaving the wavetable synth with a frozen buffer; pitch-scale CV routing and reverb switching; a saved pitch scale reaching wavetable synth grains (Automatic and Manual, random and CV, across a reboot) while the continuous oscillator stays unquantized; switching to and from the spectral engine (fade, cleared recording, frozen buffer kept in flash and restored with granular, no wavetable synth, no flash writes while frozen). A check keeps the test's copy of the main loop identical to `main()`'s |
| `beads/test/spectral.mk` | Spectral engine with sanitizers: CMSIS and ShyFFT conventions (imaginary sign, Nyquist slot, inverse scaling); the port against Clouds' own phase vocoder fed the same input and parameters: identical with ShyFFT, 94/77/63 dB agreement with CMSIS in plain settings (coarse quantization and glitches turn rounding into whole steps); every quality, mono and stereo, through the whole processor with pinned hashes; diffuser and delay mode off; controls drawn once per frame, freeze, glitch and the pitch scale; the recording cleared on engine changes |
| `beads/test/delay_mode.mk` | Delay mode through the processor with sanitizers: continuous playback with SIZE fully clockwise, the fade when switching modes, the pitch shifter and its bypass at noon, the delay bandwidth control and SIZE CV |
| `beads/test/attack.mk` | Attacks: hit detection on the input, marks in the recording, grain starts moved onto them, Off leaving the grains alone |
| `beads/test/feedback_diffuser.mk` | Evolving frozen feedback: a stable loop, running only while FREEZE is on, cleared after a fade |
| `beads/test/plaits_synth.mk` | Plaits synth: every model renders, voice allocation, model detents on FEEDBACK, FREEZE sustain, RAM bounds |
| `beads/test/profile.mk` | Profile validation, feature modes/control allocation, defaults and packed settings; ordered custom scale banks, IDs, UTF-8 names, finite note/repeat limits, reserved bytes, checksums, malformed banks and absent selections |
| `beads/test/storage.mk` | Damaged settings: an uncorrectable flash error at each calibration and settings record the firmware reads, bad checks, blank sectors with and without the calibration copy, interrupted saves and rewrites; nothing written while damaged; the SEED repair, the calibration it keeps, failed erases and programs, and power lost at each repair step |

| Program | Tests | Assertion sites |
|---|---:|---:|
| attack | 3 | 20 |
| backup | 25 | 166 |
| clouds | 5 | 17 |
| delay | 11 | 46 |
| delay_mode | 9 | 40 |
| feedback_diffuser | 3 | 17 |
| firmware | 27 | 265 |
| gain | 6 | 29 |
| granular | 24 | 114 |
| mix | 7 | 28 |
| options | 64 | 633 |
| pitch_quantizer | 5 | 49 |
| plaits_synth | 4 | 31 |
| processor | 11 | 45 |
| profile | 4 | 58 |
| spectral | 7 | 34 |
| storage | 25 | 369 |
| trigger | 11 | 31 |
| wavetable | 14 | 68 |
| **Total** | **265** | **2,060** |

Counts are named test sections and `CHECK` call sites in the test sources; loops and parameter sweeps execute those sites many times. Final-candidate CI for this expanded inventory is pending.

From the repository root: `make -f beads/test/<name>.mk test`. The TSV/documentation check is not in the C++ counts. Tests follow firmware over the manual (7.8 ms shortest requested grain, reset keeping calibration, [M] LED levels, input gain falling after an overload); see [MANUAL_ERRATA.md](MANUAL_ERRATA.md). Removing each backup safeguard must fail its test; `beads/test/mutation_check.py`
injects recorded faults into newer tests, each required to fail (392 current patterns; final full-run results pending, with 372 detected in previous-candidate CI). It runs on GitHub Actions in 8 parallel shards (`--shard K/8`) for pull requests that change the firmware or tests, weekly and on demand.

Saves without the CRC tag (every stock save, or a damaged tag) load after the range checks alone, so stock and Drizzlet read each other's saves.

Feature checks beyond the table:
- **Stereo width:** the unchanged Stock calculation and RNG sequence, zero displacement, full spread with zero randomness, CV clamping, stock and enabled [M] cycles, every saved width byte, the [A]+DRY/WET gesture, its meter and saving, unchanged bytes 12/13, WIDTH CV routing, processor and synth propagation, rendered grain pan retention, saved assignments through the stock destination clamp.
- **Delay mode:** every stored byte value, preview and settled saving, Disabled at SIZE fully clockwise in every quality and channel mode, returning from delay to granular, detent boundaries, and neutral playback within 1e-7 FS with SIZE CV inside the detent.

Expanded feature checks also cover custom scale ID15 through the actual audio
callback in all four engines; reordered and removed saved selections; the
permanent pitch extension distinguishing ID15 from legacy unset settings;
and Keep/Clear reverb tails through both synths and actual exit paths. Factory
reference output and finite custom-scale pitch conversion have focused checks.
Final module confirmation remains pending.

## Coverage of the manual

Of 117 behaviours in the Beads manual, the module page and the [April 2021 update notes](https://forum.mutable-instruments.net/t/beads-firmware-update-release-candidate/18572), host tests check 110.

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

External checks; scripts and results are not in this repository.

| Check | Result |
|---|---|
| Frozen delay with negative FM (#33), host build with AddressSanitizer and UBSan | Previous code: out-of-bounds read. Fixed code: clean; output at FM 0 and +0.5 identical |
| Wavetable interpolation past the last wave (#35) | Previous code: out-of-bounds read. Fixed code: clean; normal output identical |
| Emulator comparison, GCC 9 against GCC 4.9.3 builds of the vanilla source (2026.10.16) | 20 of 20 scenarios bit-identical |
| Emulator comparison against the previous release (2026.10.14) | 44 of 44 scenarios bit-identical: all four qualities, extreme knobs, wavetable synth, freeze, save/restore, clipping, input gain, 16 grain-scheduler cases |
| Instruction counts | No audio-path function grew |
| Damaged settings, on a development board with the same processor and caches on | 8 of 8 cases pass with real uncorrectable flash errors: errors reported instead of crashing, no stale data from the cache, clean reads after a failed one, erase and rewrite, and interrupts held off for under 1 µs |

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
| Vanilla source, same GCC 4.9.3 compiler | 0 | 14 s |
| Drizzlet 2026.10.16 | 0 | 83 s (four runs) |

Reverse, square-envelope, low-pitch and no-grain variants gave no clicks on any firmware. A desktop run of both grain schedulers produced none either, so the DSP logic is not the cause; CPU timing is the working hypothesis (the 2024 scheduler costs more in that range). Drizzlet peaks at 105,346 cycles (87.8%) on the recipe; the 2024 firmware was not timed on the module. The source difference is in CHANGELOG.md.

### Beta 1

Recorded with a Focusrite Scarlett 18i8: both outputs, a copy of the input
signal and a timecode from the Bela for alignment. CPU load comes from a
calibration build: the same code with timing counters added.

**Release candidate 3 against the vanilla source** (2026-10-17), same panel
settings. *Measured* scenarios record values for comparison and have no pass
threshold.

| Scenarios | Vanilla source | Drizzlet |
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

Compared with vanilla source: no firmware differences, dropouts or digital
clipping. Test errors were corrected. Click candidates came from music, delay
echoes and synth audio, at similar counts in both builds.

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

No audio underruns in any capture. The vanilla source has no timing counters,
so it was not timed the same way.

**Release candidate 5** (2026-10-18, Beta 1). Since candidate 3: Clouds-style
modulation rate, explicit GCC 9 rounding and tidying. Default-option emulator
audio remains identical. Module checks:

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
| Vanilla source | 9 | 2 |
| Drizzlet (all builds) | 22 | 4 |
| Reverse-engineered port | 0 | 1 |
| **Total** | **35** | **13** |

All probe installs verified; every install booted. One quiet audio update
stopped partway; replaying at full volume worked and the bootloader was intact.

About 13 hours of automated runs on the module (roughly 9.5 before the Beta 1 sessions), counted from run logs, excluding waits for a control to be moved; a run recorded by several instruments counts once.

### Beta 2

TODO: final-image module CPU figures, worst-case settings and underrun count. The measurements below are from earlier images.

The release code, including the converter fix, on 2026-10-27, and with the storage fix (#149) on 2026-10-28. Outputs recorded with an Expert Sleepers ES-8; CPU load from a timing build of the same code.

| Check | Result |
|---|---|
| Audio after changes between all four qualities | Pass: right pitch and level each time |
| Eight presses of the quality button | Pass: each fades out and back in. Every press changed the sample rate, so the converter still restarted |
| Converter pop at changes that keep the sample rate, 12 per build (separate comparison, 2026-10-25) | About +18 dB above a −34 dBFS test tone before the fix, about −50 dB after (median; worst −35 dB) |
| CV calibration with Intellijel Scales | Saved, then the factory calibration restored (below) |
| Install by audio file; storage recovery tool; SEED at power-on (2026-10-25) | Pass: settings and frozen buffer erased, calibration kept |
| Installing over Beta 1, with every Beta 1 option saved away from stock | Pass: all kept, live and saved; the new options at stock. This was before Hygrostat profiles; this release starts Drizzlet's own options from their defaults instead (not yet rechecked with profiles) |
| [M] CV at 0 V and 2 V, assigned to stereo width, then to grain diffusion | Pass: each rose by 0.40 of its range (0.2 needed) |
| Storage fix: the firmware's own settings saves (FREEZE held 60 s, then released) | Pass: two records appended, calibration unchanged |
| Storage fix: the settings sector full, so the next save rewrites it | Pass: calibration byte-identical, one new settings record, the rest erased |
| Storage fix: frozen buffer restored after a reset | Pass |

| Settings | Peak audio load |
|---|---:|
| Wavetable synth, Clouds-style reverb, width at maximum, a pitch scale, SIZE near maximum | 86.3% (87.2% with the storage fix, within session-to-session spread) |
| Wavetable synth, Clouds-style reverb, width at maximum, a pitch scale | 86.1% |
| Wavetable synth, width at maximum, a pitch scale | 85.8% |
| Evolving frozen feedback | 83.3% |
| Granular features together, frozen: Clouds-style reverb, diffusion and width at maximum, a pitch scale, Evolving frozen feedback | 83.0% |
| Granular features together, recording: as above with attacks (Snap and trigger) instead of Evolving | 81.3% |
| Granular, random intervals drawing | 79.6% |
| Plaits synth with Clouds-style reverb | 69.4% |
| Spectral with Clouds-style reverb | 17.4% |
| Plaits synth, string model | 69.8% |
| Spectral, Sunny tape | 23.6% |
| Spectral | 17.9% |

No audio underruns.

**Damaged settings** (2026-10-29), on the code with the startup guard, the module's own settings sector saved first and written back at the end:

| Check | Result |
|---|---|
| Calibration record damaged, calibration copy blank | Pass: the module started, played on the factory calibration and default settings, and locked all flash writes. The 3 s LED warning was not confirmed: the rig's check for it was faulty |
| Power on holding SEED | Pass: repaired with the factory calibration and the calibration copy written; the green repair blink was seen by eye |
| Next start | Pass: normal, nothing locked |
| Good calibration, calibration copy erased | Pass: normal start, no warning |
| Own settings sector written back | Pass: byte-identical |
| Heavy wavetable synth case, CPU | 87.2%; 84.0–84.2% with synth grains computed in pairs (#168); vanilla source 86.0%. Some repeat rows were rejected by the rig's audio check |

Not yet checked on the module: a damaged calibration restored from its copy (host tests only), a stale flash error flag before a settings save (shown on a development board with the same flash controller, and in host tests), attacks with Snap alone. Final-candidate Hygrostat profiles, custom scale selection/pitch/LEDs, the nine additional options and both synth-tail choices still need module qualification.

**Historical test module totals** (2026-10-18 to 2026-10-28; excludes later sessions). Firmware and settings installs:

TODO: final module install totals and automated run hours, including the damaged-settings, profiles and final-image sessions. The table and counts below are historical subtotals.

| Firmware | Debug probe | Audio file |
|---|---:|---:|
| Drizzlet (Beta 1 release candidate 5, CPU and Beta 2 builds) | 52 | 2 |
| Vanilla source (timing build) | 2 | 0 |
| Settings sector restore (factory calibration and settings) | 7 | – |
| Settings sector test images (Beta 1 settings, [M] assignments, synth settings, storage tests) | 10 | – |
| Storage recovery tool | 0 | 2 |
| **Total** | **71** | **4** |

Since 2026-10-11: 123 installs (106 probe, 17 audio). Probe writes verified
the expected start address; all audio installs booted. Two probe writes needed
repeating after failed checks; one audio install needed replay after a
bootloader error. About 18 additional automated hours, 31 total, counted as above.

**CV calibration** ([M]+[A]):

| Run | Result |
|---|---|
| 2026-10-26, Intellijel Scales | Saved: PITCH scale 0.11% from the factory calibration, within 9 cents from 0 to 5 V |
| 2026-10-27, Intellijel Scales | Saved: PITCH scale 0.08% from the factory calibration, within 8 cents from 0 to 5 V |

The factory calibration was restored by debug probe after each saved run. Before calibrating, the Module Tester's 1 V and 3 V read about 2.5 mV low and Scales' 2–4 mV high, against the factory calibration.

## Not tested

Everything above used one module. Also done on it for release candidate 3 (2026-10-17): trim, freeze, power cycle and stock downgrade with a trimmed save. Options page LED colours were checked by eye for all pitch scales, the limiter and a few other settings, not yet for every value.

- More than one module, or different hardware revisions.
- Long sessions and temperature.
- Power cuts while the settings memory is rewritten. About once every 4,000 settings saves, Mutable Instruments' storage code erases the settings sector and rewrites calibration and settings. A power cut during that rewrite loses the settings being saved; the calibration copy keeps the calibration. The host tests check this window and that the full rewrite keeps everything.
- The module's noise floor (hiss, #59), planned for a later release.
- Listening tests of the limiter modes. Their code is unchanged since the limiter's own measurements.
- The full bench suite was not rerun on 2026.10.16.

Report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).

## Settings sector tool

`make -f beads/build/tools/settings_sector.mk test` builds images with `settings_sector.py` (calibration only, settings only, both, a full sector) and loads each with stmlib's own storage code, as the firmware does at boot; dumps the firmware would not read are refused.

## Options checks

- `make -f beads/build/tools/options_tables.mk test` builds the TSV exporter and compares README.txt's Options reference with the firmware tables. `./build/options_tables` prints page, LED, knob, setting, value, colour and pulse.
- The UI host checks all twelve page/knob slots, cycling, the left-to-right page LEDs, RAM page memory, inert Not-used knobs, the stored-value preview when a page opens, takeover, a sweep with one settled save, idle pages, close gestures, pending saves after closing, SEED scale cycling, steady-green stock values, and distinct colour/pattern combinations within each setting.
- Renderer checks cover the full 2.048 s pattern, and every width byte on [A]+DRY/WET against Beads' meter. Entry, close and save confirmations last 100 ms; idle controls do not repeat the save flash.
- Regressions: width hysteresis at the bottom of [A]+DRY/WET; short SEED taps and long holds during a pending save; volatile commit publication; a second FREEZE press before the pending save settles. Closing restores the normal [A] indication immediately.
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
