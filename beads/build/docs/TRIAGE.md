<!--
Copyright 2026 Insolent Construct.
Author: Insolent Construct

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

# RC1 request triage

The [request tracker][tracker] counts requesters. The [roadmap][roadmap]
and current vote govern release scope. This is a prototype selection.
Costs below are estimates unless marked measured. Stock risk describes
the paths that need regression coverage. Fixed Hygrostat choices use no
factory slot, gesture or saved field.

## New work

| Request | Feasibility, cost and Mutable Instruments precedent | Stock risk and verdict |
| --- | --- | --- |
| [#39][39] Finer TIME CV | Fixed one-eighth CV range. Reuse calibrated input processing with a second pair of coefficients. No extra work per block by design, two floats plus profile metadata and small setup code. Mutable Instruments suggested zoom and declined coupling sensitivity to DENSITY. | **Prototype now.** Same TIME knob, attenurandomizer, patch detection and persisted calibration. Exercise both CV polarities and recalibration. |
| [#49][49] Hold TIME and shorten grains at boundaries | Trim against the available span in the direction of travel, after fractional start correction. A few operations per grain start, no new buffer, small code in each quality or a dedicated renderer, Fixed. Mutable Instruments said the position clamps could be commented out. | **Later.** Removing clamps alone changes wrap behavior. Define minimum useful envelope and interpolation margins for live/frozen, reverse, trimmed and attack-snapped starts first. Its gain and timing changes are audible in exactly the requested cases. |
| [#28][28] Easier audio-rate DENSITY | Remap just the free-running granular generator's high-rate ends. Small control-rate math or lookup, no state, small code/table, Fixed. Start from Mutable Instruments' stock clock and random-rate curves. | **Later.** Pick the audio band and endpoints by render. A shared knob remap would also move clock divisions, delay timing and synth FM. |
| [#55][55] Independent bipolar ASSIGN amounts | Marbles has a bipolar law and Beads has unsigned custom-assignment levels. Signed mode needs three persistent values or an explicit migration design, neutral pickup and a cue on the existing [M]+knob gesture. Small UI and mix-control CPU, a few bytes of state and modest code. | **Later.** Reinterpreting the three stock bytes makes saved positive levels change meaning after profile switches or downgrade. Keep stock and signed storage independent. |
| [#128][128] Faster delay pitch shift | A 512-sample rotary head can reuse the existing ring buffer and two read heads. Stock uses 2048 samples (~21 ms mean at 48 kHz), the proposed window ~5 ms. Similar On CPU if specialized; separate delay renderer adds flash, no recording RAM, Fixed. | **Later.** Measure low-note modulation and feedback transients first. The tap template sits inside the sample loop, and its window sets both phase wrap and the minimum readable delay. |
| [#11][11] Steadier level across SIZE | Compensation is cheap at grain start, small table/code, no extra audio buffer, Fixed. Stock Beads uses envelope shaping and a shared output limiter. | **Later.** To agree on a gain law, compare sparse and overlapping grains, SHAPE, recording type, and how the gain affects feedback and trigger audibility. |
| [#12][12] Variable lo-fi rate and repitched frozen audio | Stock qualities change recording rate; Mutable Instruments suggested replaying frozen slices at a speed controlled by PITCH. Frozen-only transport can reuse the buffer with extra interpolation/state and code; real-time lo-fi resampling costs more. Needs a profile mode and a clear SIZE/PITCH mapping. | **Later.** Split the two behaviors. Retain converter timing and stock live rotary shifting. Check loop seams, rate changes and feedback. |
| [#14][14] Discrete clocked-delay divisions | Reuse the existing ratio table and stock external-clock crossfades, remove interpolation only in the selected clocked mode. Little CPU/state/flash; Fixed. | **Later.** Audition division positions, detent hysteresis, and live jumps under CV with #45. Leave tap tempo and unclocked delay intact. |
| [#15][15] Clock delay from FREEZE input | Reuse external sync through a Fixed source selector. Small code/state, no buffer, modest event-path work. Stock routes that jack to the freeze gate. | **Later.** Define jack versus button behavior, FREEZE priority, and routing with #45. A clock patched to SEED is the stock equivalent for timing alone. |
| [#23][23] Independent reverb quality | A fixed topology override costs little, following the Clouds-character option. A truly independent reverb rate adds resampling CPU/state and changes delay memory use. Profile choice. | **Later.** Settle whether coloration, topology or rate is wanted. Audition decays and reverb tails across quality transitions. |
| [#24][24] More simultaneous grains | CPU and grain state grow with polyphony. Clouds trades mono for more grains. A cap or cheaper renderer would add code and require explicit mode/budget choices. | **No for RC1.** Keep 32 grains. More voices spend real deadline headroom at the worst moment. |
| [#45][45] Freeze on SEED beat | One pending edge and small timeout/LED state and code. Use Beads' delay-clock condition and a custom profile choice. External clock crossfade is timing precedent. | **Later.** Define the unfreeze policy, clock loss, button indication, and #15 interaction before changing event ownership. |
| [#46][46] SEED multiplication | Predict periodic edges with a bounded scheduler, a few timers and added code; extra grains have a real CPU cost. Mutable Instruments limited this to regular clocks. Fixed DENSITY mapping. | **Later.** Loss/reacquisition, tempo jumps, and existing 100% acceptance positions matter. Common design with #218. |
| [#52][52] Alternating ping-pong delay | Cross-route taps/feedback in optional stereo rendering. Reuses delay RAM, adds per-sample routing and code. Stock's two taps are precedent for routing, but Mutable Instruments declined ping-pong. | **Later.** Measure mono-to-stereo behavior and loop gain with the two taps, including unity feedback, time crossfades and OUT R gate. |
| [#142][142] Input low-pass on FEEDBACK | A stereo pre-buffer filter is feasible, a small state and coefficient cost. Clouds and stock delay filter paths provide DSP precedent, with different placement. | **Later.** Wait for clarification on feedback access. An external low-pass already covers the basic input routing. |
| [#300][300] Frozen hit slices | 16 to 64 hits, strongest-first selection needs per-mark strength and a sort at freeze. Reuse the existing Attacks marks and Beads' frozen replay; additional RAM/code and a TIME mode. | **Later.** Vote item. Define missing/overfull marks and fallback TIME, then measure freeze-entry work. |
| [#305][305] [A]+main knobs | Mix knob pickup is Beads precedent. Main knobs need a pickup policy for live playing, CV handling and gesture arbitration, with small control CPU, per-knob state and extra code. | **Later.** Shared controls and release jumps make it bigger than a profile-only option. |

## Requests with existing work

| Issue | Disposition and remaining risk |
| --- | --- |
| [#10][10] Unity feedback | **No duplicate.** ID 35, stock delay feedback and limiter precedent. Existing Fixed notch reuses taps and filter state; check loop gain at both taps and crossfades. |
| [#29][29] Divided grain gate | **No duplicate.** ID 51 uses a small counter on the existing Beads OUT R gate path; check pulse width and bypass. |
| [#30][30] Manual delay without pitch bend | **No duplicate.** ID 36 reuses Mutable Instruments' clock crossfades with timing state and overlapping tap work in the enabled fade. |
| [#38][38] Independent decay | **No duplicate.** ID 54 in the base. Stock loop and mix mapping, separate Fixed renderer. Full-wet range is a separate question. |
| [#48][48] Send mix and grain tails | **No duplicate.** ID 32 Send already implemented. Stock grain lifetimes provide precedent. Recording/feedback remain active; tails and default entry need coverage. |
| [#51][51] Wavetable level | **No duplicate.** ID 50 adds synth output trim in a selected callback, following Beads' level controls and preserving OUT R gates. An independent live gain gesture remains a separate request. |
| [#53][53] Quality-switch pop | **Later.** Same-rate fades and kept codec clocks already addressed much of this. Mutable Instruments called the stock reset click and buffer clear normal. Residual rate-change analog transient needs converter evidence, no panel change. |
| [#58][58] Attack detection | **No duplicate.** Attacks and sensitivity shipped. Arbhar inspired the requested behavior; existing Beads gain follower and mark table keep the cost bounded. Snap alone and sensitivity still need module checks. |
| [#88][88] Monsoon-style resonator | **Later** for the alternate Processor mode. Rings-derived modal option already exists. The Parasites study reused 128 KiB of delay RAM, about 9.8 KiB flash and 620 B state; it still needs a distinct control mapping and vote. |
| [#183][183] SEED latch pop | **No duplicate.** Beta 3 opt-in fade. Stock tapped/knob timing switch is the source in emulator tests. Remaining work is a reproducible module row with the fade On and Off. |
| [#237][237] Shipped grain stealing | **No duplicate.** ID 34 follows the owner decision and 2021/2024 stock. Module cost previously 3.4–3.6 CPU points, Cold digital still open. |
| [#243][243] False notes during slow glides | **No duplicate.** Existing Recent notes refinement extends the settle rule; verify stepped notes and slow glides. Custom feature, small timing state. |
| [#285][285] Page opening and preview | **No duplicate.** IDs 37/38, two fixed UI choices on the existing page controller, no sample work; check LED timing. |
| [#287][287] Startup quality suggestion | **No duplicate.** ID 39 existing, using profile identity and stock quality values. Verify saved settings and frozen recording precedence. |
| [#301][301] Configurable [A] gestures | **No duplicate.** Existing custom toggle and mix assignments reuse Beads' button combos, with profile validation for bank access. Main knob takeover remains #305. |
| [#313][313] Distinct pitch-notch colour | **No duplicate.** Blue cue already in the base. Existing optional cue and LED driver; check physical visibility at reduced brightness. |

## Vote, engineering and other scope

| Issue | Disposition |
| --- | --- |
| [#165][165] Brown-out reset | **Later.** MCU option bytes persist across stock installs and do not protect an erase already underway. Variable-supply qualification first. |
| [#174][174] CPU estimates | **Later.** Existing cost study is historical. Measure current per-rate worst cases before deriving estimates. No audio/control changes required for the study. |
| [#194][194] Plaits Palette models | **No duplicate.** Existing owner-approved blob work. Preserve model identities and default banks, check flash fit and per-model CPU/voice reset. |
| [#195][195] Custom wavetables | **No duplicate.** Owner-approved RC1 work in its own lane. Use the stock pre-integrated format in the existing WAVE region. Editor renders, writer/readback and fallback are the work. |
| [#214][214] Time stretch | **Later.** Clouds WSOLA is direct precedent. Correlation costs CPU and scratch; extra Processor path and controls add flash. Vote and profile/switching design first. |
| [#215][215] Repeatable/drifting randomness | **Later.** Marbles Déjà vu is direct precedent. One loop prototype adds 3.1 KB flash and 224 B state. Set loop capture and control semantics first. |
| [#217][217] Shimmer | **Later.** Prototype reused Beads rotary shifting/reverb, adds ~1.8 KB code. Constrain feedback stability per quality first. |
| [#218][218] Euclidean/beat-gridded grains | **Later.** Reuse Beads SEED timing; prototype adds ~1.2 KB flash. Resolve pattern and irregular clocks alongside #46. |
| [#219][219] CV on OUT R | **Later.** Marbles random CV is precedent, and Beads has a gate output. Envelope/state cost is small, but output headroom and hardware summing into unpatched L require measurement. |
| [#221][221] Alternate recording media | **Later.** Stock qualities couple tone/feedback/reverb. Prototype saturation offline first. Candidate filters, tables and oversampling have different costs. |
| [#245][245] Plugin | **Later.** Separate shared engine extraction, host resampling and UI project. Strict firmware identity proof after extraction. |
| [#246][246] VCV fix audit | **Later.** Findings exist. Preparing separate upstream patches and tests has no firmware control or DSP cost. |
| [#254][254] Plaits AUX | **Later.** Plaits OUT/AUX and Rings voice split are precedent. AUX-only depends on the model, and current blobs omit AUX work. Need variant/region build and per-bank limits before exposing a choice. |
| [#255][255] Plaits LPG colour | **Later.** Stock Plaits offers the setting. Small coefficient change, profile only, minimal CPU/RAM, but own proposal awaits interest. |
| [#256][256] Plaits root octave | **Later.** Stock Plaits frequency range is precedent. Could reuse a fixed root bias with negligible state/code, profile only, pending interest. |
| [#270][270] Plaits CPU study | **No duplicate.** Ongoing measurement of Mutable Instruments' engines, AUX elimination and shared work. Verify output and per-model peaks for changes. |
| [#275][275] Unused FFT tables | **No duplicate.** Existing work specializes the spectral FFT size, with no audio/control changes and ~38.9 KB flash savings in the recorded build. |
| [#68][68] Other-module tuneups | **Later in this lane.** Separate firmware baselines, compiler tests and module qualification. No Beads controls or DSP change. |

[tracker]: https://github.com/insolentconstruct/eurorack/issues/130
[roadmap]: https://github.com/insolentconstruct/eurorack/issues/32
[10]: https://github.com/insolentconstruct/eurorack/issues/10
[11]: https://github.com/insolentconstruct/eurorack/issues/11
[12]: https://github.com/insolentconstruct/eurorack/issues/12
[14]: https://github.com/insolentconstruct/eurorack/issues/14
[15]: https://github.com/insolentconstruct/eurorack/issues/15
[23]: https://github.com/insolentconstruct/eurorack/issues/23
[24]: https://github.com/insolentconstruct/eurorack/issues/24
[28]: https://github.com/insolentconstruct/eurorack/issues/28
[29]: https://github.com/insolentconstruct/eurorack/issues/29
[30]: https://github.com/insolentconstruct/eurorack/issues/30
[38]: https://github.com/insolentconstruct/eurorack/issues/38
[39]: https://github.com/insolentconstruct/eurorack/issues/39
[45]: https://github.com/insolentconstruct/eurorack/issues/45
[46]: https://github.com/insolentconstruct/eurorack/issues/46
[48]: https://github.com/insolentconstruct/eurorack/issues/48
[49]: https://github.com/insolentconstruct/eurorack/issues/49
[51]: https://github.com/insolentconstruct/eurorack/issues/51
[52]: https://github.com/insolentconstruct/eurorack/issues/52
[53]: https://github.com/insolentconstruct/eurorack/issues/53
[55]: https://github.com/insolentconstruct/eurorack/issues/55
[58]: https://github.com/insolentconstruct/eurorack/issues/58
[68]: https://github.com/insolentconstruct/eurorack/issues/68
[88]: https://github.com/insolentconstruct/eurorack/issues/88
[128]: https://github.com/insolentconstruct/eurorack/issues/128
[142]: https://github.com/insolentconstruct/eurorack/issues/142
[165]: https://github.com/insolentconstruct/eurorack/issues/165
[174]: https://github.com/insolentconstruct/eurorack/issues/174
[183]: https://github.com/insolentconstruct/eurorack/issues/183
[194]: https://github.com/insolentconstruct/eurorack/issues/194
[195]: https://github.com/insolentconstruct/eurorack/issues/195
[214]: https://github.com/insolentconstruct/eurorack/issues/214
[215]: https://github.com/insolentconstruct/eurorack/issues/215
[217]: https://github.com/insolentconstruct/eurorack/issues/217
[218]: https://github.com/insolentconstruct/eurorack/issues/218
[219]: https://github.com/insolentconstruct/eurorack/issues/219
[221]: https://github.com/insolentconstruct/eurorack/issues/221
[237]: https://github.com/insolentconstruct/eurorack/issues/237
[243]: https://github.com/insolentconstruct/eurorack/issues/243
[245]: https://github.com/insolentconstruct/eurorack/issues/245
[246]: https://github.com/insolentconstruct/eurorack/issues/246
[254]: https://github.com/insolentconstruct/eurorack/issues/254
[255]: https://github.com/insolentconstruct/eurorack/issues/255
[256]: https://github.com/insolentconstruct/eurorack/issues/256
[270]: https://github.com/insolentconstruct/eurorack/issues/270
[275]: https://github.com/insolentconstruct/eurorack/issues/275
[285]: https://github.com/insolentconstruct/eurorack/issues/285
[287]: https://github.com/insolentconstruct/eurorack/issues/287
[300]: https://github.com/insolentconstruct/eurorack/issues/300
[301]: https://github.com/insolentconstruct/eurorack/issues/301
[305]: https://github.com/insolentconstruct/eurorack/issues/305
[313]: https://github.com/insolentconstruct/eurorack/issues/313
