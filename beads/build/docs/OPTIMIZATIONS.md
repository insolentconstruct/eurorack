# Optimizations

Measure audio-path changes by worst-case cycles and sound identity.

## TL;DR

Same panel test, whole callback: Drizzlet peaks at 94,311/120,000 cycles
(78.6%); vanilla source at 110,947 (92.5%). Audio is unchanged.

| Module, whole audio callback, worst block | Stock, panel | Stock, stress test | Clouds-style, panel |
|---|---:|---:|---:|
| Vanilla source (GCC 4.9.3) | 110,947 (92.5%) | 114,053 (95.0%) | – |
| Drizzlet before the integer Hermite read | 102,433 (85.4%) | 106,014 (88.3%) | 103,665 (86.4%) |
| Drizzlet with it | 94,311 (78.6%) | 98,802 (82.3%) | 96,022 (80.0%) |

Headroom in budget points: development-board DSP only, except Hermite and
total (whole module callback, stock panel settings).

| Change | Headroom |
|---|---:|
| GCC 9.3.1 instead of 4.9.3 (no code changes) | about 2.0 |
| Mono/stereo chosen once per block | 2.2 |
| Grains summed in a local frame | 3.0 |
| Unsigned modulo for the delay-line read | 0.6 |
| Grains interpolate the stored integers (integer Hermite) | 6.8 |
| Total, module | 13.9 |

Clouds-style tails-first diffusion gains 1.0 budget point. Plain GCC 11–16
adds 5,800–9,300 stock-peak cycles over GCC 9; unverified GCC 10.3 adds 10,200.
Raising inlining puts GCC 11 within noise. Explicit FMA/rounding matches GCC 9
audio/buffer hashes in 28 scenarios. Bootloader remains on 4.9.3.

Vanilla-only [beads-tuneup](https://github.com/insolentconstruct/eurorack/tree/beads-tuneup):
development-board stock peak, 24 scenarios, all hashes matched.

| Build | Cycles | Budget |
|---|---:|---:|
| Vanilla source (beads-build-kit, GCC 4.9.3) | 99,511 | 82.9% |
| beads-tuneup (GCC 9 and the first three code changes) | 90,529 | 75.4% |
| beads-tuneup with the integer Hermite read (not yet on that branch) | 82,911 | 69.1% |

- **Sound:** GCC 9 matched 4.9.3 in 20 of 20 emulator scenarios. Each code change matched bit for bit on the development board (28 of 28 hashes) and in the host tests; the emulator agreed wherever it was run (integer Hermite: 70 of 70 comparisons).
- **Rejected:** newer compilers (slower as plain builds, no clear gain with tuning), ITCM code placement, an extra grain/delay template, freeze state as a template parameter, synth grains rendered a block at a time, one Hermite read per synth sample.

## Build comparison

Budget is 120,000 cycles per block at 480 MHz. "Stock" is the worst stock case: q3 (48 kHz digital), stereo, 32 grains held active (stress32). "Clouds-style" is the same case with the Clouds-style reverb. Same probe and 28 scenarios on every row; flash is the whole application binary.

| Build | App flash | Stock peak / median | Clouds-style peak | Sound vs GCC 9 | Verdict |
|---|---:|---|---:|---|---|
| GCC 9.3.1 baseline | 274,340 B | 97,255 / 95,610 (81.05%) | 100,009 | reference | kept |
| GCC 9.3.1 + mono/stereo template | 273,700 B | 94,719 / 93,111 (78.93%) | 97,287 | bit-identical | kept |
| GCC 9.3.1 + all three changes | 275,076 B | 90,333 / not recorded (75.28%) | not recorded | bit-identical | kept, Beta 1 |
| GCC 10.3 | not measured | not measured | not measured | not measured | not run: no verified archive checksum |
| GCC 11.3 | 251,644 B | 100,634 / 98,974 (83.86%) | 103,118 | differs, 0.000296535 FS | rejected |
| GCC 12.3 | 251,944 B | 99,675 / 98,147 (83.06%) | 102,586 | differs, 0.000296535 FS | rejected |
| GCC 13.3 | 252,296 B | 99,516 / 98,011 (82.93%) | 102,243 | differs, 0.000296535 FS | rejected |
| GCC 14.3 | 255,696 B | 100,369 / 98,826 (83.64%) | 102,830 | differs, 0.000296535 FS | rejected |
| GCC 15.3 | 253,760 B | 100,899 / 99,455 (84.08%) | 103,331 | differs, 0.0282450 FS | rejected |

GCC 11–15 use GCC 9 baseline source; template-source stock peaks were
100,069–101,526. `-fno-inline-functions` changed sound/timing by under 0.1%.
All-three flash is template +1,376 B. Worst errors across 13 long scenarios
exceed the -90 dBFS (0.0000316 FS) gate by 9–890 times. Seed3 hashes match
except GCC 15.3 (19/28).

**Earlier sweep (vanilla source, GCC 4.9 to 15, emulator).** Relative to GCC 4.9.3 (image 268,384 B, median 96,826, worst 101,804 cycles):

| GCC | Image | Median | Worst | Output vs 4.9.3 | Verdict |
|---|---:|---:|---:|---|---|
| 4.9.3 | 268,384 B | 96,826 | 101,804 | reference | previous build |
| 5, 6 | -3.5, -4.6% | +1.3, +0.5% | +1.2, +0.5% | 20/20 exact | not chosen |
| 7 | -6.6% | +0.7% | +0.1% | 16/20, max 1 float ulp | not chosen |
| 8 | -6.7% | -1.2% | -1.7% | 16/20, max 1 float ulp | runner-up |
| 9 | -2.3% | -2.2% | -2.3% | 16/20, max 1 ulp (20/20 with the pan pin) | chosen |
| 10 to 15 | -10 to -11.5% | +0.2 to +1.5% | +0.4 to +1.6% | 8/20 or 12/20, max 1.4e-5 | rejected |

Seed3 worst/median remeasure: GCC 4.9.3 101,773/96,818; 8.3.1
99,957/95,533; 9.3.1 99,430/94,672. Older source/different probe: GCC 15.3
was 5.7% slower at peak than 4.9.3; 12/20 short scenarios matched.

## Compiler

- **GCC 9 vs 8:** both improve median/peak over 4.9.3. GCC 8 saves more flash
  (-6.7% vs -2.3%), but GCC 9 lowers peak load (82.9% vs 83.3%) and alone
  was confirmed bit-exact with the pan pin. Headroom takes priority over flash.
- **GCC 10+ inlining:** emulator instructions, not module cycles. GCC 10's
  `-O2` enables `-finline-functions` and lowers `max-inline-insns-single`
  200→70. Hermite reads stop inlining: 384 extra calls/block in stress32,
  with stack register saves. `--param max-inline-insns-single=140` lets GCC 10
  beat GCC 9 instruction count for about 12 KB flash; lowering GCC 9's limit
  reproduces the slowdown. Against GCC 9: GCC 10/12/14/16 add
  7.9/4.4/0.9/1.5% instructions (vanilla: +6.0/+2.2/-0.7/-0.4%).
  `always_inline` or the parameter could permit newer compilers; sound
  differences remain a separate gate.
- **Build.** GCC 9.3.1 (Arm GNU Embedded 9-2020-q2), `-std=gnu++98` as for the vanilla source. The build kit still builds the vanilla app with 4.9.3 as a fixed reference. Both compiler licences ship with the release.
- **Pan pin:** `randomized_parameter.h` uses explicit `fmaf` for
  `0.5 + (u - 0.5) * min(spread * v, 1)`, one ARM instruction. Without it,
  inconsistent contraction makes GCC 7–9 differ from 4.9.3 by up to one float
  ulp in 4/20 scenarios.
- **Evidence.** GCC 9 matched 4.9.3 in 20 of 20 emulator scenarios. The emulator runs the real linked ARM code with fused multiply-add and flush-to-zero modelled.
- **GCC 10+ rejection:** stock peak 99,516–100,899 vs 97,255 (+2.3–3.7%).
  Of 13 long scenarios, 11 match; others differ up to 0.000296535 FS (11–14)
  or 0.0282450 FS (15). Short suites missed this. Separate per-sample calls
  in the saturated digital renderer add 1.60% instructions and 84.42% stack
  instructions. Disabling contraction worsens sound differences.
- **Bootloader:** GCC 4.9.3, byte-identical to Mutable Instruments' build.
  Runs only at startup/updates; WAV updates never replace it. Keep recovery stable.

## Code changes

Cycles saved on the Seed3 worst stock and worst Clouds-style cases, peak / median, against the build in the "Against" column. Run 1 is the figure used; run 2 is a repeat. Negative is a slowdown.

| Change | Based on | Against | Stock | Clouds-style | Flash | RAM | Sound | Verdict |
|---|---|---|---|---|---:|---|---|---|
| GCC 9 instead of 4.9.3 | Drizzlet: compiler choice; an explicit `fmaf` pins one pan line so the arithmetic stays Mutable Instruments' | GCC 4.9.3 | about 2% in the worst block | not measured | about -2% | n/a | 20/20 emulator | kept |
| Mono/stereo chosen once per block (19-line template) | Mutable Instruments: template specialization on configuration (STM32 miscellania; Clouds' grain renderer) | GCC 9 | 2,669 / 2,499 | 2,828 / 2,756 | -640 B | 0 | 28/28 Seed3, 20/20 emulator, 13/13 long, error 0 | kept |
| Sum delay grains in a local frame | Mutable Instruments: keep loop state in locals across the block (Timing and processes) | template | 3,630 / 3,725 (run 2: 3,826 / 3,732) | 3,805 / 3,904 (run 2: 3,788 / 3,907) | +1,648 B | 0 | same gates | kept |
| Unsigned modulo for the delay-line read index | Mutable Instruments: the same file's `LoadQuad` already uses `size_t(i) % items_per_bank()` | local frame | 739 / 556 (run 2: 636 / 550) | 832 / 626 (run 2: 871 / 628) | -272 B | 0 | gates run on the combined tree | kept |
| Both pass-2 changes together | the two rows above | template | 4,369 / 4,281 | 4,637 / 4,530 | +1,376 B | 0 | 28/28, 20/20, 13/13, error 0 | kept |
| All sample loops in ITCM | Mutable Instruments: stmlib's `IN_RAM` placement | GCC 9 | -765 / -770 | -270 / -377 | +336 B | +21,440 B ITCM | 28/28 Seed3 | rejected |
| ITCM on top of the template | Mutable Instruments: stmlib's `IN_RAM` placement | template | -611 / -505 | -213 / -277 | +360 B | +23,448 B ITCM | 28/28 Seed3 | rejected |
| Extra grain/delay template | Mutable Instruments: template specialization | GCC 9 | -1,693 / -1,812 | -1,406 / -1,627 | -336 B | 0 | 28/28 Seed3 | rejected |
| Freeze state as a template parameter | Mutable Instruments: template specialization | both pass-2 changes | 769 / 728 | 343 / 378 | not built | n/a | 28/28 Seed3 | rejected |
| Diffuser: all delay-line tails read before the eight all-passes | Drizzlet: the compiler cannot move a load above an earlier store that may alias, so the in-order core waited on every load, conversion and multiply; Clouds' diffuser shape and constants unchanged | current tree | within noise (about 200) | 1,234 / 1,257 (run 2: 1,244 / 1,254) | -116 B | -32 B | 28/28 Seed3 hashes; host tests 6/6, Clouds goldens unchanged; emulator not run | kept |
| Diffuser: one body for stock and modulated-tap use (`template<bool>`), tails-first helpers shared as `AllPassMemory` | Mutable Instruments: template specialization on configuration; the same helpers read the long all-passes of the frozen-loop prototype | tails-first diffuser | within noise (worst 91,700 vs 91,796) | not recorded | not recorded | 0 | 28/28 Seed3 flag off and flag on unfrozen | kept |
| Diffuser: constant amount and `Load` instead of `Read` | Mutable Instruments: keep loop state in locals; Clouds' reverb uses `Load` | current tree | within noise | 4 / -3 (noise) | not recorded | 0 | 28/28 | rejected: no gain |
| Diffuser: one pass per all-pass over the block | Mutable Instruments: block processing | current tree | -43 / -13 (noise) | -925 / -977 (slower) | not recorded | 0 | 28/28 | rejected |
| Diffuser: 32-bit float storage (Clouds' format) | Mutable Instruments: Clouds' `FORMAT_32_BIT` diffuser | tails-first diffuser | within noise | 602 / 449 more | not recorded | +4,096 B DTCM | 26/28 (differs) | rejected |
| Grain Hermite read on the stored integers, scaled once | Mutable Instruments: Clouds' `ReadHermite` interpolates the unscaled samples and multiplies by `scale` once (`clouds/dsp/audio_buffer.h:241-277`) | diffuser change | 6,775 / 7,140 (run 2: 6,840 / 7,130) | 7,089 / 7,301 (run 2: 6,924 / 7,307) | -504 B | 0 | 28/28 Seed3 hashes (audio and buffers); host tests, 154 fault checks | kept |
| Synth: two playing grains computed side by side, mixed in index order | Drizzlet: the in-order core overlaps two independent grains' sample and envelope chains; each grain's arithmetic is unchanged | current tree | unchanged (synth only) | unchanged (synth only) | +2,120 B | 0 | 14/14 synth and 28/28 granular Seed3 hashes; emulator long runs; host tests, `TestSynthGrainPairs` | kept |

**Template:** first paired peak savings are 2,669/2,828 (template peak
94,586). The second run in Build comparison peaks at 94,719, giving
2,536/2,722 against the first baseline. q1 stereo stress32 slows 794–808
median cycles; some q1/q2 cases regress. q3/Clouds-style set the peak.

**Local frame:** possible aliasing forced per-grain, per-sample output
loads/stores. A local register sum adds once with identical addition order.
GCC leaves `Render` out of line, costing flash. Off-peak dense/delay cases
slow up to 558 cycles; q2 mono stress32 by 1,023 (all ≤60,000 cycles).

**Unsigned modulo:** callers keep write-pointer-plus-delay nonnegative.
Unsigned `%` removes per-read sign correction. Combined changes improve
26/28 scenarios; worst off-peak loss 544 cycles.

**Freeze template.** Small gain, but it shifts GCC's inlining (q2 mono delay median 40,546 to 47,444) and changes the grain `Render` signature used at 17 host-test call sites.

**Diffuser:** removal saves 2,772/2,740 cycles; a scratch second copy costs
2,819/2,878. Cost is about 2,800 per 12-sample block (235/sample), of the
2,700–3,200 Clouds-style overhead. Each dependent tail read/conversion/multiply
stalled about 28 cycles after the previous store. Disjoint delay lines allow
all eight reads first; interleaved conversions/filter chains reduce this to
about 125 cycles/sample. Expressions/rounding and all 28 audio/buffer hashes
match. The engine now supplies only layout; unused `Start` LFO is removed
(-32 B RAM). Mutation check flips the all-pass output sign.

Per-stage block loops match but slow down without overlap/unrolling.
Float storage saves another 449–602 cycles but removes 16-bit rounding/clipping:
0.1-FS noise differs at -77 dBFS, above the -90 dBFS gate; full-scale clips
only in 16-bit. Two-sample unrolling was untried for style reasons.

**Integer Hermite:** interpolate `Item::raw`, then scale once, as Clouds'
`ReadHermite` does (`clouds/dsp/audio_buffer.h:241-277`). Stereo reads need
two multiplies instead of eight. µ-law and delay taps retain float reads.
Keep `c` first as `float(c) * 0.5f`: another order changes fused rounding and
10/28 hashes. Delay taps differ too. Host check: `TestHermiteRawMatchesScaled`.

**Synth pairs:** separate 14-scenario probe (not the 28 granular scenarios).
Saved median/peak budget points: worst synth (Clouds-style, reverb 0.7)
3.80/3.95; 32 grains 3.71/4.00; 32 frozen grains 3.35/3.25; stock synth
2.89/3.66; repeat within 0.15. Fixed pairs compute both samples, then both
envelopes, then mix in order; lone grains render alone. Emulator: 600,000
blocks across three qualities, FREEZE held/gated, 32 grains, fades/restarts,
and 20 fixtures match bitwise. Pairing arbitrary active grains saved only
0.3–0.6 points.

**ITCM.** It helped some delay cases but cost more than it saved in the saturated case. Hot state and the stack already sit in fast core-coupled memory.

**Result.** On the development board the stock worst block goes 97,255 (GCC 9 baseline) to 90,333 with the first three changes, and to 83,345 (69.5%) with the integer Hermite read; the worst of all 28 scenarios is 84,567 (70.5%), under the 90,000-cycle (75%) DSP-only target. On the module the whole callback is in the TL;DR table.

**Block-per-grain synth (rejected):** three bit-identical versions split
blocks at grain starts. Development-board heavy-synth median worsened from
83,099 to 84,332–87,093 cycles; retained block state gave no benefit.

**Single Hermite synth read (rejected):** crossfade neighbouring wave
samples before interpolation in `WavetableGrain::ReadSample`. Saves about six
float operations in exact arithmetic, but costs about two budget points on
Cortex-M7: two independent reads overlap; one combined chain cannot.
Audio differs by about -87 dBFS RMS at 16-bit, peaks -35 dBFS at 4-bit.

**Attack template (prototyped, not measured).** Templating the granular paths on the attack setting was prototyped but not kept: attacks already cost nothing when Off (worst block 84,434 against 84,643 without the feature).

**Untried ideas** (none fits the rules below): whole-block rendering per granular grain (changes the sound through feedback and per-sample triggers); delay versus granular as a per-block template (1.7k slower); the local-sum idea in the multitap delay (not the peak).

## Method

- **Seed3 probe.** The real `Processor::Process` at 480 MHz with caches on and memory placed as in the firmware (state and stack in DTCM, delay buffers in AXI and D2 SRAM, reverb in D3 SRAM). 16,000 warm blocks, then 4,096 measured; median is the upper middle. 28 scenarios: q0 to q3, stereo and mono, dense, stress32 and delay, Clouds-style and pitch-scale cases. Stress32 holds all 32 grains active at +4x speed. Peaks are finite-sample maxima, not proven bounds.
- **Timed.** Granular scheduling and rendering, feedback and filtering, wet/dry, reverb, output limiter. Not timed: setup, ADC and CV, UI, codec conversion, interrupts, DMA contention, flash saving, wavetable synth mode. The full callback on the module is higher ([TESTING.md](TESTING.md)).
- **Module calibration.** A calibration build adds DWT cycle counters around `Process` and the whole callback; it ran the same stress32 fixture and the panel settings in [TESTING.md](TESTING.md) on the module. The 90,000-cycle reserve target applies to the DSP-only Seed3 figures above, not to the whole callback.
- **Bit-identical checks.** Seed3 output and buffer hashes; 20 short and 13 long emulator scenarios (272,000 blocks in all) compared sample by sample; host tests with sanitizers. Counts are in [TESTING.md](TESTING.md).

## Rules for future changes

### From Mutable Instruments

From Mutable Instruments code and tech notes ([STM32 miscellania](https://pichenettes.github.io/mutable-instruments-documentation/tech_notes/stm32_miscellania/), "Timing and processes", the Beads history page).

1. Specialize on configuration with template parameters: Clouds' grain renderer on channel count, quality and interpolation, stmlib's filter on its type, Warps on FIR templates. It is not always faster, as the freeze template shows.
2. Look at the data format first. Mutable Instruments' own Beads gain was changing delay-line addresses from 47:16 to 21:10 fixed point, which avoids an int64-to-float conversion.
3. Keep loop state in registers across the block; watch for state reloaded every sample, aliasing through `this`, and calls inside the sample loop.
4. Judge the worst block, not the mean. Mutable Instruments toggle a pin around the block callback, design for 80 to 95% of the CPU, and treat 99% of the block as the ceiling. DSP-only, Beads is at about 75 to 81% here.

### Drizzlet's own rules

These acceptance gates are Drizzlet policy.

1. Follow [STYLE.md](STYLE.md): upstream master conventions win, Beads first; keep compatible Drizzlet policies marked `(Drizzlet)`.
2. Measure the gain on the worst case (q3 stereo stress32, plus Clouds-style). Report peak and median, flash and RAM, and what got slower.
3. Prefer bit-identical output; show it with Seed3 hashes, emulator audio and long runs.
4. Any difference is flagged with its largest error and whether it drifts. It is accepted only if the error stays strictly below -90 dBFS (0.0000316 FS) with no drift, and is documented here.
5. Reject changes that spend RAM or ITCM for a small gain, or that help one quality and hurt the one that sets the peak.
6. No compiler change without the same comparison: a faster compiler that changes the sound is not an optimization. The app uses GCC 9, where Mutable Instruments used 4.9.3; the bootloader stays on 4.9.3.

## Stereo width and delay mode

Measured with the Cortex-M7 emulator, counting executed Thumb instructions rather than hardware cycles (so not module headroom). Default audio stays byte-identical in all 20 scenarios (420,000 stereo frames).

- **Width:** conversion and CV clamping run once per callback; each new grain receives a normalized width and an enabled flag, and active grains keep their pan. DSP-only q3 stereo stress32 median / peak: 77,723 / 77,737 instructions; delay 51,548.5 / 51,680 (+11 and +88). A filter-only template increased both and was removed.
- **Delay mode:** Wide/smooth filter Q uses coefficients selected outside the sample loop. The ramp changes whole-callback counts by -34..+35, accepted to remove an audible click. In q3 stereo granular playback, median / peak goes 13,040 / 15,180 → 13,043 / 15,183; delay 53,319 / 53,505 → 53,323 / 53,509. The wide-detent and ramp-position checks add 1..13 instructions per granular, mono or delay callback; wavetable synth counts are unchanged. Of 8,000 whole-callback counts, 4,800 differ; compiler inlining and register allocation affect them. No compiler hints, padding or whole-loop specialization were kept.
- **Ramp:** Q coefficients approach their target once per block with a 0.005 one-pole coefficient and a rounded 16-bit position that reaches both endpoints exactly. The wet delay regression checks 100-, 512- and 4,096-block windows, including the completed transition; each maximum output step stays within its no-switch control's range.
