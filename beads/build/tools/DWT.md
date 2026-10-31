<!--
Copyright 2026 Insolent Construct.
Author: Insolent Construct (insolentconstruct@gmail.com)
-->

**TL;DR:** a bench-only build (`DWT_CALIBRATION=1` through `beads/build/link-order.mk`, off by default) counts CPU cycles per audio block and per spectral frame; read the counters over SWD and decode them offline with `dwt.py`. `build_dwt.sh OUTPUT` archives committed sources and builds with Docker (optional `BUILD_CONTEXT`; optional `BUILD_DRAIN_DIR` stops builds when its context marker exists). It touches no hardware, and `dwt.py decode DUMP` is entirely offline.

Build from a committed checkout (including the stmlib submodule):

```sh
bash beads/build/tools/build_dwt.sh "$PWD/dwt-output"
```

Plain local Docker works too; the image is `${BEADS_BUILD_IMAGE:-beads-build-kit}`. Flash the calibration application with your usual application-only procedure at 0x08020000, preserving bootloader, settings and calibration, and verify the binary SHA256 against `calibration.bin` in `expected-hashes.sha256`. Flashing and reset requests need an operator at the bench.

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

The last three commands only print Tcl to paste into OpenOCD's console. Read prints `mdw 0x30041000 308` (1232 bytes), then saves `capture.bin` in OpenOCD's working directory with the same before/after guard on both sequence counters; discard the dump on any error. Decode with `dwt.py decode capture.bin`.

Reset **writes RAM** at 0x3004100c and does not verify the running application hash, so confirm calibration firmware first. Record generation at 0x30041010 before the write; require it to advance and request at 0x3004100c to clear. Request 3 enters the fixture; request 1 restores panel acquisition.

Stats live at 0x30041000, 1232 bytes in uncached SRAM3, dedicated NOLOAD
section `.dwt_calibration`. The supplemental linker script asserts size and
no overlap with codec DMA. Layout, little endian:
0 magic=0x44575443; 4 version=2; 8 sequence; 12 request byte;
13 synthetic bool; 14..15 reserved; 16 reset generation; 20 block counter.
The C++ fields are `synthetic` and `block`.
The decoder exposes these as `fixture` and `blocks_elapsed`.
Four 272-byte brackets at 24,296,568,840: maximum u32, count u32, sum u64,
64 u32 histogram counters. Bins are 2000 cycles; last is [126000,infinity).
Brackets: callback, DSP inner, fixture preparation, callback minus fixture.
The decoder returns quantile intervals, including budget percentages.
Spectral frames at 1112 (version 2): frame sequence u32, reserved u32, then
two 56-byte channels (left, right) at 1120 and 1176: generation, produced,
completed, skipped, backlog maximum, wall maximum, exclusive maximum, STFT
done count, discontinuities, reserved (u32 each), wall sum and exclusive sum
(u64). The main loop writes them under
the frame sequence; the callback never does. A channel belongs to the current
acquisition only if its generation equals the header's.
Snapshot acceptance requires both sequences even and identical before/after the read.

Request 1 clears and captures 240000 blocks (~60 s). Request 3 clears, selects the q3 stereo stress32 fixture, warms 16000 blocks (4 s) and captures 4096 (1.024 s). Acquisition then stops updating stats so SWD reads them coherently while audio continues; a new request starts another acquisition. Counts must reach the expected window before accepting a result.

The fixture requires settled q3 stereo, Stock limiter, pitch scale Off, no switching, UI, background save or buffer trim (do not trim during acquisition). It resets DSP, buffers and PRNG, and primes 32 +4x grains every block from Seed3's deterministic input, gates and parameters (the extracted preparation helper preserves the original samples, PRNG seed, gate sequence, parameters and grain setup). Its compiled instruction layout and instrumentation overhead can change, so earlier cycle measurements are not measurements of this image. The initial buffer clear is outside acquisition and can overrun the warmup callback: mute monitoring for fixture entry and ignore its first callback. The fixture replaces live audio and controls until the next normal reset. Warmup is not in the stats. Preparation is measured separately; callback minus fixture subtracts that block's preparation, not independent maxima.

Spectral frames: each channel's call to `STFT::Buffer` that computes a frame
is timed in the main loop. Wall time is DWT cycles from before the call to
after it, interrupts included. Exclusive time subtracts the callback bracket
cycles recorded meanwhile (interrupts are masked for the paired reads of the
cycle counter and the bracket sum, a few cycles); ISR entry/exit, the callback
code outside its bracket and other interrupts stay in it. Skipped counts
frames `STFT::Buffer` dropped as overdue; backlog is the frames one call took
(the computed one plus the skipped). Produced is completed + skipped + frames
still queued, as of the channel's last recorded frame. Discontinuities count
recorded frames that do not follow the previous one (an STFT restart, such as
a quality change, within the acquisition); a valid run has none. Only
panel acquisitions record frames, and only frames that start and end inside
one: a frame a reset or the end of acquisition overlaps is discarded. A
channel's counters are reset by its first frame after a reset. The hop
deadline is 1024 samples at the spectral rate: 48 kHz for Bright digital
(10,240,000 cycles), 32 kHz for Cold digital (15,360,000), 24 kHz for the tapes
(20,480,000).

Timing: DWT barriers match Seed3; unsigned subtraction handles CYCCNT wrap within a block; 64-bit sums avoid overflow; no logging in the callback. IRQ preemption inside brackets is counted; IRQ work outside the callback and ISR entry/exit are not. Prologue, constructor/reset bookkeeping, the final callback histogram update and return machinery are outside the total's read pair; inner and preparation histogram updates are inside it. Readout runs after acquisition, so there is no SWD traffic in the measurement.

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

Scenario 7 passes when total callback maximum is strictly below 115800
cycles (the 48 kHz deadline, below) and SAI DAC underrun is not newly set.
Capture SAI before reset and after the completed capture; record ADC
overrun too. A pre-existing DAC flag makes the underrun check inconclusive. This settled capture does not prove
transition safety. Existing Stock reserve criterion remains maximum <=90000
cycles for rows 1/4/5/6 (callback) and Stock fixture (callback minus fixture).
Report Clouds and combined-option margins separately; no hardware result
is implied by offline checks.

Deadline: the DAC reads slightly ahead, so a late block is heard before
the 120000-cycle budget runs out. The first stale sample appears at a
callback maximum of about 115800 cycles at 48 kHz (q1, q3, synth), 113900
at 32 kHz (q2) and 111500 at 24 kHz (q0). These were measured on the
development board (same core, codec driver and clocks), not on the module.

Options: hold the CV-assign button [M] alone for 3 s, then release. On page 1 (FEEDBACK LED), FEEDBACK selects pitch Off/Chromatic/Scale by thirds (in Scale, short SEED taps cycle the scales through Shri). One [M] tap reaches page 2 (DRY/WET LED), where DRY/WET selects reverb Stock/Clouds-style (lower/upper half). Page 3 (REVERB LED): REVERB lower half is Synth start: Automatic (Stock). Page 4 (all three LEDs): REVERB bottom third is the Stock limiter. Move the pot by more than 10% to qualify a change; it saves after one second at rest. Close by holding [M] for 3 s or pressing another button. Restore the scenario mix controls and settle before capture.

Operator sequence: read both SAI status registers; record generation;
write request 1; confirm generation advanced and request cleared; wait 65 s
without debug traffic or control changes; run the printed read commands;
read both SAI registers again; decode the saved dump. Use distinct dump
names per scenario. For the fixture phase use request 3, mute monitoring at
entry, wait 7 s, then read. Ordinary request 1 restores panel acquisition.

`test_dwt.py` is suitable for offline CI. Release builds keep calibration off.
