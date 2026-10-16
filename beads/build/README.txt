Drizzlet
========
Alternative firmware for Mutable Instruments Beads; not a Mutable
Instruments product.

ALPHA: please report any problems you find. TESTING.md in the repository
shows how it was tested.

Note from the Mutable Instruments eurorack README:
"Code provided as is, builds on a Desktop platform but not verified to
build and run on a module. Install at your own risks."

Install at your own risk. Please don't contact Mutable Instruments;
report problems at:
https://github.com/insolentconstruct/eurorack/issues

Based on the 2024 recovery firmware (beads_patch_firmware.wav) plus one
change to grain stealing, as in the released source. In a test on one
module, the 2021 and 2024 firmware clicked with DENSITY and PITCH near
fully clockwise; builds with this change did not. This release adds
monophonic grains and self-play on request, reduces flash wear from a
modulated FREEZE, and fixes two delay bugs; the app is now built with GCC 9.
The output limiter option came in the previous release. Details are in
CHANGELOG.md in the repository.

Beads manual and development history:
https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/
https://pichenettes.github.io/mutable-instruments-documentation/trivia_and_history/beads_history/

Monophonic grains
-----------------
Hold MEDIA first, then press SEED to toggle monophonic grains; the setting
is saved and defaults off. The MEDIA/quality LED is half as bright when on
(the recording reserve can dim it further). This enables Mutable Instruments'
original code and gesture. Each new grain fades the previous grains out.
It suits the square and decay SHAPE settings. Slower shapes can go nearly
silent because each new grain cuts the previous one's attack. To recover
sound, switch to square or decay, reduce the trigger rate, shorten SIZE,
or use the same gesture to turn monophonic grains off. Turn them off before
downgrading to stock firmware.

Frozen-buffer autosave
----------------------

Autosave normally starts after FREEZE has been on for 10 seconds. A save cut
short after erasing doubles the next wait, up to 320 seconds, until a save
completes or FREEZE has been off for a minute; power-up starts at 10 seconds.
If FREEZE is released during self-play, the minute starts when self-play ends.

Options page
------------
Hold ASSIGN alone for three seconds, keeping the mix knobs still; then release.
The assignment LEDs pulse together on entry.
The feedback, dry/wet and reverb LEDs mark three categories; the open one's
LED pulses slowly. The page opens on dry/wet. Tap ASSIGN to cycle categories.
In each category, the three mix knobs (FEEDBACK, DRY/WET, REVERB) are its three
settings. Move a knob more than one tenth of its travel to start selecting; its
zones pick the value, shown on the quality LED.

Output limiter: dry/wet category, DRY/WET knob.
  Left third: Stock (steady green).
  Middle: Fast release (one blue pulse).
  Right third: Off (two amber pulses); loud peaks can hard-clip.
Self-play: Feedback LED category, FEEDBACK knob. Lower half selects Automatic
(stock, steady cyan); upper half selects On request (one magenta pulse per
1.2 seconds). Move more than one tenth of the travel to begin; the midpoint
has a small dead band to prevent flicker.
With On request, self-play never starts by itself. With both audio inputs
unpatched and FREEZE off, hold MEDIA for 1.5 seconds to start self-play;
hold again to stop. Patching either input also stops it (once FREEZE is off).
With an input patched, the hold restarts automatic gain as usual.
Automatic keeps stock behaviour.
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

Hold ASSIGN for three seconds to save and exit. After five seconds without
changes the page also exits and saves any change. Another button cancels.
While you edit, the mix settings keep their values; after exit they catch up
smoothly with the knobs.

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
