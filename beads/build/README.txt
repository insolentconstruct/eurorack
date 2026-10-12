Unofficial Beads released-source audio update
==============================================

PLEASE READ FIRST. Emilie Gillet's own note on the released Beads code
(eurorack README, 2026-10-12):

  "Code provided as is, builds on a Desktop platform but not verified to
   build and run on a module. Install at your own risks."

This file is an UNOFFICIAL build of that code, made and tested by the
community, not by Emilie Gillet or Mutable Instruments. It is not a Mutable
Instruments product. Install at your own risk. Please do NOT contact Emilie
Gillet or Mutable Instruments about it; report problems on the GitHub
repository this release came from.

Built unmodified from Emilie Gillet's released Beads source, eurorack commit
d8f473915232af74347762b6365202bfd26f73f6. Credits: Emilie Gillet,
Frederic Meslin / Fred's Lab, Oliver Rockstedt. Copyright and permission
notices are in NOTICE.txt; keep them when redistributing this file.


What changed from earlier firmware
---------------------------------
This release is the 2024 recovery firmware (beads_patch_firmware.wav) plus
one change to how grains are stolen when they arrive faster than 25 ms apart.
The full list, back to the 2021 forum release, is in CHANGES.md in the build
kit (beads/build/CHANGES.md in the GitHub repository).


Firmware update procedure
-------------------------
(From the Beads manual by Emilie Gillet, licensed CC-BY-SA 3.0:
https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#firmware
Reproduced unchanged; this section remains under CC-BY-SA 3.0.)

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


Notes for this build
--------------------
* Do NOT hold SEED while powering on. On this firmware, SEED held at power-on
  erases and rewrites the settings sector, which holds your calibration.
  A valid calibration is kept, but back it up first if you can (below).
* Playback: this file has been flashed successfully from a 44.1 kHz audio
  interface line output; update files in the same format have also been
  flashed from a laptop headphone output. The file itself is 48 kHz. In offline stress testing, decoding at the
  native 48 kHz failed once under an extreme clock error combined with
  noise. If updates keep failing, set your computer's output to 44.1 kHz
  or try another device.
* This update file writes only the application area (0x08020000-0x0807FFFF).
  It does not touch the bootloader or the settings/calibration sector.
* Last resort if audio updates keep failing: program the release's -combo.bin file (bootloader +
  this firmware) with an ST-Link. See the build kit README for the command.


Calibration backup and restore (ST-Link + OpenOCD; SAME UNIT ONLY)
------------------------------------------------------------------
Back up the 128 KiB settings sector (calibration and settings):

  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"

Check that beads-settings.bin is 131072 bytes and keep it labelled with the
unit it came from. Calibration is specific to each module: never restore a
backup onto a different unit. To restore onto the SAME unit:

  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000; verify_image beads-settings.bin 0x081E0000; reset run; shutdown"

With valid settings, a normal power-on does not write flash. If the settings
sector is corrupt, the module rewrites it and starts in factory-test mode
("breathing" LEDs) even without SEED held; restoring your backup fixes that.
