# Drizzlet

Alternative firmware for Mutable Instruments Beads, built on the released Mutable Instruments source; not a Mutable Instruments product.

**TL;DR:** Beta 1 "Mist", a call for testers. New since alpha 2: buffer trim, pitch scales, Clouds-style reverb, stereo width and delay mode, on a three-page options page that saves by itself. New settings default to stock behaviour. The grain engine needs about 6,900 fewer cycles per block in the heaviest stock case, with the same sound.

Tested on one module so far. Please report problems, and what works, in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues); do not contact Mutable Instruments. [TESTING.md](docs/TESTING.md) shows how it was tested.

From the [Mutable Instruments eurorack README](https://github.com/pichenettes/eurorack/blob/master/README.md):

> Code provided as is, builds on a Desktop platform but not verified to build and run on a module. Install at your own risks.

Install at your own risk.

## Quick start

Buttons use the letters from the Beads manual: [A] is the quality button, and [M] is the CV-assign button below the FEEDBACK [J] knob (CV input 7).

Hold the CV-assign button [M] for three seconds, then release, to open the options page. Tap [M] to change page; move a mix knob and rest it for one second to save. Guides: [text](docs/OPTIONS.md), [printable](docs/options-guide.svg).

![Options page phone guide](docs/options-guide-mobile.svg)

## Downloads

From [GitHub Releases](https://github.com/insolentconstruct/eurorack/releases):

- `<tag>-wav.zip`: audio update, with `README.txt`, `NOTICE.txt` and the options guides.
- `<tag>-combo.zip`: bootloader + app for ST-Link recovery.

Module manual: [Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/) and [development history](https://pichenettes.github.io/mutable-instruments-documentation/trivia_and_history/beads_history/). Changes, including against older firmware: [CHANGELOG.md](CHANGELOG.md). Where the manual and the firmware disagree: [MANUAL_ERRATA.md](docs/MANUAL_ERRATA.md).

Developers: [STYLE.md](docs/STYLE.md), [OPTIMIZATIONS.md](docs/OPTIMIZATIONS.md), [PORTING.md](docs/PORTING.md), and [FEATURES.md](docs/FEATURES.md) (which buttons, settings bytes, flash and RAM are taken). Starting your own firmware? The [`beads-tuneup`](https://github.com/insolentconstruct/eurorack/tree/beads-tuneup) branch is the released source plus bug fixes, the GCC 9 build kit and the CPU optimizations, without Drizzlet's features; see its [README-BASE.md](https://github.com/insolentconstruct/eurorack/blob/beads-tuneup/README-BASE.md).

## Delay mode

This is the manual's delay or beat slicer, which turns on at SIZE fully clockwise.
On page 2 (FEEDBACK LED), DRY/WET selects Auto (Stock) / Never /
Wide/smooth filter in thirds. Auto (Stock) keeps the usual switch to delay at SIZE fully
clockwise. Never keeps granular playback there. Wide/smooth filter keeps the switch
and makes the delay filter neutral in the middle of its range, with no
resonant boost at either end. The filter is controlled by the SIZE
attenurandomizer and SIZE CV. The quality LED [A] shows steady green / yellow / red.
The setting is saved and defaults to Auto (Stock). It does not affect wavetable synth mode.

## Stereo width

On page 1 (REVERB LED), DRY/WET sets the stereo width of the grains.
At the bottom of the knob it is Stock. Turning clockwise spreads the grains
wider, up to a full random spread fully clockwise. At the lowest width the
input stereo position passes through and mono stays centred. Width randomizes
every grain, even at DENSITY fully clockwise. Each grain's pan is fixed when it
starts, so changes affect new grains only, including synth.
The [A] LED is steady green for Stock; width blinks slowly, green through
yellow to red for the amount.
When width is on, short [M] presses cycle the three stock destinations, then
WIDTH; all three mix LEDs pulse together for WIDTH. Custom mix assignment
still works. The assignable CV input (7) adds to the width. Stock keeps the
stock cycle and pan rounding.

## Monophonic grains

Hold [A] first, then press SEED to toggle monophonic grains. The setting
is saved and defaults off; the [A] LED is dimmed when on. This is Mutable
Instruments' original code and gesture: each new grain fades the previous
grains out. It suits the square and decay SHAPE settings. Slower shapes can go
nearly silent. To recover sound, switch to square or decay, reduce the trigger
rate, shorten SIZE, or toggle monophonic grains off. Turn them off before
downgrading to stock firmware.

## Buffer trim

Hold [A] first, then press FREEZE to start a new recording window from
zero length and clear the FREEZE button latch. Release both buttons, record
the length you want, then freeze. A FREEZE gate keeps the window empty until
it goes low. Grains already playing finish normally.
Frozen-buffer saves keep the trimmed length; stock firmware loads them at full
length. The gesture comes from Mutable Instruments' disabled buffer-trim code.

## Frozen-buffer autosave

A frozen buffer is saved after FREEZE has been on for ten seconds. A save cut
short (by releasing FREEZE, for example) doubles the next wait, up to a cap,
until a save completes or FREEZE has been off for a minute; power-up resets the
wait. If FREEZE is released during wavetable synth, the minute starts when
wavetable synth ends.

## Options page

Visual and text guides: OPTIONS.md, options-guide.svg (printable) and
options-guide-mobile.svg (phone). They sit beside this README in the release
zips, and in docs/ in the source download.

Hold the CV-assign button [M] alone for three seconds with the mix knobs still; then release.
Tap [M] to change page: page 1 (REVERB LED), page 2 (FEEDBACK LED), page 3
(DRY/WET LED). The page LED pulses slowly. Reopening returns to the last page
until power-off; power-up starts on page 1. [A] shows the first available
setting on each page, then the last knob you took over. A knob must move a
little before it takes over. Knobs marked Not used yet do nothing.

Page / pulsing LED  FEEDBACK      DRY/WET       REVERB
1 / REVERB          Pitch scale   Stereo width  Reverb character
2 / FEEDBACK        Not used yet  Delay mode    Not used yet
3 / DRY/WET         Wavetable synth     Limiter       Not used yet

Changes save by themselves about a second after you stop turning; wait that
long before powering off. Close by holding [M] for three seconds or pressing
another button. Mix controls then catch up smoothly. On page 1 with Scale
selected, short SEED taps step the scale instead of closing.
Every first value, including Pitch Off, is steady green. Other values follow
yellow, red, then green/yellow/red slow and fast blinking. Raag Shri uses two yellow pulses then a pause.
The page LED flashes on entry, close and save. See [OPTIONS.md](docs/OPTIONS.md) for every value.

Output limiter: Stock is steady green; Fast release is steady yellow;
Off is steady red. With Off, loud peaks can hard-clip.

Wavetable synth: page 3 (DRY/WET LED), FEEDBACK knob. Lower half selects
Automatic (Stock) (steady green); upper half selects Manual (steady yellow).
Automatic (Stock) starts the granular wavetable synth after 10 s with both
audio inputs unpatched, as the Beads manual describes. Manual never starts it
by itself. To start it by hand, unpatch both audio inputs, turn FREEZE off and
hold [A] for about a second and a half; hold again to stop. Patching either
input also stops it (once FREEZE is off); with an input patched, the hold
restarts automatic gain as usual. The setting is saved. Stock firmware ignores
it; saving after a downgrade resets it to Automatic (Stock).

Reverb character: page 1 (REVERB LED), REVERB knob. Lower half selects
Stock (default, steady green); upper half selects Clouds-style (steady yellow).
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

Each grain's PITCH knob, CV and random offset snap together at its start,
on a grid rooted at the unshifted pitch; +1 V is still one octave.
Playing grains keep their pitch. Synth grains snap too; the continuous
synth oscillator and delay mode keep their existing pitch behaviour.
Scales use Mutable Instruments Marbles' tunings and notes with weight at
least 64 (Pelog has four notes). Major and Minor pentatonic are subsets
of its Major and Minor scales. Pelog and the raags are microtonal.

## Updating your module

Follow the [official Beads firmware update procedure](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#firmware); the full steps are in the zip's `README.txt`.

**Do not hold SEED at power-on unless you mean to: it resets the settings (a valid calibration is kept).**

### Calibration backup and restore

Back up the settings/calibration sector with ST-Link + OpenOCD (same unit only):

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; dump_image beads-settings.bin 0x081E0000 0x20000; reset run; shutdown"
```

Restore the backup to the same unit only:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
    -c "init; halt; flash write_image erase beads-settings.bin 0x081E0000; verify_image beads-settings.bin 0x081E0000; reset run; shutdown"
```

### ST-Link recovery

Back up calibration first, then flash the combo `.bin` from `<tag>-combo.zip`; this command doesn't touch the settings sector. Use your downloaded filename in place of `drizzlet-YYYY.MM.DD-combo.bin`:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; halt; flash write_image erase drizzlet-YYYY.MM.DD-combo.bin 0x08000000; verify_image drizzlet-YYYY.MM.DD-combo.bin 0x08000000; reset run; shutdown"
```

## Saved data

The settings and frozen-buffer formats are frozen for Beta 1. Zero and unknown values select the stock behaviour. Stock firmware ignores the Drizzlet bytes and clears them when it saves, and loads trimmed buffers at full length. Turn monophonic grains off before downgrading. Byte layout: [FEATURES.md](docs/FEATURES.md).

## Building it yourself

On an amd64 host with Docker, run from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
docker build --platform linux/amd64 --target compiler -t beads-build-kit beads/build
bash beads/build/build.sh "$PWD/artifacts" --wav
```

The app is built with GCC 9.3.1; the bootloader keeps GCC 4.9.3 and its previous bytes. Builds verify the firmware and WAV hashes. CI also builds the unchanged released app with GCC 4.9.3 as a golden reference (locally: `bash beads/build/golden.sh "$PWD/golden"`, needs a full Git checkout). Releases publish from `drizzlet-*` tags and include the tag's CHANGELOG.md section.

## License and credits

Code: MIT, from the Mutable Instruments source, with credits to Frédéric Meslin / Fred's Lab and Oliver Rockstedt; see [LICENSE](LICENSE).
The manual excerpt and hardware designs are licensed CC-BY-SA 3.0.
Copyright and permission notices are in `NOTICE.txt`; preserve them when redistributing.
