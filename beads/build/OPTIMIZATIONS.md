# Optimizations

What Drizzlet changed to run faster or smaller, what was measured and rejected, and the rules for future changes. Audio-path work is judged on two things: cycles saved on a measured worst case, and whether the sound changes.

## TL;DR

- **Compiler:** the app is built with GCC 9.3.1 instead of 4.9.3, after a sweep of GCC 4.9 to 15. GCC 8 was the runner-up (smaller image, less headroom); GCC 10 and later were slower and changed the sound. The bootloader stays on 4.9.3, bytes unchanged. One source change was needed to keep the sound identical (see Compiler).
- **Code changes in Beta 1:** three small changes, all bit-identical: mono/stereo chosen once per block (template), grain output summed in a local frame, and an unsigned modulo for the delay-line read index. Together they take the stock worst case (q3 stereo, 32 grains held active) from 97,255 to 90,333 cycles per block (75.3% of the 120,000 budget) and the worst block of all 28 scenarios to 92,486 (77.1%). Before them, GCC 9 alone was about 2% faster than 4.9.3 in the worst audio block and about 2% smaller.
- **Sound:** every change is bit-identical to the build before it: 20 of 20 emulated scenarios for GCC 9 against 4.9.3, and 28 of 28 Seed3 hashes, 20 of 20 emulator scenarios and 13 of 13 long runs (max error 0) for the code changes.
- **Rejected:** GCC 10.3 to 15.x (slower in the worst case, and the sound changes), ITCM code placement, an extra grain/delay template, and freeze state as a template parameter.
- **Caveat:** all timings are DSP-only, measured on a Daisy Seed3 (an STM32H750: the same die as the module's STM32H743, same Cortex-M7 at 480 MHz), not on the module. Whole-callback timing on the module is still to be calibrated.

## Build comparison

Budget is 120,000 cycles per block at 480 MHz. Percentages are of the 120,000-cycle budget. "Stock peak / median" is the worst stock case: q3 (48 kHz digital), stereo, 32 grains held active (stress32). "Clouds-style peak" is the worst scenario with the Clouds-style reverb, also q3 stereo stress32. "Sound" is audio compared with the GCC 9 baseline.

**Study A (2026-10-16 ladder):** current source, same probe and 28 scenarios on every row. Flash is the whole application binary.

| Build | App flash | Stock peak / median (% of budget) | Clouds-style peak | Sound vs GCC 9 | Verdict |
|---|---:|---|---:|---|---|
| GCC 9.3.1 baseline | 274,340 B | 97,255 / 95,610 (81.05%) | 100,009 | reference | kept |
| GCC 9.3.1 + mono/stereo template | 273,700 B | 94,719 / 93,111 (78.93%) | 97,287 | bit-identical (max error 0) | kept |
| GCC 9.3.1 + all three changes | 275,076 B | 90,333 / not recorded (75.28%) | not recorded | bit-identical (max error 0) | kept, ships in Beta 1 |
| GCC 10.3 default and `-fno-inline-functions` | not measured | not measured | not measured | not measured | not run: no verified archive checksum |
| GCC 11.3 default | 251,644 B | 100,634 / 98,974 (83.86%) | 103,118 | FLAGGED, 0.000296535 FS | rejected |
| GCC 11.3 `-fno-inline-functions` | 250,868 B | 100,634 / 98,974 (83.86%) | 103,118 | FLAGGED, 0.000296535 FS | rejected |
| GCC 12.3 default | 251,944 B | 99,675 / 98,147 (83.06%) | 102,586 | FLAGGED, 0.000296535 FS | rejected |
| GCC 12.3 `-fno-inline-functions` | 250,624 B | 99,675 / 98,147 (83.06%) | 102,586 | FLAGGED, 0.000296535 FS | rejected |
| GCC 13.3 default | 252,296 B | 99,516 / 98,011 (82.93%) | 102,243 | FLAGGED, 0.000296535 FS | rejected |
| GCC 13.3 `-fno-inline-functions` | 250,976 B | 99,605 / 98,002 (83.00%) | 102,351 | FLAGGED, 0.000296535 FS | rejected |
| GCC 14.3 default | 255,696 B | 100,369 / 98,826 (83.64%) | 102,830 | FLAGGED, 0.000296535 FS | rejected |
| GCC 14.3 `-fno-inline-functions` | 254,320 B | 100,369 / 98,826 (83.64%) | 102,830 | FLAGGED, 0.000296535 FS | rejected |
| GCC 15.3 default | 253,760 B | 100,899 / 99,455 (84.08%) | 103,331 | FLAGGED, 0.0282450 FS | rejected |
| GCC 15.3 `-fno-inline-functions` | 252,720 B | 100,899 / 99,455 (84.08%) | 103,331 | FLAGGED, 0.0282450 FS | rejected |
| GCC 4.9.3, current source | not measured | not measured | not measured | not measured | the 20 of 20 emulator comparison exists; no Seed3 timing |

The GCC 11 to 15 rows use the GCC 9 baseline source. The same compilers on the template source were no better: stock peak 100,069 to 101,526, Clouds-style peak 102,103 to 103,694, all FLAGGED with the same errors. The 28-scenario Seed3 hashes matched on every row except GCC 15.3, where 19 of 28 matched. The long-run errors above are the worst of 13 long scenarios; the -90 dBFS gate is 0.0000316 FS, so every flagged build is 9 to 890 times over it (-70.56 dBFS for 11 to 14, -30.98 dBFS for 15).

The "all three changes" row is the template row plus the two pass-2 changes below. Its flash is the template row plus 1,376 B. A repeat run of the template base in pass 2 gave a stock peak of 94,702, against 94,719 here.

**Study B (2026-10-11, GCC 4.9.3 vs 15.3):** an older source and a different probe (q3 stereo grains, not stress32), so its cycle counts must not be read against Study A.

| Build | App image | q3 stereo grains worst / median | Sound vs 4.9.3 | Verdict |
|---|---:|---|---|---|
| GCC 4.9.3 | 271,056 B | 98,782 / 93,326 (82.32%) | reference | previous build |
| GCC 15.3 default | 243,560 B (-27,496 B) | scenario worst 104,452 (+5.72%), highest-scenario median +6.79% | 12 of 20 short scenarios exact, 11 of 13 long exact (long max 0.0187689 FS) | rejected |
| GCC 15.3, FMA contraction off | 246,376 B | worst +3.28%, highest-scenario median +4.23% | 0 of 20 exact, 0 of 13 long exact (long peak error 0.875354916 FS) | rejected |

Study B's "worst 104,452" is the worst across all scenarios, so it is not the same cell as the q3 stereo row in the first line.

**Study C (2026-10-12 sweep, released source, GCC 4.9 to 15):** a third probe, run on the unmodified released source and compared with GCC 4.9.3, so its cycles are not comparable with Studies A or B. Image size, median and worst block are relative to 4.9.3 (image 268,384 B, median 96,826, worst 101,804 cycles, headroom 15.2% of 120,000). Output is the emulator's 20 scenarios against 4.9.3, without the pan pin.

| GCC (Arm release) | Image size | Median block | Worst block | Headroom | Stack | Output vs 4.9.3 | Verdict |
|---|---:|---:|---:|---:|---:|---|---|
| 4.9.3 (stock) | 268,384 B | 96,826 | 101,804 | 15.2% | 3,988 B | reference | previous build |
| 5 | -3.5% | +1.3% | +1.2% | 14.1% | 4,076 B | 20/20 exact | not chosen |
| 6 | -4.6% | +0.5% | +0.5% | 14.7% | 4,052 B | 20/20 exact | not chosen |
| 7 | -6.6% | +0.7% | +0.1% | 15.1% | 3,936 B | 16/20, max 1 float ulp | not chosen |
| 8 (8-2019-q3, 8.3.1) | -6.7% | -1.2% | -1.7% | 16.6% | 3,800 B | 16/20, max 1 float ulp | runner-up |
| 9 | -2.3% | -2.2% | -2.3% | 17.1% | 3,664 B | 16/20, max 1 float ulp (20/20 with the pin) | chosen |
| 10 to 13 | -10 to -11.5% | +0.2 to +1.5% | +0.4 to +1.6% | 13.8 to 14.8% | about 3,750 B | 8/20, max 1.4e-5 | rejected |
| 14 to 15 | -10.2 to -10.4% | +0.9 to +1.3% | +0.5 to +0.9% | 14.4 to 14.7% | about 3,690 B | 12/20, max 1.4e-5 | rejected |

Seed3 re-measure of the same source (2026-10-14, 6,000 blocks x 20 runs), with the pan pin ("builtin") unless noted. Worst / median, as a share of the 120,000-cycle budget:

| Build | Worst | Median | Worst % | Median % |
|---|---:|---:|---:|---:|
| GCC 4.9.3, unpinned source | 101,773 | 96,818 | 84.8% | 80.7% |
| GCC 4.9.3, pinned | 102,020 | 97,023 | 85.0% | 80.9% |
| GCC 8.3.1, pinned | 99,957 | 95,533 | 83.3% | 79.6% |
| GCC 9.3.1, pinned | 99,430 | 94,672 | 82.9% | 78.9% |

Sound for GCC 8 with the pin: not measured. Only GCC 9 was confirmed bit-exact with it.

## Compiler

**Why GCC 9, not GCC 8.** In the sweep only GCC 8 and GCC 9 beat 4.9.3 in both median and worst block. GCC 8 gave the smaller image (-6.7%, against -2.3% for GCC 9) but less headroom: on the Seed3 re-measure its worst block is 99,957 cycles (83.3%) and median 95,533 (79.6%), against 99,430 (82.9%) and 94,672 (78.9%) for GCC 9. Headroom decides whether the audio callback survives its worst block, and flash is not scarce, so headroom won. GCC 9 was also the one confirmed bit-exact with the pin.

**Why GCC 9.** The released source was written for GCC 4.9.3. GCC 9 gives a smaller binary and slightly faster worst blocks on this code, and its output stays exactly the same once one expression is pinned (below). The app is built with GCC 9.3.1 (Arm GNU Embedded 9-2020-q2); the bootloader keeps GCC 4.9.3 so its bytes do not change. The build kit still builds the released app with 4.9.3 as a fixed reference. Both compiler licences ship with the release.

**What the source needed.**
- Without the pin, GCC 7, 8 and 9 differed from 4.9.3 by at most 1 float ulp in 4 of 20 emulator scenarios, traced to this one line. The random pan spread `0.5 + (u - 0.5) * min(spread * v, 1)` in `randomized_parameter.h` is now one explicit fused multiply-add (`MulAdd`, `__builtin_fmaf`; `fmaf` in host tests). Without it the compilers may or may not fuse the multiply and add, which changes the last bit of the pan. Pinning it makes the rounding the same for every compiler. The sources do not say which compiler fused it first.
- Build flags: `-std=gnu++98`, as for the released source. An uninitialized-variable warning exception was needed at first; it was dropped after the undefined reads in the dummy CPU-load grains and past the last wavetable were fixed (output unchanged).

**Evidence.** All 44 emulated scenarios were bit-identical between GCC 9 and 4.9.3 when the change landed; the later 20-scenario suite is also 20 of 20. The emulator runs the real linked ARM code with fused multiply-add and flush-to-zero modelled.

**GCC 10 and later, measured and rejected.**
- Slower where it matters: stock peak is 99,516 to 100,899 cycles against 97,255 for GCC 9, a rise of 2.3% to 3.7%. Flash is 252,000 to 256,000 bytes against 274,340, but the worst case has less headroom, not more.
- Sound changes: 11 of 13 long scenarios are exact, the rest differ by up to 0.000296535 FS (11 to 14) or 0.0282450 FS (15). Short suites alone missed this.
- Cause not identified. GCC 10 and later enable `-finline-functions` at `-O2` and place FMA contraction differently, but `-fno-inline-functions` changed the image size and not one timed DSP cycle on any release tested, so the inlining default is not the cause. Study B also shows that switching contraction off gave a larger sound difference, not a smaller one. In the saturated digital core the newer compilers emit separate per-sample renderer calls where GCC 9 keeps one large body; executed instructions rise 1.60% (20,438,458 to 20,765,808 over 256 blocks) and stack-memory instructions rise 84.42% (791 more per block). No single compiler pass was isolated.

**Bootloader.** It stays on GCC 4.9.3 and is byte-identical to the build Mutable Instruments released. It runs only at power-up and during updates, WAV updates never replace it, and a compiler change would only add risk to the recovery path.

## Code changes

Cycles are for the Seed3 worst stock case and the worst Clouds-style case; "saved" is against the row named, otherwise the GCC 9 baseline. All kept changes below are on the Beta 1 branch and ship in Beta 1. RAM is static RAM.

| Change | Saved, peak / median (stock) | Saved, peak / median (Clouds-style) | Flash | RAM | Sound | Verdict |
|---|---|---|---:|---|---|---|
| GCC 9 instead of 4.9.3 | about 2% in the worst audio block (earlier sweep) | not measured | about 2% smaller | not measured | bit-identical, 20 of 20 | kept |
| Mono/stereo chosen once per block (19-line template) | 2,669 / 2,499 | 2,828 / 2,756 | -640 B | 0 | bit-identical: 28 of 28 Seed3, 20 of 20 emulator, 13 of 13 long, error 0 | kept |
| Sum delay grains in a local frame (pass 2, against the template build) | 3,725 / 3,630 (run 2: 3,732 / 3,826) | 3,904 / 3,805 (run 2: 3,907 / 3,788) | +1,648 B | 0 | bit-identical: 28 of 28 Seed3, 20 of 20 emulator, 13 of 13 long, error 0 | kept |
| Use an unsigned modulo for the delay line read index (pass 2, against the previous row) | 556 / 739 (run 2: 550 / 636) | 626 / 832 (run 2: 628 / 871) | -272 B | 0 | bit-identical (gates run on the combined tree) | kept |
| Freeze state as a grain-renderer template parameter (pass 2) | 728 / 769 | 378 / 343 | not built | not measured | Seed3 28 of 28 hashes match | rejected |
| All sample loops in ITCM | -765 / -770 | -270 / -377 | +336 B | +21,440 B ITCM | Seed3 28 of 28 hashes match | rejected |
| ITCM on top of the template | -611 / -505 (vs template) | -213 / -277 | +360 B | +23,448 B ITCM | Seed3 28 of 28 hashes match | rejected |
| Extra grain/delay template | -1,693 / -1,812 | -1,406 / -1,627 | -336 B | 0 | Seed3 28 of 28 hashes match | rejected |

Negative savings are slowdowns. The template change is not a gain everywhere: q1 stereo stress32 is slower by 794 to 808 cycles at median, and some q1 and q2 cases regress. It improves the q3 and Clouds-style cases that set the peak. Two repeat runs gave stock baseline 95,610 / 97,255 and 95,610 / 97,250, and template 93,111 / 94,586 and 93,102 / 94,719; the table uses the 94,719 peak, the commit message the first pair (94,586, saving 2,669). Sources disagree on one number: the stock peak saved is 2,531 to 2,669 depending on which template run is compared. ITCM helped some delay cases (q3 stereo delay 60,276 / 61,797 to 59,868 / 60,496) but cost more than it saved in the saturated case. Hot state and the stack already sit in fast core-coupled memory.

After the template change alone the stock peak was 94,719 (78.93% of the budget) and Clouds-style 97,287 (81.07%). With all three changes the worst block of all 28 scenarios is 92,486 (77.1%) and the stock peak 90,333 (75.3%), just above the 90,000-cycle (75%) DSP-only target for a 25% reserve.

For context, the earlier CPU gate (a different base commit and harness) measured the stock peak at 98,314 (81.93%) and the diffuser plus Clouds-style reverb at 101,895 (84.91%). Its flash numbers (267,136 B base) differ from Study A's 274,340 B because the sources differ.

## Second pass

Two further changes, measured on top of the template build (GCC 9.3.1 on Seed3, 16,000 warm and 4,096 measured blocks per scenario, each pair run twice). Together they save 4,281 cycles at median and 4,369 at peak on q3 stereo stress32, and 4,530 / 4,637 on Clouds-style stress32.

- **Sum delay grains in a local frame (kept).** The left and right output values were loaded and stored once per grain per sample, because stores to grain state may alias them. A local sum stays in registers and is added once; the output is zeroed before rendering, so the additions and their order are unchanged. Costs 1,648 B of flash because GCC keeps the grain player's `Render` out of line, and some off-peak cases slow down (dense and delay up to 558 at median, q2 mono stress32 by 1,023), all at or below 60,000 cycles.
- **Use an unsigned modulo for the delay line read index (kept).** The read index is the write pointer plus a delay and every caller keeps it non-negative, so a signed `%` by a constant only adds a sign correction, run four times per grain per sample. `LoadQuad` already uses the unsigned form. Saves 272 B. With both changes 26 of 28 scenarios get faster and the worst off-peak loss is 544 cycles.
- **Freeze state as a template parameter (rejected).** It saves only 343 cycles on Clouds-style and 769 on stock, shifts GCC's inlining (q2 mono delay median 40,546 to 47,444, +6,898; q1 mono delay +4,784; q0 stereo delay +1,076) and changes the grain `Render` signature used at 17 host-test call sites.

Result against the GCC 9 build with the template change: worst block of all 28 scenarios 97,123 to 92,486 (77.1%); stock stress32 peak 94,702 to 90,333 (75.3%). Against the GCC 9 baseline before any change, the stock peak is 97,255 to 90,333.

Gates for the pair: 28 of 28 Seed3 output and buffer hashes, 20 of 20 emulator scenarios, 13 of 13 long runs (272,000 blocks, hashes equal to the base, so max error 0), and all 13 host test targets. The unsigned modulo alone was not run through the emulator and long suites; its gates come from the combined tree.

Checked and clean: no 64-bit arithmetic or `__aeabi` conversion calls in the stereo grain renderer (the only multiply-long is the modulo by a constant; the fraction uses a fixed-point convert). The grain loop is about 165 instructions per grain per sample.

Not done, because none fits the rules below: Hermite interpolation on unscaled integers, scaling once afterwards (the 2^-15 scale is exact, so bit-identical; removes 6 of 8 multiplies per read, estimated 1.5k to 2k cycles, not measured, needs a raw-value accessor on six `Item` specializations); whole-block rendering per grain (changes the sound through feedback and per-sample triggers); delay versus granular as a per-block template (1.7k slower); the local-sum idea in the multitap delay (not the peak).

## Method

- **Seed3 probe.** A Daisy Seed3 (same die as the module's STM32H750 class part) runs the real `Processor::Process` at 480 MHz with caches on and memory placed as in the firmware: state and stack in DTCM, delay buffers in AXI and D2 SRAM, reverb in D3 SRAM. Each scenario runs 16,000 warm blocks, then 4,096 measured blocks; median is the upper middle, p99.9 the nearest rank. Scenarios: q0 to q3, stereo and mono, dense, stress32 and delay, plus Clouds-style and pitch-scale cases (28 in Study A). Stress32 holds all 32 grains active at +4x speed every block. Peaks are finite-sample maxima, not proven bounds.
- **What is timed.** Granular scheduling and rendering, feedback and filtering, wet/dry, reverb, and the output limiter. Not timed: setup, ADC and CV, UI, codec conversion, interrupts, DMA contention, saving to flash, synth mode. Percentages are DSP-only; the full callback on the module will be higher.
- **Module calibration build (planned).** A build with DWT cycle counters around `Process` and around the whole callback, running the same stress32 fixture on the module. Ratio of module to Seed3 median calibrates the Seed3 figures; the module must stay at or below 90,000 cycles total to keep a 25% reserve. Not yet run.
- **Emulator scenarios.** The real linked ARM code runs in an M7 emulator over 20 short scenarios (q0 to q3 each with grains, mono, delay, synth, self-play) and 13 long ones (up to 48,000 blocks, with UI events and codec transitions, 272,000 blocks in all), comparing raw audio sample by sample.
- **Bit-identical checks.** Seed3 output and buffer hashes (28 scenarios), emulator audio, long-run drift, and the host test suite with sanitizers. Counts are in [TESTING.md](TESTING.md).

## Rules for future changes

1. Follow Mutable Instruments style ([STYLE.md](STYLE.md)): their idioms first, no clever constructs that need a comment to justify.
2. Measure the gain on the worst case (q3 stereo stress32, plus Clouds-style) before proposing. Report peak and median, flash and RAM, and what got slower.
3. Prefer bit-identical output. Show it with Seed3 hashes, emulator audio and long runs.
4. Any difference, however small, is flagged and reported with its largest error and whether it drifts. A flagged change is accepted only if the error stays strictly below -90 dBFS (0.0000316 FS) with no drift, and is documented here.
5. Reject changes that spend RAM or ITCM for a small gain, or that help one quality and hurt the one that sets the peak.
6. No compiler change without the same comparison: a faster compiler that changes the sound is not an optimization.
7. Look at the data format before the instructions. Mutable Instruments' own Beads gain came from changing delay-line addresses from 47:16 to 21:10 fixed point, which avoids an int64-to-float conversion; hunt for 64-bit arithmetic and integer-to-float conversions on the audio path first.
8. Keep loop state in registers across the block (Mutable Instruments' "Timing and processes" note). Look for state reloaded every sample, aliasing through `this`, and calls inside the sample loop.
9. Judge the worst block, not the mean: per-sample cost spikes are why work is done per block, and the deadline is on the whole block. Mutable Instruments measure it by toggling a pin around the block callback and treat 99% of the block as the ceiling.

## Precedents

- Specializing on configuration with template parameters is Mutable Instruments' own approach, described in their "STM32 miscellania" tech note (https://pichenettes.github.io/mutable-instruments-documentation/tech_notes/stm32_miscellania/): Clouds' grain renderer is specialized on channel count, quality and interpolation; stmlib's filter takes its type as a template parameter; Warps uses FIR templates. The mono/stereo template follows this. It is not always faster: see the rejected freeze-state template.
- Mutable Instruments design modules to use 80 to 95% of the CPU, with 99% of the block as the hard ceiling. DSP-only, Beads sits at about 75 to 81% here.
