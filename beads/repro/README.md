# Repro kit

**TL;DR:** a scenario is a small TOML file that describes a panel, some
audio, some gates and CV, and what the output must do. The kit runs it and
prints PASS, FAIL, MEASURE or INVALID per rule. Two tiers exist. The
**desktop** tier is the host build of the processor, in Docker: fast, but x86
floats, so it never matches the firmware bit for bit; compare observables, not
hashes. The **emulator** tier runs the firmware's own `app.elf` in a Cortex-M7
emulator (Unicorn), so its render matches the firmware bit for bit for that
ELF; it takes minutes per scenario instead of seconds.

## Run

From the repository root (needs Docker, and the `stmlib` submodule):

```sh
git submodule update --init stmlib
beads/repro/repro.sh run desktop --tier desktop         # every scenario the desktop tier runs
beads/repro/repro.sh run desktop --scenario b-size-cv   # one scenario
beads/repro/repro.sh --native run desktop --tier desktop  # no Docker: needs g++, make, python 3.11+, numpy
beads/repro/repro.sh list                               # sets and scenarios
```

Results go to `repro-out/<set>/<tier>/`: one directory per scenario
(`stimulus.sha256`, `render.npy`, `observables.json`, `verdict.json`,
`provenance.json`, `timing.json`) plus `merged.json` and `merged.tsv`. The
exit code is 1 on any FAIL or INVALID.

`--jobs N` (or `BEADS_JOBS=N`) sets the worker processes; the default is
the smaller of 4 and the usable cores (container CPU limits count). Results
are byte-identical for any job count; only `timing.json` varies.

Tests: `cd beads/repro && python3 -m unittest discover -s tests -t .`

## Emulator tier

```sh
beads/repro/repro.sh run beta --tier emulator --elf PATH/app.elf --scenario beta-scale-fifths
beads/repro/repro.sh run beta --tier emulator --elf PATH/app.elf --shard 0/4 --jobs 4
beads/repro/repro.sh merge beta --tier emulator --out merged shard0 shard1 shard2 shard3
```

`app.elf` is the debug ELF from the build kit (`beads/build/build.sh`), or the
`repro-app-elf` artifact of the CI run. The emulator reads everything it needs
from that file: function and object addresses from the symbol table, field
offsets and enumerator values from the DWARF. Nothing about the firmware is
written down in the kit, so a layout change in the source cannot go stale
silently; a missing symbol or field stops the run with its name.

What a run does: boots the firmware as it boots on the module (its own
`Reset_Handler`, `main`, `Init()` and `Settings::Init`, reading a synthetic
settings bank), plays 1.5 s of silence so the codec start and the jack
detector settle, plays the lead-in of any tone a scenario says was already
running (`phase_s`, up to 17 s), then the scenario. The panel reaches the
firmware as pot ADC words (through the pot laws) and settings bytes; CV
reaches it as ADC words through the stored calibration; gates are codec event
bits; audio is codec frames at the media's own rate. Results do not depend on
the job count. `--window SECONDS` stops each render early and leaves the
stimulus unchanged.

Not modelled: the analog path (codec gain and noise, output polarity), the
real-time deadline, and DMA. Those scenarios stay on the module.

Golden test: `BEADS_ELF=PATH/app.elf python3 -m unittest tests.test_emu_golden`
re-runs 20 fixed fixtures (four media, five modes) and compares each output
hash with `tests/data/emulator_golden.tsv`. It skips without an ELF the table
was recorded on.

Timing (37 scenarios run together, 26 at a time on a 28-core host; one
scenario alone is about a quarter faster): a 19 to 24 s scenario takes 2.5 to
3.5 minutes, a 27 to 30 s one 4 to 6 minutes, the pitch-scale and random
scenarios (30 s plus a 17 s lead-in) about 9 minutes, and the 150 s Clouds A/B
scenario 21 minutes. All 37 together are about 4 CPU-hours; split four ways by
name hash that is 10 to 20 minutes of ideal wall time per shard on four cores,
far below GitHub's 6 hour limit.

## Scenarios and sets

`scenarios/behaviours/` holds the documented-behaviour scenarios (13) and
`scenarios/beta/` the Beta feature scenarios (30: media, mono, limiter, ASSIGN
destination, random side effects, pitch scales, crosstalk, quality switching).
Each lists the tiers it can run on; `repro list` prints them. Sets are in
`sets.toml`: `desktop` (the seven the host driver runs), `behaviours`, `beta`
and `p0`.

| Tier | Scenarios |
|---|---|
| desktop | b-seed-trigger, b-density-gated, b-freeze-gate, b-time-cv, b-pitch-cv, b-size-cv, b-shape-cv |
| emulator, module | those seven, plus b-assign-cv, u-delay, u-latched-clocked, self-play and the Beta scenarios that need grains or delay |
| module only | cal-dry, b-agc-limiter, beta-dry-crosstalk, beta-media*-dry (they measure the analog path) |

A scenario's `tiers` says where it can run.

## Add a scenario

1. Copy one from `scenarios/behaviours/` and rename it (`name` must be unique).
2. Set `[panel]` (everything not listed is the bench panel: knobs at noon,
   attenuverters fully clockwise, wet). A knob value is its position, 0 fully
   counter-clockwise to 1 fully clockwise, attenuverters included (0 is their
   randomizing end); `pitch` is the semitones the detune pot reports. Optional keys: `limiter`
   (`stock`, `fast`, `off`), `pitch_scale` (`off` or a scale name),
   `assign_destination` (`feedback`, `dry_wet`, `reverb`),
   `inputs_patched` (`false` unplugs both audio inputs), `self_play`
   (`automatic`, `on_request`), `reverb_style` (`stock`, `clouds`),
   `delay_mode` (`auto`, `never`, `wide_detent`), `width_assigned` and
   `stereo_width` (the saved byte, 0 to 255). A tier that cannot set one
   refuses the scenario; the desktop tier sets none of them.
3. Describe the audio in `[stimulus]`: `sine`, `chirps` or `silence` (optional
   `noise` uses the scenario seed), or `generator = "layers"` with one
   `[[stimulus.layers]]` table per signal, summed: `sine` (optionally repeated
   with `every` and `count`, with `dur_s` or `stop`), `steps` (back-to-back
   tones, `freqs` written out or `{ start, per_octave, count }`), `chirp`,
   `cyclic_chirp`, and seeded uniform `noise`. A layer plays on `channel`
   0, 1 or both.
4. Add `[[events]]`: `trigger` (gate pulses at `times`, written out or
   `{ start, step, count }`, with an optional `width`), `hold` (gate from `t0`
   to `t1`), `cv` (set a CV to `level` at `t`, optionally over `ramp` seconds).
   Times are seconds; levels are Bela-style fractions of full scale.
5. Add `[[rules]]`. Level: `silent`, `not_silent`, `level_step`, `dry_passes`,
   `band_ratio`, `tail_increase`. Grains: `onsets_match_triggers`,
   `onset_count`, `onset_count_min`, `rate_increasing`, `rate_order`,
   `rate_divided`, `duration_monotonic`, `direction`, `pitch_ratio`,
   `attack_slow_vs_bell`, `age_increasing`. Delay: `loop_period`,
   `echo_peak_ratio`, `pitch_shift`. Beta: `random_side_statistics`,
   `assign_cv_effect`, `grain_pitch_scale`, `dry_crosstalk`, `clouds_ab`.
   Windows are `[start, end]` seconds. A tier that measures a noise floor or
   the quality-switch times passes them as rule context; without them
   `dry_crosstalk` and `clouds_ab` are INCONCLUSIVE.
6. List it in `sets.toml`.

SIZE fully clockwise is delay mode, not a bigger grain. Scenarios say what
they expect with `[guard] delay_mode = "never"` (default), `"always"` or
`"any"`; a mismatch makes the run INVALID, and a file with the knob up but
the default guard is refused when loaded.

## Layout

`repro/` holds the Python package (scenario schema, stimulus, shared rules,
runner, tier adapters), `repro/emu/` the emulator (machine, ELF reader, panel
mapping, firmware driver), `desktop/` the host driver and makefile,
`scenarios/` the TOML files, `tests/` the checks. The Seed3 and module tiers
come later and plug into `repro/tiers/`.
