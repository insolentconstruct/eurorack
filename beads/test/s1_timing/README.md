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

# RC1 S1 timing application

This overlay targets RC1 firmware commit `f7608b61`. In the GCC 9 build kit,
run `make -f beads/build/s1-timing.mk` from the source root. It applies
the timing edits to a staged copy after checking every source preimage.
The release uses DWARF line tables in its ELF pin, so the ordinary source
files are kept byte-for-byte. Use the default `OUT=/out` for reproducible
DWARF paths. There is no WAV or programming target.

`receipt.json` gives the ABI addresses and identities of the built ELF and
complete stock application/profile/region flash span. `SHA256SUMS` covers
the output files. Run `make -f beads/build/s1-timing.mk verify` to check
the stock timing artifact against its separate pins. A live policy must pin the exact
profiles and regions it uses, including the fault and repaired images.

## ABI 1

All fields are little-endian. The ELF exports `s1_snapshot`, `s1_trace`,
`s1_mailbox`, and a separate `s1_observer_cycles` diagnostic word. The
bench ABI at `eb7809d` defines the 26 snapshot words, the 44-byte header and
56-byte frame. The snapshot publishes about every 20 ms with an odd/even
generation. TIM2 supplies elapsed microseconds. Boot identity comes from
two hardware RNG words. Region status is captured from the loader.
The main loop samples the assignment LED synth-fault predicate and counts
its lit onsets. The version display uses a separate cue. SysTick retains the
release watchdog, UI, and factory-test polling path. Factory-test entry is
disabled at boot in the timing image.

Each frame's former reserved word at byte offset 30 carries row evidence.
The low byte identifies the function pointer Fill called. Values are 0 for
unknown, 1 for Process, 2 for ProcessSynthLevel, 3 for ProcessAbyss, 4 for
ProcessOliverb, 5 for ProcessExtendedReverb and 6 for calibration or any
other callback. The high byte is 1 for half-transfer and 2 for
transfer-complete. Older captures have zero in both bytes. The 26-word
snapshot has no spare word and its layout stays intact.

The 15 KiB AXI reservation is after the last delay bank's samples and
freeze tail. Abyss reserves at most 471,040 bytes of that physical bank. The trace
begins at byte 508,928. The last two delay banks trade physical locations, keeping
their capacities. The publish word at offset 15356 stays zero until cache
cleaning has made the sealed chunk visible. The collector checks that
word before and after the SWD read, against the seal in the header.

The mailbox has five 32-bit words in order: request, count, codec rate,
synth rate, status. Supply the arguments first and write the request
last. Wait for request to return to zero before writing another request.

| Request | Meaning | Status |
| --- | --- | --- |
| 1 | Capture exactly 2–273 callbacks. Arm during steady audio or after startup preparation. | 1 while armed, 2 once published |
| 2 | Stage 24, 32 or 48 kHz Plaits and a separate 24, 32 or 48 kHz synth-mode codec rate. Codec rate 0 selects the normal rate. | 3 staged. Verify new render and rates after blanking. |
| 4 | Pause and prepare new Plaits voices at the already verified rate and posture. | 4 prepared |
| 5 | Start a prepared, armed Plaits acquisition at its first render. | 5 started |
| 6 | Cancel an armed acquisition and leave startup preparation, if active. If the final IRQ already completed, report its publication, including when cancellation arrives during or after the flush. | 6 cancelled, 2 published |
| 7 | Pre-arm 2–273 callbacks in a ring. The firmware seals on its own SEED edge, keeping the codec word's 1 to count − 1 callbacks before the edge callback. | 7 while armed, 2 once published |
| 8 | Count 1 forces the no-input path (synth mode) with inputs patched; count 0 restores automatic detection. Count 1 requires active and requested Granular mode and a running codec that can change mode. Count 0 is always accepted. | 8 accepted, 9 settled, 0xffffffff unsupported. A clear can instead preserve the acquisition status. |
| 9 | As 7, sealing on the first missed callback instead: it keeps recording while an overload starves the main loop, and publishes once the load is released. | 9 while armed, 2 once published |
| | Invalid or busy | 0xffffffff |

The firmware main loop owns the status word. The audio IRQ hands it a
pending seal, and the main loop publishes status 2 after writing the seal.
Each consumed command supersedes any pending request 8 settlement. The UI
processes its events before the mailbox is polled. Count 1 checks the active
processor, the UI's selected processor, and the committed setting, which
can lead the UI selection by a tick. Count 1 refuses while FREEZE,
calibration, factory test, codec blanking, or startup blocks the mode
change. An accepted mode change reports 8 while settling and 9 when the
codec runs in the requested posture. An idle count 0 reports 9 immediately
during prepared startup, outside Granular mode, or while the UI blocks
mode changes. Automatic input detection resumes when the state machine
can act on it. A count 0 during acquisition or while status 2 is visible clears
the force and consumes the request while preserving the acquisition status.
These clears schedule no later status write.

A SEED-latched chunk is in callback order, its header covering only the
retained callbacks. A trigger earlier than the requested lead seals a shorter
chunk. The forced path is the stock one, without the 10 s idle wait or the
self-play MEDIA hold.

Placement codes in the frame and snapshot are 0 (off), 1 (after grains),
2 (before buffer), 3 (in feedback), 4 (Chord after grains), and 5 (48 kHz
Bloom with both resonator flags). Code 5 selects the 16-mode resonator
downstream of the grains, immediately before Bloom reverb. Codes 4 and 5
extend ABI 1 without changing its layout.

The ELF also exports `s1_ceiling`, the CPU ceiling hold (version 2) of the
Beta 4 DWT build, at the receipt's `ceiling_address` in DTCM. The host writes
its command word as before. The hold sine replaces the audio channels.
Gate-output mode keeps the callback's right-channel gate or silence and
the normal callback wrappers apply profile trim. The ceiling command sample is a timed observation. Command handling and DMA store probes stay in the measured time.

A codec block stays 250 µs in nominal time. The Plaits path renders
6, 8 or 12 frames at the selected synth rate. When the codec rate
differs, event buckets retain SEED edges and a sample-and-hold bridges
the output rates. Blob-local time constants still use the pinned 48 kHz
model code. The rate override applies only to Plaits. A rate change
reinitializes Bloom's character envelope and the common reverb, trigger,
limiter, and modulation state at the new rate. The two rates and
the actual render path remain separate in the records.

## Measurement limits

ABI 1 frame `cycles` is the Fill delta from immediately after `Begin`
through event assembly, the callback and DMA audio stores, minus the
measured observations. `s1_observer_cycles` reports their latest sum.
Only the ceiling command sample and the two Wave Terrain observations
are deducted. Each saves PRIMASK, masks ordinary interrupts for the
two-stamp scope, then restores the prior mask before updating the sum.
The pre-render Terrain observation precedes the release parameter
stores. The ceiling command handler and its DMB run outside the scope.
`Begin`, inline trace field collection, the driver store probes and their
DSBs are counted. The short masks defer a pending priority-0 SysTick or
TIM8 until PRIMASK is restored. The pinned GCC 9 ELF has at most 14,
20 and 33 instructions from `cpsid i` through the restoring `msr`
in the ceiling sample, pre-render Terrain probe and post-render
Terrain probe respectively. The scopes have no calls, loops, device
polls or barriers. This bounds the added masking in instruction count.
Physical delay also depends on instruction and data access latency.
The deferred service contributes to the active measurement.

The common DMA1 Stream 1 IRQ services both halves. The callback delta
starts before the indirect call in Fill and ends after the returned
callback's DSB and ISB, matching the Seed3 phase-2 callback bracket. It
includes any trim in `ProcessSynthLevel` and `ProcessExtendedReverb`.
`Process` covers granular, Spectral, Beads Synth, Plaits and Rings.
Oliverb, Abyss and extended reverb (including Bloom) have dedicated RC1
callbacks.

The naked IRQ entry reads DWT CYCCNT before calling the DMA handler body
and again after the body returns. The interval includes the complete
body's entry and return instructions, DMA status and NDTR reads, Fill
and acknowledgement. Architectural exception entry and return are
outside this interval, as on Seed3. Trace and peak bookkeeping start after
the closing stamp. The body tests TC first, rereads status to test HT when
TC is clear and acknowledges after Fill. An additional pre-acknowledgement
status sample detects a pending opposite half. Because the selected bit
stays set until acknowledgement, the probe uses RX NDTR phase with the
opposite bit to detect coalescence. The entry NDTR supplies the distance
to the next completion of the selected half. This catches coalescence
after late entry even when the end NDTR is already back in the opposite
phase and fewer than two block periods have elapsed in the handler.
The trace marks a miss for the opposite pending half and increments the
dropped count when the selected half is inferred to have coalesced.

| ELF symbol | Measurement |
| --- | --- |
| `s1_callback_cycles` | Latest raw callback delta |
| `s1_irq_cycles` | Latest raw IRQ delta |
| `s1_callback_gate_cycles` | Latest callback delta less its measured observations |
| `s1_irq_gate_cycles` | Latest IRQ delta less its measured observations |
| `s1_callback_peak` | Maximum raw callback delta |
| `s1_irq_peak` | Maximum raw IRQ delta |
| `s1_callback_gate_peak` | Maximum corrected callback count |
| `s1_irq_gate_peak` | Maximum corrected IRQ count |

Subtraction saturates at zero and happens per block before the maximum.
Read the four peak addresses from `receipt.json` after status 2 and seal
validation. Request 1 covers the requested blocks. Requests 7 and 9 cover
the entire armed interval, including callbacks overwritten in the ring.
All peaks include the final IRQ and reset on the next successful arm.

The absolute RC1 limits for both corrected peaks are 104,215 cycles at
48 kHz, 99,655 at 32 kHz and 95,095 at 24 kHz. Use natural-load captures
for the gate. The corrections leave the cost of taking and accumulating
timestamps, extra branches and changes to compiler code generation.
Emulator comparisons use instruction counts. Physical DWT measurements
also reflect cache, memory, and pipeline timing. The SAI status and
DMA-half check can flag overruns or delayed service. Qualification of
the physical first-word deadline needs an independent observer.

Synthetic `DWT_CALIBRATION` cannot share the ABI 1 trace reservation.
The staged overlay rejects that combination at compile time.

A chunk holds about 68 ms. The current single sealed buffer supports short
offline replay and requires release before another arm. Contiguous
multi-chunk acquisition and the longer Rings windows need a qualified
producer and transport. Physical captures also need the exact recording
aperture, calibrated per-slot stimulus and image return.

`replay.py ELF REGION OUTPUT [--release RELEASE_ELF]` uses the existing transition emulator
with simulated TIM2, DMA status and RNG. Its output includes wire-format
files, raw codec frames, callback-bracket counts and whole-IRQ
instruction counts. With a release ELF, the paired fixtures assert
that each corrected value bounds the corresponding release block.
They also inject pending SysTick and TIM8 service at the ceiling
observation and verify that the corrected counts include that service.
A callback reading DMA status past its 120,000-count block sees the next half's
flag; the TX stream position is not modelled, so the hold's store counters
mean nothing there.
