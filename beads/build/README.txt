Unofficial Beads released-source audio update
Not a Mutable Instruments product. Built from Emilie Gillet's eurorack source
commit d8f473915232af74347762b6365202bfd26f73f6. Credits: Emilie Gillet,
Frederic Meslin / Fred's Lab, Oliver Rockstedt, ARM and STMicroelectronics.
Copyright/permission notices are in NOTICE.txt; retain them when redistributing.
The release tag identifies the reviewed BUILD KIT commit, not the source commit.

Audio flashing (official Beads manual procedure):
Unplug all CV inputs/outputs. Connect a 44.1 kHz audio interface's line output
to IN L. The tested playback path uses 44.1 kHz at line level. Set TIME near
its midpoint. Leave SEED released; hold FREEZE while powering on to enter the
updater. Disconnect speakers/monitors and prevent notifications, background
music, equalizers, AGC and other audio effects from reaching the output.
Play the complete WAV without interruption. TIME sets input gain: adjust it
until the recording-quality LED is yellow (optimal level). Do not simply turn
TIME fully clockwise; the updater's gain is quadratic and excessive level clips.
FREEZE/SEED alternate while waiting, faster while syncing/receiving; all LEDs
light during a flash write. Errors blink all indicators (red per the manual).
Successful reception reaches the end of transmission and starts the app.
If it fails, power-cycle with FREEZE held and SEED released, then retry; the
manual also permits pressing FREEZE to retry after an error. Adjust level or
try another interface. Last resort: ST-Link with the released combo.bin; see
the build README for commands. Do not erase settings as part of that recovery.

The file is native 48 kHz. Nominal 48 kHz vendor decoding passes offline, but
a recorded +500 ppm / gain 0.3 / -60 dBFS noise stress case failed. Native-rate
48 kHz playback is not the qualified path. Use a 44.1 kHz interface; offline
resampling proof does not qualify every player or analog signal chain.

Calibration backup/restore (ST-Link; SAME UNIT ONLY):
Before changes, save the complete 128 KiB settings sector, 0x081E0000-0x081FFFFF.
With host OpenOCD and the ST-Link connected to this unit:
  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"
Verify that beads-settings.bin is 131072 bytes and record its SHA256. Keep the
backup labelled with the unit's identity. Restore ONLY to that same unit:
  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000; verify_image beads-settings.bin 0x081E0000; reset run; shutdown"
Restoration erases/rewrites settings. Holding SEED at power-on erases and
rewrites this settings sector on this firmware. With valid settings, ordinary
boot does not write flash; corrupt/invalid settings are rewritten and the
module enters factory test even without SEED. This is a boot statement, not
a promise that background audio-buffer backup never writes flash.

This supplied WAV writes app blocks 0x08020000-0x0806FFFF, erasing three 128 KiB
sectors through 0x0807FFFF. It excludes the bootloader and settings 0x081E0000.
Evidence: bootloader.cc InitializeReception sets kStartAddress=0x08020000;
five decoded 64 KiB blocks drive ProgramPage's sector-aligned erases and
32-byte writes. EraseAllFirmwareSectors uses bank 1 sectors 1-7. The updater
has no general upper write bound: the settings claim applies to this bounded,
hash-verified file, not arbitrary oversized WAVs.
