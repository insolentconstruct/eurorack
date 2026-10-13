# Unofficial Beads firmware build

An unofficial, community build of Émilie Gillet's released Beads source; not a Mutable Instruments product.

From [Émilie's eurorack README](https://github.com/pichenettes/eurorack/blob/master/README.md):

> Code provided as is, builds on a Desktop platform but not verified to build and run on a module. Install at your own risks.

Install at your own risk. Please do not contact Émilie Gillet or Mutable Instruments about it; report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).

## Downloads

Get the files from [GitHub Releases](https://github.com/insolentconstruct/eurorack/releases).

- `<tag>-wav.zip`: audio update, with `README.txt` and `NOTICE.txt`.
- `<tag>-combo.zip`: bootloader + app for ST-Link recovery.

For changes compared with older firmware, see [CHANGELOG.md](CHANGELOG.md).
Contributing code: follow [STYLE.md](STYLE.md).

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

Use your downloaded filename in place of `beads-2026.10.11-combo.bin`:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; halt; flash write_image erase beads-2026.10.11-combo.bin 0x08000000; verify_image beads-2026.10.11-combo.bin 0x08000000; reset run; shutdown"
```

## Building it yourself

On an amd64 host with Docker, run from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
docker build --platform linux/amd64 --target compiler -t beads-build-kit beads/build
docker build --platform linux/amd64 --target encoder -t beads-encoder beads/build
bash beads/build/build.sh "$PWD/artifacts" --wav
```

The build checks every firmware output against pinned hashes.
GitHub Actions builds and publishes releases from `beads-*` tags.

## License and credits

Code: MIT, by Émilie Gillet, with credits to Frédéric Meslin / Fred's Lab and Oliver Rockstedt; see [LICENSE](LICENSE).
The manual excerpt and hardware designs are licensed CC-BY-SA 3.0.
Copyright and permission notices are in `NOTICE.txt`; preserve them when redistributing.
