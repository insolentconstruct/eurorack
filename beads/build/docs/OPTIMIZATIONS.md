# Optimizations

What this branch changed to run faster, what was measured and rejected, and the rules for future changes. Audio-path work is judged on cycles saved on a measured worst case, and on whether the sound changes.

Mutable Instruments' April 2021 update had the same goal: it fixed a CPU overload (crackles on the left channel in 48 kHz stereo) "through several code optimizations having no negative impact on the module’s functionality" ([release notes](https://forum.mutable-instruments.net/t/beads-firmware-update-release-candidate/18572)).

## TL;DR

On a development board with the same chip and clock as Beads (DSP only), the busiest stock block takes 82,911 of the 120,000 cycles available (69.1%). The vanilla source takes 99,511 (82.9%) on the same test: 16,600 fewer cycles, 13.8 points of headroom. The later reverb change saves a further 1,238 cycles on that case (82,838 to 81,600 in its own run). Audio and buffer hashes match in all 24 scenarios. The wavetable synth, which those scenarios do not run, takes 89,579 cycles in its busiest case instead of 96,404 (5.7 points). These are development-board figures. On a 2021 module, peak audio-callback load (timing builds, heavy test patches) falls from 91% to 76% in granular mode and from 85–86% to 81–82% in the wavetable synth.

| Build | Cycles of 120,000 | Budget |
|---|---:|---:|
| Vanilla source (beads-build-kit, GCC 4.9.3) | 99,511 | 82.9% |
| GCC 9 and the first three code changes | 90,529 | 75.4% |
| beads-tuneup (with the integer Hermite read) | 82,911 | 69.1% |

Headroom gained per change, in points of the budget on the busiest stock block. Each was measured on its own against the build before it, on Drizzlet's tree, so the sum (13.4) differs a little from the total measured on this branch:

| Change | Headroom |
|---|---:|
| GCC 9.3.1 instead of 4.9.3 (no code changes) | about 2.0 |
| Mono/stereo chosen once per block | 2.2 |
| Grains summed in a local frame | 3.0 |
| Unsigned modulo for the delay-line read | 0.6 |
| Grains interpolate the stored integers (integer Hermite) | 5.6 |
| Total, measured on this branch | 13.8 |
| Synth grains computed in pairs (synth only, its busiest case; not in the total) | 5.7 |
| Reverb reads before its writes (all modes; synth busiest case also 1.0; not in the total) | 1.0 |

Plain GCC 11 to 16 builds are 5,800 to 9,300 cycles slower than GCC 9 on the stock peak (GCC 10.3, from an unverified archive, 10,200); raising the inlining limit brings GCC 11 within noise of GCC 9. An explicit fused multiply-add in the random pan spread keeps GCC 9's audio identical to 4.9.3's. Another in the grain pan mix, and two values rounded separately, make newer compilers round as GCC 9 does (emulated audio identical in 20 scenarios). The bootloader stays on 4.9.3.

- **Sound:** GCC 9 matched 4.9.3 in 20 of 20 emulator scenarios. Each code change matched bit for bit on the development board (28 of 28 hashes) and in the host tests.
- **Rejected:** newer compilers (slower as plain builds, no clear gain with tuning), ITCM code placement, an extra grain/delay template, freeze state as a template parameter.

## Build comparison

Budget is 120,000 cycles per block at 480 MHz. "Stock" is the worst stock case: q3 (48 kHz digital), stereo, 32 grains held active (stress32). Same probe and scenarios on every row.

| Build | Stock peak / median | Sound vs GCC 9 | Verdict |
|---|---|---|---|
| GCC 9.3.1 baseline | 97,255 / 95,610 (81.05%) | reference | kept |
| GCC 9.3.1 + mono/stereo template | 94,719 / 93,111 (78.93%) | bit-identical | kept |
| GCC 9.3.1 + all three changes | 90,333 / not recorded (75.28%) | bit-identical | kept |
| GCC 10.3 | not measured | not measured | not run: no verified archive checksum |
| GCC 11.3 | 100,634 / 98,974 (83.86%) | differs, 0.000296535 FS | rejected |
| GCC 12.3 | 99,675 / 98,147 (83.06%) | differs, 0.000296535 FS | rejected |
| GCC 13.3 | 99,516 / 98,011 (82.93%) | differs, 0.000296535 FS | rejected |
| GCC 14.3 | 100,369 / 98,826 (83.64%) | differs, 0.000296535 FS | rejected |
| GCC 15.3 | 100,899 / 99,455 (84.08%) | differs, 0.0282450 FS | rejected |

These rows, and the per-change figures below, are historical measurements on Drizzlet's tree (28 scenarios), which also carried its options set to their stock values; the changes measured are the ones on this branch. This branch's own figures are in the TL;DR. GCC 11 to 15 were built from the GCC 9 baseline source; on the template source they were no better (stock peak 100,069 to 101,526). `-fno-inline-functions` changed sound and timings by under 0.1%. The sound columns were measured before the rounding pins in the TL;DR. Sound errors are the worst of 13 long scenarios; the -90 dBFS gate is 0.0000316 FS, so every differing build is 9 to 890 times over it. Development-board hashes matched on every row except GCC 15.3 (19 of 28).

**Earlier sweep (vanilla source, GCC 4.9 to 15, emulator).** Relative to GCC 4.9.3 (image 268,384 B, median 96,826, worst 101,804 cycles):

| GCC | Image | Median | Worst | Output vs 4.9.3 | Verdict |
|---|---:|---:|---:|---|---|
| 4.9.3 | 268,384 B | 96,826 | 101,804 | reference | previous build |
| 5, 6 | -3.5, -4.6% | +1.3, +0.5% | +1.2, +0.5% | 20/20 exact | not chosen |
| 7 | -6.6% | +0.7% | +0.1% | 16/20, max 1 float ulp | not chosen |
| 8 | -6.7% | -1.2% | -1.7% | 16/20, max 1 float ulp | runner-up |
| 9 | -2.3% | -2.2% | -2.3% | 16/20, max 1 ulp (20/20 with the pan pin) | chosen |
| 10 to 15 | -10 to -11.5% | +0.2 to +1.5% | +0.4 to +1.6% | 8/20 or 12/20, max 1.4e-5 | rejected |

Development-board re-measure of the same source, worst / median: GCC 4.9.3 101,773 / 96,818; GCC 8.3.1 99,957 / 95,533; GCC 9.3.1 99,430 / 94,672. An older source on a different probe (GCC 4.9.3 vs 15.3) showed the same pattern: 15.3 was 5.7% slower in the worst scenario and 12 of 20 short scenarios were exact.

## Compiler

- **Why GCC 9, not 8.** Only GCC 8 and 9 beat 4.9.3 in both median and worst block. GCC 8 gave the smaller image (-6.7% against -2.3%) but less headroom (worst 83.3% against 82.9%); headroom decides whether the callback survives its worst block, and flash is not scarce. Only GCC 9 was confirmed bit-exact with the pan pin.
- **Why GCC 10 and later are slower** (emulator instruction counts, not module cycles). GCC 10 enabled `-finline-functions` at `-O2` and lowered `max-inline-insns-single` from 200 to 70. The Hermite delay-line read is no longer inlined into the grain loop: 384 extra calls per block in the stress case, each saving registers on the stack. With `--param max-inline-insns-single=140`, GCC 10 runs slightly fewer instructions than GCC 9 for about 12 KB more flash; lowering the limit on GCC 9 reproduces the slowdown. Instructions per block against GCC 9: GCC 10 +7.9%, 12 +4.4%, 14 +0.9%, 16 +1.5% (vanilla source +6.0%, +2.2%, -0.7%, -0.4%). Marking the read `always_inline`, as stmlib does for its interpolation helpers, or the parameter above would allow a newer compiler, but the newer compilers' small sound differences are a separate effect, so builds stay on GCC 9 for now.
- **Build.** GCC 9.3.1 (Arm GNU Embedded 9-2020-q2), `-std=gnu++98` as for the vanilla source. The build kit still builds the vanilla app with 4.9.3 as a fixed reference.
- **Source change.** The random pan spread `0.5 + (u - 0.5) * min(spread * v, 1)` in `randomized_parameter.h` is one explicit fused multiply-add (standard `fmaf`, which GCC compiles to one instruction). Without it GCC 7 to 9 differed from 4.9.3 by at most 1 float ulp in 4 of 20 scenarios, because compilers may or may not fuse the multiply and add.
- **Evidence.** GCC 9 matched 4.9.3 in 20 of 20 emulator scenarios. The emulator runs the real linked ARM code with fused multiply-add and flush-to-zero modelled.
- **GCC 10 and later: rejected.** Stock peak is 99,516 to 100,899 cycles against 97,255 (+2.3% to +3.7%). Of 13 long scenarios 11 are exact, the rest differ by up to 0.000296535 FS (11 to 14) or 0.0282450 FS (15); short suites missed this. In the saturated digital core the newer compilers emit separate per-sample renderer calls where GCC 9 keeps one body (executed instructions +1.60%, stack-memory instructions +84.42%), because of the inlining limit above. Switching contraction off made the sound difference larger.
- **Bootloader.** It stays on GCC 4.9.3 and is byte-identical to Mutable Instruments' build. It runs only at power-up and during updates, WAV updates never replace it, and a compiler change would only add risk to the recovery path.

## Code changes

Cycles saved on the development board's worst stock case (historical, on Drizzlet's tree), peak / median, against the build in the "Against" column. Run 1 is the figure used; run 2 is a repeat. Negative is a slowdown.

| Change | Based on | Against | Stock | Flash | RAM | Sound | Verdict |
|---|---|---|---|---:|---|---|---|
| GCC 9 instead of 4.9.3 | Compiler choice; an explicit `fmaf` pins one pan line so the arithmetic stays Mutable Instruments' | GCC 4.9.3 | about 2% in the worst block | about -2% | n/a | 20/20 emulator | kept |
| Mono/stereo chosen once per block (19-line template) | Mutable Instruments: template specialization on configuration (STM32 miscellania; Clouds' grain renderer) | GCC 9 | 2,669 / 2,499 | -640 B | 0 | 28/28 development board, 20/20 emulator, 13/13 long, error 0 | kept |
| Sum delay grains in a local frame | Mutable Instruments: keep loop state in locals across the block (Timing and processes) | template | 3,630 / 3,725 (run 2: 3,826 / 3,732) | +1,648 B | 0 | same gates | kept |
| Unsigned modulo for the delay-line read index | Mutable Instruments: the same file's `LoadQuad` already uses `size_t(i) % items_per_bank()` | local frame | 739 / 556 (run 2: 636 / 550) | -272 B | 0 | gates run on the combined tree | kept |
| Both pass-2 changes together | the two rows above | template | 4,369 / 4,281 | +1,376 B | 0 | 28/28, 20/20, 13/13, error 0 | kept |
| Grain Hermite read on the stored integers, scaled once | Mutable Instruments: Clouds' `ReadHermite` interpolates the unscaled samples and multiplies by `scale` once (`clouds/dsp/audio_buffer.h:241-277`) | the three changes | 6,775 / 7,140 (run 2: 6,840 / 7,130) | -504 B | 0 | 28/28 development board hashes (audio and buffers); host tests | kept |
| Synth: two playing grains computed side by side, mixed in index order | Port from Drizzlet: the in-order core overlaps two grains' independent sample and envelope chains; each grain's arithmetic is unchanged | integer Hermite, on this branch | unchanged (synth only) | +2,712 B | 0 | 14/14 synth development board hashes (audio and reverb); granular image byte-identical; host tests, `TestSynthGrainPairs` | kept |
| Reverb: all-pass tails and modulated taps loaded before the sample's writes | Drizzlet's Clouds-style diffuser fix; the reads never alias the writes they now precede, and each value's arithmetic is unchanged | beads-tuneup-2026.11.09, on this branch | 1,238 / 1,329 (all 24 granular scenarios: 330 to 1,436 / 444 to 1,411 saved; all 14 synth scenarios: 1,121 to 1,318 / 1,228 to 1,265 saved) | +184 B | 0 | 24/24 granular and 14/14 synth development board hashes (audio and buffers); host tests and audio reference | kept |
| Delay grains computed in pairs (port of the synth pairs) | the synth pairs row | beads-tuneup-2026.11.09 | -19,222 / -19,004 (GCC keeps the split steps out of line); with `always_inline` -2,044 / -1,696 | -4,312 B; -456 B | 0 | 24/24 development board | rejected |
| Synth grains summed in a local frame | the delay local-frame row | beads-tuneup-2026.11.09 | synth -851 / -439 at its peak, slower in all 14 synth scenarios | -64 B | 0 | 14/14 development board | rejected |
| All sample loops in ITCM | Mutable Instruments: stmlib's `IN_RAM` placement | GCC 9 | -765 / -770 | +336 B | +21,440 B ITCM | 28/28 development board | rejected |
| ITCM on top of the template | Mutable Instruments: stmlib's `IN_RAM` placement | template | -611 / -505 | +360 B | +23,448 B ITCM | 28/28 development board | rejected |
| Extra grain/delay template | Mutable Instruments: template specialization | GCC 9 | -1,693 / -1,812 | -336 B | 0 | 28/28 development board | rejected |
| Freeze state as a template parameter | Mutable Instruments: template specialization | both pass-2 changes | 769 / 728 | not built | n/a | 28/28 development board | rejected |

**Template.** The 2,669 peak saving comes from the first paired baseline and template run (94,586 template peak); the Build comparison row shows the second template run (94,719), which gives 2,536 against the first baseline. It is not a gain everywhere: q1 stereo stress32 is 794 to 808 cycles slower at median, and some q1 and q2 cases regress. It improves the q3 cases that set the peak.

**Local frame.** The output values were loaded and stored once per grain per sample because stores to grain state may alias them; a local sum stays in registers and is added once. The additions and their order are unchanged. GCC keeps the grain player's `Render` out of line (hence the flash cost) and some off-peak cases slow down (dense and delay up to 558, q2 mono stress32 by 1,023, all at or below 60,000 cycles).

**Unsigned modulo.** The read index is the write pointer plus a delay and every caller keeps it non-negative, so a signed `%` by a constant only adds a sign correction, run on every grain read. With both changes 26 of 28 scenarios get faster and the worst off-peak loss is 544.

**Freeze template.** Small gain, but it shifts GCC's inlining (q2 mono delay median 40,546 to 47,444) and changes the grain `Render` signature used at 17 host-test call sites.

**ITCM.** It helped some delay cases but cost more than it saved in the saturated case. Hot state and the stack already sit in fast core-coupled memory.

**Result.** On Drizzlet's tree the stock worst block went 97,255 (GCC 9 baseline) to 90,333 with the three code changes. Measured on this branch against the vanilla source on GCC 4.9.3, it goes from 99,511 to 90,529 with them, and to 82,911 with the integer Hermite read (TL;DR).

**Integer Hermite.** Grain reads converted four stored samples to float and scaled each one before the Hermite polynomial. Like Clouds' `ReadHermite` (`clouds/dsp/audio_buffer.h:241-277`), they now interpolate the stored integers (`Item::raw`) and scale the result once: two multiplies per stereo read instead of eight. 8-bit µ-law lines and the delay taps keep the float read. Identical sound depends on keeping each rounding step in place: `c` stays `float(c) * 0.5f`, computed first, otherwise GCC fuses a different multiply-add and 10 of 28 hashes change (the delay taps differed the same way, so they keep the old read). `TestHermiteRawMatchesScaled` checks it on the host.

**Synth pairs.** Measured on this branch (before the fade change) with a 14-scenario synth probe, cycles saved peak / median: busiest synth case (SIZE 0.97, DENSITY 0.95, reverb 0.7) 6,825 / 6,649 (5.7 / 5.5 points); 32 grains 7,032 / 6,479; 32 grains with FREEZE held 5,302 / 5,279; stock synth (SIZE 0.85, +24 st, 16-bit) 6,063 / 5,369 (5.1 / 4.5 points). Two runs agree to the cycle. When both grains of a fixed pair play, both samples are computed, then both envelopes, then both are mixed; otherwise each grain renders alone.

**Reverb reads first.** Each reverb sample read eight all-pass tails and two modulated taps from the same 16-bit buffer it writes, interleaved with the writes; the compiler cannot move a load above a store that may alias it, so the in-order core waited on each load. Every read is now done first. None aliases a write it moves above: all-pass writes go to each line's head (and `ap1` at 100), tails sit at the far end, and the `del1`/`del2` taps stay 4,420 and 6,211 samples or more from their heads. The first `ap1` read already came before any write. Measured on this branch, two runs agree to the cycle: the stock stress32 peak goes from 82,838 to 81,600 and the synth busiest case from 90,150 to 88,970; every scenario is faster.

**Inlining budget.** `processor.cc` is at GCC's `inline-unit-growth` limit, so any size change in that file, the reverb included, changes which calls the inliner still takes. A first version of this change refactored `Interpolate` to call the new `Tap`. That inlined one more `RandomizedParameter::value` call (grain start, once per trigger) into the 48 kHz stereo grain renderer, whose 32-grain loop then ran slower, and the stock peak was 68 cycles slower. Linking one build's reverb into the other's put that peak at 81,707 (old renderer, new reverb) and 84,253 (new renderer, old reverb); the two halves do not add up exactly because the swap also moves code. Unused padding in `processor.cc`, which the linker drops, moved the stock peak by -67 to +362 cycles. `Interpolate` now keeps Mutable Instruments' body and `Tap` repeats it; that keeps the renderer's grain loop as fast as before (its three `value` calls stay out of line). This is a compiler-budget effect, not a rule: re-measure the stock peak after any change that reaches `processor.cc`.

**Untried ideas** (none fits the rules below): whole-block rendering per grain (changes the sound through feedback and per-sample triggers); delay versus granular as a per-block template (1.7k slower); the local-sum idea in the multitap delay (not the peak).

## Method

- **Development-board probe.** The real `Processor::Process` at 480 MHz with caches on and memory placed as in the firmware (state and stack in DTCM, delay buffers in AXI and D2 SRAM, reverb in D3 SRAM). 16,000 warm blocks, then 4,096 measured; median is the upper middle. Scenarios: q0 to q3, stereo and mono, dense, stress32 and delay (24 on this branch; 28 on the tree above, which adds option cases). Stress32 holds all 32 grains active at +4x speed. Peaks are finite-sample maxima, not proven bounds.
- **Timed.** Granular scheduling and rendering, feedback and filtering, wet/dry, reverb. Not timed: setup, ADC and CV, UI, codec conversion, interrupts, DMA contention, flash saving, wavetable synth mode (measured separately: Synth pairs). The full callback on a module is higher.
- **Bit-identical checks.** Development-board output and buffer hashes; 20 short and 13 long emulator scenarios (272,000 blocks in all) compared sample by sample. CI compares native-host audio byte for byte against checked-in pre-optimization DSP headers across 20 scenarios, including all four recording qualities and synth resolutions; host floating-point comparisons do not establish linked-ARM equivalence. See `beads/test/README.md` for the exact checks and limits.

## Audio deadline

Measured in November 2026 on one 2021 module, and on a development board with the same chip and clock.

**TL;DR:** Audio breaks when the callback stores a block's first left word after the DAC DMA has already read it, one slot before sending it [mechanism inferred]. That comes before a full-block underrun, and the SAI flags nothing. Without the one-frame output delay, the module's first left word stays on time up to 90.8% of the block at 48 kHz, 86.6% at 32 kHz and 82.7% at 24 kHz; with it (this branch), up to 96.9%, 97.3% and 97.7%. In the crackle recipe this branch peaks about 18 points under the 48 kHz edge.

- **Module (measured).** A Drizzlet test build with the same codec driver held the audio callback at set lengths; firmware counters flag a block whose first left word is stored after the DMA has read it. Each edge was found twice. Callback peak cycles, budget 120,000:

| Codec rate (qualities) | Clean up to | First late block |
|---|---:|---:|
| 48 kHz (Bright digital, Sunny tape, synth) | 108,908 (90.8%) | 109,504 (91.3%) |
| 32 kHz (Cold digital) | 103,935 (86.6%) | 104,520 (87.1%) |
| 24 kHz (Scorched cassette) | 99,182 (82.7%) | 99,762 (83.1%) |

- **Development board (measured).** With the check made at callback exit (has the DMA already read the first word?), the callback maximum was clean up to about 91.5% of the block at 48 kHz, 87.5% at 32 kHz and 83.5% at 24 kHz; every block failed from 92.1%, 88.1% and 84.1%. The first store comes before exit, so the true deadline sits a little above these bounds. Three repeats per rate, 60 s per step. No SAI underrun or overrun at any load up to 105%. The module's edges are 0.7 to 0.9 points lower.
- **Why the first left word [inferred].** The TX DMA runs in direct mode and reads each word from RAM one slot before sending it (RM0433). When the callback is late, the slot still holds the value from one ring lap earlier (0.5 ms), and the DMA keeps the SAI FIFO fed, so the SAI never underruns. The first right word is read one slot later, so in the loads studied only OUT L clicked.
- **The historical left-channel click.** Reproduced on one module with the 2024 source rebuilt with GCC 9 (plus deadline tracing), in the historical crackle recipe: Bright digital (48 kHz), DENSITY 0.98, PITCH +24, SIZE 0.92. Its callback peaked at 93.5 to 94.0%, above the module's 48 kHz edge, and OUT L clicked. In the same recipe, this branch's code with the 2024 grain stealing (81.7 to 82.7%) and this branch (78.1 to 78.8%) stayed clean [measured]. The clicks are block-locked and match the output one ring lap earlier, which points to a late first left word [inferred; moderately high confidence, exploratory, not seen on the codec data line].
- **Input level (measured).** Level barely moves CPU: a 4.5x louder input moved the module's callback peak by at most 0.7 points; on the development board (vanilla DSP, 24 scenarios), silence to overload moved the peak by at most 0.74 points.
- **Headroom.** Without the delay the module is clean only up to 90.8% at 48 kHz, not 99%, and Cold digital (32 kHz) and Scorched cassette (24 kHz) need their own, lower limits, under 86.6% and 82.7%. With the delay (this branch), keep a point under the measured edges: about 95.5%, 96% and 96.5%.

### One-frame output delay

This branch writes each block's output one frame late: the codec driver stores the last block's last frame at the start of the half before the callback runs, then copies the callback's first N−1 frames after it. The first word the callback must reach in time is then the third word of the half, one frame later, and the deadline moves to about the next codec interrupt. The same driver change is measured in Drizzlet; the development-board figures below are for that driver.

- **Development board (measured, 3 repeats, 60 s per step, 0.1% steps).** Callback maximum, budget 120,000:

| Codec rate | Before: no late word | With the delay: no late word | With the delay: next interrupt not yet pending |
|---|---:|---:|---:|
| 48 kHz | 109,765 (91.5%) | 119,483 (99.6%) | 118,995 (99.2%) |
| 32 kHz | 104,963 (87.5%) | 119,835 (99.9%) | 119,115 (99.3%) |
| 24 kHz | 100,179 (83.5%) | 119,967 (100.0%) | 119,247 (99.4%) |

  From about the block itself the next interrupt is already due when the driver finishes. In these steps that overlap alone did not corrupt output; late words followed 0.3 to 0.6 points higher.
- **Sound (measured).** Bit-identical to the driver without the delay, shifted by exactly one frame, at all three rates (development board, 1,000 halves each, words read from RAM as the DMA sends them).
- **Cost (measured).** `Codec::Fill` with Beads' conversion loop, codec stopped, mean: +144, +93 and +86 cycles per block at 48, 32 and 24 kHz (about 0.12% of the block or less). The copy runs after the callback, so the DWT load figures do not include it. The bootloader links the same driver; the delay is compiled for the application only, so the bootloader is unchanged.
- **Behaviour.** Latency rises by one frame. Patches that feed OUT back to IN, or line audio up with gates or triggers, shift by one frame. A pause or restart (quality changes) drops the one pending frame, inside the existing fade.
- **Why Beads differs.** Mutable Instruments' other STM32 modules share one codec pattern (callback output written in place), but the Beads H7 DMA reads ahead of its sends, and Mutable Instruments noted that every project has something unique in hardware that makes reusable layers above the GPIO level of limited use ([STM32 miscellania](https://pichenettes.github.io/mutable-instruments-documentation/tech_notes/stm32_miscellania/)).
- **Module (measured, one 2021 module).** A test build of this branch held the callback at set lengths, as above. Callback maximum, budget 120,000:

| Codec rate | Clean up to | Without the delay | Gain |
|---|---:|---:|---:|
| 48 kHz | 116,297 (96.9%) | 108,908 (90.8%) | +6.1 points |
| 32 kHz | 116,700 (97.3%) | 103,935 (86.6%) | +10.7 points |
| 24 kHz | 117,228 (97.7%) | 99,182 (82.7%) | +15.0 points |

  The firmware counter flagged the first late blocks half a point to a point higher, while OUT L was still clean. At about 101% every block is late and the output is scrambled at full level (the tone breaks into sidebands); heavier overruns were not measured.
- **Clicks on the module (measured).** In the historical crackle recipe the release build made no clicks on OUT L in about 6 minutes, as beads-tuneup-2026.11.09. Installing by audio, start-up and quality changes passed; quality changes that keep the sample rate stayed click-free.
- **Rate changes (measured).** The small rate-change click remains, as in beads-tuneup-2026.11.09. In 30 rounds per build of four rate changes each (15 rounds of this build, 30 of the previous release, then 15 of this build), its rarer features occurred about as often on both builds: a high-pass transient as the audio fades back in after 48 to 24 kHz (6 of 30 here, 8 of 29 before) and a larger converter power-down step after 32 to 48 kHz (1 of 30 here; before, 0 of 30 by step size and 1 of 30 by peak level). Thirty rounds rule out only a large increase, not small differences. One 24 to 48 kHz change on this build clicked about 5 dB above the previous release's usual level, and one on the previous release was louder on OUT R; with one each, no difference in rate can be read.

## Rules for future changes

### From Mutable Instruments

These come from Mutable Instruments' code and tech notes ([STM32 miscellania](https://pichenettes.github.io/mutable-instruments-documentation/tech_notes/stm32_miscellania/), "Timing and processes", the Beads history page).

1. Specialize on configuration with template parameters: Clouds' grain renderer on channel count, quality and interpolation, stmlib's filter on its type, Warps on FIR templates. It is not always faster, as the freeze template shows.
2. Look at the data format first. Mutable Instruments' own Beads gain was changing delay-line addresses from 47:16 to 21:10 fixed point, which avoids an int64-to-float conversion.
3. Keep loop state in registers across the block; watch for state reloaded every sample, aliasing through `this`, and calls inside the sample loop.
4. Judge the worst block, not the mean. Mutable Instruments toggle a pin around the block callback, design for 80 to 95% of the CPU, and treat 99% of the block as the ceiling. DSP-only, this branch is at about 68% here.

### This branch's own rules

Mutable Instruments' notes describe no sound-identity checks and none of the acceptance gates below; these are stricter.

1. Follow [STYLE.md](STYLE.md): upstream master conventions win, Beads first.
2. Measure the gain on the worst case (q3 stereo stress32). Report peak and median, flash and RAM, and what got slower.
3. Prefer bit-identical output; show it with development-board hashes, emulator audio and long runs.
4. Any difference is flagged with its largest error and whether it drifts. It is accepted only if the error stays strictly below -90 dBFS (0.0000316 FS) with no drift, and is documented here.
5. Reject changes that spend RAM or ITCM for a small gain, or that help one quality and hurt the one that sets the peak.
6. No compiler change without the same comparison: a faster compiler that changes the sound is not an optimization. The app uses GCC 9, where Mutable Instruments used 4.9.3; the bootloader stays on 4.9.3.
