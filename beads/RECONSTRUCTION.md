# Beads 1.7 (2021) reconstruction

**TL;DR.** This branch is a reconstruction for diagnostic purposes, made by Insolent Construct. It rebuilds the application firmware in the 2021 Beads v1.7 update file (`beads_1.7.wav`) byte for byte. It is **not** Mutable Instruments' source for that version. Mutable Instruments has not published one.

The branch is upstream master plus the smallest source changes that reproduce the shipped image exactly. The real history may be different: identical bytes show what the code compiled to, not how it was written.

## Reference files

| File | SHA-256 |
|---|---|
| `beads_1.7.wav` (the update file) | `2a2b58c95e9ecbff1500e1c0ae8768bc72f8e9212b777c53ff67612ac0ccfb13` |
| Application decoded from it, 327,680-byte slot padded with `0xff` | `c7ad6d1a08b6637e91c4805c426fc042d2c1f3a0b72d30d1e1bcbcc864d3a372` |
| Application payload (first 267,008 bytes) | `6e2546db9257fc3a6cbd73e6e63b7099ebe1dddd9496857bd073cd567e9b1b3c` |

`beads/reconstruction.sha256` holds the two application hashes.

## Rebuilding

- **Toolchain:** GNU Tools for ARM Embedded Processors 4.9-2015-q2 (`arm-none-eabi-gcc` 4.9.3 20150529, binutils 2.24). This is the `arm-4.9` toolchain that `beads/makefile` expects.
- **stmlib:** the submodule is pinned to `0818267`, the last stmlib revision before `742b6df` removed tracking from `HysteresisFilter`. Run `git submodule update --init stmlib` after checkout.
- **Link order:** `beads/reconstruction.mk` includes `beads/makefile` and sorts the sources inside each package, keeping the package order, with the startup object last. GNU make 3.81 and 4.3+ sort wildcards this way, but 3.82 to 4.2 do not. A different object order still builds, but the bytes differ.

From the repository root:

```
make -f beads/reconstruction.mk TOOLCHAIN_PATH=<toolchain>/ \
    -o beads/resources.cc -o beads/resources.h reconstruction
```

`-o` keeps the checked-in resources. The `reconstruction` target pads `build/beads/beads.bin` to the 0x50000-byte slot as `build/beads/beads_padded.bin` and checks both files against `beads/reconstruction.sha256`.

`make -f beads/makefile wav` encodes the update file with Python 2, like upstream. A Python 3 port of the same encoder gives a file with the same header and length as `beads_1.7.wav`, but about 1 in 12 samples differ by one LSB. This is most likely floating-point rounding in the encoder's runtime. So the update file is not reproduced byte for byte, but the application it carries is.

## Changes from master

Each change is inferred: it is needed to match the shipped bytes, and the result matches all of them.

1. **No settings recovery at power-on** (`beads.cc`, `settings.h`, `settings.cc`). Master reads SEED at boot and passes `recovery_mode` to `Settings::Init()`, which calls `ChunkStorage::Recover()`. The 2021 image has no SEED read and no recovery call. stmlib `0818267` has no `Recover()` method either.
2. **HysteresisFilter with tracking** (stmlib pin). `0818267` still has `Init(threshold, tracking_time)` and the tracking counter. Each pot channel's state is 28 bytes instead of 20.
3. **Delay density and FM smoothing** (`dsp/delay_controller.cc`). The dead band is 0.005, not 0.0025, and the FM filter is initialised with a tracking time of 8.
4. **No extra filter on slice selection** (`dsp/delay_controller.h/.cc`). Master filters `slice_modulation` through `slice_index_filter_` before quantizing. The 2021 image quantizes it directly.
5. **Clocked delay keeps the FM inputs apart** (`dsp/delay_controller.cc`). Master adds exponential FM to linear FM when synchronized, so both move the divider. The 2021 image does not.
6. **Grain stealing** (`dsp/granular_player.h`). The cheap "steal the oldest grain" path is used below the 5 ms instant crossfade time, not below `kSimpleSchedulerThreshold` (25 ms), and the fade-cap swap is gone. `kSimpleSchedulerThreshold` stays defined but unused. This produces no code.

Resources, compiler flags and the linker script are unchanged.
