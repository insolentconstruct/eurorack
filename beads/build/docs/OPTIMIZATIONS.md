# Optimizations

What Drizzlet changed to run faster or smaller, what was measured and rejected, and the rules for future changes. Audio-path work is judged on cycles saved on a measured worst case, and on whether the sound changes.

Mutable Instruments' April 2021 update had the same goal: it fixed a CPU overload (crackles on the left channel in 48 kHz stereo) "through several code optimizations having no negative impact on the module’s functionality" ([release notes](https://forum.mutable-instruments.net/t/beads-firmware-update-release-candidate/18572)).

## TL;DR

On the module, with the panel settings of the CPU tests, the busiest stock block now takes 94,311 of the 120,000 cycles available (78.6%). The vanilla source takes 110,947 (92.5%) on the same test. The sound is unchanged.

| Module, whole audio callback, worst block | Stock, panel | Stock, stress test | Clouds-style, panel |
|---|---:|---:|---:|
| Vanilla source (GCC 4.9.3) | 110,947 (92.5%) | 114,053 (95.0%) | – |
| Drizzlet before the integer Hermite read | 102,433 (85.4%) | 106,014 (88.3%) | 103,665 (86.4%) |
| Drizzlet with it | 94,311 (78.6%) | 98,802 (82.3%) | 96,022 (80.0%) |

Headroom gained per change, in points of the budget on the busiest stock block (development board, DSP only, except the Hermite row and the total, which are the whole callback on the module with the stock panel settings):

| Change | Headroom |
|---|---:|
| GCC 9.3.1 instead of 4.9.3 (no code changes) | about 2.0 |
| Mono/stereo chosen once per block | 2.2 |
| Grains summed in a local frame | 3.0 |
| Unsigned modulo for the delay-line read | 0.6 |
| Grains interpolate the stored integers (integer Hermite) | 6.8 |
| Total, module | 13.9 |

The Clouds-style reverb also gains 1.0 point from its diffuser reading its tails first. Plain GCC 11 to 16 builds are 5,800 to 9,300 cycles slower than GCC 9 on the stock peak (GCC 10.3, from an unverified archive, 10,200); raising the inlining limit brings GCC 11 within noise of GCC 9. One explicit fused multiply-add and a few explicit roundings make every compiler's audio and buffer hashes match GCC 9 in all 28 scenarios. The bootloader stays on 4.9.3.

The same changes on the vanilla source alone ([beads-tuneup](https://github.com/insolentconstruct/eurorack/tree/beads-tuneup), no Drizzlet features): development board, busiest stock block, 24 scenarios, all hashes matched:

| Build | Cycles | Budget |
|---|---:|---:|
| Vanilla source (beads-build-kit, GCC 4.9.3) | 99,511 | 82.9% |
| beads-tuneup (GCC 9 and the first three code changes) | 90,529 | 75.4% |
| beads-tuneup with the integer Hermite read (not yet on that branch) | 82,911 | 69.1% |

- **Sound:** GCC 9 matched 4.9.3 in 20 of 20 emulator scenarios. Each code change matched bit for bit on the development board (28 of 28 hashes) and in the host tests; the emulator agreed wherever it was run (integer Hermite: 70 of 70 comparisons).
- **Rejected:** newer compilers (slower as plain builds, no clear gain with tuning), ITCM code placement, an extra grain/delay template, freeze state as a template parameter.

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

GCC 11 to 15 were built from the GCC 9 baseline source; on the template source they were no better (stock peak 100,069 to 101,526). `-fno-inline-functions` changed sound and timings by under 0.1%. The "all three changes" flash is the template build plus 1,376 B. Sound errors are the worst of 13 long scenarios; the -90 dBFS gate is 0.0000316 FS, so every differing build is 9 to 890 times over it. Seed3 hashes matched on every row except GCC 15.3 (19 of 28).

**Earlier sweep (vanilla source, GCC 4.9 to 15, emulator).** Relative to GCC 4.9.3 (image 268,384 B, median 96,826, worst 101,804 cycles):

| GCC | Image | Median | Worst | Output vs 4.9.3 | Verdict |
|---|---:|---:|---:|---|---|
| 4.9.3 | 268,384 B | 96,826 | 101,804 | reference | previous build |
| 5, 6 | -3.5, -4.6% | +1.3, +0.5% | +1.2, +0.5% | 20/20 exact | not chosen |
| 7 | -6.6% | +0.7% | +0.1% | 16/20, max 1 float ulp | not chosen |
| 8 | -6.7% | -1.2% | -1.7% | 16/20, max 1 float ulp | runner-up |
| 9 | -2.3% | -2.2% | -2.3% | 16/20, max 1 ulp (20/20 with the pan pin) | chosen |
| 10 to 15 | -10 to -11.5% | +0.2 to +1.5% | +0.4 to +1.6% | 8/20 or 12/20, max 1.4e-5 | rejected |

Seed3 re-measure of the same source, worst / median: GCC 4.9.3 101,773 / 96,818; GCC 8.3.1 99,957 / 95,533; GCC 9.3.1 99,430 / 94,672. An older source on a different probe (GCC 4.9.3 vs 15.3) showed the same pattern: 15.3 was 5.7% slower in the worst scenario and 12 of 20 short scenarios were exact.

## Compiler

- **Why GCC 9, not 8.** Only GCC 8 and 9 beat 4.9.3 in both median and worst block. GCC 8 gave the smaller image (-6.7% against -2.3%) but less headroom (worst 83.3% against 82.9%); headroom decides whether the callback survives its worst block, and flash is not scarce. Only GCC 9 was confirmed bit-exact with the pan pin.
- **Why GCC 10 and later are slower** (emulator instruction counts, not module cycles). GCC 10 enabled `-finline-functions` at `-O2` and lowered `max-inline-insns-single` from 200 to 70. The Hermite delay-line read is no longer inlined into the grain loop: 384 extra calls per block in the stress case, each saving registers on the stack. With `--param max-inline-insns-single=140`, GCC 10 runs slightly fewer instructions than GCC 9 for about 12 KB more flash; lowering the limit on GCC 9 reproduces the slowdown. Instructions per block against GCC 9: GCC 10 +7.9%, 12 +4.4%, 14 +0.9%, 16 +1.5% (vanilla source +6.0%, +2.2%, -0.7%, -0.4%). Marking the read `always_inline`, as stmlib does for its interpolation helpers, or the parameter above would allow a newer compiler, but the newer compilers' small sound differences are a separate effect, so releases stay on GCC 9 for now.
- **Build.** GCC 9.3.1 (Arm GNU Embedded 9-2020-q2), `-std=gnu++98` as for the vanilla source. The build kit still builds the vanilla app with 4.9.3 as a fixed reference. Both compiler licences ship with the release.
- **Source change.** The random pan spread `0.5 + (u - 0.5) * min(spread * v, 1)` in `randomized_parameter.h` is one explicit fused multiply-add (`MulAdd`, standard `fmaf`, which GCC compiles to one instruction). Without it GCC 7 to 9 differed from 4.9.3 by at most 1 float ulp in 4 of 20 scenarios, because compilers may or may not fuse the multiply and add.
- **Evidence.** GCC 9 matched 4.9.3 in 20 of 20 emulator scenarios. The emulator runs the real linked ARM code with fused multiply-add and flush-to-zero modelled.
- **GCC 10 and later: rejected.** Stock peak is 99,516 to 100,899 cycles against 97,255 (+2.3% to +3.7%). Of 13 long scenarios 11 are exact, the rest differ by up to 0.000296535 FS (11 to 14) or 0.0282450 FS (15); short suites missed this. In the saturated digital core the newer compilers emit separate per-sample renderer calls where GCC 9 keeps one body (executed instructions +1.60%, stack-memory instructions +84.42%), because of the inlining limit above. Switching contraction off made the sound difference larger.
- **Bootloader.** It stays on GCC 4.9.3 and is byte-identical to Mutable Instruments' build. It runs only at power-up and during updates, WAV updates never replace it, and a compiler change would only add risk to the recovery path.

## Code changes

Cycles saved on the Seed3 worst stock and worst Clouds-style cases, peak / median, against the build in the "Against" column. Run 1 is the figure used; run 2 is a repeat. Negative is a slowdown.

| Change | Based on | Against | Stock | Clouds-style | Flash | RAM | Sound | Verdict |
|---|---|---|---|---|---:|---|---|---|
| GCC 9 instead of 4.9.3 | Drizzlet: compiler choice; a `MulAdd` helper pins one pan line so the arithmetic stays Mutable Instruments' | GCC 4.9.3 | about 2% in the worst block | not measured | about -2% | n/a | 20/20 emulator | kept |
| Mono/stereo chosen once per block (19-line template) | Mutable Instruments: template specialization on configuration (STM32 miscellania; Clouds' grain renderer) | GCC 9 | 2,669 / 2,499 | 2,828 / 2,756 | -640 B | 0 | 28/28 Seed3, 20/20 emulator, 13/13 long, error 0 | kept |
| Sum delay grains in a local frame | Mutable Instruments: keep loop state in locals across the block (Timing and processes) | template | 3,630 / 3,725 (run 2: 3,826 / 3,732) | 3,805 / 3,904 (run 2: 3,788 / 3,907) | +1,648 B | 0 | same gates | kept |
| Unsigned modulo for the delay-line read index | Mutable Instruments: the same file's `LoadQuad` already uses `size_t(i) % items_per_bank()` | local frame | 739 / 556 (run 2: 636 / 550) | 832 / 626 (run 2: 871 / 628) | -272 B | 0 | gates run on the combined tree | kept |
| Both pass-2 changes together | the two rows above | template | 4,369 / 4,281 | 4,637 / 4,530 | +1,376 B | 0 | 28/28, 20/20, 13/13, error 0 | kept |
| All sample loops in ITCM | Mutable Instruments: stmlib's `IN_RAM` placement | GCC 9 | -765 / -770 | -270 / -377 | +336 B | +21,440 B ITCM | 28/28 Seed3 | rejected |
| ITCM on top of the template | Mutable Instruments: stmlib's `IN_RAM` placement | template | -611 / -505 | -213 / -277 | +360 B | +23,448 B ITCM | 28/28 Seed3 | rejected |
| Extra grain/delay template | Mutable Instruments: template specialization | GCC 9 | -1,693 / -1,812 | -1,406 / -1,627 | -336 B | 0 | 28/28 Seed3 | rejected |
| Freeze state as a template parameter | Mutable Instruments: template specialization | both pass-2 changes | 769 / 728 | 343 / 378 | not built | n/a | 28/28 Seed3 | rejected |
| Diffuser: all delay-line tails read before the eight all-passes | Drizzlet: the compiler cannot move a load above an earlier store that may alias, so the in-order core waited on every load, conversion and multiply; Clouds' diffuser shape and constants unchanged | current tree | within noise (about 200) | 1,234 / 1,257 (run 2: 1,244 / 1,254) | -116 B | -32 B | 28/28 Seed3 hashes; host tests 6/6, Clouds goldens unchanged; emulator not run | kept |
| Diffuser: constant amount and `Load` instead of `Read` | Mutable Instruments: keep loop state in locals; Clouds' reverb uses `Load` | current tree | within noise | 4 / -3 (noise) | not recorded | 0 | 28/28 | rejected: no gain |
| Diffuser: one pass per all-pass over the block | Mutable Instruments: block processing | current tree | -43 / -13 (noise) | -925 / -977 (slower) | not recorded | 0 | 28/28 | rejected |
| Diffuser: 32-bit float storage (Clouds' format) | Mutable Instruments: Clouds' `FORMAT_32_BIT` diffuser | tails-first diffuser | within noise | 602 / 449 more | not recorded | +4,096 B DTCM | 26/28 (differs) | rejected |
| Grain Hermite read on the stored integers, scaled once | Mutable Instruments: Clouds' `ReadHermite` interpolates the unscaled samples and multiplies by `scale` once (`clouds/dsp/audio_buffer.h:241-277`) | diffuser change | 6,775 / 7,140 (run 2: 6,840 / 7,130) | 7,089 / 7,301 (run 2: 6,924 / 7,307) | -504 B | 0 | 28/28 Seed3 hashes (audio and buffers); host tests, 154 fault checks | kept |

**Template.** The 2,669 / 2,828 peak savings come from the first paired baseline and template run (94,586 template peak); the Build comparison row shows the second template run (94,719), which gives 2,536 / 2,722 against the first baseline. It is not a gain everywhere: q1 stereo stress32 is 794 to 808 cycles slower at median, and some q1 and q2 cases regress. It improves the q3 and Clouds-style cases that set the peak.

**Local frame.** The output values were loaded and stored once per grain per sample because stores to grain state may alias them; a local sum stays in registers and is added once. The additions and their order are unchanged. GCC keeps the grain player's `Render` out of line (hence the flash cost) and some off-peak cases slow down (dense and delay up to 558, q2 mono stress32 by 1,023, all at or below 60,000 cycles).

**Unsigned modulo.** The read index is the write pointer plus a delay and every caller keeps it non-negative, so a signed `%` by a constant only adds a sign correction, run four times per grain per sample. With both changes 26 of 28 scenarios get faster and the worst off-peak loss is 544.

**Freeze template.** Small gain, but it shifts GCC's inlining (q2 mono delay median 40,546 to 47,444) and changes the grain `Render` signature used at 17 host-test call sites.

**Diffuser.** Removing the diffuser entirely saves 2,772 / 2,740 on the stress32 Clouds-style block, and running a second copy on a scratch buffer (same output) costs 2,819 / 2,878, so it takes about 2,800 cycles per 12-sample block (about 235 per sample) of the 2,700 to 3,200 Clouds-style adds over stock. The loop was latency bound: each all-pass read its tail, converted it and multiplied it after the previous all-pass had stored its head, and the compiler cannot hoist a load above a store that may alias, so every stage stalled the in-order core about 28 cycles. The eight reads of a sample never depend on its writes (the lines are disjoint), so they now come first; the compiler interleaves their conversions with the left and right filter chains and the loop drops to about 125 cycles per sample. Arithmetic and rounding are the same expressions (the Seed3 audio and buffer hashes match in all 28 scenarios), the engine is used for the delay line layout only, and `Start` no longer runs the unused LFO (-32 B RAM). The `mutation_check.py` diffuser case now mutates the all-pass output sign. Per-stage block loops are bit-identical but slower: an in-order core gets no overlap between loop iterations without unrolling. Float storage saves a further 449 to 602 cycles but drops the 16-bit rounding and clipping: in a host comparison with noise at 0.1 full scale the difference is -77 dBFS (above the -90 dBFS gate), and full-scale input clips in 16 bit but not in float, so it is not an optimization in the sense above. A two-samples-per-iteration unroll was not tried; it would not read like Mutable Instruments code.

**Integer Hermite.** Grain reads converted four stored samples to float and scaled each one before the Hermite polynomial. Like Clouds' `ReadHermite` (`clouds/dsp/audio_buffer.h:241-277`), they now interpolate the stored integers (`Item::raw`) and scale the result once: two multiplies per stereo read instead of eight. 8-bit µ-law lines and the delay taps keep the float read. Identical sound depends on keeping each rounding step in place: `c` stays `float(c) * 0.5f`, computed first, otherwise GCC fuses a different multiply-add and 10 of 28 hashes change (the delay taps differed the same way, so they keep the old read). `TestHermiteRawMatchesScaled` checks it on the host.

**ITCM.** It helped some delay cases but cost more than it saved in the saturated case. Hot state and the stack already sit in fast core-coupled memory.

**Result.** On the development board the stock worst block goes 97,255 (GCC 9 baseline) to 90,333 with the first three changes, and to 83,345 (69.5%) with the integer Hermite read; the worst of all 28 scenarios is 84,567 (70.5%), under the 90,000-cycle (75%) DSP-only target. On the module the whole callback is in the TL;DR table.

**Untried ideas** (none fits the rules below): whole-block rendering per grain (changes the sound through feedback and per-sample triggers); delay versus granular as a per-block template (1.7k slower); the local-sum idea in the multitap delay (not the peak).

## Method

- **Seed3 probe.** The real `Processor::Process` at 480 MHz with caches on and memory placed as in the firmware (state and stack in DTCM, delay buffers in AXI and D2 SRAM, reverb in D3 SRAM). 16,000 warm blocks, then 4,096 measured; median is the upper middle. 28 scenarios: q0 to q3, stereo and mono, dense, stress32 and delay, Clouds-style and pitch-scale cases. Stress32 holds all 32 grains active at +4x speed. Peaks are finite-sample maxima, not proven bounds.
- **Timed.** Granular scheduling and rendering, feedback and filtering, wet/dry, reverb, output limiter. Not timed: setup, ADC and CV, UI, codec conversion, interrupts, DMA contention, flash saving, wavetable synth mode. The full callback on the module is higher ([TESTING.md](TESTING.md)).
- **Module calibration.** A calibration build adds DWT cycle counters around `Process` and the whole callback; it ran the same stress32 fixture and the panel settings in [TESTING.md](TESTING.md) on the module. The 90,000-cycle reserve target applies to the DSP-only Seed3 figures above, not to the whole callback.
- **Bit-identical checks.** Seed3 output and buffer hashes; 20 short and 13 long emulator scenarios (272,000 blocks in all) compared sample by sample; host tests with sanitizers. Counts are in [TESTING.md](TESTING.md).

## Rules for future changes

### From Mutable Instruments

These come from Mutable Instruments' code and tech notes ([STM32 miscellania](https://pichenettes.github.io/mutable-instruments-documentation/tech_notes/stm32_miscellania/), "Timing and processes", the Beads history page).

1. Specialize on configuration with template parameters: Clouds' grain renderer on channel count, quality and interpolation, stmlib's filter on its type, Warps on FIR templates. It is not always faster, as the freeze template shows.
2. Look at the data format first. Mutable Instruments' own Beads gain was changing delay-line addresses from 47:16 to 21:10 fixed point, which avoids an int64-to-float conversion.
3. Keep loop state in registers across the block; watch for state reloaded every sample, aliasing through `this`, and calls inside the sample loop.
4. Judge the worst block, not the mean. Mutable Instruments toggle a pin around the block callback, design for 80 to 95% of the CPU, and treat 99% of the block as the ceiling. DSP-only, Beads is at about 75 to 81% here.

### Drizzlet's own rules

Mutable Instruments' notes describe no sound-identity checks and none of the acceptance gates below; these are Drizzlet's, and stricter.

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
