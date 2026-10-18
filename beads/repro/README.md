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
beads/repro/repro.sh run p0 --tier desktop              # the p0 set
beads/repro/repro.sh run p0 --scenario b-size-cv        # one scenario
beads/repro/repro.sh --native run p0 --tier desktop     # no Docker: needs g++, make, python 3.11+, numpy
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

## Add a scenario

1. Copy one from `scenarios/behaviours/` and rename it (`name` must be unique).
2. Set `[panel]` (everything not listed is the bench panel: knobs at noon,
   attenuverters fully clockwise, wet) and `[stimulus]` (`sine`, `chirps` or
   `silence`; optional `noise` uses the scenario seed).
3. Add `[[events]]`: `trigger` (gate pulses at `times`), `hold` (gate from
   `t0` to `t1`), `cv` (set a CV to `level` at `t`, optionally over `ramp`
   seconds). Times are seconds; levels are Bela-style fractions of full scale.
4. Add `[[rules]]`: `silent`, `onsets_match_triggers`, `onset_count`,
   `onset_count_min`, `rate_increasing`, `duration_monotonic`, `direction`.
   Windows are `[start, end]` seconds.
5. List it in `sets.toml`.

SIZE fully clockwise is delay mode, not a bigger grain. Scenarios say what
they expect with `[guard] delay_mode = "never"` (default), `"always"` or
`"any"`; a mismatch makes the run INVALID, and a file with the knob up but
the default guard is refused when loaded.

## Layout

`repro/` holds the Python package (scenario schema, stimulus, shared rules,
runner, tier adapters), `desktop/` the host driver and makefile, `scenarios/`
the TOML files, `tests/` the checks. The emulator, Seed3 and module tiers
come later and plug into `repro/tiers/`.
