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

## Flashing

The release zip's `README.txt` is the user-facing guide. It contains:

1. **The official procedure.** The Beads manual's firmware update procedure, reproduced unchanged under its CC-BY-SA 3.0 license. Source: [the Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#firmware), by Émilie Gillet.
2. **Notes for this build:**
   - **Do not hold SEED at power-on.** On this firmware it erases and rewrites the settings sector, which holds the calibration.
   - The update file writes only the application area.
3. **Calibration backup and restore** with an ST-Link, for the same unit only (below).

The manual's own advice covers quiet sources: *"On non-studio audio equipment … you might have to turn up the gain to the maximum."*

### Calibration backup and restore with ST-Link (same unit only)

```sh
# back up the 128 KiB settings sector (calibration + settings)
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"

# restore onto the SAME unit only
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000; verify_image beads-settings.bin 0x081E0000; reset run; shutdown"
```

- Check that the backup is 131,072 bytes, and label it with its unit. Calibration is per-module.
- With valid settings, a normal power-on does not write flash.
- If the settings are corrupt, the module rewrites the sector and starts in factory test even without SEED held. Restoring the backup fixes this.

### Last resort: ST-Link with combo.bin

Back up the calibration first (above). Then program only the combo's range: the bootloader and the app. This does not erase the settings sector.

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; halt; flash write_image erase artifacts/combo.bin 0x08000000; verify_image artifacts/combo.bin 0x08000000; reset run; shutdown"
```

### Notes for builders and reviewers

- **What this update writes.** It writes 0x08020000–0x0806FFFF, erasing three 128 KiB sectors through 0x0807FFFF. The bootloader and the settings sector (0x081E0000) are excluded.
  - Evidence from `bootloader.cc`:
    - `kStartAddress=0x08020000`;
    - five decoded 64 KiB blocks drive `ProgramPage`'s sector-aligned erases and its 32-byte writes;
    - `EraseAllFirmwareSectors` covers bank 1, sectors 1–7.
  - The updater itself has no general upper write bound. So this claim covers this bounded, hash-verified file, not arbitrary oversized WAVs.
- **Settings evidence.**
  - `beads.cc` reads the immediate SEED state into `recovery_mode`, and `settings.cc` erases when recovery is requested.
  - In `sector_storage_h7xx.h`, `ChunkStorage::Init` rewrites invalid CALI/STAT records, and the resulting false return sends `beads.cc` into factory test.
- **Playback tests.** Tested on hardware: a Bela at 44.1 kHz line level (it is quiet, so TIME was turned fully up) and a laptop headphone output.
  - Offline, the vendor decoder passes the nominal 48 kHz and the resampled 44.1 kHz cases.
  - One severe stress case failed: native 48 kHz with +500 ppm clock error, gain 0.3 and −60 dBFS noise.

Credits: Émilie Gillet, Frédéric Meslin / Fred's Lab, Oliver Rockstedt, ARM, STMicroelectronics and the toolchain contributors. Firmware and kit helpers have their applicable licenses reproduced in `NOTICE.txt`; hardware designs remain CC-BY-SA-3.0. Preserve notices when distributing binaries. See [LICENSE](LICENSE) and generated `NOTICE.txt`.
