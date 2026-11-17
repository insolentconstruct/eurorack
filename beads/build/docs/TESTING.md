# Testing

This source contains 20 host programs, 310 named test sections, 2,598
assertion sites and 526 deliberate fault patterns (Beta 2: 275, 2,199 and
396). On `c9608d0`, [host CI](https://github.com/insolentconstruct/eurorack/actions/runs/43678309834)
passed and [mutation CI](https://github.com/insolentconstruct/eurorack/actions/runs/43678309848)
detected all 484 faults across eight shards. Named sections and assertion sites
are source inventory counts; loops and parameter sweeps execute them repeatedly.

The Beta 2 release build reproduces application `2558dd77…`, timing image
`8459e0fa…` and WAV `28e28efa…`, and decodes the WAV back to the application.
This source pins its own unreleased application `02a0b227…` and WAV
`cba8fee2…`. The release documentation and generated guides pass their
checker. Beta 3 module results are under [On a module](#beta-3); some
checks are still pending.

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
| `beads/test/options.mk` | Options page: entry, pages (optional pages 5-7: order, wrap, LED masks), knob zones, one-second save-on-settle, close gestures, the optional idle timeout (off by default; restarts, jitter, slow cumulative turns, holds, clock wrap, unchanged and pending saves), settings format. Stock UI: [M] routing, amounts and LEDs, the autosave chord and display, SEED latch gestures, quality cycling, the [A] gain hold, manual gain and meter, the input-level blink, the FREEZE LED while saving, recovery mode keeping calibration, input patch detection, the quality lock while frozen, the trigger-output chord, the overwrite indicator, a corrupted calibration rewritten once. Limiter arithmetic and mode switching. Monophonic grain gesture, LED cue and saving; engine setting (zones, LED, applied when the page closes), storage and the [A] hold; pitch and reverb option storage, zones and scale cycling; Random pitch steps' three zones, colours and saving; trim gesture and FREEZE gate. Settings saves until the flash sector fills and rolls over, with power cut at each of the six rewrite steps: options and calibration survive a clean rollover, and the windows where the stock storage loses them are pinned. Settings storage (#149): a stale flash error flag, refused or stuck erases, refused programs and a non-blank free slot never write over old records, and normal saves match stmlib's byte for byte; the Plaits bank gesture: stepping, wrap, saving, the profile's starting bank, its [A] cue, FREEZE kept, buffer trim in granular and the wavetable synth, and custom profiles with Synth start or monophonic grains on [A]+FREEZE |
| `beads/test/pitch_quantizer.mk` | Major and Minor pentatonic grids and nearest-note boundaries, negative pitches, octave wrapping and ties; randomized pitch on scale notes, including microtonal scales; custom bank boundaries, repeat intervals and finite pitch ratios at the full synth input range; Recent notes: a note counts once the CV settles (stepped after 10 ms; glides from 6.7 down to 0.12 semitone per second, with noise, an RC glide and vibrato add no notes on the way), draws stay within the newest one to eight notes, Free when unpatched, the scale applied afterwards, Free and Fifths and octaves unchanged |
| `beads/test/clouds.mk` | Stock output hashes, Clouds-style output and switching, diffuser impulse energy and stereo channels |
| `beads/test/gain.mk` | Input gain with sanitizers: the learning window, headroom after a level rise, digital and tape clipping, and gain on quiet inputs |
| `beads/test/trigger.mk` | Grain triggering with sanitizers: free-running rates at both ends of DENSITY, silence at noon (free and clocked), clock probability and divisions, SEED and gate enabling, one grain per press or gate at noon, and density FM |
| `beads/test/granular.mk` | Grain playback and attenurandomizers with sanitizers: TIME, direction, SIZE, SHAPE, the PITCH notches and the smooth PITCH law, patched and unpatched knob laws, random distributions and the shared random stream; through the processor, parameters fixed at grain start, envelopes, voice stealing and the grain count, PITCH CV response, random pan, and CV channel separation; monophonic grains fading earlier grains; quantized grain pitch and trimmed-window reads; Recent notes grains |
| `beads/test/delay.mk` | Frozen delay with sanitizers: clocked delay with short and start-up clock intervals, free-running delay with FM from −0.7 to +0.5, slice changes only at slice boundaries, every clock divider ratio, and the clock's idle counter stopping at its limit; every tap reads a valid delay. Also DENSITY and its CV, the secondary tap, SEED clocking and its crossfade, TIME multipliers, SHAPE envelopes and TIME's random modulation |
| `beads/test/backup.mk` | Frozen-buffer save format byte by byte; no save before 10 s of FREEZE and a clean cancel; stock saves load; out-of-range headers refused when loading; corrupted audio or header refused on saves carrying the CRC tag; power lost at 428 points (including half-erased sectors) never restores a partial save; flash errors at 103 points abandon cleanly; trimmed and empty buffer saves, older saves, invalid lengths and trim under a FREEZE gate; settings saved during a save; the backoff after cut-short saves, its cap and resets, across the clock wrapping and after a restored frozen buffer |
| `beads/test/wavetable.mk` | Synth mode with sanitizers: last-bank, last-wave reads stay inside the table, and the guard row matches the last wave; the FM bank's four frequency ratios; FEEDBACK's bank selection and its hysteresis; dry/wet, FREEZE, the output resolution of each quality, 1 V/octave and pitch spread, and the dry sine level at octaves from C3 to C9; quantized synth grains; stereo width in rendered grains (Stock, zero, small and full displacement, CV clamped at both ends, grains keep the width they started with) |
| `beads/test/processor.mk` | The whole audio processor with sanitizers: each quality's sample rate, bit depth and buffer length, mono and stereo; FREEZE from the button and the gate; frozen grains keep playing; the crossfade when recording resumes; trim dispatch for every quality, mono and stereo |
| `beads/test/mix.mk` | Mixing with sanitizers: feedback into the recording, feedback saturation per quality, dry/wet, reverb after dry/wet and its tail when the send is closed, stereo output from a mono recording, feedback echoes in delay mode; the Mix law's stock and gentle gains and rendered grain levels; Reverb input Grains (no reverb fully dry, the dry neither sent nor ducked, delay and spectral); the options block matching the stock block bit for bit with no option on (every quality, mono, Clouds-style, diffusion, delay mode, spectral, gate output) |
| `beads/test/firmware.mk` | The firmware's own start-up, main loop and audio callback (`beads.cc`, unmodified) with hardware stubs and sanitizers: the wavetable synth after 10 s and on a [A] hold, mono/stereo switching, the quality lock while frozen, the overwrite indicator, the trigger output, CV routing, the power-on reset and corrupted-calibration branches, the pitch chain from CV input, and leaving the wavetable synth with a frozen buffer; pitch-scale CV routing and reverb switching; a saved pitch scale reaching wavetable synth grains (Automatic and Manual, random and CV, across a reboot) while the continuous oscillator stays unquantized; switching to and from the spectral engine (fade, cleared recording, frozen buffer kept in flash and restored with granular, no wavetable synth, no flash writes while frozen). A check keeps the test's copy of the main loop identical to `main()`'s; the Plaits banks from a blank sector, [A]+FREEZE to the played model, power cycles and a profile's starting bank; Mix law and Reverb input set at power-on by the profile, Off by default, both synths unchanged; Recent notes from PITCH CV through the main loop to the grains |
| `beads/test/codec.mk` | Real codec driver with a fake HAL: reset asserted during old clocks, 2048 old-rate frames before clock removal, new clocks stable before reset release, silent TX and callback detached throughout, unchanged boot start, null callback and SAI initialization failures |
| `beads/test/spectral.mk` | Spectral engine with sanitizers: CMSIS and ShyFFT conventions (imaginary sign, Nyquist slot, inverse scaling); the 4096-point FFT set-up equal to CMSIS' generic one; the port against Clouds' own phase vocoder fed the same input and parameters: identical with ShyFFT, 94/77/63 dB agreement with CMSIS in plain settings (coarse quantization and glitches turn rounding into whole steps); every quality, mono and stereo, through the whole processor with pinned hashes; diffuser and delay mode off; controls drawn once per frame, freeze, glitch and the pitch scale; the recording cleared on engine changes |
| `beads/test/delay_mode.mk` | Delay mode through the processor with sanitizers: continuous playback with SIZE fully clockwise, the fade when switching modes, the pitch shifter and its bypass at noon, the delay bandwidth control and SIZE CV |
| `beads/test/attack.mk` | Attacks: hit detection on the input and its three sensitivities, marks in the recording, grain starts moved onto them, Off leaving the grains alone |
| `beads/test/feedback_diffuser.mk` | Evolving frozen feedback: a stable loop, running only while FREEZE is on, cleared after a fade |
| `beads/test/plaits_synth.mk` | Plaits synth: every model renders, with and without AUX (five engines skip it, as in the firmware), the 6-op banks unpacked once for all voices as each engine unpacks them, with notes switching banks on every voice pinned to per-voice unpacking, voice allocation, the eight-model banks on FEEDBACK (detents, both ends and the held detent, including at noon, after a bank change, notes keeping their model), FREEZE sustain, RAM bounds; Speech word banks: the sliced decode matches Mutable Instruments' for every bank and after interrupted loads, at most 32 frames per block, and the word after a bank change is sample-exact to Mutable Instruments' controller with the change made once the bank is loaded; a waiting word is dropped by a Reset or the phonemes; the bit-reversal table matches a computed reversal for all 256 bytes; the banks decoded once for all voices leave each voice's RAM as its own decode does, call for call |
| `beads/test/profile.mk` | Profile validation, feature modes/control allocation, page slots 1-21, defaults and packed settings; ordered custom scale banks, IDs, UTF-8 names, finite note/repeat limits, reserved bytes, checksums, malformed banks and absent selections |
| `beads/test/storage.mk` | Damaged settings: an uncorrectable flash error at each calibration and settings record the firmware reads, bad checks, blank sectors with and without the calibration copy, interrupted saves and rewrites; nothing written while damaged; the SEED repair, the calibration it keeps, failed erases and programs, and power lost at each repair step |

| Program | Tests | Assertion sites |
|---|---:|---:|
| attack | 4 | 22 |
| backup | 25 | 166 |
| clouds | 5 | 17 |
| codec | 4 | 55 |
| delay | 11 | 46 |
| delay_mode | 14 | 89 |
| feedback_diffuser | 4 | 23 |
| firmware | 36 | 351 |
| gain | 6 | 29 |
| granular | 28 | 128 |
| mix | 10 | 49 |
| options | 69 | 795 |
| pitch_quantizer | 7 | 83 |
| plaits_synth | 11 | 62 |
| processor | 11 | 45 |
| profile | 6 | 108 |
| spectral | 8 | 42 |
| storage | 26 | 389 |
| trigger | 11 | 31 |
| wavetable | 14 | 68 |
| **Total** | **310** | **2,598** |

Counts are named test sections and `CHECK` call sites in the test sources; loops and parameter sweeps execute those sites many times.

From the repository root: `make -f beads/test/<name>.mk test`. The TSV/documentation check is not in the C++ counts. Tests follow firmware over the manual (7.8 ms shortest requested grain, reset keeping calibration, [M] LED levels, input gain falling after an overload); see [MANUAL_ERRATA.md](MANUAL_ERRATA.md). Removing each backup safeguard must fail its test; `beads/test/mutation_check.py`
injects recorded faults into newer tests, each required to fail (526 current patterns; Beta 2 had 396). It runs on GitHub Actions in 8 parallel shards (`--shard K/8`) for pull requests that change the firmware or tests, weekly and on demand.

Saves without the CRC tag (every stock save, or a damaged tag) load after the range checks alone, so stock and Drizzlet read each other's saves.

Feature checks beyond the table:
- **Stereo width:** the unchanged Stock calculation and RNG sequence, zero displacement, full spread with zero randomness, CV clamping, stock and enabled [M] cycles, every saved width byte, the [A]+DRY/WET gesture, its meter and saving, unchanged bytes 12/13, WIDTH CV routing, processor and synth propagation, rendered grain pan retention, saved assignments through the stock destination clamp.
- **Delay mode:** every stored byte value, preview and settled saving, Disabled at SIZE fully clockwise in every quality and channel mode, returning from delay to granular, detent boundaries, and neutral playback within 1e-7 FS with SIZE CV inside the detent.

Expanded feature checks also cover custom scale ID15 through the actual audio
callback in all four engines; reordered and removed saved selections; the
permanent pitch extension distinguishing ID15 from legacy unset settings;
Keep/Clear reverb tails through both synths and actual exit paths; stock
versus faded delay-mode SEED latching in all four qualities; and the four
profile choices of IDs 27-30, powered on through the firmware's own profile
block: unity manual gain for a blank gain (saved gains win), Attacks
thresholds, SEED unlatched with the latch options and fade, and the smooth
PITCH law without the notch cue, plus the Off defaults; and the Mix law
and Reverb input choices (IDs 32-33) from the profile. Recent notes is checked from the CV input to the grains.
Factory reference output and finite custom-scale pitch conversion have
focused checks. Beta 3 module results are under [On a module](#beta-3).

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

### Beta 3

**Images** (2026-11-14). Timing builds (the release code plus DWT cycle
counters) of `12180c6` and of the first LED fix `8da49f7` (even dimming plus a slower
quality-LED PWM; Beta 3 keeps only the even dimming, the PWM change moved to Beta 4), each with a
qualification Hygrostat profile, then the Beta 2
release image (`2558dd77…`) put back, verified, with the calibration checked
at the end of each run. Budget 120,000 cycles per block.

| Check | Image | Result |
|---|---|---|
| Options timeout, 5 s: open the pages, hands off | `12180c6` | Pass: the pages closed by themselves 5.0 s after the release (4,951–5,054 ms) |
| Pages 5–7: four [M] taps about 2 s apart | `12180c6` | Pass: the taps kept the pages open (18.7 s in all), then they closed by themselves. The bench does not read which page is shown |
| Option cost: Off, IDs 23, 26, 28, 30, 32 and 33 each alone, all On, Off again (granular stress fixture, 4,096 blocks each) | `12180c6` | Callback peaks 84.2–85.1%; each row's mean and peak within about 0.5 point of Off; the two Off means agree within 0.1%. The fade was armed outside delay mode and attacks had a silent input, so delay-mode fade cost and attack-driven grain counts are not measured |
| Hot granular: pinned music at about 5 V, FEEDBACK 0.9, heavy granular CVs, Snap, all options On (140 s) | `12180c6` | Pass: callback peak 98,450 (82.0%), no DAC underrun; the input limiter engaged. The click scan was not sensitive at this output level |
| Hot granular, rerun | `8da49f7` | Pass: callback peak 98,308 (81.9%), no DAC underrun |
| Recent notes CPU: the same melody with the attenurandomizer clockwise (follow) and counter-clockwise (Recent notes) | `12180c6` | Peaks 32,816 (follow) and 32,552 (Recent notes); the Recent notes mean 85 cycles higher (0.07 point) |
| Recent notes, stepped 16-note melody, 1.25 s per note (91 steps judged) | `12180c6` | Pass: 96.7% of steps sounded only pitch classes of the last eight notes |
| Recent notes, the same melody with 0.3 s glides | `12180c6` | Fail (#243): 28.6% of steps met that rule; mid-glide pitches registered as notes. The glide rule now in Beta 3 is not yet rerun on the module |
| Quality LED registers at 66%, white | `8da49f7` | Pass: TIM1 prescaler 7 (not in Beta 3); CCR 129/129/118 as computed. Beta 3 now uses the rule chosen by eye on the module (CCR 153/125/114 at 66%, 219/184/179 at 33%) |
| Quality LED registers at 100% and 33%, and quality 0 at all three levels | `8da49f7` | Pending |
| Noise floor at 66% against 100% LED brightness | `8da49f7` | Pending: the run stopped at its first noise check, a fault in the check itself |
| Plaits Speech word-bank change (#225): inputs unplugged, TIME stepped through the word banks every second | `c9608d0` | Fail: callback peak 203,724 (169.8%) against 38,428 (32.0%) with the bank held; fixed after `c9608d0` (word banks decode over several blocks), not yet re-measured on a module |

**CPU ceiling** (2026-11-15). Test builds of Beta 3 before the Speech fix
(`b92c048`, and `f547e2a` with the 2024 grain stealing restored) held the
callback at set lengths. Firmware counters flag a block whose first left
sample is written after the DAC DMA has read it. Each edge was found twice;
a first 48 kHz run (2026-11-14, `961bf6e`) agreed within 100 cycles.

| Codec rate (qualities) | Clean up to | First late block |
|---|---:|---:|
| 48 kHz (Bright digital, Sunny tape, synths) | 108,908 (90.8%) | 109,504 (91.3%) |
| 32 kHz (Cold digital) | 103,935 (86.6%) | 104,520 (87.1%) |
| 24 kHz (Scorched cassette) | 99,182 (82.7%) | 99,762 (83.1%) |

Heavy granular patch per quality: callback peak, and margin in points to
that rate's clean edge. The first left sample was on time in every row.

| Quality | Beta 3 | Margin | 2024 grain stealing (#237) | Margin |
|---|---:|---:|---:|---:|
| Bright digital | 98,625 (82.2%) | 8.6 | 102,751 (85.6%) | 5.1 |
| Sunny tape | 75,598 (63.0%) | 27.8 | 79,833 (66.5%) | 24.2 |
| Cold digital | 77,679 (64.7%) | 21.9 | not measured | – |
| Scorched cassette | 61,116 (50.9%) | 31.7 | 65,380 (54.5%) | 28.2 |

The 2024 grain stealing costs 3.4 to 3.6 points. The granular code is
unchanged since these builds but the code layout moved, so the heavy rows
will be rerun on the final build.

Not yet checked on the module for Beta 3: install by audio file, upgrade
from Beta 2, Off-equals-stock output, Plaits banks, custom scales and the
Gentler dry/wet and grains-only reverb sound.

### Beta 2

**Release image** (2026-11-07 and 2026-11-08): application `b11fe81c…`, standard
options. Peak load of the whole audio callback from timing builds of the same
code; Stock is a GCC 4.9.3 build of the vanilla source, Tuneup is beads-tuneup.
Budget 120,000 cycles.

| Heavy test patch | Stock | Tuneup | Beta 2 |
|---|---:|---:|---:|
| Granular | 91% | 77% | 79% |
| Wavetable synth | 86% | 83% | 84% |
| Wavetable synth, Clouds-style reverb, SIZE 97% | | | 84% (83.7%) |
| Plaits string model | | | 70% |
| Spectral | | | 18% |

No audio underruns in any capture; no late or skipped spectral frames.

| Check | Result |
|---|---|
| Install by audio file | Pass: application and WAV as pinned |
| Upgrade from Beta 1 (2026.10.18) | Pass: expected settings, calibration kept |
| Invalid Hygrostat profile | Pass: standard options, calibration kept |
| Storage repair (SEED at power-on), then a normal start | Pass |
| Settings after a power cycle | Pass: kept |
| Beta 1 or a vanilla build started once without saving, then Beta 2 | Pass: settings kept |
| Pops and fades at quality, mono/stereo and synth changes | Pending |

Profiles, custom scales, LED indications and synth-tail choices were not
checked on the module.

**Earlier images.** The release code, including the converter fix, on 2026-10-27, and with the storage fix (#149) on 2026-10-28. Outputs recorded with an Expert Sleepers ES-8; CPU load from a timing build of the same code.

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

Not yet checked on the module: a damaged calibration restored from its copy (host tests only), a stale flash error flag before a settings save (shown on a development board with the same flash controller, and in host tests), attacks with Snap alone. Hygrostat profiles, custom scale selection/pitch/LEDs, the nine additional options and both synth-tail choices were not checked on the module.

**Historical test module totals** (2026-10-18 to 2026-10-28; excludes later sessions). Firmware and settings installs:

Installs and automated hours after 2026-10-28 were not counted.

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

## Beta 3 host measurements

- Attack sensitivity, synthetic drums: High caught ghost notes 6 dB below the
  main hit where Normal caught about half; Low kept every main hit and ignored
  ghosts 3 dB down. Noise jumping by up to 6 dB every 10 ms: High marked 4
  false attacks in 4 s, Normal none. Steady and tremolo tones: none at any
  setting.
- Delay SEED latch fade: the choice is made once per block outside the sample
  loop; Off runs the stock delay code with no extra instructions per block
  (GCC 9 disassembly).
- PITCH knob: read once per audio block; Notches runs the stock law plus one
  comparison.
- Mix law: wet against dry at 9 o'clock (20%) is −7.6 dB with Gentle,
  −2.5 dB stock (Clouds −8.8 dB); at noon Gentle's wet is 3.5 dB under
  stock. Rendered grain levels follow the law: at 20% the wet energy is
  0.1296 (stock) and 0.0400 (Gentle) of fully wet, the squared gains.
- Reverb input: with Grains, fully dry leaves no reverb tail and the dry
  passes unducked with REVERB up; stock reverbs the dry in all three
  engines checked (granular, delay, spectral).
- Both choices: `Processor::Process` makes one test per block and runs the
  stock block when TIME scan, Mix law and Reverb input are all off; the
  options run a copy of the block in `processor_options.cc`. Measured on the
  GCC 9 release ELF in an emulator (instructions, not cycles), nine
  granular, delay and spectral scenarios: Off output is identical to the
  previous build and `Process` runs 4-8 fewer instructions per block. The
  test alone moved register allocation by 1-4 more; computing
  `sample_rate()` once (same output) more than recovers it. Heavy
  granular (Bright digital, stereo, Clouds-style, diffusion, 52,126 per
  block Off): Gentle +85, Grains +94, both +91; TIME scan On costs 77
  more than before, still the same output. Flash +8.5 KB.
- Plaits banks: FEEDBACK uses stmlib's hysteresis quantizer over eight equal
  detents. Against the first Beta 3 build's inset range, detent edges move by
  at most 0.58% of the knob's travel. A bank change now keeps the knob's
  side of a detent edge; before, hysteresis worked on the model number, so at
  noon green to red gave red's fourth model and red to yellow yellow's fifth,
  whichever side the knob came from. Idle voices start on model 0 on every
  synth entry (before, re-entry started them on the last selected model).
- Plaits Speech word banks (#225): stock decodes a whole bank in the block
  that selects it, up to 164,041 cycles for the largest (926 frames) on the
  Seed3. At most one voice changes bank per block (host count over drone,
  notes at every DENSITY, FREEZE, stepped, fast and random TIME), so the
  module's 169.8% was one decode. The bank now decodes 32 frames per block
  (at most 8,092 cycles); the voice holds its current frame meanwhile and a
  word triggered then starts when the bank is loaded: 7, 7, 16, 18 or 28
  blocks later (1.75 to 7 ms) for banks 1 to 5, each time a voice changes
  bank. From then on the output is sample-exact to Mutable
  Instruments' code with the change made at that block. Seed3, six voices
  (`PlaitsVoice` only), worst block before and after: note on a new bank
  every block 179,243 and 54,852; all six voices changing bank in one block
  1,000,112 and 62,409; five String voices beside Speech notes changing bank
  221,951 and 64,875, against six steady String voices at 65,290. Module
  estimate (69.8% with String, plus the Seed3 difference x 1.044): about 70%
  at worst; the stock sweep estimates 169%, as measured.

## Beta 4 host measurements

- Delay feedback, Unity notch (desktop renders, knob timing, one tap):
  a repeat at 1 kHz loses 0.003 dB (Bright digital) to 0.10 dB (Scorched
  cassette) per pass; at 100 Hz 0.09 to 0.12 dB, against 1.27 to 1.30 dB
  with stock's 40 Hz filter at stock unity. Fully clockwise still grows
  2.6 to 3.3 dB per pass, as stock. Past noon on DENSITY (two taps) the loop
  never grows; a short burst fades 5 to 10 dB over 4 s on the tape and
  Cold digital qualities and dies out on Bright digital, where stock grows
  into the limiter. Granular playback is identical with the choice on.
- Delay time, Crossfade (750 Hz sine, DENSITY 0.258 to 0.3): a step bends
  the pitch 4,553 to 5,950 cents with the glide and 2 to 12 cents with the
  crossfade; a one-second sweep 316 to 1,907 cents against 2 to 13 (cycles
  12 dB down skipped). Holding the delay time until DENSITY has been still
  for 50 ms makes the sweep one equal-power fade: the level changes by
  -6.7 to +3.5 dB on a step or a sweep, where a linear fade on every
  filter step dipped up to 9.7 dB on a step and 32 dB on a sweep. Gliding
  while DENSITY moves instead bent the pitch up to 5,950 cents; settling
  after one block, or 10 ms, still chained fades on a sweep (dips of 14
  and 31 dB). Clocked timing and TIME are identical with it on.
- Both choices Off, release ELF in an emulator (instructions, not cycles),
  19 granular, delay and spectral scenarios: output and executed
  instructions per block identical to the previous build. Knob-timing delay
  renders hash as before in all four qualities, mono and stereo. On, per
  block: Unity notch +154 to +224; Crossfade +243 to +378 (+331 to +905
  before the block-rate decision); both +338 to +479. Flash +6.7 KB.
- Unity notch with DENSITY jumping across noon every two blocks for 4 s:
  the loop does not grow, and the feedback at each block's end holds unity
  for the taps at both of its ends.
- Both choices on, FEEDBACK at the notch, a tone in phase at the old and
  new delay times: the fade's taps add to 1.41, the loop gain stays at
  1.00001 at most (it reached 1.414 before the notch allowed for the
  fade) and the passage that faded comes round within -0.13 to +0.06 dB
  (it was up to +2.3 dB). Allowing for the fade costs +4 to +6
  instructions per block with Unity notch, +7 to +13 with Crossfade and
  +14 to +19 with both; Off is unchanged. FREEZE slices glide with
  Crossfade on, identical to stock.

## Not tested

Everything above used one module. Also done on it for release candidate 3 (2026-10-17): trim, freeze, power cycle and stock downgrade with a trimmed save. Options page LED colours were checked by eye for all pitch scales, the limiter and a few other settings, not yet for every value.

- More than one module, or different hardware revisions.
- Long sessions and temperature.
- Power cuts while the settings memory is rewritten. About once every 4,000 settings saves, Mutable Instruments' storage code erases the settings sector and rewrites calibration and settings. A power cut during that rewrite loses the settings being saved; the calibration copy keeps the calibration. The host tests check this window and that the full rewrite keeps everything.
- The module's noise floor (hiss, #59), planned for a later release.
- Attack sensitivity on a module.
- Listening tests of the limiter modes. Their code is unchanged since the limiter's own measurements.
- The full bench suite was not rerun on 2026.10.16.

Report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).

## Settings sector tool

`make -f beads/build/tools/settings_sector.mk test` builds images with `settings_sector.py` (calibration only, settings only, both, a full sector) and loads each with stmlib's own storage code, as the firmware does at boot; dumps the firmware would not read are refused.

## Options checks

- `make -f beads/build/tools/options_tables.mk test` builds the TSV exporter and compares README.txt's Options reference with the firmware tables. `./build/options_tables` prints page, LED, knob, setting, value, colour and pulse.
- The UI host checks all 21 page/knob slots, cycling and wrap over sparse pages, the page LED masks, RAM page memory, inert Not-used knobs, the stored-value preview when a page opens, takeover, a sweep with one settled save, idle pages, close gestures, pending saves after closing, SEED scale cycling, steady-green stock values, and distinct colour/pattern combinations within each setting.
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

- `beads/test/ci-jobs.json` lists the host jobs and their commands; the workflow's `list-host-tests` job reads it into the matrix. Short suites share three jobs; delay mode uses three, Clouds two, and spectral and Plaits one each.
- Each make call uses `-j$(nproc)` and keeps its sanitizer flags. Native tests use `-O1` rather than `-Os`, which made sanitized DSP renders slow on the Ubuntu 24.04 compiler. Firmware flags are unchanged.
- The quantizer delay comparison warms each quality/channel processor once and restores that state, keeping all eight quality/channel cases, the 16000-block warmup and 1200 comparison blocks per output.
- Selection and local commands: [host test README](../../test/README.md).
