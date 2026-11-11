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
`beads/build/expected-hashes.sha256` before building calibration.
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
No Drizzlet options or spectral counters are imported. Do not use the current
Drizzlet version-2 decoder. Compare matched settled panel workloads and the
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

## Deadline trace

`DEADLINE_TRACE=1` (with `DWT_CALIBRATION=1`) adds `beads/deadline_trace.h`:
a per-block record of when the output stores ran against the DAC DMA. Without
it the calibration image is unchanged. Use its own BUILD_ROOT:

```sh
make -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/makefile TOOLCHAIN_PATH="$TOOLCHAIN_PATH" KIT_GCC9=1 \
  BUILD_ROOT="$PWD/dwt-output/trace/" DWT_CALIBRATION=1 DEADLINE_TRACE=1 -j8 bin
```

The trace is disarmed at boot. Request byte 4 is request 1 plus an armed
trace: the request's own block is record 0 (its entry snapshot is taken in the
callback, and its cadence is not checked), so the stats and the trace cover the
same 240000 blocks. Requests 1 and 3 disarm a running trace. Each record (32 bytes) holds CYCCNT at interrupt
entry, before and after the output stores and at exit; DMA1 stream 0 (DAC)
NDTR at those points; stream 1 NDTR and LISR at entry; and priority-0
(SysTick, TIM8) time inside the bracket. The firmware classifies each block:
on time if the DAC DMA had not reached the half's first word when the stores
completed; definitely stale if it had already read past it before the first
store; ambiguous otherwise. Coalesced interrupts, cadence breaks, laps, missing
store brackets and long snapshots are invalid. A 768-record ring freezes 384
records after the first trigger (lower-bound slack of 1 word or less, a
coalesced or off-cadence interrupt, or a new SAI fault bit); the totals cover
the whole window and are published when it completes.

The 24832-byte NOLOAD section sits at 0x30041460 in uncached SRAM3, after the
stats; the linker asserts its size, address and the uncached region's end. The
live state is in DTCM's `.dtcm` section, so every other RAM address matches the
calibration image. The header (256 bytes) holds magic 0x444C5452, version 1,
a sequence counter (odd while writing), state, completion reason (window end,
abort, geometry), a window-invalid flag, generation, geometry, ring position,
total block count, trigger index, the compile-time delay, SAI status baseline
and first-set blocks, and the totals: minimum lower-bound slack, per-class
counts, per-reason invalid counts, priority-0 time and a slack histogram.
Codec Pause, Stop and Restart complete the window as aborted.
`deadline_trace.py DUMP --generation G` decodes a dump and reports whether the
window is admissible; `structural_trace.py ROOT` (as `structural_dwt.py`, with
`ROOT/trace` added) drives the codec interrupt in the oracle and checks audio
equality, classes, completion, abort and the order of the executed output
stores. `DEADLINE_TRACE_DELAY=N` spins N cycles before the stores; it is for
Seed3 validation only and has no makefile switch.

Offline decoder tests: `python3 -m unittest discover -s beads/build/tools -p test_deadline_trace.py`.
