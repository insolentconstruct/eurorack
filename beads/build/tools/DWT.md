<!--
Copyright 2026 Insolent Construct.
Author: Insolent Construct (insolentconstruct@gmail.com)
-->

DWT calibration is a bench-only build, enabled by `DWT_CALIBRATION=1` through
`beads/build/link-order.mk`; it is absent by default. `build_dwt.sh OUTPUT`
archives committed sources using Docker (optional `BUILD_CONTEXT`).
Optional `BUILD_DRAIN_DIR` stops builds when its context marker exists. No hardware is
accessed by that build script. `dwt.py decode DUMP` is entirely offline.

Build from a committed checkout (including the stmlib submodule):

```sh
bash beads/build/tools/build_dwt.sh "$PWD/dwt-output"
```

`BUILD_CONTEXT` is optional; plain local Docker works too. The build image is
`${BEADS_BUILD_IMAGE:-beads-build-kit}`. Flash the calibration application with your usual
application-only programmer procedure at 0x08020000, preserving bootloader,
settings and calibration. Verify the binary SHA256 against `calibration.bin` in
`expected-hashes.sha256`. Flashing and reset requests require an operator at the bench.

Start OpenOCD with an ST-LINK and a board-appropriate configuration that
attaches without resetting or halting the running core, for example:

```sh
openocd -f interface/stlink.cfg \
  -f target/stm32h7x.cfg -c 'adapter speed 1000' -c init
# In another terminal: telnet localhost 4444
python3 beads/build/tools/dwt.py openocd read
python3 beads/build/tools/dwt.py openocd reset
python3 beads/build/tools/dwt.py openocd sai
```

The last three commands only print Tcl to paste into OpenOCD's console.
Read prints `mdw 0x30041000 278` (1112 bytes), then saves `capture.bin` in
OpenOCD's working directory with the same before/after sequence guard.
Discard the dump on any error. Decode with `dwt.py decode capture.bin`.
Reset **writes RAM** at 0x3004100c; it does not verify the running application
hash. Confirm calibration firmware first. Record generation at 0x30041010
before the write; require it to advance and request at 0x3004100c to clear.
Use request 3 for fixture entry, request 1 to restore panel acquisition.

Stats live at 0x30041000, 1112 bytes in uncached SRAM3, dedicated NOLOAD
section `.dwt_calibration`. The supplemental linker script asserts size and
no overlap with codec DMA. Layout, little endian:
0 magic=0x44575443; 4 version=1; 8 sequence; 12 request byte;
13 synthetic bool; 14..15 reserved; 16 reset generation; 20 block counter.
The C++ fields are `synthetic` and `block`; ABI version and offsets are unchanged.
The decoder exposes these as `fixture` and `blocks_elapsed`.
Four 272-byte brackets at 24,296,568,840: maximum u32, count u32, sum u64,
64 u32 histogram counters. Bins are 2000 cycles; last is [126000,infinity).
Brackets: callback, DSP inner, fixture preparation, callback minus fixture.
The decoder returns quantile intervals, including budget percentages.
Snapshot acceptance requires identical even sequence before/after the read.

Normal request=1 clears and captures 240000 blocks (~60 s). Request=3
(request byte 3) clears, selects the explicit q3 stereo stress32 fixture,
warms 16000 blocks (4 s), and captures 4096 (1.024 s). Acquisition then stops
updating stats so SWD can read them coherently while audio continues. A new
request starts another acquisition. Return to panel with request byte 1. Counts must reach the expected window before accepting a result.

Fixture requires settled q3 stereo, Stock limiter, pitch scale Off, no
switching/UI/background save or buffer trim. Do not trim during acquisition. The extracted preparation helper preserves the original samples, PRNG seed,
gate sequence, parameters and grain setup. Its compiled instruction layout
and instrumentation overhead can change; prior cycle measurements must not
be treated as measurements of this image. It resets DSP/buffers/PRNG, primes 32 +4x
grains every block, uses Seed3 deterministic input, gates and parameters.
The initial buffer clear is deliberately outside acquisition and can overrun
that warmup callback; mute monitoring for fixture entry. Do not use its
first callback as a deadline observation. The fixture replaces live audio
and controls until the next normal reset. Warmup is not included in stats.
Fixture preparation is measured separately; callback minus fixture subtracts
that same block's preparation, rather than subtracting independent maxima.

DWT barriers match Seed3. Unsigned subtraction handles CYCCNT wrap within
a block. 64-bit sums avoid overflow within capture windows. No logging in
the callback. IRQ preemption within brackets is included; IRQ work outside
callback and ISR entry/exit are not. Prologue, constructor/reset bookkeeping,
final callback histogram updates and return machinery are outside the total's
read pair. Inner and preparation histogram updates are inside it; callback
minus fixture retains those updates and subtracts only the preparation read
pair. Readout runs after acquisition, avoiding SWD traffic in the measurement
interval.

Offline checks: `python3 -m unittest test_dwt` from this directory.
`verify_dwt.py RELEASE_BIN CALIBRATION_BIN` checks artifact SHA256 identity;
it does not emulate firmware or establish hardware timing.

Read SAI1 block A SR (DAC) with `mdw 0x4001581c 1` and block B SR (ADC)
with `mdw 0x4001583c 1`. Bit 0 is DAC underrun / ADC overrun respectively.
These reads never clear either sticky flag. Read before and after capture;
a pre-existing flag makes a new occurrence inconclusive. Avoid debug traffic
during acquisition.

Seven settled panel scenarios use stereo/polyphonic operation, Stock limiter
and pitch scale Off except where specified. Common controls: Bright digital,
DENSITY .95, SIZE .85, PITCH +24 semitones, SHAPE/TIME .5, FEEDBACK .8,
DRY/WET .8, REVERB .7. Patch music into both inputs; use fixed CV for fixed
controls. Settle 4 s after changes/saves, reset, wait 65 s, then read. Do not
trim or change controls during capture. Preserve stimulus and capture logs.

| # | Scenario | Changes from common controls |
|---:|---|---|
| 1 | Stock worst patch | Stock reverb; also capture matching synthetic fixture |
| 2 | Clouds-style | Clouds-style on; also capture matching synthetic fixture |
| 3 | Pitch scale | Clouds-style on; Scale=Shri; optional matched fixture |
| 4 | Delay | SIZE fully CW; Stock reverb |
| 5 | Synth | Stock reverb; wavetable synth Automatic (Stock); disconnect both inputs, wait >10 s quiet then settle |
| 6 | Sunny tape | Sunny tape quality; Stock reverb; reconnect both inputs |
| 7 | Crackle | Bright digital, Clouds-style on, DRY/WET fully CW, FEEDBACK fully CCW, REVERB as left by options exit, DENSITY .98, PITCH +24 semitones, SIZE .92, SHAPE .5 by fixed CV; music into both inputs |

Scenario 7 passes when total callback maximum is strictly below 120000
cycles and SAI DAC underrun is not newly set. Capture SAI before reset and
after the completed capture; record ADC overrun too. A pre-existing DAC flag
makes the underrun check inconclusive. This settled capture does not prove
transition safety. Existing Stock reserve criterion remains maximum <=90000
cycles for rows 1/4/5/6 (callback) and Stock fixture (callback minus fixture).
Report Clouds and combined-option margins separately; no hardware result
is implied by offline checks.

Options open on page 1 (REVERB LED): hold [M] (ASSIGN) alone for 3 s, then
release. On page 1, FEEDBACK selects pitch Off/Chromatic/Scale by thirds (in
Scale, short SEED taps cycle the scales through Shri), and REVERB selects
Stock/Clouds-style reverb (lower/upper half). Two [M] taps reach page 3
(DRY/WET LED): DRY/WET bottom third is the Stock limiter, FEEDBACK lower half
is Wavetable synth: Automatic (Stock). Move the relevant pot by more than 10%
to qualify a change; it saves after one second at rest. Close by holding [M]
for 3 s or pressing another button. Restore the scenario mix controls after
exit and settle before capture.

Operator sequence: read both SAI status registers; record generation;
write request 1; confirm generation advanced and request cleared; wait 65 s
without debug traffic or control changes; run the printed read commands;
read both SAI registers again; decode the saved dump. Use distinct dump
names per scenario. For the fixture phase use request 3, mute monitoring at
entry, wait 7 s, then read. Ordinary request 1 restores panel acquisition.

`test_dwt.py` is suitable for offline CI. Release builds keep calibration off.
