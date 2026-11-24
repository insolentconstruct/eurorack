# Settings backup and recovery

Two flash sectors hold the CV calibration and the settings: the settings sector (0x081E0000: calibration and stock settings) and its copy sector (0x081C0000: a copy of the calibration, then Drizzlet's options). Back both up with ST-Link + OpenOCD; use a backup on the same unit only. Recalibrating the CV inputs: [CALIBRATION.md](CALIBRATION.md).

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; dump_image beads-options.bin 0x081C0000 0x20000; reset run; shutdown"
```

Check that both files are 131072 bytes and label them with the module they came from.

To restore, first dump both sectors as they are now with the same command, naming the files `beads-now.bin` and `beads-options-now.bin`. Then build the two images with Python 3:

- **Both:** `python3 beads/build/tools/settings_sector.py --calibration beads-settings.bin --settings beads-settings.bin --options beads-options.bin --now beads-now.bin --mirror-out beads-restore-copy.bin beads-restore.bin`
- **Calibration only**, keeping today's settings and options, e.g. to undo a recalibration: `--calibration beads-settings.bin --settings beads-now.bin --options beads-options-now.bin`, the rest as above.
- **Settings only**, keeping today's calibration: `--calibration beads-now.bin --calibration-copy beads-options-now.bin --settings beads-settings.bin --options beads-options.bin`, the rest as above.

Without `--options`, Drizzlet's options start from the profile's defaults. The tool prints `write 1` and `write 2`, each an image and its address: write them in that order, and go on only if the first verifies. The first command leaves the module halted, so the firmware doesn't start between the two:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase IMAGE1 ADDRESS1; verify_image IMAGE1 ADDRESS1; shutdown"
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase IMAGE2 ADDRESS2; verify_image IMAGE2 ADDRESS2; reset run; shutdown"
```

The copy sector goes first while the settings sector holds a usable measured calibration. Otherwise the settings sector goes first. For a settings-only restore with damaged or factory calibration in the settings sector, supply the current copy-sector dump with `--calibration-copy`. The tool uses the same calibration preference as a normal boot. Each image holds only the calibration and the latest settings or options, so later saves land on erased flash; don't write the 128 KB backups themselves back. If the module showed the damaged-settings warning, restoring this way also repairs it. The tool requires the requested record in each input, rejects a calibration more than 25% from factory values, and creates only new files. To reset the settings and options to defaults instead, hold SEED while powering on; calibration is kept.

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
