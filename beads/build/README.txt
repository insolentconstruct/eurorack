Drizzlet
========
Alternative firmware for Mutable Instruments Beads; not a Mutable
Instruments product.

Beta 1, a call for testers: tested on one module so far.
Please report any problems you find. TESTING.md in the repository
shows how it was tested.

Note from the Mutable Instruments eurorack README:
"Code provided as is, builds on a Desktop platform but not verified to
build and run on a module. Install at your own risks."

Install at your own risk. Please don't contact Mutable Instruments;
report problems at:
https://github.com/insolentconstruct/eurorack/issues

Based on the 2024 recovery firmware (beads_patch_firmware.wav) plus one change
to grain stealing, as in the released source. On our module, the 2021 and 2024
firmware clicked with DENSITY and PITCH near fully clockwise and the released
source / Drizzlet did not. A desktop run of both grain schedulers produced no
clicks, so that test found no click in the DSP itself. CPU timing is the current
hypothesis: the 2024 scheduler costs more in that range. Drizzlet peaks at
87.8% of the CPU budget there; the 2024 firmware was not timed on the module. New since alpha 2: buffer trim, pitch scales,
Clouds-style reverb, stereo width and delay mode. The new settings default to
stock behaviour. Details are in CHANGELOG.md in the repository.

Beads manual and development history:
https://pichenettes.github.io/mutable-instruments-
documentation/modules/beads/manual/
https://pichenettes.github.io/mutable-instruments-
documentation/trivia_and_history/beads_history/

Buffer trim
-----------
Hold [A] (MEDIA) first, then press FREEZE to start a new recording window from
zero length and clear the FREEZE button latch. Release both buttons, record
the desired length, then freeze. A FREEZE gate keeps the window empty until
the gate goes low. Grains already playing finish normally.
Frozen-buffer saves keep the trimmed length. Stock firmware loads these
saves at full length. The gesture comes from Mutable Instruments' disabled
buffer-trim code.

Monophonic grains
-----------------
Hold [A] first, then press SEED to toggle monophonic grains; the setting
is saved and defaults off. The [A] LED is dimmed when on. This enables Mutable Instruments'
original code and gesture. Each new grain fades the previous grains out.
It suits the square and decay SHAPE settings. Slower shapes can go nearly
silent because each new grain cuts the previous one's attack. To recover
sound, switch to square or decay, reduce the trigger rate, shorten SIZE,
or use the same gesture to turn monophonic grains off. Turn them off before
downgrading to stock firmware.

Frozen-buffer autosave
----------------------

Autosave normally starts after FREEZE has been on for ten seconds. A save cut
short (by powering off, for example) makes the next wait longer, until a save
completes or FREEZE has been off for a minute; power-up starts at ten seconds.
If FREEZE is released during wavetable synth, the minute starts when wavetable synth ends.

Options page
------------
Visual and text guides: OPTIONS.md, options-guide.svg (printable), and
options-guide-mobile.svg (phone). Find them beside this README in release
zips, or beside the build documentation in the source download.

Hold [M] (ASSIGN) alone for three seconds with the mix knobs still; then release.
Tap [M] to cycle page 1 (REVERB LED), page 2 (FEEDBACK LED), page 3
(DRY/WET LED), then page 1. The page LED pulses slowly. All three pulse
on entry. Reopening remembers the last page until power-off; power-up
starts on page 1. [A] shows the first available setting on each page,
then the last knob you moved. A knob must be moved a little before it
edits, so a bumped knob changes nothing. Knobs marked Not used yet do nothing, including to [A] and saving.

Page / pulsing LED  FEEDBACK      DRY/WET       REVERB
1 / REVERB          Pitch scale   Stereo width  Reverb character
2 / FEEDBACK        Not used yet  Delay mode    Not used yet
3 / DRY/WET         Wavetable synth     Limiter       Not used yet

Changes are saved by themselves about a second after you stop turning.
Close by holding [M] for three seconds or pressing another button; there is no
idle close or cancel. On page 1 with Scale selected, short SEED taps step
the scale instead of closing options. A pending change still saves after closing: wait one second
before powering off. Mix controls resume with smooth catch-up after closing.
Every first value, including Pitch Off, is steady green. Other values follow
yellow, red, then green/yellow/red slow and fast blinking. Raag Shri uses two yellow pulses then a pause.
The page LED flashes briefly on entry, close and after a changed batch is
written. [A] keeps showing the value. See below for every value.

On page 1 (REVERB LED), DRY/WET sets the stereo width of the grains.
At the bottom of the knob it is Stock. Turn clockwise from there and the
grains spread wider, up to a full random spread fully clockwise. With the
lowest width the input stereo positioning passes through and mono stays
centred. Width randomizes every grain, even at DENSITY fully clockwise with no
other randomness. Pan is fixed when a grain starts, so changes affect new
grains only, including synth.
The [A] LED is steady green for Stock. Width blinks slowly, using
Beads' green through yellow to red meter colour for the amount.
When width is on, short [M] presses cycle the three stock destinations then
WIDTH; all three mix LEDs pulse together for WIDTH. The existing custom
mix assignment remains available. The assignable CV input (7) adds to the
set width. Stock keeps the stock cycle and pan rounding.
Delay mode: Auto (Stock) keeps the usual delay switch at SIZE fully clockwise.
Never keeps granular playback there. Wide detent keeps the switch, with
an unfiltered zone in the middle of the delay filter (SIZE attenurandomizer
plus SIZE CV) and no resonant boost. It does not affect wavetable synth mode.
For delay mode, the [A] LED shows steady green / yellow / red
for Auto (Stock) / Never / Wide detent.

Output limiter: Stock is steady green; Fast release is steady yellow;
Off is steady red. With Off, loud peaks can hard-clip.
Wavetable synth: page 3 (DRY/WET LED), FEEDBACK knob. Lower half selects
Automatic (Stock) (steady green); upper half selects Manual (steady yellow).
The knob must be moved a little before it switches, so a bumped knob
changes nothing.
Automatic (Stock) starts Beads as a granular wavetable synth after 10 s with both
audio inputs unpatched, as the Beads manual describes. With Manual, it never starts by itself. With both audio inputs
unpatched and FREEZE off, hold [A] for about a second and a half to start the wavetable synth;
hold again to stop. Patching either input also stops it (once FREEZE is off).
With an input patched, the hold restarts automatic gain as usual.
Automatic (Stock) keeps stock behaviour.
The setting is saved. Stock firmware ignores it; saving after a downgrade
resets it to Automatic (Stock).

Reverb character: page 1 (REVERB LED), REVERB knob. Lower half selects
Stock (default, steady green); upper half selects Clouds-style (steady yellow).
The knob must be moved a little before it switches. The [A] LED shows the last knob moved on this page.
Clouds-style uses Mutable Instruments' Clouds reverb timings and modulation,
plus fixed diffusion on the grain wet output. In delay and wavetable synth modes,
only the reverb character changes. Switching briefly fades the sound out and
back in and clears the old tail. The setting is saved; stock firmware ignores
it and resets it to Stock when saving after a downgrade.

Pitch quantization: Reverb LED page, FEEDBACK knob. Three zones
select Off (steady green), Chromatic (steady yellow), or Scale. In Scale,
tap SEED briefly to cycle Fifths and octaves (steady red), Major (slow green),
Minor (slow yellow), Major pentatonic (slow red), Minor pentatonic (fast green),
Pelog (fast yellow), Raag Bhairav (fast red), and Raag Shri (two yellow pulses
then a pause). Holds do not cycle. SEED taps here do not trigger or latch
grains; other buttons close the page. The setting is saved; default is Off.

Each grain's PITCH knob, CV and random offset snap together at its start.
The grid has a fixed root at the unshifted pitch; +1 V remains one octave.
Playing grains keep their pitch. Synth grains snap too; the continuous
synth oscillator and delay mode keep their existing pitch behaviour.
Scales use Mutable Instruments Marbles' tunings and notes with weight at
least 64, including four-note Pelog. Major and Minor pentatonic are subsets
of its Major and Minor scales.
Pelog and the raags are microtonal.

Options reference (TSV)
page	LED	knob	setting	value	colour	pulse
1	REVERB	FEEDBACK	Pitch scale	Off	green	steady
1	REVERB	FEEDBACK	Pitch scale	Chromatic	yellow	steady
1	REVERB	FEEDBACK	Pitch scale	Fifths and octaves	red	steady
1	REVERB	FEEDBACK	Pitch scale	Major	green	slow (0.98 Hz)
1	REVERB	FEEDBACK	Pitch scale	Minor	yellow	slow (0.98 Hz)
1	REVERB	FEEDBACK	Pitch scale	Major pentatonic	red	slow (0.98 Hz)
1	REVERB	FEEDBACK	Pitch scale	Minor pentatonic	green	fast (3.91 Hz)
1	REVERB	FEEDBACK	Pitch scale	Pelog	yellow	fast (3.91 Hz)
1	REVERB	FEEDBACK	Pitch scale	Raag Bhairav	red	fast (3.91 Hz)
1	REVERB	FEEDBACK	Pitch scale	Raag Shri	yellow	2 yellow pulses / 2.048 s
1	REVERB	DRY/WET	Stereo width	Stock	green	steady
1	REVERB	DRY/WET	Stereo width	Width (wider →)	meter	slow (0.98 Hz)
1	REVERB	REVERB	Reverb character	Stock	green	steady
1	REVERB	REVERB	Reverb character	Clouds-style	yellow	steady
2	FEEDBACK	FEEDBACK	Not used yet	Not used yet	unchanged	steady
2	FEEDBACK	DRY/WET	Delay mode	Auto (Stock)	green	steady
2	FEEDBACK	DRY/WET	Delay mode	Never	yellow	steady
2	FEEDBACK	DRY/WET	Delay mode	Wide detent	red	steady
2	FEEDBACK	REVERB	Not used yet	Not used yet	unchanged	steady
3	DRY/WET	FEEDBACK	Wavetable synth	Automatic (Stock)	green	steady
3	DRY/WET	FEEDBACK	Wavetable synth	Manual	yellow	steady
3	DRY/WET	DRY/WET	Limiter	Stock	green	steady
3	DRY/WET	DRY/WET	Limiter	Fast release	yellow	steady
3	DRY/WET	DRY/WET	Limiter	Off	red	steady
3	DRY/WET	REVERB	Not used yet	Not used yet	unchanged	steady
End options reference

Saved data
----------
The settings and frozen-buffer format below are frozen for Beta 1. Off,
Automatic (Stock) and Stock keep the stock behaviour; zero and unknown option
values
select those defaults.

Byte 12/13 values are frozen at Beta 1. Future options use a free byte
17–22, with 0 meaning the Beta 1 behaviour so existing saves stay valid.

Width uses byte 14: 0 is Stock; 1..255 encode enabled width from zero to full.
WIDTH assignment uses byte 16: 0 is unassigned, 1 is assigned; other values
are unassigned. Stock sees FEEDBACK and clears these extensions on save.

Settings offsets are bytes in the 24-byte State payload:

| Byte | Setting | Saved values | Stock firmware |
|---|---|---|---|
| 10 | Limiter | 0 Stock, 0xA1 Fast release, 0xA2 Off; unknown -> Stock | Ignores; clears to 0 on save |
| 11 | Wavetable synth | 0 Automatic (Stock), 0xA1 Manual; unknown -> Automatic (Stock) | Ignores; clears to 0 on save |
| 12 | Pitch scale | 0 Off, 0xA1 Chromatic, 0xA2 Fifths and octaves, 0xA3 Major, 0xA4 Minor, 0xA5 Major pentatonic, 0xA6 Minor pentatonic, 0xA7 Pelog, 0xA8 Raag Bhairav, 0xA9 Raag Shri; unknown -> Off | Ignores; clears to 0 on save |
| 13 | Reverb | 0 Stock, 0xA2 Clouds-style; all other values, including reserved 0xA1, -> Stock | Ignores; clears to 0 on save |
| 14 | Stereo width | 0 Stock; 1..255 enabled width, zero to full spread | Ignores; clears to 0 on save |
| 15 | Delay mode | 0 Auto (Stock), 0xA1 Never, 0xA2 Wide detent; unknown -> Auto (Stock) | Ignores; clears to 0 on save |
| 16 | WIDTH assignment | 0 unassigned, 1 assigned; other values -> unassigned | Sees FEEDBACK; clears extension on save |

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
https://pichenettes.github.io/mutable-instruments-
documentation/modules/beads/manual/#firmware

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
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run;
shutdown"

Check that beads-settings.bin is 131072 bytes. Label it with its unit.
Restore to the same unit only; calibration is specific to each module:

  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000;
verify_image beads-settings.bin 0x081E0000; reset run; shutdown"

Corrupt settings make the module start in factory-test mode (breathing
LEDs); restoring the backup fixes it.

Credits: Mutable Instruments, Frederic Meslin / Fred's Lab, Oliver Rockstedt.
Copyright and permission notices are in NOTICE.txt; keep them when
redistributing.
