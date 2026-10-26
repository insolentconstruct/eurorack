# Recovery firmware

> **WARNING: use this ONLY if your module is stuck and does not start, and
> ONLY if its updater still works: power on with the FREEZE button (B) held,
> and the module must show the firmware update screen instead of starting.**
>
> **This is not a working firmware.** When it has finished, the module does
> nothing else. You must then install a normal firmware update file.

TL;DR: play `storage-recovery.wav` into the module the usual way. It erases the
settings and the saved frozen buffer, keeps the CV calibration when it can,
and blinks green when done. Then install a normal firmware: Mutable
Instruments' Beads 1.7 (2021), the early-2024 patch firmware, or a Drizzlet
release, again with FREEZE held at power-on.

This is an Insolent Construct tool. It does not come from Mutable Instruments.
**Do not contact Mutable Instruments for support with it: report problems on
this repository's [issues page](https://github.com/insolentconstruct/eurorack/issues).**

## What it does

On start-up, before reading any settings:

- Erases flash sectors 8 to 14 (bank 2 sectors 0 to 6, 0x08100000 to
  0x081dffff): the saved frozen buffer. They are not read first.
- Reads the CV calibration from sector 15 (bank 2 sector 7, 0x081e0000). It
  is kept if it reads back without errors and is within 25% of the factory
  values; otherwise it is reset to the factory values.
- Erases sector 15 and rewrites it: the calibration, and the settings at
  their factory values.

## LEDs

- **Working** (up to about 30 s): quality LED [A] blue, the three ASSIGN LEDs
  chasing.
- **Done**: [A] blinking green once per second. If the three ASSIGN LEDs are
  also lit, the calibration was reset: recalibrate after installing the normal
  firmware.
- **Failed**: [A] blinking red four times per second, the three ASSIGN LEDs
  blinking with it.

## Playing the file

Follow the firmware update procedure in the Beads manual. Unplug all CV
cables, patch your audio interface to IN L, set the TIME knob to an
intermediate position, and power on with FREEZE held. While the file plays,
[A] is a level meter: yellow is the optimal level. If all LEDs blink red, the
level is wrong: press FREEZE and retry with a different gain.

## Building

From the repository root, with the toolchain in `/usr/local/arm-4.9/`:

    make -f beads/recovery/makefile
    make -f beads/recovery/makefile wav

Host test of the flash logic: `make -f beads/test/recovery.mk test`.
