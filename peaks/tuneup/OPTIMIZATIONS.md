# Peaks optimizations

Counts are issued instructions per 4-sample block in an emulator, including
skipped conditional instructions, averaged over 9 scenarios (3 parameter sets
by 3 gate periods). Output is checked against vanilla in 960 more cases: both
control modes, 40 parameter sets, random and periodic gates.

## Compiler

The application is built with GCC 15.3 (Arm 15.3.Rel2) and the makefile's
flags, `-Werror` included; nothing is set per file. Across GCC 4.8 to 16, 15.3
had the lowest total and the lowest worst case. The bootloader stays on GCC 4.8.

Instructions per block, averaged as above:

| | Vanilla, GCC 4.8 | GCC 4.8, with the changes | GCC 15.3, with the changes |
|---|---|---|---|
| Envelope | 240 | 240 | 211 |
| LFO | 241 | 185 | 171 |
| Bass drum | 495 | 467 | 429 |
| Snare drum | 979 | 896 | 805 |
| High hat | 999 | 891 | 765 |
| FM drum | 397 | 397 | 398 |
| Pulse shaper | 481 | 461 | 513 |
| Pulse randomizer | 379 | 378 | 313 |
| Bouncing ball | 99 | 99 | 101 |
| Mini sequencer | 94 | 94 | 84 |
| Number station | 431 | 407 | 363 |
| Sum | 4,836 | 4,515 | 4,152 |

The output of GCC 15.3 matches GCC 4.8 in 1,408 cases per build: all 11
processors, both control modes, 8 parameter sets each, 4 gate patterns (random and
periodic), with and without new parameters every 97 blocks. A build with one
constant changed in the high hat differs in all 128 of its cases. The
application shrinks from 86,848 to 74,776 bytes.

The pulse shaper (+11% against the 4.8 build) is the one processor that
GCC 15.3 makes noticeably slower; see below. The FM drum and the bouncing
ball gain 1 and 2 instructions.

## Retained

| Change | GCC 4.8 | GCC 15.3 | Output |
|---|---|---|---|
| Pulse randomizer: step the slots with a pointer | 379 → 378 | 420 → 313 | identical |
| Pulse shaper: handle idle slots first and `continue` | 481 → 461 | 518 → 513 | identical |
| Drum filters: coefficients set in the setters, punch only in the bass drum | see below | see below | identical |
| LFO: shape chosen once per block, sine and steps settings in `set_parameter` | see below | see below | identical |

GCC 14 and later keep two copies of the slot pointer and move one into the
other on every pass through the 32 slots.
The pointer loop removes the copy in the randomizer. In the shaper, GCC 15.3
still copies it; the change saves 5 instructions there and 20 with GCC 4.8.

The filter (`drums/svf.h`) recomputed its coefficients behind a `dirty_`
flag and checked the bass drum's punch on every call, up to four times per
sample. The setters now compute the coefficient they change, and the
other drums call a `Process` that skips the punch:

| | GCC 4.8 | GCC 15.3 |
|---|---|---|
| High hat | 999 → 891 | 845 → 765 |
| Snare drum | 975 → 896 | 864 → 805 |
| Bass drum | 495 → 467 | 453 → 429 |
| Number station | 431 → 407 | 387 → 363 |

Output matches vanilla in 1,920 more cases for these four processors, with
the parameters changed every 97 blocks while they play.

Rejected: keeping the high hat's six phases in locals for the block. It saves
75 instructions with GCC 4.8 but costs 13 with GCC 13 to 15, which run out of
registers.

The LFO called its shape through a member pointer on every sample, and
the sine and steps shapes recomputed values from the parameter each time.
`Process` now picks a loop specialized for the shape from a table once per
block, the shape functions are inline, and `set_parameter` computes the
sine's fold gain and the steps' levels and scale. Average over 3 parameter
values and 2 gate periods:

| | GCC 4.8 | GCC 15.3 |
|---|---|---|
| Sine | 291 → 246 | 272 → 215 |
| Triangle | 224 → 180 | 226 → 177 |
| Square | 203 → 157 | 199 → 150 |
| Steps | 214 → 145 | 212 → 132 |
| Noise | 241 → 190 | 227 → 176 |

Output matches vanilla in 480 more cases, with the parameters (and so the
shape) changed every 97 blocks.

Fixing the triangle's power-up bug (#90) moves its slopes into
`set_parameter` too: triangle 180 → 157 with GCC 4.8 and 177 → 149 with
GCC 15.3. With a plain triangle played first, the output matches the
previous change in all 480 cases; from power-up, 98 differ, all from the bug.

## Candidates

Each is measured before it is kept:

- A local `previous_sample` accumulator in the FM drum.
