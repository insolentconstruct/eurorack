# CV calibration

Beads is factory-calibrated. Recalibrate only for clearly wrong PITCH CV
tracking, with reliable 1 V and 3 V references. Poor references worsen tracking
and replace factory calibration. Back up settings first. This undocumented
Mutable Instruments mode works the same in stock firmware and Drizzlet.

## What you need

Two steady voltages, 1 V and 3 V, each accurate to about ±1–2 mV. At PITCH, 1 mV is about 1.2 cents. Calibrate and check any source first:

- **A factory-calibrated quantizer**, such as Intellijel Scales set to chromatic with no transpose, fed roughly 1 V and 3 V. Hold each note steady.
- **Mutable Instruments Module Tester** (DIY) after its own calibration: the "c1 note" and "c3 note" modes hold 1 V and 3 V. Older tester firmware lacks them; its "2oct arp" alternates 1 V and 3 V, so you must time each press to the right half.
- **A calibrated MIDI-to-CV converter**, such as Yarns (notes 48 and 72 are 1 V and 3 V) with no transpose, bend or glide.
- **Any stable adjustable source checked with a meter** whose specified DC accuracy, not its resolution, is good to ±1–2 mV, measured while plugged into Beads.

DC-coupled audio interfaces (Expert Sleepers ES-8, ES-9) are not calibrated sources unless you have measured and corrected their outputs.

## Before you start

- Back up the settings sector ([RECOVERY.md](RECOVERY.md)). It holds the calibration.
- Unplug every CV input except the reference. The first [A] press also stores the 0 V point of TIME, SIZE, SHAPE and the [M] CV input.

## Steps

| Step | Patch | Then | LEDs afterwards |
|---|---|---|---|
| 1 | | Press [M] and [A] together | FREEZE lit, [A] green, [M] LEDs breathing |
| 2 | 1 V into PITCH | Press [A] | [A] yellow |
| 3 | 3 V into PITCH | Press [A] | SEED lit, [A] green |
| 4 | 1 V into DENSITY | Press [A] | [A] yellow |
| 5 | 3 V into DENSITY | Press [A] | Normal: saved |

Only the last press saves. A scale more than 10% from nominal (the 1–3 V
step measures outside about 1.8–2.2 V) is rejected: all LEDs blink for about
a second, then normal operation resumes without saving. **Power-cycle to
reload the stored calibration:** rejected values may remain active until then.
This catches gross errors such as unplugged cables; 2 V and 4 V or slightly
wrong references still pass.

To undo a calibration, restore only the calibration from the backup ([RECOVERY.md](RECOVERY.md)); your settings stay as they are.
