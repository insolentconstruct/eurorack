# Recovery firmware

> **WARNING: use this ONLY if your module is stuck and does not start, and
> ONLY if its updater still works: power on with the FREEZE button (B) held,
> and the FREEZE and SEED LEDs must flash alternately instead of the module
> starting.**
>
> **This is not a working firmware.** When it has finished, the module does
> nothing else. You must then install a normal firmware update file.
>
> **YOUR SETTINGS WILL BE ERASED, AND POSSIBLY YOUR CALIBRATION.** If neither
> stored copy of the CV calibration can be read cleanly and looks right, it is
> reset to the factory defaults (the three ASSIGN LEDs stay lit at the end). You MUST then
> recalibrate the CV inputs, and that needs accurately calibrated 1 V and 3 V
> reference voltages ([CALIBRATION.md](https://github.com/insolentconstruct/eurorack/blob/drizzlet/beads/build/docs/CALIBRATION.md)).
> Without them, PITCH CV will not track 1 V/octave. Do not run this tool
> unless you can do that.
>
> **WE CANNOT SUPPORT YOUR HARDWARE.** If this tool does not fix your module,
> the only remaining route is an ST-LINK debug probe and a stable wire harness
> soldered or clipped to the module's debug header, to erase and reflash it
> directly ([ST-Link recovery](https://github.com/insolentconstruct/eurorack/blob/drizzlet/beads/build/README.md#st-link-recovery)).
> You will have to learn and do that yourself; we cannot help with it.

**Why this exists:** ModWiggler users reported the same startup failure with
Mutable Instruments' own firmware in 2023–2024, before Drizzlet existed, and
quoted Mutable Instruments putting it down to corrupted settings /
frozen-buffer flash. Installing a firmware update does not directly overwrite that
area. We have no evidence that Drizzlet causes it, but we have not ruled it
out.

TL;DR: play `storage-recovery.wav` into the module the usual way. It erases the
settings, Drizzlet's options and the saved frozen buffer, keeps the CV
calibration when either stored copy reads back cleanly and is within 25% of
the factory values, and blinks green when done. Then install a normal firmware: Mutable
Instruments' Beads 1.7 (2021), the early-2024 patch firmware, or a Drizzlet
release, again with FREEZE held at power-on.

This is an Insolent Construct tool. It does not come from Mutable Instruments.
**Do not contact Mutable Instruments about it.** You can say whether it
worked on this repository's [issues page](https://github.com/insolentconstruct/eurorack/issues),
but we cannot diagnose or repair modules.

## What it does

On start-up, before erasing anything:

- Reads the CV calibration from sector 15 (bank 2 sector 7, 0x081e0000) and
  from its copy in sector 14 (bank 2 sector 6, 0x081c0000), which Drizzlet
  (from Beta 2) and beads-tuneup keep. A copy is usable if it reads back
  without uncorrectable errors and is within 25% of the factory values.
- Keeps sector 15's copy, as the firmware does, unless it is exactly the
  factory values and sector 14 holds a calibration of the module's own. If
  neither copy is usable, the factory values are used.

Then:

- Erases sectors 8 to 13 (bank 2 sectors 0 to 5, 0x08100000 to 0x081bffff):
  the saved frozen buffer. They are not read.
- Rewrites sectors 14 and 15 one at a time, the one holding the copy it
  keeps last. Neither is erased unless the other holds a usable calibration,
  read back first. Sector 14 ends with the calibration only, so Drizzlet's
  options go back to their defaults; sector 15 with the calibration and the
  settings at their factory values.

If the power is cut while it works, a usable calibration stays in one of the
two sectors. Power the module on normally (without FREEZE held) and the tool
runs again from the start.

## LEDs

- **Working** (up to about 30 s): quality LED [A] blue, the three ASSIGN LEDs
  chasing.
- **Done**: [A] blinking green once per second. If the three ASSIGN LEDs are
  also lit, the module now has the factory calibration values (reset, or
  already stored that way): recalibrate after installing the normal firmware.
- **Failed**: [A] blinking red four times per second, the three ASSIGN LEDs
  blinking with it.

## Playing the file

Follow the firmware update procedure in the Beads manual. Unplug all CV
cables, patch your audio interface to IN L, set the TIME knob to an
intermediate position, and power on with FREEZE held. While the file plays,
[A] is a level meter: yellow is the optimal level. If all LEDs blink red, the
level is wrong: press FREEZE and retry with a different gain.

## Building

From the repository root, in the build kit (the golden compiler and the
pinned encoder, as the firmware release build uses):

    bash beads/recovery/build.sh /absolute/empty/output/dir

Host tests: `make -f beads/test/recovery.mk test` (the flash logic, with a
power cut at every erase and program) and
`bash beads/test/recovery_compat.sh /absolute/output/dir` (each firmware's own
settings code, before and after recovery).
