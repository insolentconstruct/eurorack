# Base tests

Run `beads/test/run.sh OUTPUT_DIR` from the repository root. It wraps the
build kit Docker image and runs eleven suites, bidirectional stock storage
compatibility, a golden audio hash and the optional optimization comparison.
Most suites use address and undefined-behaviour sanitizers. Backup and storage
use mapped flash models with injected failures. No module is needed.

`storage_compat.sh OUTPUT_DIR [STOCK_SOURCE_DIR]` builds actual stock and base
readers and exchanges settings and frozen buffers in both directions for all
eight media/channel formats. It extracts four stock files from `f4cfa76`, or
uses the optional exported directory (also accepted as `STOCK_SOURCE_DIR`).
Stock runs without sanitizers because its writer reads a full flash word from
a smaller header.

`audio_reference.sh OUTPUT_DIR` hashes every captured float byte against
`audio_reference.sha256`. Use the pinned image for stable compiler output.
`--optimizations` additionally compares against the four preserved DSP headers
in `reference/`, built with the same flags. Twenty scenarios cover four media
and stereo grains, mono grains, delay, synth and unpatched synth. Each uses
2,400 warm-up blocks and 2,000 capture blocks, checks finite/nonzero output,
and exercises feedback, reverb, modulation and freeze at stock defaults.
Host floating point comparisons do not establish ARM equivalence.

CI runs these checks and two pinned firmware/WAV release builds, checks release
hashes and WAV decoding with corruption, missing-EOT and wrong-payload controls.

## Stub models

Include order is suite overrides, shared `stubs/`, then real source headers.
Keep overrides limited to the hardware or dependencies a suite touches.

| Directory | Model |
|---|---|
| `stubs/` | Common switch pins, ADC channels, normalization, GPIO/settings flash and LED state |
| `backup_stubs/` | Processor fixture and flash with lock, failure, power-loss and partial erase injection |
| `hardware_stubs/` | Processor/audio fixtures: codec, calibrator, gain and backup |
| `firmware_stubs/` | Callback ADC/codec, inputs, system/debug/watchdog; sector-aware shared flash model |
| `storage_stubs/` | Settings-sector flash with sticky error flags, refused or stuck operations and overwrite counting |
| `delay_mode_stubs/` | Codec handle declarations |
| `gain_stubs/` | Codec gain writes |

Backup flash globals are declared `extern` in its HAL header; each executable
owns its initial state. Shared LED methods live in `stubs/leds.cc`.

## Adding or changing a test

Everything runs in the build kit image; no module is needed:

```sh
docker build --platform linux/amd64 -t beads-build-kit:gcc9 beads/build
beads/test/run.sh /tmp/beads-tests
```

One suite, from the repository root (inside the image):

```sh
docker run --rm --network none -v "$PWD:/src:ro" beads-build-kit:gcc9 \
  make -j4 -f beads/test/mix.mk BUILD_ROOT=/tmp/t/ TARGET=/tmp/t/mix_test test
```

A failure prints the test name and failing condition:

```text
ERROR: TestDryWetEndpoints: mix_error < 0.00001f
```

Write a `void TestSomething()` in the closest `*_test.cc`, start it with
`test_checker.Start("TestSomething")`, use `CHECK()`, and call it from `main()`.
Every suite calls `InitTestFpu()` at startup. For a new suite, copy a short
`.mk`, set `TARGET`, `CC_FILES`, optional `STUBS` and `SUITE_FLAGS`, and include
`suite.mk`. `run.sh` discovers these files automatically; there is no second
suite list. `SANITIZERS =` disables sanitizers only for mapped flash tests.
Object/dependency files live under each target's own build directory.

List host suites without Docker or builds:

```sh
beads/test/run.sh --list
```

`--list` prints one host suite per line, excluding the separate storage and
audio drivers. A plain run executes all suites, storage compatibility and audio
reference checks, including the optimization comparison.

CI builds the shared kit, then runs one host-tests job and two independent
release builds in parallel. A final job waits for all three and diffs the two
release `SHA256SUMS` manifests. All host suites together take about 40 seconds
in Docker; sharding costs more in per-job image downloads than it saves.

`TestMainLoopMatchesHarness` in `firmware_test.cc` compares the source text of
`beads.cc`; editing `main()` requires updating that harness check too.

If DSP changes intentionally, inspect and listen to the capture, then re-baseline
in the pinned image:

```sh
UPDATE=1 beads/test/run.sh /tmp/beads-tests
```

Or, inside the image, run `UPDATE=1 beads/test/audio_reference.sh /tmp/beads-tests`.
Commit `beads/test/audio_reference.sha256` with the change. The full runner also
checks preserved optimization headers; intentional changes to those optimizations
may require refreshing the preserved headers or running the golden check alone.
Firmware and WAV hashes are re-pinned with
`UPDATE_HASHES=1 beads/build/build.sh /tmp/beads-base-update --wav`.

The runner mounts source read-only by default, read/write for `UPDATE=1`.
For a remote Docker daemon, copy the checkout into a container, export stock
sources if Git metadata is absent, and set `BEADS_TEST_CONTAINER=1` to invoke
the same in-container runner. `JOBS` controls parallel compilation (default min(4, cores)).

## Mutation check

`beads/test/mutation_check.py [--only TEST] [OUTPUT_DIR]` copies `beads` and
`stmlib`, breaks one recorded bug fix or optimization at a time and runs only
the named test, which must fail (`ERROR: TEST:` or a sanitizer report; a
mutation that does not compile is an error). Survivors are listed and fail
the run. `--patterns-only` checks, without building, that every replaced text
occurs exactly once. The cases cover #33, #57, #36, #56 and #35 and the
local grain sum. Run it inside the image, from the repository root:

```sh
mkdir -p /tmp/mutations
docker run --rm --network none -v "$PWD:/src" -v /tmp/mutations:/out \
  -w /src beads-build-kit:gcc9 python3 beads/test/mutation_check.py /out
```

The script makes a scratch copy under `beads/test`, so the checkout must be
writable. The weekly `Mutation check` workflow runs it. About 15 seconds.

Not covered, because the tests cannot see them: the upper slice bound (#33),
the flash-failure aborts in `TestFlashErrors` (#36), the dummy grain trigger
fields (#34), the mono/stereo template and the unsigned read index. The audio
reference stays identical when the grain player is disabled, so it cannot
vouch for those paths.

`run.sh` accepts `DOCKER_CONTEXT` for a remote Docker host: it copies sources
in and results out instead of bind-mounting.
