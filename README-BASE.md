# Beads base for firmware modders

This branch starts at the vanilla source in `origin/master` (`f4cfa76`).
It carries bug fixes, a reproducible compiler kit and five audio-preserving
CPU optimizations. Controls, gestures, settings layout and default sound
follow that source; a copy of the calibration is also kept in bank 2
sector 6, which stock firmware leaves unused. Options that master compiles
out stay compiled out.
Hardware operation has not been tested as part of this port.

CPU, busiest stock audio block on a development board (same die and clock as
Beads, DSP only; audio and buffer hashes match in all 24 scenarios):

| Build | Cycles of 120,000 | Budget |
|---|---:|---:|
| Vanilla source (GCC 4.9.3) | 99,511 | 82.9% |
| GCC 9 and the first three optimizations | 90,529 | 75.4% |
| This branch (with the integer Hermite read) | 82,911 | 69.1% |

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
- **Behaviour fix:** settings saves check every flash operation and only write onto erased flash. A corrected ECC error left over from an earlier read made the HAL skip the sector erase, and the records were then written over the old ones, leaving a module that crashes at every start ([#149](https://github.com/insolentconstruct/eurorack/issues/149)).
- **Behaviour fix:** bank 2 (settings and frozen buffer) is read with faults masked, so an uncorrectable ECC word no longer crashes the module at every start; a damaged frozen buffer is skipped ([#148](https://github.com/insolentconstruct/eurorack/issues/148)).
- **Behaviour fix:** the calibration is copied to bank 2 sector 6 before each settings erase and restored from it when the stored one is lost or implausible, so a save interrupted by a power cut keeps the calibration; at worst the settings return to their defaults. Master instead resets the calibration to factory values and starts the factory test on a damaged calibration record, and resets everything when no settings follow it ([#148](https://github.com/insolentconstruct/eurorack/issues/148)).
- **Behaviour fix:** with both copies of the calibration lost, the module plays on factory calibration and default settings, blinks all LEDs for 3 s at each power-up and writes nothing. SEED held at power-up repairs it (the [A] LED blinks green slowly with the three [M] LEDs lit: repaired, recalibrate; it blinks red fast: failed) and erases damaged frozen-buffer sectors. On a healthy module SEED at power-up resets the settings as in master ([#148](https://github.com/insolentconstruct/eurorack/issues/148)).
- **Behaviour fix:** no flash erase or write starts while the 3.3 V supply is below the programmable voltage detector's level 6 (about 2.8 V); a save cut short is handled like a power cut. No option bytes change ([#165](https://github.com/insolentconstruct/eurorack/issues/165)).
- Refuse to link an application that reaches bank 2 ([#155](https://github.com/insolentconstruct/eurorack/issues/155)).
- Add `beads/build/tools/settings_sector.py`: restore the calibration, the settings or both from a settings-sector backup.
- **Behaviour fix:** quality, mono/stereo and synth changes that keep the sample rate leave the converter running on silence instead of stopping it, which removes the pop it made; changes to or from vintage digital (32 kHz) and dirty tape (24 kHz) still restart it ([#53](https://github.com/insolentconstruct/eurorack/issues/53)).
- **Behaviour fix:** quality, mono/stereo and synth changes fade the output out over 10 ms before the converter pauses or restarts, then fade it back in ([#53](https://github.com/insolentconstruct/eurorack/issues/53)).
- Compile the app with pinned GCC 9 and the bootloader with pinned GCC 4.9.3; explicit `fmaf` preserves random-pan rounding.
- Round pan, shape and speed explicitly: sound is identical to the previous build in 20 emulated scenarios, and GCC 9-16 now match as measured on Drizzlet.
- Select mono/stereo processing once per block with a template, sum grains locally and use unsigned read-index modulo; audio stays bit-identical.
- Interpolate grain reads on the stored integers and scale once, as Clouds' `ReadHermite` does; audio stays bit-identical.
- Compute the synth's playing grains in pairs, mixed in index order: 5.7 points of headroom in its busiest case; audio stays bit-identical.
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
The combo ZIP holds bootloader + app as `.hex`, which needs no address, and
`.bin`, which goes at 0x08000000. Erase only the sectors being written: a
full-chip or mass erase also wipes calibration and settings.
`DOCKER_CONTEXT` works with remote contexts: source and outputs travel through
Docker's copy API. Compiler archive hashes and the base image digest are in
`beads/build/Dockerfile`; local verified archives can be supplied with its
`TC_LOCAL` and `GCC9_LOCAL` build arguments.

With `RELEASE_NAME` unset or `beads-base-dev`, hash mismatches warn and the
build continues. Other release names fail on a mismatch. To accept intentional
firmware and WAV changes, use `UPDATE_HASHES=1 beads/build/build.sh
/tmp/beads-base-update --wav` and commit both updated hash files. Re-pin after
cherry-picking an optimization if its expected hashes conflict.

To back up and restore the settings sector (calibration and settings) with
ST-Link and OpenOCD, on the same unit only:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"
python3 beads/build/tools/settings_sector.py --calibration beads-settings.bin \
    --settings beads-settings.bin beads-restore.bin
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-restore.bin 0x081E0000; verify_image beads-restore.bin 0x081E0000; reset run; shutdown"
```

Give `--calibration` or `--settings` a dump of the sector as it is now to
restore only the other one. Each dump must hold a valid calibration and at
least one settings record, as the firmware loads it. The image holds just
the two records, so later saves land on erased flash; don't write the
128 KB dump back. Restoring this way also repairs a module showing the
damaged-settings warning.

Host tests run in the same Docker image:

```sh
beads/test/run.sh /tmp/beads-base-tests
```

Most suites use address and undefined-behaviour sanitizers. The backup suite
uses a mapped flash model with injected failures. No module is needed.
See [Adding or changing a test](beads/test/README.md#adding-or-changing-a-test)
for suite registration, stubs and audio re-baselining.
See [OPTIMIZATIONS.md](beads/build/docs/OPTIMIZATIONS.md) and [PORTING.md](beads/build/docs/PORTING.md) for DSP notes, and [STYLE.md](beads/build/docs/STYLE.md) for code style.
[MANUAL_ERRATA.md](beads/build/docs/MANUAL_ERRATA.md) lists where the Beads manual differs from the firmware.

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
