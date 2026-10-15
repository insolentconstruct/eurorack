# Drizzlet

Alternative firmware for Mutable Instruments Beads, built on the released Mutable Instruments source; not a Mutable Instruments product.

**Alpha:** tested only by its developers, on one module; see [TESTING.md](TESTING.md) for what was covered. Expect bugs, and please report them.

From the [Mutable Instruments eurorack README](https://github.com/pichenettes/eurorack/blob/master/README.md):

> Code provided as is, builds on a Desktop platform but not verified to build and run on a module. Install at your own risks.

Install at your own risk. Please do not contact Mutable Instruments about it; report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).

## Downloads

Get the files from [GitHub Releases](https://github.com/insolentconstruct/eurorack/releases).

- `<tag>-wav.zip`: audio update, with `README.txt` and `NOTICE.txt`.
- `<tag>-combo.zip`: bootloader + app for ST-Link recovery.

For the module itself, read the [Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/) and the [Beads development history](https://pichenettes.github.io/mutable-instruments-documentation/trivia_and_history/beads_history/).
What changed, including against older firmware: see [CHANGELOG.md](CHANGELOG.md).
Where the Beads manual and the firmware disagree, see [MANUAL_ERRATA.md](MANUAL_ERRATA.md).

Contributing code: follow [STYLE.md](STYLE.md). Before adding a feature, check [FEATURES.md](FEATURES.md) for which buttons, settings bytes, flash and RAM are already taken.

## Options page

Hold ASSIGN alone for three seconds, keeping the mix knobs still; then release.
The assignment LEDs pulse together on entry.
Options come in three categories, marked by the feedback, dry/wet and reverb LEDs: the open category's LED pulses slowly. The page opens on the dry/wet category. Tap ASSIGN to cycle categories.
In each category, the three mix knobs (FEEDBACK, DRY/WET, REVERB) are its three settings.
Move a knob more than one tenth of its travel to start selecting; its thirds pick the value, shown on the quality LED (next to MEDIA).

| Category | Knob | Setting | Left third | Middle | Right third |
|---|---|---|---|---|---|
| Dry/wet LED | DRY/WET | Output limiter | Stock (steady green) | Fast release (one blue pulse) | Off (two amber pulses); loud peaks can hard-clip |

The other knobs and categories are reserved for later options and do nothing yet.
Hold ASSIGN for three seconds to save and exit. After five seconds without changes the page also exits and saves any change. Another button cancels.
The mix knobs stay held during editing and catch up smoothly after exit.

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

The build checks every firmware output against pinned hashes.
GitHub Actions builds and publishes releases from `drizzlet-*` tags.

## License and credits

Code: MIT, from the Mutable Instruments source, with credits to Frédéric Meslin / Fred's Lab and Oliver Rockstedt; see [LICENSE](LICENSE).
The manual excerpt and hardware designs are licensed CC-BY-SA 3.0.
Copyright and permission notices are in `NOTICE.txt`; preserve them when redistributing.
