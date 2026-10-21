# Peaks optimizations

Counts are issued instructions per 4-sample block in an emulator, including
skipped conditional instructions, averaged over 9 scenarios (3 parameter sets
by 3 gate periods). Output is checked against vanilla in 960 more cases: both
control modes, 40 parameter sets, random and periodic gates.

## Retained

| Change | GCC 4.8 | GCC 15.3 | Output |
|---|---|---|---|
| Pulse randomizer: step the slots with a pointer | 379 → 378 | 420 → 313 | identical |
| Pulse shaper: handle idle slots first and `continue` | 481 → 460 | 518 → 513 | identical |

GCC 14 and later keep two copies of the slot pointer and move one into the
other on every pass through the 32 slots.
The pointer loop removes the copy in the randomizer. In the shaper, GCC 15.3
still copies it; the change saves 5 instructions there and 21 with GCC 4.8.

## Candidates

Each is measured before it is kept:

- A local `previous_sample` accumulator in the FM drum.
- Choosing the LFO shape once per block instead of an indirect call per sample.
