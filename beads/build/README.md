# Drizzlet

Alternative firmware for Mutable Instruments Beads, built on the released Mutable Instruments source; not a Mutable Instruments product.

**Beta 1 preparation:** module checks and outside testing are still pending. Please report any problems you find. [TESTING.md](TESTING.md) shows how it was tested.

From the [Mutable Instruments eurorack README](https://github.com/pichenettes/eurorack/blob/master/README.md):

> Code provided as is, builds on a Desktop platform but not verified to build and run on a module. Install at your own risks.

Install at your own risk. Please do not contact Mutable Instruments about it; report problems in [this repository's issues](https://github.com/insolentconstruct/eurorack/issues).

## Downloads

Get the files from [GitHub Releases](https://github.com/insolentconstruct/eurorack/releases).

- `<tag>-wav.zip`: audio update, with `README.txt` and `NOTICE.txt`.
- `<tag>-combo.zip`: bootloader + app for ST-Link recovery.

For the module itself, read the [Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/) and the [Beads development history](https://pichenettes.github.io/mutable-instruments-documentation/trivia_and_history/beads_history/).
What changed, including against older firmware: see [CHANGELOG.md](CHANGELOG.md).
Where the Beads manual and the firmware disagree, see [MANUAL_ERRATA.md](MANUAL_ERRATA.md).

Contributing code: follow [STYLE.md](STYLE.md). What the build changed to run faster, and how to propose more: [OPTIMIZATIONS.md](OPTIMIZATIONS.md). Building the DSP elsewhere: [PORTING.md](PORTING.md). Before adding a feature, check [FEATURES.md](FEATURES.md) for which buttons, settings bytes, flash and RAM are already taken.

**TL;DR:** buffer trim, pitch scales and a Clouds-style reverb setting are new
since alpha 2. Pitch scale defaults to Off and reverb to Stock.

## Delay mode

In the options page's Feedback category, DRY/WET selects Auto / Never /
Wide detent in thirds. Auto keeps the usual switch to delay at SIZE fully
clockwise. Never keeps granular playback there. Wide detent keeps automatic
switching and makes the delay filter neutral from 40% to 60% of its range,
with no resonant boost at either end. The filter is controlled by the SIZE
attenurandomizer and SIZE CV; its neutral sound is unfiltered. Outside the
neutral zone the remaining travel reaches the usual low-pass and high-pass
ends. The MEDIA LED shows steady green / one blue pulse / two amber pulses.
The setting is saved and defaults to Auto. It does not affect synth mode.

## Stereo width

In the options page's Feedback category, REVERB selects Stock / Narrow /
Wide in thirds. Stock is unchanged. Narrow keeps one-quarter of the stock
pan excursion around centre, preserving more of a stereo recording's image.
Wide gives every grain the full random pan range, even with no parameter
randomness and every trigger accepted. It also applies to synth grains.
The MEDIA LED shows steady green / one blue pulse / two amber pulses.
The setting is saved and defaults to Stock.

## Monophonic grains

Hold MEDIA first, then press SEED to toggle monophonic grains; the setting
is saved and defaults off. The MEDIA/quality LED is half as bright when on
(the recording reserve can dim it further). This enables Mutable Instruments'
original code and gesture. Each new grain fades the previous grains out.
It suits the square and decay SHAPE settings. Slower shapes can go nearly
silent because each new grain cuts the previous one's attack. To recover
sound, switch to square or decay, reduce the trigger rate, shorten SIZE,
or use the same gesture to turn monophonic grains off. Turn them off before
downgrading to stock firmware.

## Buffer trim

Hold MEDIA first, then press FREEZE to start a new recording window from
zero length and clear the FREEZE button latch. Release both buttons, record
the desired length, then freeze. A FREEZE gate keeps the window empty until
the gate goes low. Grains already playing finish normally.
Frozen-buffer saves keep the trimmed length. Stock firmware loads these
saves at full length. The gesture comes from Mutable Instruments' disabled
buffer-trim code.

## Frozen-buffer autosave

Autosave normally starts after FREEZE has been on for 10 seconds. A save cut
short after erasing doubles the next wait, up to 320 seconds, until a save
completes or FREEZE has been off for a minute; power-up starts at 10 seconds.
If FREEZE is released during self-play, the minute starts when self-play ends.

## Options page

Hold ASSIGN alone for three seconds, keeping the mix knobs still; then release.
The assignment LEDs pulse together on entry.
The feedback, dry/wet and reverb LEDs mark three categories; the open one's LED pulses slowly. The page always opens on Reverb (pitch scale on FEEDBACK).
Tap ASSIGN to cycle Reverb → Feedback → Dry/wet → Reverb.
On entry, the quality LED shows the saved pitch scale (Off: white).
In each category, the three mix knobs (FEEDBACK, DRY/WET, REVERB) are its three settings.
Move a knob more than one tenth of its travel to start selecting; its zones pick the value, shown on the quality LED (next to MEDIA).

| Category | Knob | Choices |
|---|---|---|
| Feedback LED (page 0) | FEEDBACK | Self-play: Automatic / On request (halves) |
| Dry/wet LED (page 1) | DRY/WET | Limiter: Stock / Fast release / Off (thirds) |
| Dry/wet LED (page 1) | REVERB | Reverb: Stock / Clouds-style (halves) |
| Reverb LED (page 2) | FEEDBACK | Pitch scale: Off / Chromatic / Scale (thirds); tap SEED on Scale to cycle |

Output limiter: Stock is steady green; Fast release is one blue pulse;
Off is two amber pulses. With Off, loud peaks can hard-clip.

Self-play: Feedback LED category, FEEDBACK knob. Lower half selects Automatic
(stock, steady cyan); upper half selects On request (one magenta pulse per
1.2 seconds). Move more than one tenth of the travel to begin; the midpoint
has a small dead band to prevent flicker.
With On request, self-play never starts by itself. With both audio inputs
unpatched and FREEZE off, hold MEDIA for 1.5 seconds to start self-play;
hold again to stop. Patching either input also stops it (once FREEZE is off).
With an input patched, the hold restarts automatic gain as usual. Automatic keeps stock behaviour.
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
Raag Bhairav (violet), and Raag Shri (pink). Holds do not cycle. SEED taps
here do not trigger or latch grains; other buttons cancel as usual. The
setting is saved; default is Off.

Each grain's PITCH knob, CV and random offset snap together at its start.
The grid has a fixed root at unity (0 semitones); +1 V remains one octave.
Playing grains keep their pitch. Synth grains snap too; the continuous
synth oscillator and delay mode keep their existing pitch behaviour.
Scales use Mutable Instruments Marbles' tunings and notes with weight at
least 64, including four-note Pelog. Major and Minor pentatonic are subsets
of its Major and Minor scales.
Pelog and the raags are microtonal.

The remaining knobs are reserved for later options.
Hold ASSIGN for three seconds to save and exit. After five seconds without changes the page also exits and saves any change. Another button cancels, except SEED taps while selecting a scale.
While you edit, the mix settings keep their values; after exit they catch up smoothly with the knobs.

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

Back up calibration first, then flash the combo `.bin` from `<tag>-combo.zip`; this command doesn't touch the settings sector.

Use your downloaded filename in place of `drizzlet-YYYY.MM.DD-combo.bin`:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "init; halt; flash write_image erase drizzlet-YYYY.MM.DD-combo.bin 0x08000000; verify_image drizzlet-YYYY.MM.DD-combo.bin 0x08000000; reset run; shutdown"
```

## Saved data

The settings and frozen-buffer format below are frozen for Beta 1. Off,
Automatic and Stock keep the stock behaviour; zero and unknown option values
select those defaults.

Byte 12/13 values are frozen at Beta 1. Future options use a free byte
14–22, with 0 meaning the Beta 1 behaviour so existing saves stay valid.

Settings offsets are bytes in the 24-byte State payload:

| Byte | Setting | Saved values | Stock firmware |
|---|---|---|---|
| 10 | Limiter | 0 Stock, 0xA1 Fast release, 0xA2 Off; unknown → Stock | Ignores; clears to 0 on save |
| 11 | Self-play | 0 Automatic, 0xA1 On request; unknown → Automatic | Ignores; clears to 0 on save |
| 12 | Pitch scale | 0 Off, 0xA1 Chromatic, 0xA2 Fifths and octaves, 0xA3 Major, 0xA4 Minor, 0xA5 Major pentatonic, 0xA6 Minor pentatonic, 0xA7 Pelog, 0xA8 Raag Bhairav, 0xA9 Raag Shri; unknown → Off | Ignores; clears to 0 on save |
| 13 | Reverb | 0 Stock, 0xA2 Clouds-style; all other values, including reserved 0xA1, → Stock | Ignores; clears to 0 on save |

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

## Building it yourself

On an amd64 host with Docker, run from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
docker build --platform linux/amd64 --target compiler -t beads-build-kit beads/build
bash beads/build/build.sh "$PWD/artifacts" --wav
```

The app is built with GCC 9.3.1; the bootloader keeps GCC 4.9.3 and its
previous bytes. Both compiler archives are pinned.

Builds verify the firmware and WAV hashes. CI also builds the unchanged
released app with GCC 4.9.3 as a fixed golden reference, and checks WAV
decoding. Releases publish from `drizzlet-*` tags; the release notes include
the tag's CHANGELOG.md section, and publishing stops if it is missing.

To build the golden reference locally (needs a full Git checkout):

```sh
bash beads/build/golden.sh "$PWD/golden"
```

## License and credits

Code: MIT, from the Mutable Instruments source, with credits to Frédéric Meslin / Fred's Lab and Oliver Rockstedt; see [LICENSE](LICENSE).
The manual excerpt and hardware designs are licensed CC-BY-SA 3.0.
Copyright and permission notices are in `NOTICE.txt`; preserve them when redistributing.
