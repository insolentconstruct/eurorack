# Beads base for firmware modders

This branch starts at the released source in `origin/master` (`f4cfa76`).
It carries bug fixes, a reproducible compiler kit and three audio-preserving
CPU optimizations. Controls, gestures, settings layout and default sound
follow that source. Options that master compiles out stay compiled out.
Hardware operation has not been tested as part of this port.

CPU, busiest stock audio block on a development board (same die and clock as
Beads, DSP only; audio and buffer hashes match in all 24 scenarios):

| Build | Cycles of 120,000 | Budget |
|---|---:|---:|
| Released source (GCC 4.9.3) | 99,511 | 82.9% |
| This branch (GCC 9 and the three optimizations) | 90,529 | 75.4% |
| Trial with the integer Hermite read (not included yet; backport planned) | 82,911 | 69.1% |

Changes from master, one line per change:

- Bound frozen delay slices to the recording: negative FM cannot read outside the buffer ([#33](https://github.com/insolentconstruct/eurorack/issues/33)).
- Keep frozen slices at least one sample long: very fast SEED clocks cannot cause a zero-length slice (division by zero / undefined slice); audible output is not guaranteed ([#33](https://github.com/insolentconstruct/eurorack/issues/33)).
- Saturate the delay clock's idle counter: the first clock after a long idle keeps the correct tempo ([#57](https://github.com/insolentconstruct/eurorack/issues/57)).
- Abort backup writes after flash failure: a failed save cannot become a valid frozen recording ([#36](https://github.com/insolentconstruct/eurorack/issues/36)).
- Erase the header sector first and write the header last: interrupted saves cannot restore partial audio ([#36](https://github.com/insolentconstruct/eurorack/issues/36)).
- Clear stale saved FREEZE before erasure: a backup skipped at startup cannot be restored after an interrupted replacement ([#36](https://github.com/insolentconstruct/eurorack/issues/36)).
- Unlock flash before each backup block: saving a setting during a backup no longer breaks the recording save ([#36](https://github.com/insolentconstruct/eurorack/issues/36)).
- Validate backup ranges and CRC: malformed tagged recordings are rejected; stock recordings still load ([#36](https://github.com/insolentconstruct/eurorack/issues/36)).
- Initialize dummy grain triggers: background CPU padding no longer reads undefined fields ([#34](https://github.com/insolentconstruct/eurorack/issues/34)).
- Pad the wavetable with a duplicate final row: interpolation at the last wave stays within the table ([#35](https://github.com/insolentconstruct/eurorack/issues/35)).
- **Behaviour fix:** interrupted FREEZE saves double the next wait from 10 s up to 320 s, reducing flash wear; completion, one minute unfrozen or reboot resets the wait ([#56](https://github.com/insolentconstruct/eurorack/issues/56)).
- Compile the app with pinned GCC 9 and the bootloader with pinned GCC 4.9.3; explicit `fmaf` preserves random-pan rounding.
- Round pan, shape and speed explicitly: sound is identical to the previous build in 20 emulated scenarios, and GCC 9-16 now match as measured on Drizzlet.
- Select mono/stereo processing once per block with a template, sum grains locally and use unsigned read-index modulo; audio stays bit-identical.
- Add pinned firmware/WAV hashes, deterministic WAV encoding, licence collection, host tests and build/test CI.
- Pin the Python 3 audio encoder; the DSP support library remains at master's revision.

## Build and verify

Initialize the two required submodules, then run from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
docker build --platform linux/amd64 -t beads-build-kit:gcc9 beads/build
beads/build/build.sh /tmp/beads-base-build --wav
```

The output directory must be empty. Named releases check every firmware hash
against `beads/build/expected-hashes.sha256`, checks the encoded WAV against
`expected-wav.sha256`, decodes the WAV through the bootloader decoder and
collects source and compiler-runtime notices. It produces WAV and combo ZIPs.
`DOCKER_CONTEXT` works with remote contexts: source and outputs travel through
Docker's copy API. Compiler archive hashes and the base image digest are in
`beads/build/Dockerfile`; local verified archives can be supplied with its
`TC_LOCAL` and `GCC9_LOCAL` build arguments.

With `RELEASE_NAME` unset or `beads-base-dev`, hash mismatches warn and the
build continues. Other release names fail on a mismatch. To accept intentional
firmware and WAV changes, use `UPDATE_HASHES=1 beads/build/build.sh
/tmp/beads-base-update --wav` and commit both updated hash files. Re-pin after
cherry-picking an optimization if its expected hashes conflict.

Host tests run in the same Docker image:

```sh
beads/test/run.sh /tmp/beads-base-tests
```

Most suites use address and undefined-behaviour sanitizers. The backup suite
uses a mapped flash model with injected failures. No module is needed.
See [Adding or changing a test](beads/test/README.md#adding-or-changing-a-test)
for suite registration, stubs and audio re-baselining.
See [OPTIMIZATIONS.md](beads/build/docs/OPTIMIZATIONS.md) and [PORTING.md](beads/build/docs/PORTING.md) for DSP notes, and [STYLE.md](beads/build/docs/STYLE.md) for code style.

## Storage and licences

Settings use the unchanged stock 24-byte state. Backup bytes 0–9 and the audio
offset remain stock-compatible; a CRC occupies formerly unused header bytes.
Untagged stock backups receive range checks only. A damaged CRC tag is treated
as an untagged stock backup, so this format cannot detect every corruption.

Firmware sources keep their original licence notices. New kit and test files
use Insolent Construct headers. See `beads/build/LICENSE`, source headers,
submodule licences and the generated `NOTICE.txt`; both compiler runtime
licences are included in that notice. This branch provides no new UI or saved
options.

## Test commits

Existing history is retained. `64da95f` carries the delay tests for #33 and
#57, the shared checker and the encoder submodule update. `db6662c` carries
backup tests for #36 and #56. `a26b7f7` carries the wavetable boundary test.
`1f088db` adds the remaining host suites, storage compatibility and preserved
header audio comparison; `53dd76b` separates stock source and binary paths.
`5403e9a` adds CI. `233a2d5` shares suite rules, stubs and FPU setup and improves capture
diagnostics. `c18cd7a` adds the golden capture, Docker runner and explicit
release re-baselining. The following documentation commit explains extension
and validation.
