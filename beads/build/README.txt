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
fully clockwise; builds with this change did not. New since alpha 2:
buffer trim, pitch scales, Clouds-style reverb, stereo width and delay mode.
The new settings default to stock behaviour. Details are in CHANGELOG.md
in the repository.

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
Hold ASSIGN alone for three seconds with the mix knobs still; then release.
Tap ASSIGN to cycle page 1 (REVERB LED), page 2 (FEEDBACK LED), page 3
(DRY/WET LED), then page 1. The page LED pulses slowly. All three pulse
on entry. Reopening remembers the last page until power-off; power-up
starts on page 1. MEDIA shows the first available setting on each page,
then the last qualified knob moved. Move a knob more than 10% of its travel
before it edits. Reserved knobs do nothing, including to MEDIA and saving.

Page / pulsing LED  FEEDBACK       DRY/WET          REVERB
1 / REVERB          Pitch scale    Stereo width    Reverb character
2 / FEEDBACK        Reserved       Delay mode      Reserved
3 / DRY/WET         Self-play      Limiter         Reserved
Reserved slots are attacks, mix law and LED brightness respectively.

Changes are saved after one second without qualified knob movement or a
value change. A sweep through zones saves once when it settles. Leave by
holding ASSIGN for three seconds or pressing another button; there is no
idle exit or cancel. Short SEED taps on Scale cycle the pitch scale instead
of leaving. A pending change still saves after leaving: wait one second
before powering off. Mix controls resume with smooth catch-up after exit.
Stock, Auto and Automatic are steady green; Off and Never are steady white.
Pulse counts repeat every 1.2 seconds. See the reference below for every value.

On page 1 (REVERB LED), DRY/WET selects Stock below 10% (3% dead band).
Above this zone it sets a continuous saved width: zero displacement at 13%
to full random spread at fully clockwise. At zero width the input stereo
positioning passes through and mono stays centred. Enabled width randomizes
every grain, including at DENSITY fully clockwise with no other randomness.
Pan is fixed at grain start; changes affect new grains only, including synth.
The MEDIA LED is green for Stock and amber with approximate brightness showing
width; adjacent saved amounts can share an LED brightness.
When enabled, short ASSIGN presses cycle the three stock destinations then
WIDTH; all three ASSIGN LEDs pulse together for WIDTH. The existing custom
mix assignment remains available. ASSIGN CV adds to resting width and clamps
to 0..1. Stock keeps the stock cycle and pan rounding. Width uses byte 14:
0 is Stock; 1..255 encode enabled width from zero to full.
WIDTH assignment uses extension byte 16. Stock firmware sees FEEDBACK
and ignores the extension settings; saving in stock firmware clears them.
Delay mode: Auto keeps the usual delay switch at SIZE fully clockwise.
Never keeps granular playback there. Wide detent keeps the switch, with
an unfiltered zone from 40% to 60% on the delay filter (SIZE attenurandomizer
plus SIZE CV) and no resonant boost. It does not affect synth mode.
For delay mode, the quality LED shows steady green / steady white /
two amber pulses for Auto / Never / Wide detent.

Output limiter: Stock is steady green; Fast release is one blue pulse;
Off is steady white. With Off, loud peaks can hard-clip.
Self-play: page 3 (DRY/WET LED), FEEDBACK knob. Lower half selects Automatic
(stock, steady green); upper half selects On request (one magenta pulse per
1.2 seconds). Move more than one tenth of the travel to begin; the midpoint
has a small dead band to prevent flicker.
With On request, self-play never starts by itself. With both audio inputs
unpatched and FREEZE off, hold MEDIA for 1.5 seconds to start self-play;
hold again to stop. Patching either input also stops it (once FREEZE is off).
With an input patched, the hold restarts automatic gain as usual.
Automatic keeps stock behaviour.
The setting is saved. Stock firmware ignores it; saving after a downgrade
resets it to Automatic.

Reverb character: page 1 (REVERB LED), REVERB knob. Lower half selects
Stock (default, steady green); upper half selects Clouds-style (one cyan pulse
per 1.2 seconds). Move more than one tenth of the travel to begin; the midpoint
has a small dead band. The quality LED shows the last knob moved on this page.
Clouds-style uses Mutable Instruments' Clouds reverb timings and modulation,
plus fixed diffusion on the grain wet output. In delay and self-play modes,
only the reverb character changes. Switching briefly fades the sound out and
back in and clears the old tail. The setting is saved; stock firmware ignores
it and resets it to Stock when saving after a downgrade.

Pitch quantization: Reverb LED category, FEEDBACK knob. Three equal zones
select Off (white), Chromatic (one blue pulse), or Scale. In Scale,
tap SEED briefly to cycle Fifths and octaves (red), Major (amber), Minor
(yellow), Major pentatonic (cyan), Minor pentatonic (lime), Pelog (steady blue),
Raag Bhairav (violet), and Raag Shri (pink). Holds do not cycle. SEED taps
here do not trigger or latch grains; other buttons leave the page. The
setting is saved; default is Off.

Each grain's PITCH knob, CV and random offset snap together at its start.
The grid has a fixed root at unity (0 semitones); +1 V remains one octave.
Playing grains keep their pitch. Synth grains snap too; the continuous
synth oscillator and delay mode keep their existing pitch behaviour.
Scales use Mutable Instruments Marbles' tunings and notes with weight at
least 64, including four-note Pelog. Major and Minor pentatonic are subsets
of its Major and Minor scales.
Pelog and the raags are microtonal.

Options reference (TSV)
page	LED	knob	setting	value	colour	pulse
1	REVERB	FEEDBACK	Pitch scale	Off	white	steady
1	REVERB	FEEDBACK	Pitch scale	Chromatic	blue	1 / 1.2 s
1	REVERB	FEEDBACK	Pitch scale	Fifths and octaves	red	steady
1	REVERB	FEEDBACK	Pitch scale	Major	amber	steady
1	REVERB	FEEDBACK	Pitch scale	Minor	yellow	steady
1	REVERB	FEEDBACK	Pitch scale	Major pentatonic	cyan	steady
1	REVERB	FEEDBACK	Pitch scale	Minor pentatonic	lime	steady
1	REVERB	FEEDBACK	Pitch scale	Pelog	blue	steady
1	REVERB	FEEDBACK	Pitch scale	Raag Bhairav	violet	steady
1	REVERB	FEEDBACK	Pitch scale	Raag Shri	pink	steady
1	REVERB	DRY/WET	Stereo width	Stock	green	steady
1	REVERB	DRY/WET	Stereo width	Enabled (1..255)	amber	amount brightness
1	REVERB	REVERB	Reverb character	Stock	green	steady
1	REVERB	REVERB	Reverb character	Clouds-style	cyan	1 / 1.2 s
2	FEEDBACK	FEEDBACK	Reserved	Reserved	unchanged	steady
2	FEEDBACK	DRY/WET	Delay mode	Auto (Stock)	green	steady
2	FEEDBACK	DRY/WET	Delay mode	Never	white	steady
2	FEEDBACK	DRY/WET	Delay mode	Wide detent	amber	2 / 1.2 s
2	FEEDBACK	REVERB	Reserved	Reserved	unchanged	steady
3	DRY/WET	FEEDBACK	Self-play	Automatic (Stock)	green	steady
3	DRY/WET	FEEDBACK	Self-play	On request	magenta	1 / 1.2 s
3	DRY/WET	DRY/WET	Limiter	Stock	green	steady
3	DRY/WET	DRY/WET	Limiter	Fast release	blue	1 / 1.2 s
3	DRY/WET	DRY/WET	Limiter	Off	white	steady
3	DRY/WET	REVERB	Reserved	Reserved	unchanged	steady
End options reference

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
