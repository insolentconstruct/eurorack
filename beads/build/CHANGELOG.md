# Changelog

Changes to Drizzlet, newest first. New changes go under an "Unreleased"
heading; a release turns it into a dated heading.

## Unreleased
- New options page layout on three pages. Page 1 (REVERB LED): pitch scale, stereo width, reverb character. Page 2 (FEEDBACK LED): delay mode. Page 3 (DRY/WET LED): wavetable synth (formerly self-play) and limiter. Changes save by themselves about a second after you stop turning; close by holding [M] for three seconds or pressing another button. Values show on the quality LED [A] in green, yellow and red, steady or blinking; stock values are steady green.
- Printable, phone and text guides to the options page, also in the release zip.
- Add pitch scale (page 1, FEEDBACK): Off, Chromatic, or one of eight scales from Marbles, chosen by tapping SEED. Each grain's pitch, including randomness, snaps to the scale when the grain starts (#6, #8).
- Add Clouds-style reverb (page 1, REVERB): the Clouds reverb's character, plus diffusion on the grains (#2, #22).
- Add stereo width (page 1, DRY/WET): spreads grains randomly across the stereo field, and can be a fourth [M] destination for CV (#5).
- Add delay mode (page 2, DRY/WET): Auto (Stock), Never (SIZE fully clockwise stays granular) or Wide detent (a neutral zone in the middle of the delay filter, without the resonant boost) (#9, #26).
- Add buffer trim: hold [A], then press FREEZE to start a fresh recording window; the next freeze keeps only what was recorded since. Saved buffers keep the trimmed length (#31).
- More CPU headroom: about 6,900 fewer cycles per audio block in the heaviest stock case, with the same sound.

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
