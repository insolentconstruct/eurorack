# Unofficial Beads source build

This is an unofficial build of Émilie Gillet's released Beads source, not a Mutable Instruments product. It builds the unmodified source from [eurorack d8f473915232af74347762b6365202bfd26f73f6](https://github.com/pichenettes/eurorack/commit/d8f473915232af74347762b6365202bfd26f73f6), with stmlib `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` and stm_audio_bootloader `1ec7d6374e2d305cb710170eb80de9b66e795792`. The release differs from the last distributed firmware by one unreleased grain-scheduler change.

From a clean checkout of this branch, with Docker on an amd64 host:

```sh
git submodule update --init --recursive
docker build --platform linux/amd64 -t beads-build-kit beads/build
mkdir artifacts
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/artifacts:/out" beads-build-kit \
  bash beads/build/build.sh /out
```

Use an empty output directory. The image pins GNU ARM Embedded 4.9-2015-q2, its archive and extracted-tree hashes, its base image digest and Debian snapshot. A separate digest-pinned Python 2.7.18 stage installs a hash-checked NumPy 1.16.6 wheel for the encoder. To use an existing compiler archive, copy it into `beads/build/` and pass `--build-arg TC_LOCAL=filename.tar.bz2`; the same hash checks apply. Remove it afterward. The amd64 compiler uses 32-bit runtimes. Arm64 is untested here; a separate beads-port `toolchain/Dockerfile.arm64` recipe exists but is not included in this kit.

The wrapper groups `.cc`, `.c`, then `.s` sources, sorts filenames within each declared package, preserves package order, sorts the additional CMSIS/HAL groups, and puts startup last. This makes byte identity independent of the stock wildcard expansion order. Upstream files, compiler flags and checked-in resources are unchanged; resource regeneration is disabled regardless of checkout timestamps. `expected-hashes.sha256` checks the bootloader sector and app payload against reproduced release-source builds; the combo hash is derived from their concatenation and independently checked against the upstream HEX merge rule. A mismatch stops the build.

Artifacts:

- `bootloader.bin`: 33,196-byte bootloader payload at `0x08000000`.
- `bootloader-padded.bin`: bootloader zero-filled to a 128 KiB sector.
- `app.bin`: 268,384-byte app payload at `0x08020000`.
- `app.elf`: app symbols and debug information; debug paths affect its hash.
- `combo.bin`: padded bootloader followed by the full app, 399,456 bytes.
- `SHA256SUMS`: checksums of generated artifacts. Logs, objects, link commands and maps are also retained locally.

**WAV pending:** the pinned encoder does not recognize the makefile's `stm32h7` target. No update WAV is shipped. `build.sh /out --wav` calls that encoder with the makefile flags (`-t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256`) and fails until H7 support is resolved; Python 2 and NumPy are included. Binary builds and CI currently omit this step.

When a verified update WAV is available, follow the [original module manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#firmware-update-procedure): unplug CV connections, connect the audio output to IN L, set TIME midway, and hold FREEZE while powering on. Play the file without effects or other system sounds, with monitors disconnected. Adjust TIME for a yellow quality LED; the module restarts on completion.

For an ST-Link, the following is the makefile's `upload_combo_jtag_erase_first_sector_and_settings` sequence, using a host OpenOCD installation with STM32H7 support. **It overwrites the bootloader and application and erases settings/calibration.** This kit never runs flashing automatically.

```sh
openocd -f interface/stlink-v2.cfg -f target/stm32h7x_dual_bank.cfg \
  -c "init" -c "halt" -c "sleep 200" \
  -c "flash erase_address 0x08000000 0x20000" \
  -c "flash erase_address 0x081e0000 0x20000" -c "reset halt" \
  -c "flash write_image erase artifacts/combo.bin 0x08000000" \
  -c "verify_image artifacts/combo.bin 0x08000000" \
  -c "sleep 200" -c "reset run" -c "shutdown"
```

Source credits: Émilie Gillet, Frédéric Meslin (Fred's Lab), and Oliver Rockstedt. Firmware and this kit are MIT licensed; hardware designs are CC-BY-SA-3.0. Preserve the copyright and permission notices in the upstream source when redistributing it. The kit's MIT notice is in [LICENSE](LICENSE).
