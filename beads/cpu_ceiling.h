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
// Bench-only CPU ceiling hold (CPU_CEILING, with DWT_CALIBRATION), for the
// one-frame output delay. A host writes one command word over SWD; the
// callback then holds until a cycle target, optionally replaces the output
// with a test sine and skips the DSP. The driver stores the carried frame
// (words 0-1 of the half) before the callback and the callback's frames
// (from word 2) after it; each run of stores is checked against the DAC DMA
// (alone, the observe flag only counts). Any reset clears the command; a
// deadman disarms a hold, sine or skip.

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
const uint32_t kVersion = 2;  // 2: driver store runs, word 2 first
const uint32_t kMaxTarget = 126000;
const uint32_t kTargetMask = 0x1ffff;
const uint32_t kObserve = 1 << 21;  // count stores at the natural load
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
// command only; everything else belongs to the firmware. A block counts once,
// as its worse store run.
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

// The TX stream position (the next word it reads), relative to the half at
// `base`, modulo the ring. The DMA has read word w of the half when
// w <= Relative < half: it preloads a word as soon as the previous one is sent.
inline uint32_t Relative(uint32_t ndtr, uint32_t base, uint32_t words) {
  return (2 * words - ndtr - base) % words;
}

inline bool Fetched(uint32_t ndtr, uint32_t base, uint32_t words, uint32_t w) {
  const uint32_t position = Relative(ndtr, base, words);
  return w <= position && position < words / 2;
}

// NDTR just before and just after a run of stores from word w of the half:
// late when the DMA had read its first word before the run, ambiguous when
// during it (later words of a run have at least one more slot).
inline Store Classify(
    uint32_t before, uint32_t after, uint32_t base, uint32_t words,
    uint32_t w) {
  if (Fetched(before, base, words, w)) {
    return STORE_LATE;
  }
  return Fetched(after, base, words, w) ? STORE_AMBIGUOUS : STORE_ON_TIME;
}

#ifdef CPU_CEILING

extern volatile Hold hold;

// Firmware-only state.
struct Block {
  uint32_t entry;
  uint32_t examined;  // the last command word taken or rejected
  uint32_t before;
  float phase;
  bool held;
  Store store;
};

extern Block block;

inline bool skip_dsp() { return hold.ack & kSkipDsp; }
inline bool sine() { return hold.ack & kSine; }
inline uint32_t target() { return hold.ack & kTargetMask; }

// At boot: an idle hold, every field cleared.
void Init();
// Process entry: stamps the hold's time origin, takes a new command and runs
// the deadman. True when this block is held or observed.
bool Enter(size_t size);
// Replaces the output conversion while held: the sine, the hold until the
// target (cycles from Enter), then the conversion into the driver's buffer.
void Output(FloatFrame* frames, Codec::Frame* out, size_t size);

// The driver brackets each run of stores into a DMA half.
inline void StoreBegin() {
  block.before = DMA1_Stream0->NDTR;
}

inline void StoreEnd(size_t offset, size_t size, uint32_t w) {
  __DSB();
  const uint32_t after = DMA1_Stream0->NDTR;
  const Store store = Classify(block.before, after, 2 * offset, 4 * size, w);
  if (store > block.store) {
    block.store = store;
  }
}

// After the driver's last store: counts a held or observed block.
inline void Filled(size_t offset, size_t size) {
  if (block.held) {
    if (Fetched(DMA1_Stream0->NDTR, 2 * offset, 4 * size, 2)) {
      ++hold.g2;
    }
    if (DMA1->LISR & (offset ? DMA_LISR_HTIF1 : DMA_LISR_TCIF1)) {
      ++hold.pending;
    }
    if (block.store == STORE_LATE) {
      if (!hold.late) {
        hold.first_late = hold.blocks;
      }
      ++hold.late;
    } else if (block.store == STORE_AMBIGUOUS) {
      ++hold.ambiguous;
    } else {
      ++hold.on_time;
    }
    ++hold.blocks;
  }
  block.held = false;
  block.store = STORE_ON_TIME;
}

#endif  // CPU_CEILING

}  // namespace cpu_ceiling
}  // namespace beads

#endif  // BEADS_CPU_CEILING_H_
