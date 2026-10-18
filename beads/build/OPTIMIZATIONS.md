# Optimizations

What Drizzlet changed to run faster or smaller, what was measured and rejected, and the rules for future changes. Audio-path work is judged on cycles saved on a measured worst case, and on whether the sound changes.

## TL;DR

- **Compiler:** the app is built with GCC 9.3.1 instead of 4.9.3, after a sweep of GCC 4.9 to 15. GCC 8 was the runner-up (smaller image, less headroom); GCC 10 and later were slower and changed the sound. The bootloader stays on 4.9.3.
- **Code:** three small bit-identical changes ship in Beta 1: mono/stereo chosen once per block (template), grain output summed in a local frame, and an unsigned modulo for the delay-line read index. The stock worst case (q3 stereo, 32 grains held active) goes from 97,255 to 90,333 cycles per block (75.3% of the 120,000 budget); the worst block of all 28 scenarios is 92,486 (77.1%).
- **Sound:** GCC 9 matched 4.9.3 in 20 of 20 emulated scenarios. The code changes matched in 28 of 28 Seed3 hashes, 20 of 20 emulator scenarios and 13 of 13 long runs (max error 0).
- **Rejected:** GCC 10.3 to 15.x, ITCM code placement, an extra grain/delay template, freeze state as a template parameter.
- **Caveat:** timings are DSP-only, measured on a separate development board (the same processor die and clock rate as the module), not on the module. First module figure: the whole delay-mode callback takes 72,088–72,472 cycles (60%), with no dropouts; the heaviest grain case on the module is still to be measured.

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

GCC 11 to 15 were built from the GCC 9 baseline source; the same compilers on the template source were no better (stock peak 100,069 to 101,526). `-fno-inline-functions` gave the same sound and timings within 0.1% (GCC 13: stock peak 99,605, median 98,002, Clouds-style 102,351; GCC 11, 12, 14, 15 unchanged). The "all three changes" flash is the template build plus 1,376 B. Sound errors are the worst of 13 long scenarios; the -90 dBFS gate is 0.0000316 FS, so every differing build is 9 to 890 times over it. Seed3 hashes matched on every row except GCC 15.3 (19 of 28).

**Earlier sweep (released source, GCC 4.9 to 15, emulator).** Relative to GCC 4.9.3 (image 268,384 B, median 96,826, worst 101,804 cycles):

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

- **Why GCC 9, not 8.** Only GCC 8 and 9 beat 4.9.3 in both median and worst block. GCC 8 gave the smaller image (-6.7% against -2.3%) but less headroom (worst 83.3% against 82.9%). Headroom decides whether the callback survives its worst block and flash is not scarce. Only GCC 9 was confirmed bit-exact with the pan pin.
- **Build.** GCC 9.3.1 (Arm GNU Embedded 9-2020-q2), `-std=gnu++98` as for the released source. The build kit still builds the released app with 4.9.3 as a fixed reference. Both compiler licences ship with the release.
- **Source change.** The random pan spread `0.5 + (u - 0.5) * min(spread * v, 1)` in `randomized_parameter.h` is one explicit fused multiply-add (`MulAdd`, standard `fmaf`, which GCC compiles to one instruction). Without it GCC 7 to 9 differed from 4.9.3 by at most 1 float ulp in 4 of 20 scenarios, because compilers may or may not fuse the multiply and add.
- **Evidence.** GCC 9 matched 4.9.3 in 20 of 20 emulator scenarios. The emulator runs the real linked ARM code with fused multiply-add and flush-to-zero modelled.
- **GCC 10 and later: rejected.** Stock peak is 99,516 to 100,899 cycles against 97,255 (+2.3% to +3.7%); flash is smaller but the worst case has less headroom. Of 13 long scenarios 11 are exact, the rest differ by up to 0.000296535 FS (11 to 14) or 0.0282450 FS (15); short suites missed this. `-fno-inline-functions` changed timed cycles by at most about 0.1% and did not recover the slowdown, so the inlining default does not explain it. Switching contraction off made the sound difference larger. In the saturated digital core the newer compilers emit separate per-sample renderer calls where GCC 9 keeps one body (executed instructions +1.60%, stack-memory instructions +84.42%). Cause not identified.
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

**Template.** The 2,669 / 2,828 peak savings come from the first paired baseline and template run (94,586 template peak); the Build comparison row shows the second template run (94,719), which gives 2,536 / 2,722 against the first baseline. It is not a gain everywhere: q1 stereo stress32 is 794 to 808 cycles slower at median, and some q1 and q2 cases regress. It improves the q3 and Clouds-style cases that set the peak.

**Local frame.** The output values were loaded and stored once per grain per sample because stores to grain state may alias them; a local sum stays in registers and is added once. The additions and their order are unchanged. GCC keeps the grain player's `Render` out of line (hence the flash cost) and some off-peak cases slow down (dense and delay up to 558, q2 mono stress32 by 1,023, all at or below 60,000 cycles).

**Unsigned modulo.** The read index is the write pointer plus a delay and every caller keeps it non-negative, so a signed `%` by a constant only adds a sign correction, run four times per grain per sample. With both changes 26 of 28 scenarios get faster and the worst off-peak loss is 544.

**Freeze template.** Small gain, but it shifts GCC's inlining (q2 mono delay median 40,546 to 47,444) and changes the grain `Render` signature used at 17 host-test call sites.

**ITCM.** It helped some delay cases but cost more than it saved in the saturated case. Hot state and the stack already sit in fast core-coupled memory.

**Result.** Against the GCC 9 build with the template, the worst block of all 28 scenarios goes 97,123 to 92,486 (77.1%) and the stock peak 94,702 to 90,333 (75.3%). Against the GCC 9 baseline, the stock peak goes 97,255 to 90,333. Both stay just above the 90,000-cycle (75%) DSP-only target for a 25% reserve. An earlier CPU gate on a different harness measured 98,314 (81.93%) stock.

**Untried ideas** (none fits the rules below): Hermite interpolation on unscaled integers with one scale afterwards (estimated 1.5k to 2k cycles, not measured; a hypothesis that would need the same sound checks, plus a raw-value accessor on six `Item` specializations); whole-block rendering per grain (changes the sound through feedback and per-sample triggers); delay versus granular as a per-block template (1.7k slower); the local-sum idea in the multitap delay (not the peak).

## Method

- **Seed3 probe.** The real `Processor::Process` at 480 MHz with caches on and memory placed as in the firmware (state and stack in DTCM, delay buffers in AXI and D2 SRAM, reverb in D3 SRAM). 16,000 warm blocks, then 4,096 measured; median is the upper middle. 28 scenarios: q0 to q3, stereo and mono, dense, stress32 and delay, Clouds-style and pitch-scale cases. Stress32 holds all 32 grains active at +4x speed. Peaks are finite-sample maxima, not proven bounds.
- **Timed.** Granular scheduling and rendering, feedback and filtering, wet/dry, reverb, output limiter. Not timed: setup, ADC and CV, UI, codec conversion, interrupts, DMA contention, flash saving, synth mode. The full callback on the module will be higher.
- **Module calibration (in progress).** DWT cycle counters around `Process` and the whole callback, running the same stress32 fixture on the module. The ratio to the Seed3 median calibrates these figures; the module must stay at or below 90,000 cycles to keep a 25% reserve.
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

Width conversion and CV clamping run once per callback. Each new grain receives a normalized width and an enabled flag; active grains retain their pan. Wide-detent filter Q uses coefficients selected outside the sample loop.

Default audio is checked against unchanged firmware with the Cortex-M7 emulator. Measurements count executed Thumb instructions, not hardware cycles; they do not establish module headroom. The retained DSP-only q3 stereo stress32 median / peak is 77,723 / 77,737 instructions, and delay is 51,548.5 / 51,680: increases of 11 and 88 instructions respectively. A filter-only template increased both costs and was removed.

The delay-mode ramp changes callback instruction counts by -34..+35; accepted to remove an audible click. All 20 emulator scenarios retain byte-identical default audio (420,000 stereo frames). Of 8,000 whole-callback counts, 4,800 differ. In q3 stereo granular playback, median / peak changes from 13,040 / 15,180 to 13,043 / 15,183; delay changes from 53,319 / 53,505 to 53,323 / 53,509. These whole-callback fixtures differ from the DSP-only stress32 probe. Compiler inlining and register allocation affect the counts. No compiler hints, instruction padding or whole-loop specialization were retained.

Q coefficients approach their target once per block with a 0.005 one-pole coefficient and a rounded 16-bit position that reaches both endpoints exactly. The wet delay regression checks 100-, 512- and 4,096-block windows, including the completed transition; each maximum output step remains within its identical no-switch control's range.

Explicit wide-detent and ramp-position checks add 1..13 executed instructions per granular, mono or delay callback in the default emulator fixtures; synth and self-play counts remain unchanged. The changes are confined to the granular processor. Default audio remains byte-identical in all 20 scenarios.
