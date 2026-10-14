# Changelog

Changes to Drizzlet, newest first. Each feature adds a line under
"Unreleased"; a release turns it into a dated heading.

## Unreleased

## 2026.10.14 (tag `drizzlet-2026.10.14`)
- Add a guard row after the last wavetable, so interpolation from the final waveform of the last bank stays inside the table (output unchanged).
- Prevent invalid frozen delay reads when negative exponential FM makes a slice longer than the buffer.
- Output limiter option (Stock, Fast release, Off), saved; hold ASSIGN for three seconds to open the options page, then set it with DRY/WET (dry/wet LED category).
  The options page has three categories (feedback, dry/wet, reverb LEDs) of three settings (the mix knobs); the other eight are reserved.
  Holding ASSIGN or five idle seconds saves a change and exits; another button cancels.
  Saved in settings byte 10 (0 Stock, 0xA1 Fast, 0xA2 Off). Bytes 11–23 are written as zero; later
  releases read a zero byte 11 as "no options saved", so every later option starts at stock.
- Named Drizzlet; release tags are now `drizzlet-YYYY.MM.DD`.
- Encoder runs on Python 3 (stm_audio_bootloader fork); the update file is unchanged.

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

The “freeze gate” and “gated density” bench tests reached the changed range. Lower CPU use in dense textures is a possible reason for the change, but has not been measured.

Monophonic mode is unchanged and compiled out of all shipped builds (`ENABLE_ADVANCED_FEATURES` is commented out).

### 2021 forum release → 2024 recovery firmware

Five changes:

1. **Settings recovery:** SEED held at power-on rewrites settings while keeping valid calibration, fixing modules stuck on boot. The 2021 release lacks this path. Do not hold SEED at power-on unintentionally.
2. **Knob/CV smoothing:** stmlib's `HysteresisFilter` loses its brief tracking mode after a change. State per channel shrinks from 28 to 20 bytes.
3. **Delay density/FM smoothing:** the dead band changes from 0.005 to 0.0025. The FM follower no longer tracks the input directly for the 7 calls after a change.
4. **Frozen delay slices:** during frozen playback, manual and clocked delay add hysteresis with a 0.0025 dead band to slice/TIME selection.
5. **Clocked delay FM:** both FM inputs affect the clock divider. Exponential FM previously bent tap ratios separately. Manual delay is unchanged.

Upgrading from 2021 to this build includes all six changes. This kit builds only the released source; patches and comparison details for the older builds are available on request.
