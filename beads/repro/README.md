# Repro kit

**TL;DR:** a scenario is a small TOML file that describes a panel, some
audio, some gates and CV, and what the output must do. The kit runs it and
prints PASS, FAIL, MEASURE or INVALID per rule. Today only the **desktop**
tier exists: the host build of the processor, in Docker. It is fast, but it
runs x86 floats, so it never matches the firmware bit for bit; compare
observables, not hashes.

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

The emulator tier is not built yet; its tag says where a scenario will run.

## Add a scenario

1. Copy one from `scenarios/behaviours/` and rename it (`name` must be unique).
2. Set `[panel]` (everything not listed is the bench panel: knobs at noon,
   attenuverters fully clockwise, wet). Optional keys: `limiter`
   (`stock`, `fast`, `off`), `pitch_scale` (`off` or a scale name),
   `assign_destination` (`feedback`, `dry_wet`, `reverb`) and
   `inputs_patched` (`false` unplugs both audio inputs). A tier that cannot set
   one refuses the scenario.
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
runner, tier adapters), `desktop/` the host driver and makefile, `scenarios/`
the TOML files, `tests/` the checks. The emulator, Seed3 and module tiers
come later and plug into `repro/tiers/`.
