# Drizzlet

Alternative firmware for Mutable Instruments Beads, built on the released Mutable Instruments source; not a Mutable Instruments product.

**Alpha:** please report any problems you find. [TESTING.md](TESTING.md) shows how it was tested.

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

## Monophonic grains

Hold MEDIA first, then press SEED to toggle monophonic grains; the setting
is saved and defaults off. The MEDIA/quality LED is half as bright when on
(the recording reserve can dim it further). This enables Mutable Instruments'
original code and gesture. Each new grain fades the previous grains out.
It suits the square and decay SHAPE settings. Slower shapes can go nearly
silent because each new grain cuts the previous one's attack. To recover
sound, switch to square or decay, reduce the trigger rate, shorten SIZE,
or use the same gesture to turn monophonic grains off. Turn them off before
downgrading to stock firmware.

## Frozen-buffer autosave

Autosave normally starts after FREEZE has been on for 10 seconds. A save cut
short after erasing doubles the next wait, up to 320 seconds, until a save
completes or FREEZE has been off for a minute; power-up starts at 10 seconds.
If FREEZE is released during self-play, the minute starts when self-play ends.

## Options page

Hold ASSIGN alone for three seconds, keeping the mix knobs still; then release.
The assignment LEDs pulse together on entry.
The feedback, dry/wet and reverb LEDs mark three categories; the open one's LED pulses slowly. The page opens on dry/wet. Tap ASSIGN to cycle categories.
In each category, the three mix knobs (FEEDBACK, DRY/WET, REVERB) are its three settings.
Move a knob more than one tenth of its travel to start selecting; its zones pick the value, shown on the quality LED (next to MEDIA).

| Category | Knob | Setting | Left third | Middle | Right third |
|---|---|---|---|---|---|
| Dry/wet LED | DRY/WET | Output limiter | Stock (steady green) | Fast release (one blue pulse) | Off (two amber pulses); loud peaks can hard-clip |

Self-play: Feedback LED category, FEEDBACK knob. Lower half selects Automatic
(stock, steady cyan); upper half selects On request (one magenta pulse per
1.2 seconds). Move more than one tenth of the travel to begin; the midpoint
has a small dead band to prevent flicker.
With On request, self-play never starts by itself. With both audio inputs
unpatched and FREEZE off, hold MEDIA for 1.5 seconds to start self-play;
hold again to stop. Patching either input also stops it (once FREEZE is off).
With an input patched, the hold restarts automatic gain as usual. Automatic keeps stock behaviour.
The setting is saved. Stock firmware ignores it; saving after a downgrade
resets it to Automatic.

Reverb character: Dry/wet LED category, REVERB knob. Lower half selects
Stock (default, steady green); upper half selects Clouds-style (one cyan pulse
per 1.2 seconds). Move more than one tenth of the travel to begin; the midpoint
has a small dead band. The quality LED shows the last knob moved on this page.
Clouds-style uses Mutable Instruments' Clouds reverb timings and modulation,
plus fixed diffusion on the grain wet output. In delay and self-play modes,
only the reverb character changes. Switching briefly fades the sound out and
back in and clears the old tail. The setting is saved; stock firmware ignores
it and resets it to Stock when saving after a downgrade.

The remaining knobs and categories are reserved for later options.
Hold ASSIGN for three seconds to save and exit. After five seconds without changes the page also exits and saves any change. Another button cancels.
While you edit, the mix settings keep their values; after exit they catch up smoothly with the knobs.

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

The app is built with GCC 9.3.1; the bootloader keeps GCC 4.9.3 and its
previous bytes. Both compiler archives are pinned.

Builds verify the firmware and WAV hashes. CI also builds the unchanged
released app with GCC 4.9.3 as a fixed golden reference, and checks WAV
decoding. Releases publish from `drizzlet-*` tags; the release notes include
the tag's CHANGELOG.md section, and publishing stops if it is missing.

To build the golden reference locally (needs a full Git checkout):

```sh
bash beads/build/golden.sh "$PWD/golden"
```

## License and credits

Code: MIT, from the Mutable Instruments source, with credits to Frédéric Meslin / Fred's Lab and Oliver Rockstedt; see [LICENSE](LICENSE).
The manual excerpt and hardware designs are licensed CC-BY-SA 3.0.
Copyright and permission notices are in `NOTICE.txt`; preserve them when redistributing.
