# Changelog

Changes to Drizzlet, newest first. New changes go under an "Unreleased"
heading; a release turns it into a dated heading.

## Unreleased
- Clouds-style reverb modulation uses nominal 0.5/0.3 Hz rates in every recording quality.
- Add generated printable and phone options guides, a text guide, and release ZIP copies.
- Reorganize options across three pages: pitch/width/reverb, delay mode, then self-play/limiter. Knobs marked Not used yet are inert. Reopening remembers the last page until power-off.
- Save options after one second of settled controls. Hold ASSIGN 3 s or press another button to close; idle pages stay open. All stock values use steady green. Other values use the green/yellow/red ladder with slow or fast blinking where needed; entry, close and completed saves flash the page LED for 100 ms.
- Show enabled width with the green/yellow/red meter with a slow blink. Pitch follows the same ladder across steady, slow and fast; Raag Shri uses two yellow pulses followed by a pause.
- Add delay mode: page 2 (FEEDBACK LED), DRY/WET knob, Auto (Stock) / Never / Wide detent in thirds. Never keeps SIZE fully clockwise granular; Wide detent gives the SIZE attenurandomizer/CV filter a 40–60% neutral zone and removes resonant boost (#9, #26). Filter resonance ramps between modes to avoid a click.
- Add stereo width: continuous saved resting width above the Stock zone; enabled width randomizes every grain. Optional fourth ASSIGN destination adds CV to width, clamped to 0..1 (#5). Width ignores stationary knob noise, and ASSIGN uses pending width changes immediately after closing options.
- Increase CPU headroom for the grain engine: about 6,900 fewer cycles per audio block in the heaviest stock case (measured on a test board), from three small changes in Mutable Instruments' own style. The sound is bit-identical.
- Start the options page at power-up on page 1 (REVERB LED), where FEEDBACK sets the pitch scale.
- Add buffer trim: hold MEDIA, then press FREEZE to start a recording window from zero; the next freeze holds only what was recorded since. Saved buffers keep the trimmed length; stock firmware loads them at full length.
- Add pitch scale: Off (default), Chromatic or eight scales. Page 1 (REVERB LED), FEEDBACK knob; on Scale, tap SEED to cycle Fifths and octaves, then Major, Minor, Major pentatonic, Minor pentatonic, Pelog, Raag Bhairav and Raag Shri, tuned as in Marbles. Each grain's final pitch, including randomness, snaps when it starts. Delay and the continuous synth oscillator keep their existing pitch behaviour.
- Preserve saved options: byte 12/13 values are frozen at Beta 1. Future options use a free byte 17–22 with 0 preserving the Beta 1 behaviour, so existing saves stay valid.
- Add Clouds-style reverb: page 1 (REVERB LED), REVERB knob, lower half Stock (default), upper half Clouds-style. Clouds reverb character in every mode, plus fixed diffusion on grain wet output. Switching briefly fades and clears the old tail.
- Keep SEED scale taps through a pending save; closing options restores normal controls while the final change settles.

## 2026.10.16 (tag `drizzlet-2026.10.16`, alpha 2)
- Self-play option (options page, FEEDBACK): Automatic (Stock) or On request, started and stopped by holding MEDIA 1.5 s with no inputs and FREEZE off (#17).
- Less flash wear from a modulated FREEZE: each interrupted buffer save doubles the wait before the next, up to 320 s, reset by a completed save or a minute unfrozen (#56).
- Monophonic grains (Mutable Instruments' original code): hold MEDIA and press SEED to toggle; as Mutable Instruments noted, this works well only with the first two SHAPE envelopes (square and decay), and should be turned off before downgrading (#31).
- App built with GCC 9 instead of 4.9.3; 20 of 20 emulated scenarios give bit-identical audio.
- Delay mode: no tempo glitch on the first clock after about 12 hours without one (#57).
- Clocked frozen delay: very fast SEED edges no longer create a zero-length slice (#33).

## 2026.10.14 (tag `drizzlet-2026.10.14`, alpha 1)
- Output limiter option (Stock, Fast release, Off): hold ASSIGN for three seconds to open the options page and set it with DRY/WET.
- Frozen-buffer backups carry a CRC and are range-checked; stock and Drizzlet still load each other's backups.
- An interrupted or failed backup is never restored.
- A setting saved during a backup no longer breaks the backup.
- Frozen delay with negative FM no longer reads outside the buffer (#33).
- Fixed undefined reads in the dummy CPU-load grains and past the last wavetable (output unchanged).
- Named Drizzlet; release tags are `drizzlet-YYYY.MM.DD`.

## 2026.10.11
- First build (tag `beads-2026.10.11`): the released Mutable Instruments Beads source, unmodified.

## Earlier Mutable Instruments firmware

- **2021 forum release:** `beads.wav`, posted on the Mutable Instruments forum in April 2021.
- **2024 recovery firmware:** `beads_patch_firmware.wav` / `beads_golden_bootloader_combo.bin`, shared in the ModWiggler Beads thread in early 2024 to fix modules stuck on boot.

These are the source changes needed to reproduce the older application images exactly, not the original history. The release bootloader matches the 2024 combo bootloader byte-for-byte. Effects below come from code and emulation, not listening tests.

### 2024 recovery firmware → released source

One change: grain stealing in polyphonic mode (`beads/dsp/granular_player.h`).

New grains fade out safety grains early to keep a voice free. The older firmware steals the oldest grain at intervals <5 ms; otherwise it sorts by remaining time. This build uses the oldest-grain method at intervals <25 ms.

At intervals >5 ms and <25 ms, this build swaps two safety-grain fade-out caps. The stolen grain fades out within 5 ms instead of one grain interval.

With identical input and state, emulation shows:
- At intervals ≥25 ms, outputs are bit-identical.
- At intervals from 5 ms to <25 ms, outputs differ.
- At intervals <5 ms, both run the same code. Differences can persist briefly if earlier intervals in the 5–25 ms range changed grain history.

The “freeze gate” and “gated density” bench tests reached the changed range.

On our module, the 2021 and 2024 firmware clicked and the released source /
Drizzlet did not. A desktop run of both grain schedulers produced no clicks, so
that test found no click in the DSP itself. CPU timing is the current hypothesis:
the 2024 scheduler costs more in that range, but module CPU timing there has not
yet been measured. Settings: DENSITY 0.98, PITCH +24 semitones, SIZE 0.92 (just
below delay) and SHAPE 0.5, set by CV; DRY/WET fully clockwise, FEEDBACK and
REVERB fully counter-clockwise; Bright digital; music into both inputs. OUT L
went to a scope with OUT R unplugged, so it carried L+R. Clicks were found by a
detector on the scope captures, not by ear:

| Firmware | Clicks | Output observed |
|---|---:|---:|
| 2021 forum release | 145 | 21 s |
| 2024 recovery firmware | 164 | 20.6 s |
| Released source, same GCC 4.9.3 compiler | 0 | 14 s |
| Drizzlet 2026.10.16 | 0 | 83 s (four runs) |

Reverse, square-envelope, low-pitch and no-grain variants gave no clicks on any of the four. CPU timing is the current hypothesis; it has not been measured in this range.

In these older builds, monophonic mode is unchanged and compiled out (`ENABLE_ADVANCED_FEATURES` is commented out).

### 2021 forum release → 2024 recovery firmware

Five changes:

1. **Settings recovery:** SEED held at power-on rewrites settings while keeping valid calibration, fixing modules stuck on boot. The 2021 release lacks this path. Do not hold SEED at power-on unintentionally.
2. **Knob/CV smoothing:** stmlib's `HysteresisFilter` loses its brief tracking mode after a change. State per channel shrinks from 28 to 20 bytes.
3. **Delay density/FM smoothing:** the dead band changes from 0.005 to 0.0025. The FM follower no longer tracks the input directly for the 7 calls after a change.
4. **Frozen delay slices:** during frozen playback, manual and clocked delay add hysteresis with a 0.0025 dead band to slice/TIME selection.
5. **Clocked delay FM:** both FM inputs affect the clock divider. Exponential FM previously bent tap ratios separately. Manual delay is unchanged.

None of these five touch grain playback, and the 2024 firmware clicks like the 2021 release.

Upgrading from 2021 to this build includes all six changes. The build kit here builds Drizzlet only; patches and comparison details for the older builds are available on request.
