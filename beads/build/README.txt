Drizzlet
========
Alternative firmware for Mutable Instruments Beads; not a Mutable
Instruments product.

Note from the Mutable Instruments eurorack README:
"Code provided as is, builds on a Desktop platform but not verified to
build and run on a module. Install at your own risks."

Install at your own risk. Please don't contact Mutable Instruments; report problems at:
https://github.com/insolentconstruct/eurorack/issues

This is the 2024 recovery firmware (beads_patch_firmware.wav) plus one
change to grain stealing; details are in CHANGELOG.md in the repository.

Options page
------------
Hold ASSIGN alone for three seconds, keeping the mix knobs still; then release.
The assignment LEDs pulse together on entry; the feedback LED marks the limiter page.
Move DRY/WET more than one tenth of its travel to start selecting.
Left third: Stock (steady green MEDIA); middle: Fast release (one blue pulse).
Right third: Off (two amber pulses); loud peaks can hard-clip.
Tap ASSIGN to cycle pages; dry/wet and reverb pages are reserved.
Hold ASSIGN for three seconds to save and exit; five seconds idle or another button cancels.
DRY/WET stays held during editing and catches up smoothly after exit.

Firmware update procedure
-------------------------
Beads manual by Emilie Gillet, CC-BY-SA 3.0; reproduced unchanged:
https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#firmware

If you think your module is behaving strangely, updating the firmware is
certainly not the right thing to do.

Unplug all CV inputs/outputs from the module. Connect the output of your
audio interface/sound card to the IN L input (1). Set the TIME knob [E] to an
intermediate position. Power on your modular system with the FREEZE
illuminated push-button (B) pressed.

Make sure that no additional sound (such as email notification sounds,
background music etc.) from your computer will be played during the
procedure. Make sure that your speakers/monitors are not connected to your
audio interface - the noises emitted during the procedure are aggressive and
can harm your hearing. On non-studio audio equipment (for example the line
output from a desktop computer), you might have to turn up the gain to the
maximum.

When you are all set, play the firmware update file into the module. While
the module receives data, the recording quality LED [A] will act as
vu-meter (lit in yellow when the signal level is optimal). Try adjusting the
TIME knob to adjust gain. When the end of the audio file is reached, the
module automatically restarts - if it is not the case, please retry the
procedure from the beginning.

In case the signal level is inadequate, all LEDs will blink in red. Press the
button [B] and retry with a higher gain. If this does not help, please retry
the procedure from another computer/audio interface, and make sure that no
piece of equipment or software effect (equalizer, automatic gain control, FX
processor) is inserted in the signal chain.

Notes
-----
* Do NOT hold SEED at power-on: it rewrites the settings/calibration
  sector, keeping a valid calibration.
* The audio update writes only the application area. It leaves the
  bootloader and settings/calibration sector untouched.
* If updates keep failing, set the computer's output to 44.1 kHz or try
  another device. As a last resort, flash the -combo.bin from -combo.zip
  via ST-Link; see the build kit README for the command.

Calibration backup and restore (ST-Link + OpenOCD; same unit only)
----------------------------------------------------------------
Back up the 128 KiB settings/calibration sector:

  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"

Check that beads-settings.bin is 131072 bytes. Label it with its unit.
Restore to the same unit only; calibration is specific to each module:

  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000; verify_image beads-settings.bin 0x081E0000; reset run; shutdown"

Corrupt settings make the module start in factory-test mode (breathing
LEDs); restoring the backup fixes it.

Credits: Mutable Instruments, Frederic Meslin / Fred's Lab, Oliver Rockstedt.
Copyright and permission notices are in NOTICE.txt; keep them when
redistributing.
