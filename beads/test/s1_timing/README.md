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
The fault-cue latch observes the assignment LED synth-fault branch, counting
its lit onsets. The version display uses a separate cue.

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
| 6 | Cancel an armed acquisition and leave startup preparation, if active. | 6 cancelled |
| 7 | Pre-arm 2–273 callbacks in a ring. The firmware seals on its own SEED edge, keeping the codec word's 1 to count − 1 callbacks before the edge callback. | 7 while armed, 2 once published |
| 8 | Count 1 forces the no-input path (synth mode) with inputs patched; count 0 returns to the detector's. | 8 accepted, 9 once the codec runs in that mode |
| 9 | As 7, sealing on the first missed callback instead: it keeps recording while an overload starves the main loop, and publishes once the load is released. | 9 while armed, 2 once published |
| | Invalid or busy | 0xffffffff |

A SEED-latched chunk is in callback order, its header covering only the
retained callbacks. A trigger earlier than the requested lead seals a shorter
chunk. The forced path is the stock one, without the 10 s idle wait or the
self-play MEDIA hold.

The ELF also exports `s1_ceiling`, the CPU ceiling hold (version 2) of the
Beta 4 DWT build, at the receipt's `ceiling_address` in DTCM. The host writes
its command word as before; idle, the hold's bookkeeping is an observation.

A codec block stays 250 µs in nominal time. The Plaits path renders
6, 8 or 12 frames at the selected synth rate. When the codec rate
differs, event buckets retain SEED edges and a sample-and-hold bridges
the output rates. Blob-local time constants still use the pinned 48 kHz
model code. The rate override applies only to Plaits. The two rates and
the actual render path remain separate in the records.

## Measurement limits

The DWT bracket runs from before driver event assembly through the DMA
audio stores. Inline observations have their own measured bracket
subtracted before writing the verdict field. Entry, trailing bookkeeping,
trace publication, and any residual timestamp overhead have separate
costs. The SAI status and DMA-half check can flag overruns or delayed
service. Qualification of the physical first-word deadline needs an
independent observer. Emulator values are instruction counts. AXI/cache
effects and the swapped physical delay banks need hardware comparison.

The common DMA1 Stream 1 IRQ services both halves. Its Fill dispatch brackets
the full selected codec callback, including the trim in `ProcessSynthLevel`
and `ProcessExtendedReverb`. `Process` covers granular, Spectral, Beads Synth,
Plaits and Rings. Oliverb, Abyss and extended reverb (including Bloom) keep
their dedicated RC1 callbacks and share the trace and hold. Blanking and
transition holds go through the ceiling output when the ceiling is armed.

The optional DTCM symbols `s1_callback_cycles` and `s1_irq_cycles` expose
the latest raw DWT delta. The former wraps the indirect codec callback.
The latter runs from before the DMA flag read through Fill and trace
recording, excluding exception entry/exit and the final diagnostic write.
`s1_callback_peak` and `s1_irq_peak` are reset when a capture is armed and
include its last IRQ. For requests 7 and 9 the peaks cover the full armed
interval, including callbacks replaced in the ring. Read the peaks after
capture publication. ABI 1 frame cycles still report the original Fill
bracket minus its timed observations.

A chunk holds about 68 ms. The current single sealed buffer supports short
offline replay and requires release before another arm. Contiguous
multi-chunk acquisition and the longer Rings windows need a qualified
producer and transport. Physical captures also need the exact recording
aperture, calibrated per-slot stimulus and image return.

`replay.py ELF REGION OUTPUT` uses the existing transition emulator
with simulated TIM2, DMA status and RNG. Its output includes wire-format
files, raw codec frames, callback-bracket counts and whole-IRQ
instruction counts. A callback
reading the DMA status past its 120,000-count block sees the next half's
flag; the TX stream position is not modelled, so the hold's store counters
mean nothing there.
