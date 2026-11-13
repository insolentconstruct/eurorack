// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Bench-only CPU ceiling hold (CPU_CEILING, with DWT_CALIBRATION). A host
// writes one command word over SWD; the callback then holds its output stores
// until a cycle target, optionally replaces the output with a test sine and
// skips the DSP, and counts whether the DAC DMA had read L0 before it was
// stored (alone, the observe flag only counts). Any reset clears the command;
// a deadman disarms a hold, sine or skip.

#ifndef BEADS_CPU_CEILING_H_
#define BEADS_CPU_CEILING_H_

#include "stmlib/stmlib.h"

#ifdef CPU_CEILING
#include "beads/drivers/codec.h"
#include "beads/dsp/frame.h"
#endif  // CPU_CEILING

namespace beads {
namespace cpu_ceiling {

const uint32_t kMagic = 0x4c494543;  // "CEIL"
const uint32_t kVersion = 1;
const uint32_t kMaxTarget = 126000;
const uint32_t kTargetMask = 0x1ffff;
const uint32_t kObserve = 1 << 21;  // count L0 stores at the natural load
const uint32_t kSkipDsp = 1 << 22;
const uint32_t kSine = 1 << 23;
const uint32_t kReservedMask = 0x001e0000;
const uint32_t kDeadmanBlocks = 300000;  // 75 s without a new command
const float kSineHz = 1037.0f;
const float kSineLevel = 0.5f;

enum State {
  STATE_IDLE,
  STATE_ARMED,
  STATE_EXPIRED
};

enum Store {
  STORE_ON_TIME,
  STORE_AMBIGUOUS,
  STORE_LATE
};

// 64 bytes in uncached SRAM3 (beads/build/cpu_ceiling.ld). The host writes
// command only; everything else belongs to the firmware.
struct Hold {
  uint32_t magic;
  uint32_t version;
  uint32_t command;
  uint32_t ack;
  uint32_t rejects;
  uint32_t armed_blocks;
  uint32_t frames;
  uint32_t blocks;
  uint32_t on_time;
  uint32_t ambiguous;
  uint32_t late;
  uint32_t first_late;
  uint32_t g2;
  uint32_t pending;
  uint32_t max_store;
  uint32_t state;
};

// A command needs a sequence number, no reserved bits and a bounded target.
inline bool Valid(uint32_t command) {
  return (command >> 24) != 0 && !(command & kReservedMask) &&
      (command & kTargetMask) <= kMaxTarget;
}

inline bool Holds(uint32_t command) {
  return (command & (kTargetMask | kObserve | kSine | kSkipDsp)) != 0;
}

// Words the TX stream has sent past the start of the half at `base`, modulo
// the ring. Below half the ring, the stream has already read the half's
// first word: the DMA preloads a word as soon as the previous one is sent.
inline uint32_t Sent(uint32_t ndtr, uint32_t base, uint32_t words) {
  return (2 * words - ndtr - base) % words;
}

inline bool Reached(uint32_t ndtr, uint32_t base, uint32_t words) {
  return Sent(ndtr, base, words) < words / 2;
}

// NDTR just before and just after the L0 store.
inline Store Classify(
    uint32_t before, uint32_t after, uint32_t base, uint32_t words) {
  if (Reached(before, base, words)) {
    return STORE_LATE;
  }
  return Reached(after, base, words) ? STORE_AMBIGUOUS : STORE_ON_TIME;
}

#ifdef CPU_CEILING

extern volatile Hold hold;

inline bool skip_dsp() { return hold.ack & kSkipDsp; }
inline bool sine() { return hold.ack & kSine; }
inline uint32_t target() { return hold.ack & kTargetMask; }

// At boot: an idle hold, every field cleared.
void Init();
// Process entry: stamps the hold's time origin, takes a new command and runs
// the deadman. True when this block is held or observed.
bool Enter(size_t size);
// Replaces the output conversion while held: the sine, the hold until the
// target (cycles from Enter), then the stores, L0 classified.
void Output(FloatFrame* frames, Codec::Frame* out, size_t size);

#endif  // CPU_CEILING

}  // namespace cpu_ceiling
}  // namespace beads

#endif  // BEADS_CPU_CEILING_H_
