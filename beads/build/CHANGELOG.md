# Changelog

## Unreleased
- Releases add `<tag>-combo.hex`, the ST-Link recovery image as Intel HEX: it carries its flash address, so programmers need none.

## 2026.10.11 (tag `beads-2026.10.11`)
- First build: the vanilla Mutable Instruments Beads source, unmodified.

## Changes from earlier Beads firmware

- **2021 forum release:** `beads.wav`, posted on the Mutable Instruments forum in April 2021.
- **2024 recovery firmware:** `beads_patch_firmware.wav` / `beads_golden_bootloader_combo.bin`, shared in the ModWiggler Beads thread in early 2024 to fix modules stuck on boot.

These are the source changes needed to reproduce the older application images exactly, not the original history. The vanilla bootloader matches the 2024 combo bootloader byte-for-byte. Effects below come from code and emulation unless a module test is named.

### 2024 recovery firmware → vanilla source

One change: grain stealing in polyphonic mode (`beads/dsp/granular_player.h`).

New grains fade out safety grains early to keep a voice free. The older firmware steals the oldest grain at intervals <5 ms; otherwise it sorts by remaining time. This build uses the oldest-grain method at intervals <25 ms.

At intervals >5 ms and <25 ms, this build swaps two safety-grain fade-out caps. The stolen grain fades out within 5 ms instead of one grain interval.

With identical input and state, emulation shows:
- At intervals ≥25 ms, outputs are bit-identical.
- At intervals from 5 ms to <25 ms, outputs differ.
- At intervals <5 ms, both run the same code. Differences can persist briefly if earlier intervals in the 5–25 ms range changed grain history.

The “freeze gate” and “gated density” bench tests reached the changed range.

On a test module, the 2021 and 2024 firmware clicked at DENSITY 0.98 and PITCH +24 semitones (145 clicks in 21 s and 164 in 20.6 s); this build did not (none in 14 s). A desktop run of both schedulers found no click in the DSP itself; CPU timing is the working hypothesis, not yet measured on the module.

Monophonic mode is unchanged and compiled out of all shipped builds (`ENABLE_ADVANCED_FEATURES` is commented out).

### 2021 forum release → 2024 recovery firmware

Five changes:

1. **Settings recovery:** SEED held at power-on rewrites settings while keeping valid calibration, fixing modules stuck on boot. The 2021 release lacks this path. Do not hold SEED at power-on unintentionally.
2. **Knob/CV smoothing:** stmlib's `HysteresisFilter` loses its brief tracking mode after a change. State per channel shrinks from 28 to 20 bytes.
3. **Delay density/FM smoothing:** the dead band changes from 0.005 to 0.0025. The FM follower no longer tracks the input directly for the 7 calls after a change.
4. **Frozen delay slices:** during frozen playback, manual and clocked delay add hysteresis with a 0.0025 dead band to slice/TIME selection.
5. **Clocked delay FM:** both FM inputs affect the clock divider. Exponential FM previously bent tap ratios separately. Manual delay is unchanged.

None of these touch grain playback, and the 2024 firmware clicks like the 2021 release. Upgrading from 2021 to this build includes all six changes. This kit builds only the vanilla source; patches and comparison details for the older builds are available on request.
