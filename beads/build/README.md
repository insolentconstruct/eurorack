# Drizzlet

Alternative firmware for Mutable Instruments Beads, built on Émilie Gillet's released source; not a Mutable Instruments product.

From [Émilie's eurorack README](https://github.com/pichenettes/eurorack/blob/master/README.md):

> Code provided as is, builds on a Desktop platform but not verified to build and run on a module. Install at your own risks.

Install at your own risk. Please do not contact Émilie Gillet or Mutable Instruments about it; report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).

## Downloads

Get the files from [GitHub Releases](https://github.com/insolentconstruct/eurorack/releases).

- `<tag>-wav.zip`: audio update, with `README.txt` and `NOTICE.txt`.
- `<tag>-combo.zip`: bootloader + app for ST-Link recovery.

What changed, including against older firmware: see [CHANGELOG.md](CHANGELOG.md).

## Updating your module

Follow the [official Beads firmware update procedure](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#firmware); the full steps are in the zip's `README.txt`.

**Do not hold SEED at power-on unless you mean to: it resets the settings (a valid calibration is kept).**

### Calibration backup and restore

Back up the settings/calibration sector with ST-Link + OpenOCD (same unit only):

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"
```

Restore the backup to the same unit only:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000; verify_image beads-settings.bin 0x081E0000; reset run; shutdown"
```

### ST-Link recovery

Back up calibration first, then flash the combo `.bin` from `<tag>-combo.zip`; this command doesn't touch the settings sector.

Use your downloaded filename in place of `drizzlet-YYYY.MM.DD-combo.bin`:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; halt; flash write_image erase drizzlet-YYYY.MM.DD-combo.bin 0x08000000; verify_image drizzlet-YYYY.MM.DD-combo.bin 0x08000000; reset run; shutdown"
```

## Building it yourself

On an amd64 host with Docker, run from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
docker build --platform linux/amd64 --target compiler -t beads-build-kit beads/build
bash beads/build/build.sh "$PWD/artifacts" --wav
```

The app uses GNU Arm Embedded 9-2020-q2-update (GCC 9.3.1). The bootloader
keeps GCC 4.9.3 so its padded image remains byte-identical to the recovery
bootloader. Both compilers are archive-pinned in the same image; the Python 3
encoder and pinned QEMU CPU are unchanged. `link-order.mk` adds `-std=gnu++98`
for the app, matching the compiler qualification build without changing the
released makefiles. It also keeps GCC 9's conservative uninitialized-variable
warnings visible without treating them as errors.

The build checks every firmware output against pinned community hashes.
GitHub Actions builds and publishes releases from `drizzlet-*` tags. CI also builds
the unchanged released app with GCC 4.9.3 and verifies its independent golden
payload hash. It checks WAV decoding and rejection controls; it does not prove
whole-firmware or hardware equivalence.

To build the optional golden reference with the same image:

```sh
bash beads/build/golden.sh "$PWD/golden"
```

This exports immutable eurorack `d8f473915232af74347762b6365202bfd26f73f6`
and stmlib `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` from Git, not the
working source or pan pin. Use a full Git checkout (CI uses `fetch-depth: 0`).
The golden app hash is `1f487b48e41dc4d813c5c457de115a4d798d001253ff90b5c5ef1b1a156eee6a`.

## License and credits

Code: MIT, by Émilie Gillet, with credits to Frédéric Meslin / Fred's Lab and Oliver Rockstedt; see [LICENSE](LICENSE).
The manual excerpt and hardware designs are licensed CC-BY-SA 3.0.
Copyright and permission notices are in `NOTICE.txt`; preserve them when redistributing.
