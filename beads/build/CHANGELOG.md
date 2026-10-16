# Changelog

Changes to Drizzlet, newest first. New changes go under an "Unreleased"
heading; a release turns it into a dated heading.

## 2026.10.16 (tag `drizzlet-2026.10.16`, alpha)
- Self-play option: Automatic (stock, default) or On request. In On request, self-play never starts by itself; hold MEDIA 1.5 s with both inputs unpatched and FREEZE off to start or stop it. Feedback category, FEEDBACK knob; steady cyan / one magenta pulse. Saved in byte 11; stock ignores it and resets it on save (#17).
- Less flash wear from a modulated FREEZE: each interrupted buffer save doubles the wait before the next, up to 320 s; a completed save or a minute unfrozen resets it (#56).
- Enable Mutable Instruments' original monophonic grain code: hold MEDIA, then press SEED to toggle and save (default off). The MEDIA LED dims to half brightness. Use square or decay SHAPE; slower attacks can be cut nearly silent. Toggle off to recover sound and before downgrading to stock (#31).
- App built with GCC 9.3.1 instead of 4.9.3. In an earlier timing sweep of the released source on an STM32H750 at 480 MHz it was about 2% faster in the worst audio block, and 2% smaller. The randomized-pan rounding is pinned so output is unchanged: all 44 emulated scenarios are bit-identical to the GCC 4.9.3 build. The bootloader stays on GCC 4.9.3 with its bytes unchanged.
- Delay mode: after about 12 hours without a SEED clock or tap, the next one could set a negative tempo and collapse the echoes for one clock period. The idle count now stops at its 4 s limit (#57).
- Clocked frozen delay: SEED edges less than 16 samples apart (a fast clock, contact bounce, plugging a cable) could make a zero-length slice, an integer division by zero. On the module it gave silent play heads; slices are now at least one sample long (#33).

## 2026.10.14 (tag `drizzlet-2026.10.14`, alpha)
- Frozen-buffer backup is checked: a CRC over the audio and header, written in bytes stock ignores, so stock and Drizzlet still load each other's backups. Header fields are range-checked before use, including on stock backups. See FEATURES.md for the format.
- An interrupted or failed backup is never restored: the header's sector is erased first and the header written last, flash errors abandon the save, and a stale frozen flag is cleared before a new backup starts.
- A setting saved during a backup no longer makes the rest of the backup fail (settings saves locked the flash).
- Initialize dummy grain triggers used for CPU padding, removing undefined reads during processing.
- Add a guard row after the last wavetable, so interpolation from the final waveform of the last bank stays inside the table (output unchanged).
- Prevent invalid frozen delay reads when negative exponential FM makes a slice longer than the buffer.
- Output limiter option (Stock, Fast release, Off), saved; hold ASSIGN for three seconds to open the options page, then set it with DRY/WET (dry/wet LED category).
  The options page has three categories (feedback, dry/wet, reverb LEDs) of three settings (the mix knobs); the other eight are reserved.
  Holding ASSIGN or five idle seconds saves a change and exits; another button cancels.
  Saved in settings byte 10 (0 Stock, 0xA1 Fast, 0xA2 Off). Bytes 11–23 are written as zero;
  2026.10.16 reads a zero byte 11 as self-play Automatic, the stock behaviour.
- Named Drizzlet; release tags are now `drizzlet-YYYY.MM.DD`.
- Encoder runs on Python 3 (stm_audio_bootloader fork); the update file is unchanged.

## 2026.10.11
- First build (tag `beads-2026.10.11`): the released Mutable Instruments Beads source, unmodified. Its grain-stealing change, absent from the 2021 and 2024 firmware, removed the clicks of #1 in a test on one module.

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

Together, these two changes removed the clicks behind the crackle report (#1) in a test on one module; they were not tested separately. Settings: DENSITY 0.98, PITCH +24 semitones, SIZE 0.92 (just below delay) and SHAPE 0.5, set by CV; DRY/WET fully clockwise, FEEDBACK and REVERB fully counter-clockwise; Bright digital; music into both inputs. OUT L went to a scope with OUT R unplugged, so it carried L+R. Clicks were found by a detector on the scope captures, not by ear:

| Firmware | Clicks | Output observed |
|---|---:|---:|
| 2021 forum release | 145 | 21 s |
| 2024 recovery firmware | 164 | 20.6 s |
| Released source, same GCC 4.9.3 compiler | 0 | 14 s |
| Drizzlet 2026.10.16 | 0 | 83 s (four runs) |

Reverse, square-envelope, low-pitch and no-grain variants gave no clicks on any of the four. CPU overload is a hypothesis; CPU load has not been measured.

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
