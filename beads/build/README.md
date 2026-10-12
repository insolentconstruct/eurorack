# Unofficial Beads released-source build kit

This is an unofficial build of Émilie Gillet's released Beads source, not a Mutable Instruments product. Source: eurorack `d8f473915232af74347762b6365202bfd26f73f6`; stmlib `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`; encoder `d391c6f71eaeaa40ffd5d3d761a6843f675bc2b1`. The release tag goes on the reviewed **kit commit**; the upstream source commit is stated separately in release notes.

## Build and verify

On an amd64 host with Docker, from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
docker build --platform linux/amd64 --target compiler -t beads-build-kit beads/build
docker build --platform linux/amd64 --target encoder -t beads-encoder beads/build
bash beads/build/build.sh "$PWD/artifacts" --wav
```

The output directory must be absent or empty. `--wav` is host orchestration: compilation/proof/packaging run in Bookworm, encoding runs separately in the digest-pinned Debian Buster Python 2.7.18 / NumPy 1.16.6 image. Python binaries are never copied between distributions. Containers have read-only source mounts, no network and run under the caller's UID, at nice 10. Set `BEADS_BUILD_IMAGE` / `BEADS_ENCODER_IMAGE` to use alternate local image names. Binary-only builds can invoke `bash beads/build/build.sh /out` inside the compiler container.

GNU ARM Embedded 4.9-2015-q2 is pinned by archive and extracted-tree SHA256; Debian packages use a fixed snapshot, and both base images use digests. A local compiler archive can be supplied through `--build-arg TC_LOCAL=filename.tar.bz2` in the build context, with identical hash checks. This kit requires amd64; arm64 is untested.

The additive make wrapper preserves package order, sorts filenames within packages and places startup last. Upstream compiler/link flags and resources stay unchanged. All five binary hashes are checked, and `--wav` must produce SHA256 `bf2c614016e0208f1a7b5e050dfe52f00b4ed8ba23f5cd9f0c6a06943574f26e`. Mismatch fails.

The newly generated waveform then passes the vendor demodulator/packet decoder at nominal 48 kHz and after 44.1 kHz resampling (Kaiser beta 14, float32 playback, return conversion to 48 kHz, gain 0.5, -70 dBFS noise, seed 0). Each case requires 1280 packets, five blocks, zero errors/overflow, EOT and exact app bytes plus FF padding to 327680 bytes. A corrupted-file control, missing-EOT control and wrong-payload control must fail acceptance. Infrastructure failures never count as a successful negative control. `decode-proof.json` binds results to the fresh WAV hash. This proves transport identity, not audio quality or arbitrary playback margins.

`NOTICE.txt` inventories both targets' compiled source and dependency files, including system headers (`-M`), preserving each original copyright/permission comment by file. It supplements those headers with actual linked archive names from the maps, toolchain license files and the ST HAL BSD-3-Clause terms. The download therefore carries applicable notices rather than credits alone.

The deterministic `beads-d8f4739-released.zip` contains the WAV, short `README.txt` and `NOTICE.txt`, sorted by name, stored uncompressed, timestamped 1980-01-01, with no extra fields or external attributes. Release assets are that zip, `app.bin`, `app.elf`, raw and padded bootloader bins, `combo.bin`, `NOTICE.txt` and `SHA256SUMS`. The kit separately pins the standalone WAV hash in `expected-wav.sha256`; SHA256SUMS covers the actual release assets. Locally retained objects, maps, source/license inventory and proof logs are not release assets.

GitHub Actions builds the images and runs `--wav` on push/PR. A `beads-*` tag triggers the same proof, then a separate release job runs `gh release create` with only that job granted `contents: write`. All actions are pinned by SHA. Review the kit before tagging: tagging it publishes the assets. No flashing occurs in CI.

## Flashing and calibration

Follow the [official Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#firmware) and the procedure below. The tested playback path was a 44.1 kHz audio interface at line level. The recorded 48 kHz stress limitation is retained explicitly.

Unplug all CV inputs/outputs. Connect a 44.1 kHz audio interface's line output
to IN L. The tested playback path uses 44.1 kHz at line level. Set TIME near
its midpoint. Leave SEED released; hold FREEZE while powering on to enter the
updater. Disconnect speakers/monitors and prevent notifications, background
music, equalizers, AGC and other audio effects from reaching the output.
Play the complete WAV without interruption. TIME sets input gain: adjust it
until the recording-quality LED is yellow (optimal level). Do not simply turn
TIME fully clockwise; the updater's gain is quadratic and excessive level clips.
FREEZE/SEED alternate while waiting, faster while syncing/receiving; all LEDs
light during a flash write. Errors blink all indicators (red per the manual).
Successful reception reaches the end of transmission and starts the app.
If it fails, power-cycle with FREEZE held and SEED released, then retry; the
manual also permits pressing FREEZE to retry after an error. Adjust level or
try another interface. Last resort: ST-Link with the released combo.bin; see
the build README for commands. Do not erase settings as part of that recovery.

The file is native 48 kHz. Nominal 48 kHz vendor decoding passes offline, but
a recorded +500 ppm / gain 0.3 / -60 dBFS noise stress case failed. Native-rate
48 kHz playback is not the qualified path. Use a 44.1 kHz interface; offline
resampling proof does not qualify every player or analog signal chain.

### Calibration backup and restore with ST-Link

**Same unit only.**
Before changes, save the complete 128 KiB settings sector, 0x081E0000-0x081FFFFF.
With host OpenOCD and the ST-Link connected to this unit:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"
```

Verify that beads-settings.bin is 131072 bytes and record its SHA256. Keep the
backup labelled with the unit's identity. Restore ONLY to that same unit:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000; verify_image beads-settings.bin 0x081E0000; reset run; shutdown"
```

Restoration erases/rewrites settings. Holding SEED at power-on erases and
rewrites this settings sector on this firmware. With valid settings, ordinary
boot does not write flash; corrupt/invalid settings are rewritten and the
module enters factory test even without SEED. This is a boot statement, not
a promise that background audio-buffer backup never writes flash.

This supplied WAV writes app blocks 0x08020000-0x0806FFFF, erasing three 128 KiB
sectors through 0x0807FFFF. It excludes the bootloader and settings 0x081E0000.
Evidence: bootloader.cc InitializeReception sets kStartAddress=0x08020000;
five decoded 64 KiB blocks drive ProgramPage's sector-aligned erases and
32-byte writes. EraseAllFirmwareSectors uses bank 1 sectors 1-7. The updater
has no general upper write bound: the settings claim applies to this bounded,
hash-verified file, not arbitrary oversized WAVs.

For last-resort ST-Link recovery, after saving calibration as above, program only the released combo's covered range:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; halt; flash write_image erase artifacts/combo.bin 0x08000000; verify_image artifacts/combo.bin 0x08000000; reset run; shutdown"
```

This overwrites bootloader and app; it does not request a settings-sector erase. Confirm the target/adapter configuration matches your unit. Backups belong to the same unit only. These commands are documentation and are never run by the kit.

Settings evidence: `beads.cc` reads immediate SEED into `recovery_mode`, then calls `settings.Init(recovery_mode)`; `settings.cc` erases when recovery is requested. `sector_storage_h7xx.h` `ChunkStorage::Init` rewrites invalid CALI/STAT records; the false return leads `beads.cc` into factory test. Valid settings are required for the no-boot-write statement.

Credits: Émilie Gillet, Frédéric Meslin / Fred's Lab, Oliver Rockstedt, ARM, STMicroelectronics and the toolchain contributors. Firmware and kit helpers have their applicable licenses reproduced in `NOTICE.txt`; hardware designs remain CC-BY-SA-3.0. Preserve notices when distributing binaries. See [LICENSE](LICENSE) and generated `NOTICE.txt`.
