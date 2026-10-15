# Testing

How far release 2026.10.14 (alpha) was tested before release, by its developers. Hardware testing used one module.

## TL;DR

- **Automated:** every build is reproducible to pinned hashes, and host tests run on every push. They cover the options page and other panel gestures, input gain, grain triggering and playback, the attenurandomizers, the frozen-buffer save, delay mode and synth mode.
- **Before release:**
  - sanitizer runs show the two out-of-bounds reads (#33, #35) before their fixes and not after;
  - an emulator comparison against the previous release found identical audio in 44 of 44 scenarios.
- **On a module:** seven checks passed, from installing the release file to a stock firmware loading a Drizzlet save.
- **Not covered:** other modules, long sessions, and listening tests of the limiter modes.

## Automated (every push)

| Check | What it covers |
|---|---|
| Reproducible build | App, bootloader, combo and update file must match pinned SHA-256 hashes; the update file is decoded back to the app with the bootloader's own decoder |
| `beads/test/options.mk` | Options page entry, categories, knob zones, idle save, cancel and settings format; stock ASSIGN routing, amounts and their LEDs, the autosave chord and display, SEED latch gestures and start-up latch, quality cycling, the MEDIA gain hold, manual gain and its meter, the input-level blink while learning, and the FREEZE LED while saving; recovery mode keeps calibration; limiter arithmetic, stereo linking and mode switching. Normal and advanced builds |
| `beads/test/gain.mk` | Input gain with sanitizers: the learning window, headroom after a level rise, digital and tape clipping, and gain on quiet inputs |
| `beads/test/trigger.mk` | Grain triggering with sanitizers: free-running rates at both ends of DENSITY, silence at noon (free and clocked), clock probability and divisions, SEED and gate enabling, one grain per press or gate at noon, and density FM |
| `beads/test/granular.mk` | Grain playback and attenurandomizers with sanitizers: TIME, direction, SIZE, SHAPE, the PITCH notches, patched and unpatched knob laws, random distributions and the shared random stream |
| `beads/test/delay.mk` | Frozen delay with sanitizers: clocked delay with short and start-up clock intervals, free-running delay with FM from −0.7 to +0.5, slice changes only at slice boundaries, every clock divider ratio, and the clock's idle counter stopping at its limit; every tap reads a valid delay. Also DENSITY and its CV, the secondary tap, SEED clocking and its crossfade, TIME multipliers, SHAPE envelopes and TIME's random modulation |
| `beads/test/backup.mk` | Frozen-buffer save format byte by byte; no save before 10 s of FREEZE and a clean cancel; stock saves load; out-of-range headers refused on every save; corrupted audio or header refused on saves carrying the CRC tag; power lost at 428 points (including half-erased sectors) never restores a partial save; flash errors at 103 points abandon cleanly; settings saved during a save |
| `beads/test/wavetable.mk` | Synth mode with sanitizers: last-bank, last-wave reads stay inside the table, and the guard row matches the last wave; the FM bank's four frequency ratios |

The table lists the tests on the `drizzlet` branch now; several were added after release 2026.10.14. Run any of them from the repository root with `make -f beads/test/<name>.mk test`. The backup test was checked by removing each safeguard in turn, and `beads/test/mutation_check.py` breaks the firmware in a recorded way for each newer test: every change makes its test fail.

Saves without the CRC tag, including every stock save, load after the range checks alone, so stock and Drizzlet can read each other's saves. A save whose CRC tag itself is damaged is treated the same way.

## Before release

Run by the developers; these scripts and results are not in this repository.

| Check | Result |
|---|---|
| Frozen delay with negative FM (#33), host build with AddressSanitizer and UBSan | Previous code: out-of-bounds read. Fixed code: clean; output at FM 0 and +0.5 identical |
| Wavetable interpolation past the last wave (#35) | Previous code: out-of-bounds read. Fixed code: clean; normal output identical |
| Emulator comparison against the previous release | 44 of 44 scenarios bit-identical: all four qualities, extreme knobs, self-play (synth), freeze, save/restore, clipping, input gain, 16 grain-scheduler cases |
| Instruction counts | No audio-path function grew |
| Code review | Five rounds by a second AI model; the first four led to fixes (three of them real bugs in the frozen-buffer save) |

## On a module

**Setup:** one Beads, driven remotely and recorded:
- a Bela Gem Multi sets control voltages and gates;
- an ST-LINK debug probe reads the processor's state without stopping it;
- an oscilloscope captures both outputs;
- a laptop plays audio into the inputs.

| # | Check | Result |
|---|---|---|
| 1 | Install with the release update file, over the stock 2021 firmware | Decoded first time; new firmware running |
| 2 | Settings from an earlier Drizzlet survive the update | Yes |
| 3 | Options page: change the limiter, idle save, power cycle | Saved and kept |
| 4 | Frozen-buffer save and restore across a power cycle | Restored audio plays, stereo |
| 5 | A setting saved while the buffer is being saved | Restore still complete (the recorded tone plays back) |
| 6 | Frozen delay, FM swept negative with SEED low, held and pulsed | No faults in 1,200 samples of the program counter |
| 7 | Stock firmware (2021 release) loads a Drizzlet save | Yes: restored audio plays |

## Not tested

- More than one module, or different hardware revisions.
- Long sessions and temperature.
- Listening tests of the limiter modes. Their code is unchanged since the limiter's own measurements.

Report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).
