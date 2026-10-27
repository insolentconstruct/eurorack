# CV calibration

**TL;DR:** you should not need this. Each Beads was calibrated at the factory. Recalibrate only when PITCH CV tracking is clearly off and you have reliable 1 V and 3 V references. A poor reference makes tracking worse, and the new calibration replaces the factory one. Back up the settings first.

Mutable Instruments left this mode undocumented: "unless something is wrong with your power supply, you will never have to do it." It is the same in the stock firmware and in Drizzlet.

## What you need

Two steady voltages, 1 V and 3 V, each accurate to about ±1–2 mV. At PITCH, 1 mV is about 1.2 cents. Examples, each only when calibrated and checked:

- **A factory-calibrated quantizer**, such as Intellijel Scales set to chromatic with no transpose, fed roughly 1 V and 3 V. It outputs exact note voltages, held steady.
- **Mutable Instruments Module Tester** (DIY) after its own calibration: the "c1 note" and "c3 note" modes hold 1 V and 3 V. Older tester firmware lacks them; its "2oct arp" alternates 1 V and 3 V, so you must time each press to the right half.
- **A calibrated MIDI-to-CV converter**, such as Yarns (notes 48 and 72 are 1 V and 3 V) with no transpose, bend or glide.
- **Any stable adjustable source checked with a meter** whose specified DC accuracy, not its resolution, is good to ±1–2 mV, measured while plugged into Beads.

DC-coupled audio interfaces (Expert Sleepers ES-8, ES-9) are not calibrated sources unless you have measured and corrected their outputs.

## Before you start

- Back up the settings sector ([README](../README.md#backup-and-restore)). It holds the calibration.
- Unplug every CV input except the reference. The first [A] press also stores the 0 V point of TIME, SIZE, SHAPE and the [M] CV input.

## Steps

| Step | Patch | Then | LEDs afterwards |
|---|---|---|---|
| 1 | | Press [M] and [A] together | FREEZE lit, [A] green, [M] LEDs breathing |
| 2 | 1 V into PITCH | Press [A] | [A] yellow |
| 3 | 3 V into PITCH | Press [A] | SEED lit, [A] green |
| 4 | 1 V into DENSITY | Press [A] | [A] yellow |
| 5 | 3 V into DENSITY | Press [A] | Normal: saved |

The calibration is saved only after the last press. If the computed scale is more than 10% from nominal (the step from 1 V to 3 V measures outside about 1.8–2.2 V), all LEDs blink for about a second and Beads returns to normal without saving. The stored calibration is kept, but power-cycle Beads to reload it: the refused attempt can leave new values in use until then. The check catches only gross errors, such as an unplugged cable. 2 V and 4 V would pass, and so would a slightly wrong reference.

To undo a calibration, restore only the calibration from the backup ([README](../README.md#backup-and-restore)); your settings stay as they are.

## Tested

On the test module (2026-10-26 and 27), calibration with Intellijel Scales was saved twice. Each time, the PITCH scale was within 0.11% of the factory calibration and PITCH stayed within 9 cents of it from 0 to 5 V. The factory calibration was restored after each run.
