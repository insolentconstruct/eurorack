Drizzlet
========
Alternative firmware for Mutable Instruments Beads; not a Mutable
Instruments product.

BETA 1 PREPARATION: module checks and outside testing are still pending.
Please report any problems you find. TESTING.md in the repository
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
fully clockwise; builds with this change did not. New since alpha 2: buffer trim, pitch scales and a Clouds-style reverb
setting. Pitch scale defaults to Off and reverb to Stock. Details are in
CHANGELOG.md in the repository.

Beads manual and development history:
https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/
https://pichenettes.github.io/mutable-instruments-documentation/trivia_and_history/beads_history/

Buffer trim
-----------
Hold MEDIA first, then press FREEZE to start a new recording window from
zero length and clear the FREEZE button latch. Release both buttons, record
the desired length, then freeze. A FREEZE gate keeps the window empty until
the gate goes low. Grains already playing finish normally.
Frozen-buffer saves keep the trimmed length. Stock firmware loads these
saves at full length. The gesture comes from Mutable Instruments' disabled
buffer-trim code.

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

Category (page)   Knob       Choices
Feedback (0)      FEEDBACK   Self-play: Automatic / On request (halves)
Dry/wet (1)       DRY/WET    Limiter: Stock / Fast release / Off (thirds)
Dry/wet (1)       REVERB     Reverb: Stock / Clouds-style (halves)
Reverb (2)        FEEDBACK   Pitch scale: Off / Chromatic / Scale (thirds);
                            tap SEED on Scale to cycle

Output limiter: Stock is steady green; Fast release is one blue pulse;
Off is two amber pulses. With Off, loud peaks can hard-clip.
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

Pitch quantization: Reverb LED category, FEEDBACK knob. Three equal zones
select Off (white), Chromatic (green), or Scale. In Scale,
tap SEED briefly to cycle Fifths and octaves (red), Major (orange), Minor
(yellow), Major pentatonic (cyan), Minor pentatonic (lime), Pelog (azure),
Raag Bhairav (violet), and Raag Shri (pink). Holds do not cycle. SEED taps here do not trigger or latch
grains; other buttons cancel as usual. The setting is saved; default is Off.

Each grain's PITCH knob, CV and random offset snap together at its start.
The grid has a fixed root at unity (0 semitones); +1 V remains one octave.
Playing grains keep their pitch. Synth grains snap too; the continuous
synth oscillator and delay mode keep their existing pitch behaviour.
Scales use Mutable Instruments Marbles' tunings and notes with weight at
least 64, including four-note Pelog. Major and Minor pentatonic are subsets
of its Major and Minor scales.
Pelog and the raags are microtonal.

The remaining knobs are reserved for later options.

Hold ASSIGN for three seconds to save and exit. After five seconds without
changes the page also exits and saves any change. Another button cancels, except SEED taps while selecting a scale.
While you edit, the mix settings keep their values; after exit they catch up
smoothly with the knobs.

Saved data
----------
The settings and frozen-buffer format below are frozen for Beta 1. Off,
Automatic and Stock keep the stock behaviour; zero and unknown option values
select those defaults.

Byte 12/13 values are frozen at Beta 1. Future options use a free byte
14–22, with 0 meaning the Beta 1 behaviour so existing saves stay valid.

Settings offsets are bytes in the 24-byte State payload:

| Byte | Setting | Saved values | Stock firmware |
|---|---|---|---|
| 10 | Limiter | 0 Stock, 0xA1 Fast release, 0xA2 Off; unknown -> Stock | Ignores; clears to 0 on save |
| 11 | Self-play | 0 Automatic, 0xA1 On request; unknown -> Automatic | Ignores; clears to 0 on save |
| 12 | Pitch scale | 0 Off, 0xA1 Chromatic, 0xA2 Fifths and octaves, 0xA3 Major, 0xA4 Minor, 0xA5 Major pentatonic, 0xA6 Minor pentatonic, 0xA7 Pelog, 0xA8 Raag Bhairav, 0xA9 Raag Shri; unknown -> Off | Ignores; clears to 0 on save |
| 13 | Reverb | 0 Stock, 0xA2 Clouds-style; all other values, including reserved 0xA1, -> Stock | Ignores; clears to 0 on save |

Monophonic grains use the original byte 9 (0 off, 1 on). Stock retains this
byte but compiles out the gesture, so turn mono off before downgrading.

The frozen-buffer header is 32 bytes, little-endian. Bytes 20–23 hold an
unsigned 32-bit count of frames removed from the full buffer, not the remaining
length: 0 means full length; the full frame count means empty. The count must
not exceed the full length for the saved quality and mono/stereo mode. It is
covered by the audio/header CRC and read only with the CRC1 tag in bytes
16–19. Alpha 1 and 2 wrote zero here; untagged older saves load at full length.
Stock reads only bytes 0–9 and loads a trimmed save at full length. Stock
rewrites do not preserve the trimmed length. Bytes 24–31 remain reserved and
are written as zero. The audio payload and header size are unchanged.

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
