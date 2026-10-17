# Base DSP optimizations

These are the three retained code changes. None changes the order of floating
point additions or grain scheduling.

| Change | Reference | Purpose | Audio contract |
|---|---|---|---|
| `ProcessInternal<mono>` | GCC 9 without specialization | Choose recording format once per block | Identical mono and stereo output |
| Local grain sum | Template build | Keep accumulation in registers | Same grain order, starting from zero |
| Unsigned read-index modulo | Local-sum build | Remove signed division correction | Callers supply non-negative indices |
| GCC 9 plus `MulAdd` | GCC 4.9.3 released source | Improve compiler output while pinning pan rounding | `fmaf` gives one fused rounding on every platform |

Earlier measurements of these same changes on a separate timing harness
reported 2,669, 3,630 and 739 fewer peak cycles respectively in its heaviest
stock case. Those figures are historical evidence, not measurements of this
branch. No timing calibration or hardware benchmark is included here.

CI compares native-host audio byte for byte against checked-in pre-optimization
DSP headers across 20 scenarios, including all four recording qualities and
synth resolutions. It also runs the bug-fix, firmware-callback and stock storage
compatibility suites and reproduces pinned release builds. See
`beads/test/README.md` for the exact checks and limits. Historical linked-ARM
comparisons are recorded only in the validation report; CI runs no emulator.
Host floating-point comparisons do not establish linked-ARM equivalence.
