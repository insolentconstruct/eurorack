Drizzlet
========
Alternative firmware for Mutable Instruments Beads; not a Mutable
Instruments product.

TL;DR: Beta 2 "Fog". New since Beta 1: Plaits as a second synth, a
spectral processor, grain diffusion, attacks, evolving frozen feedback and
random intervals, on a four-page options page. A module with damaged
settings flash now starts instead of crashing. Drizzlet Hygrostat
(https://insolentconstruct.github.io/eurorack/) builds your own update file,
with a profile choosing features, controls and defaults, and nine more
options that are off in this standard firmware. Every option
here defaults to stock behaviour. Details are in CHANGELOG.md in the
repository.

Tested on one module so far. Please report problems at
https://github.com/insolentconstruct/eurorack/issues (not to Mutable
Instruments). TESTING.md in the repository shows how it was tested.

Note from the Mutable Instruments eurorack README:
"Code provided as is, builds on a Desktop platform but not verified to
build and run on a module. Install at your own risks."
Install at your own risk.

Based on the 2024 recovery firmware (beads_patch_firmware.wav) plus one change
to grain stealing, as in the vanilla source. On our module, the 2021 and 2024
firmware clicked with DENSITY and PITCH near fully clockwise and this one did
not; see CHANGELOG.md and TESTING.md.

Buttons use the letters from the Beads manual: [A] is the quality button,
and [M] is the CV-assign button below the FEEDBACK [J] knob (CV input 7).

Beads manual and development history:
https://pichenettes.github.io/mutable-instruments-
documentation/modules/beads/manual/
https://pichenettes.github.io/mutable-instruments-
documentation/trivia_and_history/beads_history/

Buffer trim
-----------
Hold the quality button [A] first, then press FREEZE to start a new recording window from
zero length and clear the FREEZE button latch. Release both buttons, record
the length you want, then freeze. A FREEZE gate keeps the window empty until
it goes low. Grains already playing finish normally.
Frozen-buffer saves keep the trimmed length; stock firmware loads them at full
length. The gesture comes from Mutable Instruments' disabled buffer-trim code.

When the buffer is cleared
--------------------------
FREEZE never clears the recording buffer: releasing it resumes recording
where it stopped, with a short crossfade. As on stock firmware, the buffer
starts empty after changing the recording quality, switching between mono and
stereo input (for example, patching or unpatching IN R while IN L is
patched), leaving the wavetable synth, or
powering up without a saved frozen buffer. Buffer trim restarts the recording
window without erasing the old audio; grains already playing finish on it,
and new grains read only the new window.

Monophonic grains
-----------------
Hold [A] first, then press SEED to toggle monophonic grains. The setting
is saved and defaults off; the [A] LED is dimmed when on. This is Mutable
Instruments' original code and gesture: each new grain fades the previous
grains out. It suits the square and decay SHAPE settings. Slower shapes can go
nearly silent. To recover sound, switch to square or decay, reduce the trigger
rate, shorten SIZE, or toggle monophonic grains off. Turn them off before
downgrading to stock firmware.

Stereo width
------------
Hold [A] and turn DRY/WET to set the stereo width of the grains; releasing
[A] saves it. At the bottom of the knob it is Stock; turn it up a little to
switch width on. Turning clockwise spreads the grains wider, up to a full
random spread fully clockwise. At the lowest width the input stereo position
passes through and mono stays centred. Width randomizes every grain, even at
DENSITY fully clockwise. Each grain's pan is fixed when it starts, so changes
affect new grains only, including synth.
While you turn, the [A] LED shows the width as a meter, from green through
yellow to red. A tap of [A] without turning DRY/WET still changes the
recording quality.
When width is on, short [M] presses cycle the three stock destinations, then
WIDTH; all three mix LEDs pulse together for WIDTH. Custom mix assignment
still works. The assignable CV input (7) adds to the width. Stock keeps the
stock cycle and pan rounding.

Quieter switching
-----------------
Changes that keep the sample rate are pop-free: mono/stereo switching, and synth start/stop at 48 kHz. Quality presses that change the rate to or from 32 or 24 kHz fade the output but still restart the converter. The brief silence and empty recording buffer remain; the synth’s reverb tail stops with it.

Fixes
-----
Settings and frozen-buffer saves are skipped while the supply sags. A save cut short is handled like a power cut; saving can resume once the supply recovers (#175).

The brown-out reset level option (#165) is not in Beta 2; the reset level
stays as in the Mutable Instruments firmware.

Latching SEED in stock delay mode can pop; this is stock behaviour (#183).

SEED starts latched at power-up, as in stock firmware.

Frozen-buffer autosave
----------------------
A frozen buffer is saved after FREEZE has been on for ten seconds. A save cut
short (by releasing FREEZE, for example) doubles the next wait, up to a cap,
until a save completes or FREEZE has been off for a minute; power-up resets the
wait. If FREEZE is released during wavetable synth, the minute starts when
wavetable synth ends.

Options page
------------
Visual and text guides: OPTIONS.md, options-guide.svg (printable) and
options-guide-mobile.svg (phone). They sit beside this README in the release
zips, and in docs/ in the source download.

Hold the CV-assign button [M] alone for three seconds with the mix knobs still; then release.
Tap [M] to change page. The pages light the mix LEDs left to right: page 1
(FEEDBACK LED), page 2 (DRY/WET LED), page 3 (REVERB LED), page 4 (all three
LEDs). The page LED pulses slowly. Reopening returns to the last page until
power-off; power-up starts on page 1.
When a page opens, [A] shows each stored value in turn, left to right, for
about two seconds each, with that knob's LED lit steady. Turning a knob, or
tapping SEED to step the scale, ends this. [A] then shows the first setting
on the page, then the last knob you took over. A knob must move a little
before it takes over. Knobs marked Not used yet do nothing.

Page / pulsing LED  FEEDBACK      DRY/WET           REVERB
1 / FEEDBACK        Pitch scale   Random intervals  Attacks
2 / DRY/WET         Delay mode    Reverb character  Frozen feedback
3 / REVERB          Processor     Synth             Synth start
4 / all three       Not used yet  Not used yet      Limiter

Changes save by themselves about a second after you stop turning; wait that
long before powering off. Close by holding [M] for three seconds or pressing
another button. Mix controls then catch up smoothly. On page 1 with Scale
selected, short SEED taps step the scale instead of closing.
Every first value, including Pitch Off, is steady green. Other values follow
yellow, red, then green/yellow/red slow and fast blinking. Raag Shri uses two yellow pulses then a pause.
The page LED flashes on entry, close and save. See below for every value.

Delay mode: Auto (Stock) keeps the usual switch to delay at SIZE fully clockwise.
Disabled keeps granular playback there. Wide/smooth filter keeps the switch and makes the
delay filter (SIZE attenurandomizer plus SIZE CV) neutral in the middle, with no
resonant boost. The [A] LED shows steady green / yellow / red for Auto (Stock) /
Disabled / Wide/smooth filter. It does not affect wavetable synth mode.

Output limiter: page 4 (all three LEDs), REVERB knob, in thirds. Stock is
steady green; Fast release is steady yellow; Off is steady red. With Off,
loud peaks can hard-clip.

Hygrostat profile options

Hygrostat can add the following options to a profile. They are off in the
standard firmware. Page settings appear at the knob chosen in Hygrostat;
turning and saving them works like the other options.

Version at boot gives a short light-blue Drizzlet flash followed by two
light-blue pulses for Beta 2, without delaying audio startup.
Delay indication flashes it red briefly when entering or leaving delay mode.
Pitch notch indication lights it green while the PITCH knob is on the unison
or an octave plateau, including the +/-2-octave knob ends, with a little
hysteresis at the edges. CV, random pitch and quantization do not affect it. These cues yield to options, warnings and manual meters.
Live assignment LEDs scale the stock patterns by the magnitude of assignable
CV, retaining distinct width/diffusion patterns and stock amount weighting. FREEZE during saves keeps the FREEZE LED at full
intensity while a frozen buffer is being saved.

Input attenuation extends [A]+FEEDBACK down to silence. Around noon the gain
snaps to unity; below it, the input is attenuated, and above it the input is
amplified. Automatic gain still works as usual and never attenuates. Hold [A]
to restart automatic gain. A saved manual gain uses the extended range as soon
as this feature is enabled. Stock never attenuates below unity: Mutable
Instruments kept unity as the minimum deliberately ("it's a sensitivity
setting"). The 120-136 unity band and green meter cue keep unity easy to find.

LED brightness offers 100% / 66% / 33%, Fixed via Hygrostat or Adjustable
in thirds of a selected options-page knob, with a live preview while turning.
Warnings dim too. Bright digital RGB white is balanced against the white LEDs
at the reduced levels; 100% is the release's own LEDs. The default is 100%. This is a profile option; there is no power-on gesture.
SEED latch offers four values in quarters of the selected knob:
Off (stock divider with a patched cable), Ignore (free-running, cable ignored),
Sync (free-running; SEED edges restart grain-clock phase, useful on the
periodic side of DENSITY), Add (free-running; one extra grain per edge).
The divider is unavailable except in Off. These choices apply only while
latched with a cable patched; unpatched attack triggers keep working. Ignore
also suppresses trigger LED flashes. Synth grain clocks share these modes;
delay free-runs for all non-Off values. Hygrostat can fix any value or place
it in any options page slot.

TIME modulation offers Random in the lower half and Scan in the upper half.
With Scan and TIME CV unpatched, the TIME attenurandomizer moves the playback
position: noon stops, clockwise moves forwards, counter-clockwise backwards.
At either end it takes four seconds to scan the buffer. TIME offsets the
position, which wraps at the buffer boundary. Try it on a frozen recording.
Patching TIME CV restores its usual response and pauses the scan. Scan also
pauses in delay, spectral and synth modes. The position is not saved.

Processor: page 3 (REVERB LED), FEEDBACK knob. Lower half selects Granular
(Stock) (steady green); upper half selects Spectral (steady yellow).
Spectral replaces the grains with the spectral processing of Mutable
Instruments' Clouds. TIME picks the stored spectrum, SIZE warps it, SHAPE
quantizes it and PITCH shifts it (following the pitch scale). DENSITY sets how
quickly the spectrum is renewed, and scrambles its phases towards either end.
Holding SEED glitches it; FREEZE holds it. The attenurandomizers draw a new
value for each spectrum. FEEDBACK, DRY/WET, REVERB, the recording quality,
the assignable CV and the R gate output work as usual; there is no grain
diffusion, delay mode or wavetable synth.
A new processor takes over when the page closes: the sound fades out, the
recording is cleared, and the sound fades back in. A frozen buffer saved to
flash stays there and comes back with Granular. The setting is saved. Stock
firmware ignores it; saving after a downgrade resets it to Granular (Stock).

Synth: page 3 (REVERB LED), DRY/WET knob. Lower half selects
Beads (Stock) (steady green), the wavetable synth; upper half selects Plaits
(steady yellow): six voices of Mutable Instruments' Plaits play instead. Each
SEED trigger starts the next voice, which keeps its pitch and settings.
FEEDBACK picks one of the 24 models; TIME, SIZE and SHAPE set HARMONICS,
TIMBRE and MORPH, each note drawing its own random values; PITCH, DENSITY,
SEED, REVERB, quality, stereo width and the pitch scale work as in the
wavetable synth. DRY/WET sets the decay and FREEZE sustains. With DENSITY at
noon and SEED unpatched, the last voice drones until SEED plays a note. The
setting is saved; stock firmware ignores it.

Synth start: page 3 (REVERB LED), REVERB knob. Lower half selects Automatic
(Stock) (steady green): the synth starts after 10 s with both audio inputs
unpatched, as the Beads manual describes. Upper half selects Manual (steady
yellow): it never starts by itself. To start it by hand, unpatch both audio
inputs, turn FREEZE off and hold [A] for about a second and a half; hold
again to stop. Patching either input also stops it (once FREEZE is off);
with an input patched, the hold restarts automatic gain as usual. The
setting is saved; stock firmware ignores it.

Reverb character: page 2 (DRY/WET LED), DRY/WET knob. Lower half selects
Stock (default, steady green); upper half selects Clouds-style (steady yellow).
Clouds-style uses Mutable Instruments' Clouds reverb timings and modulation,
plus diffusion on the grain wet output until an amount is set with [A]+REVERB
(below). In delay and wavetable synth modes, only the reverb character changes. Switching briefly fades the sound out and
back in and clears the old tail. The setting is saved; stock firmware ignores
it and resets it to Stock when saving after a downgrade.

Random intervals: page 1 (FEEDBACK LED), DRY/WET knob. Lower half selects
Free (default, steady green); upper half selects Fifths and octaves (steady
yellow). With the PITCH attenurandomizer counter-clockwise, each grain's
random offset then lands on fifths and octaves around the knob's pitch;
clockwise it stays continuous. The pitch scale still applies. Grains and
synth grains only. The setting is saved; stock firmware ignores it.

Attacks: page 1 (FEEDBACK LED), REVERB knob, in thirds. Off (default,
steady green), Snap (steady yellow): while recording, Beads marks the attacks
in the input, and a grain that would start just after one starts on it
instead, so grains begin on a hit rather than partway through its decay.
Snap and trigger (steady red) also starts a grain on each attack, as a SEED
pulse would. Only granular playback uses the marks; a patched SEED still
works. The setting is saved; stock firmware ignores it.

Frozen feedback: page 2 (DRY/WET LED), REVERB knob. Lower half selects
Stock (default, steady green); upper half selects Evolving (steady yellow).
With Evolving, while FREEZE is on, the grains run through a slowly moving
all-pass loop, so the frozen sound drifts and its attacks soften instead of
repeating exactly; FEEDBACK sets how far it goes. It fades in and out
quickly and does nothing while FREEZE is off. The setting is saved; stock
firmware ignores it.

Grain diffusion: hold [A] and turn REVERB to set the amount, with either
reverb; the [A] LED shows it as a meter, and fully CCW is none. Until it is
set, Stock has none and Clouds-style keeps its own amount. Once an amount is
set, short presses of [M] add DIFFUSION after REVERB (and after WIDTH); the
REVERB LED pulses alone while it is the CV destination. Diffusion acts on the
grains only, not in delay or wavetable synth modes. The amount is saved; stock
firmware ignores it.

Pitch quantization: page 1 (FEEDBACK LED), FEEDBACK knob. Three zones
select Off (steady green), Chromatic (steady yellow), or Scale. In Scale,
tap SEED briefly to cycle Fifths and octaves (steady red), Major (slow green),
Minor (slow yellow), Major pentatonic (slow red), Minor pentatonic (fast green),
Pelog (fast yellow), Raag Bhairav (fast red), and Raag Shri (two yellow pulses
then a pause). Holds do not cycle. SEED taps here do not trigger or latch
grains; other buttons close the page. The setting is saved; default is Off.

Each grain's PITCH knob, CV and random offset snap together at its start,
on a grid rooted at the unshifted pitch; +1 V is still one octave.
Playing grains keep their pitch. Synth grains snap too; the continuous
synth oscillator and delay mode keep their existing pitch behaviour.
Scales use Mutable Instruments Marbles' tunings and notes with weight at
least 64 (Pelog has four notes). Major and Minor pentatonic are subsets
of its Major and Minor scales. Pelog and the raags are microtonal.

Options reference (TSV)
page	LED	knob	setting	value	colour	pulse
1	FEEDBACK	FEEDBACK	Pitch scale	Off	green	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Chromatic	yellow	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Fifths and octaves	red	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Major	green	slow (0.98 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Minor	yellow	slow (0.98 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Major pentatonic	red	slow (0.98 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Minor pentatonic	green	fast (3.91 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Pelog	yellow	fast (3.91 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Raag Bhairav	red	fast (3.91 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Raag Shri	yellow	2 yellow pulses / 2.048 s
1	FEEDBACK	DRY/WET	Random intervals	Free (Stock)	green	steady
1	FEEDBACK	DRY/WET	Random intervals	Fifths and octaves	yellow	steady
1	FEEDBACK	REVERB	Attacks	Off	green	steady
1	FEEDBACK	REVERB	Attacks	Snap	yellow	steady
1	FEEDBACK	REVERB	Attacks	Snap and trigger	red	steady
2	DRY/WET	FEEDBACK	Delay mode	Auto (Stock)	green	steady
2	DRY/WET	FEEDBACK	Delay mode	Disabled	yellow	steady
2	DRY/WET	FEEDBACK	Delay mode	Wide/smooth filter	red	steady
2	DRY/WET	DRY/WET	Reverb character	Stock	green	steady
2	DRY/WET	DRY/WET	Reverb character	Clouds-style	yellow	steady
2	DRY/WET	REVERB	Frozen feedback	Stock	green	steady
2	DRY/WET	REVERB	Frozen feedback	Evolving	yellow	steady
3	REVERB	FEEDBACK	Processor	Granular (Stock)	green	steady
3	REVERB	FEEDBACK	Processor	Spectral	yellow	steady
3	REVERB	DRY/WET	Synth	Beads (Stock)	green	steady
3	REVERB	DRY/WET	Synth	Plaits	yellow	steady
3	REVERB	REVERB	Synth start	Automatic (Stock)	green	steady
3	REVERB	REVERB	Synth start	Manual	yellow	steady
4	all three	FEEDBACK	Not used yet	Not used yet	unchanged	steady
4	all three	DRY/WET	Not used yet	Not used yet	unchanged	steady
4	all three	REVERB	Limiter	Stock	green	steady
4	all three	REVERB	Limiter	Fast release	yellow	steady
4	all three	REVERB	Limiter	Off	red	steady
End options reference

Saved data
----------
Settings from stock firmware carry over. Drizzlet's own options, stereo
width and grain diffusion saved by earlier Drizzlet releases start from their
defaults; the calibration and the stock settings are kept. Downgrading to
stock firmware is safe: it ignores the Drizzlet bytes and clears them when it
saves, and loads trimmed buffers at full length. Turn monophonic grains off
before downgrading. Earlier Drizzlet releases read the new settings as stock.
Byte layout: FEATURES.md in the repository.

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
* Stock settings and calibration carry over. Earlier Drizzlet options,
  stereo width and grain diffusion reset to the profile defaults.
* Do NOT hold SEED at power-on: it rewrites the settings/calibration
  sector, keeping a valid calibration.
* If all LEDs blink fast for 3 seconds at power-up, the calibration and its
  copy are both lost: the module plays on defaults and saves nothing. Power
  on holding SEED to repair it. Then: green quality LED with the three mix
  LEDs lit = repaired, recalibrate; red with the mix LEDs blinking = repair
  failed, try again.
* The audio update writes only the application area. It leaves the
  bootloader and settings/calibration sector untouched.
* If updates keep failing, set the computer's output to 44.1 kHz or try
  another device. As a last resort, flash the combo image from -combo.zip
  via ST-Link: the -combo.hex needs no address; the -combo.bin goes at
  0x08000000. See the build kit README for the command.
  Erase only the sectors written: a full-chip or mass erase also wipes
  calibration and settings.

Calibration backup and restore (ST-Link + OpenOCD; same unit only)
----------------------------------------------------------------
Back up the 128 KiB settings/calibration sector:

  openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run;
shutdown"

Check that beads-settings.bin is 131072 bytes. Label it with its unit.
Restore to the same unit only; calibration is specific to each module. To
restore, build a restore image from the backup with
beads/build/tools/settings_sector.py and write that, as the build kit README
shows ("Backup and restore"); don't write the 128 KiB backup itself back.

Credits: Mutable Instruments, Frederic Meslin / Fred's Lab, Oliver Rockstedt.
Copyright and permission notices are in NOTICE.txt; keep them when
redistributing.
