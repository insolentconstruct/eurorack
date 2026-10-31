<!--
Copyright 2026 Insolent Construct.
Author: Insolent Construct (insolentconstruct@gmail.com)
MIT; see beads/build/LICENSE.
-->

DWT calibration is a bench build for comparing tuneup with stock on the module.
It is disabled by default. `DWT_CALIBRATION=1` through
`beads/build/link-order.mk` enables the hooks, fixture helpers and supplemental
linker script. Use a fresh BUILD_ROOT for each configuration; make does not
track flag changes.

From the repository root, with the pinned stmlib submodule initialized and the
release GCC 9 toolchain installed, build both variants:

```sh
TOOLCHAIN_PATH="$TOOLCHAIN_PATH" bash beads/build/tools/build_dwt.sh "$PWD/dwt-output"
```

TOOLCHAIN_PATH is the directory containing the toolchain's `bin` directory.
The helper runs locally in the prepared build environment. It builds the current
working tree, including uncommitted changes, without Docker, SSH or hardware
access. Output must be an empty absolute directory without whitespace.
It checks `release/beads/beads.bin` against the pinned `app.bin` SHA256 in
`beads/build/expected-hashes.sha256` before building calibration:
`a221ee50f3ce05d8fdde174f6e0065aefc3b566a51563ae192f3b9dca514c2f2`.
A mismatch fails the build. The helper writes logs, object/link lists, sizes,
SHA256SUMS and a copy of `dwt.py` with BUILD_SHA set to the calibration binary's
hash. The source decoder has no application pin.

The equivalent individual application builds are:

```sh
make -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/makefile TOOLCHAIN_PATH="$TOOLCHAIN_PATH" KIT_GCC9=1 \
  BUILD_ROOT="$PWD/dwt-output/release/" DWT_CALIBRATION=0 -j8 bin
make -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/makefile TOOLCHAIN_PATH="$TOOLCHAIN_PATH" KIT_GCC9=1 \
  BUILD_ROOT="$PWD/dwt-output/calibration/" DWT_CALIBRATION=1 -j8 bin
```

These commands produce `beads/beads.{elf,bin}` beneath each BUILD_ROOT. Omitting
DWT_CALIBRATION is equivalent to setting it to zero. Use the ordinary release
build helper for bootloader, combo and WAV packaging. Do not enable calibration
for a release or bootloader build.

The stock calibration port's deterministic stimulus and version-1 RAM ABI are
preserved, allowing comparison with the stock GCC 4.9.3 calibration image.
No drizzlet options or spectral counters are imported. Do not use the current
drizzlet version-2 decoder. Compare matched settled panel workloads and the
q3 stereo stress32 fixture, interleave stock/tuneup captures, and report spread
as well as maxima. CPU numbers require module captures; offline checks do not
measure CPU cycles.

Stats occupy a 1112-byte NOLOAD section at 0x30041000 in uncached SRAM3.
The linker asserts the size and no overlap with codec DMA. Little-endian layout:
magic 0x44575443 at +0, version 1 at +4, sequence at +8, request byte at +12,
synthetic byte at +13, generation at +16, block at +20. Four 272-byte brackets
at +24, +296, +568 and +840 hold u32 maximum, u32 count, u64 sum and 64 u32
histogram bins (2000 cycles each, final bin unbounded). Brackets are callback,
inner DSP, fixture preparation and callback minus preparation.

The callback timer encloses the #53 codec fade, including its early silence
return and output gain ramp. Inner brackets measure only `synth.Render` or
`processor.Process`. DWT barriers and unsigned subtraction match the stock
port; IRQ preemption inside brackets is included, ISR entry/exit is excluded.
Callback prologue, reset bookkeeping, final histogram updates and return are
outside the callback read pair. Inner/preparation histogram updates remain
inside it. Callback minus preparation subtracts each block's preparation,
not independently observed maxima. Early silence returns have no inner sample;
use settled states for A/B measurements.

Request 1 clears and records 240000 panel callbacks. Request 3 clears and
selects q3 stereo stress32, warms 16000 blocks and captures 4096. A completed
capture freezes counters while audio continues. Request 1 returns to panel.
The fixture preserves the stock port's deterministic samples, PRNG seed,
gates, parameters and 32 +4x grains. Enter only with settled q3 stereo and no
trim, UI changes or save. Its initial buffer clear is outside acquisition and
may overrun the entry callback; mute monitoring at entry. Full fixture warmup
still requires qualification on hardware.

The offline decoder prints OpenOCD commands without connecting to hardware:

```sh
python3 beads/build/tools/dwt.py openocd read
python3 beads/build/tools/dwt.py openocd reset
python3 beads/build/tools/dwt.py openocd sai
python3 beads/build/tools/dwt.py decode capture.bin
```

Reset commands write RAM and require confirmed calibration firmware. Record
generation before requesting acquisition; require the request to clear and
generation to advance. Wait 65 s for panel or 7 s for fixture with no debug
traffic or control changes, then read. Accept only identical even sequence
values around the entire dump and the full expected sample count. Read SAI
status before and after capture; pre-existing sticky underrun/overrun flags
make a new occurrence inconclusive. The decoder checks ABI and histogram
consistency; it cannot authenticate the running firmware or prove acquisition
completion. Register the generated ELF, binary and decoder hashes in the bench
BUILDS table and flash allow-list before a module session.

Offline decoder tests: `python3 -m unittest discover -s beads/build/tools -p test_dwt.py`.
`structural_dwt.py ROOT` expects `ROOT/release/beads/beads.{elf,bin}` and
`ROOT/calibration/beads/beads.{elf,bin}`, plus an `oracle` directory alongside
it from the qualified external oracle (`tools/oracle`, with sibling
`tools/map`, `tools/recon` and `tools/official`). Run in that prepared offline
environment. It tests callback brackets, reset acknowledgment, fixture fills,
capture stop and flag-on/off audio equality. CYCCNT advances 1000 synthetic
ticks per read; these are structural results, never CPU measurements. The
short test skips most warmup. No hardware operation is part of these checks.
