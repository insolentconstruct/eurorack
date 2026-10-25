# Beads early-2024 patch reconstruction

**TL;DR.** This branch is a reconstruction for diagnostic purposes, made by Insolent Construct. It rebuilds the application firmware in the early-2024 Beads patch update file (`beads_patch_firmware.wav`) byte for byte. That is the "stuck on boot" fix shared in early 2024. This branch is **not** Mutable Instruments' source for that version. Mutable Instruments has not published one.

The branch is upstream master plus the smallest source change that reproduces the shipped image exactly. The real history may be different: identical bytes show what the code compiled to, not how it was written. The build date of the patch firmware is unknown.

## Reference files

| File | SHA-256 |
|---|---|
| `beads_patch_firmware.wav` (the update file) | `41bbb91003f3aa448bb4d60bb01c4fb9957a20c7c18e4cc90401a1b16649e1ae` |
| Application decoded from it, 327,680-byte slot padded with `0xff` | `1015e4916a049da062113dcb1484dd9343a1539aaf1b292eb56d7ba4fad68dbd` |
| Application payload (first 267,904 bytes) | `7c48c43878eb286d78b880ac8d9b12a09e353849a0ffaa46e3133321f3c5181d` |

`beads/reconstruction.sha256` holds the two application hashes.

## Rebuilding

- **Toolchain:** GNU Tools for ARM Embedded Processors 4.9-2015-q2 (`arm-none-eabi-gcc` 4.9.3 20150529, binutils 2.24). This is the `arm-4.9` toolchain that `beads/makefile` expects.
- **stmlib:** the submodule commit pinned by master, `e3bd7c9`. Run `git submodule update --init stmlib` after checkout.
- **Link order:** `beads/reconstruction.mk` includes `beads/makefile` and sorts the sources inside each package, keeping the package order, with the startup object last. GNU make 3.81 and 4.3+ sort wildcards this way, but 3.82 to 4.2 do not. A different object order still builds, but the bytes differ.

From the repository root:

```
make -f beads/reconstruction.mk TOOLCHAIN_PATH=<toolchain>/ \
    -o beads/resources.cc -o beads/resources.h reconstruction
```

`-o` keeps the checked-in resources. The `reconstruction` target pads `build/beads/beads.bin` to the 0x50000-byte slot as `build/beads/beads_padded.bin` and checks both files against `beads/reconstruction.sha256`.

`make -f beads/makefile wav` encodes the update file with Python 2, like upstream. A Python 3 port of the same encoder gives a file with the same header and length as `beads_patch_firmware.wav`, but about 1 in 12 samples differ by one LSB. This is most likely floating-point rounding in the encoder's runtime. So the update file is not reproduced byte for byte, but the application it carries is.

## Changes from master

One change, in `dsp/granular_player.h`. It is inferred: it is needed to match the shipped bytes, and the result matches all of them.

- **Grain stealing.** The cheap "steal the oldest grain" path is used below the 5 ms instant crossfade time, not below 25 ms. The `simple_scheduler_threshold_` member and its initialisation are removed, and so is the fade-cap swap for intervals between 5 and 25 ms. `kSimpleSchedulerThreshold` stays defined but unused. This produces no code.

Resources, compiler flags, the linker script and stmlib are unchanged.
