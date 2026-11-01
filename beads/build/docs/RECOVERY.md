# Settings backup and recovery

The settings sector holds the CV calibration and the settings. Back it up with ST-Link + OpenOCD; use a backup on the same unit only. Recalibrating the CV inputs: [CALIBRATION.md](CALIBRATION.md).

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"
```

Check that `beads-settings.bin` is 131072 bytes and label it with the module it came from.

To restore, build an image with Python 3 from the backup and, for a partial restore, a dump of the sector as it is now (`beads-now.bin`, made with the backup command):

- **Both:** `python3 beads/build/tools/settings_sector.py --calibration beads-settings.bin --settings beads-settings.bin beads-restore.bin`
- **Calibration only**, keeping today's settings, e.g. to undo a recalibration: `python3 beads/build/tools/settings_sector.py --calibration beads-settings.bin --settings beads-now.bin beads-restore.bin`
- **Settings only**, keeping today's calibration: `python3 beads/build/tools/settings_sector.py --calibration beads-now.bin --settings beads-settings.bin beads-restore.bin`

Then write it:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-restore.bin 0x081E0000; verify_image beads-restore.bin 0x081E0000; reset run; shutdown"
```

The image holds only the calibration and the latest settings. If the module showed the damaged-settings warning, restoring a backup this way also repairs it. OpenOCD erases the whole sector and programs just those, so later saves land on erased flash; don't write the 128 KB backup itself back. The tool refuses a dump the firmware would not read and never overwrites a file. To reset the settings to defaults instead, hold SEED while powering on; calibration is kept.

## ST-Link recovery

Back up calibration first, then flash the combo image from `<tag>-combo.zip`.
Erase only the sectors being written, as OpenOCD's `write_image erase` does.
Never use a full-chip or mass erase (STM32CubeProgrammer's "Full chip erase"): it also wipes calibration and settings.
The `.hex` carries its address, so STM32CubeProgrammer and most programmers need none; the `.bin` goes at 0x08000000. With OpenOCD, use your downloaded filename in place of `drizzlet-YYYY.MM.DD-combo.bin`:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; halt; flash write_image erase drizzlet-YYYY.MM.DD-combo.bin 0x08000000; verify_image drizzlet-YYYY.MM.DD-combo.bin 0x08000000; reset run; shutdown"
```

Older releases have no `.hex`; make one from the `.bin`:

```sh
arm-none-eabi-objcopy -I binary -O ihex --change-addresses 0x08000000 drizzlet-YYYY.MM.DD-combo.bin drizzlet-YYYY.MM.DD-combo.hex
```
