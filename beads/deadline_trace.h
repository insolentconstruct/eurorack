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
// Deadline trace: when each block's output stores ran against the DAC DMA.

#ifndef BEADS_DEADLINE_TRACE_H_
#define BEADS_DEADLINE_TRACE_H_

#ifdef DEADLINE_TRACE
#include <stdint.h>

#include "stmlib/stmlib.h"

#ifndef DEADLINE_TRACE_DELAY
#define DEADLINE_TRACE_DELAY 0
#endif  // DEADLINE_TRACE_DELAY

namespace beads {
namespace dwt_calibration {
namespace trace {

const uint32_t kMagic = 0x444c5452;
const uint32_t kVersion = 1;
const uint8_t kRequest = 4;
const unsigned kRecords = 768;
const unsigned kPostTrigger = 384;
// As kPanelBlocks: the stats window.
const unsigned kWindow = 240000;
// 480 MHz over 4000 blocks per second, at every codec rate.
const uint32_t kBlockCycles = 120000;
// Below one DMA lap at every rate, so NDTR differences cannot alias.
const uint32_t kLapCycles = 210000;
const uint32_t kSnapshotBound = 200;
const int8_t kThreshold = 1;
const unsigned kSlackBins = 20;
const unsigned kSaiBits = 7;
// SAI SR bits tracked (all but FREQ), and those that trigger (not MUTEDET).
const uint32_t kSaiTracked = 0x77;
const uint32_t kSaiFaults = 0x75;
const uint32_t kNone = 0xffffffff;

const uint32_t kCyccnt = 0xe0001004;
const uint32_t kDmaLisr = 0x40020000;
const uint32_t kTxNdtr = 0x40020014;
const uint32_t kRxNdtr = 0x4002002c;
const uint32_t kSaiASr = 0x40015818;
const uint32_t kSaiBSr = 0x40015838;
const uint32_t kTcif1 = 1 << 11;
const uint32_t kHtif1 = 1 << 10;

enum State {
  STATE_IDLE,
  STATE_PENDING,
  STATE_RECORDING,
  STATE_RING_FROZEN,
  STATE_COMPLETE
};

enum Reason {
  REASON_NONE,
  REASON_WINDOW_END,
  REASON_ABORT,
  REASON_GEOMETRY
};

enum Class {
  CLASS_ON_TIME,
  CLASS_STALE,
  CLASS_AMBIGUOUS,
  CLASS_INVALID
};

enum Flag {
  FLAG_SILENCE = 1,
  FLAG_NO_STORE = 2,
  FLAG_SNAPSHOT_LONG = 4,
  FLAG_COALESCED = 8,
  FLAG_CADENCE = 16,
  FLAG_LAP = 32,
  FLAG_SPURIOUS = 64,
  FLAG_TRIGGER = 128
};

const uint8_t kInvalidFlags = FLAG_NO_STORE | FLAG_SNAPSHOT_LONG |
    FLAG_COALESCED | FLAG_CADENCE | FLAG_LAP | FLAG_SPURIOUS;

struct Record {
  uint32_t t_entry;
  uint32_t t_store_begin;
  uint32_t t_store_end;
  uint32_t t_exit;
  uint8_t tx_ndtr[3];
  uint8_t rx_ndtr_entry;
  uint8_t lisr_entry;
  uint8_t lisr_exit;
  // Bit 0 the half; bits 1-2 the firmware's class.
  uint8_t half;
  uint8_t flags;
  uint16_t p0_cycles_pre;
  uint16_t p0_cycles_store;
  uint8_t p0_count_pre;
  uint8_t p0_count_store;
  uint16_t sequence;
};

struct Totals {
  int32_t min_slack;
  uint32_t on_time;
  uint32_t stale;
  uint32_t ambiguous;
  uint32_t invalid;
  uint32_t coalesced;
  uint32_t cadence;
  uint32_t lap;
  uint32_t spurious;
  uint32_t no_store;
  uint32_t snapshot_long;
  uint32_t silence;
  uint32_t p0_count;
  uint32_t padding;
  uint64_t p0_cycles;
  uint32_t histogram[kSlackBins];
};

struct Header {
  uint32_t magic;
  uint32_t version;
  uint32_t sequence;
  uint8_t state;
  uint8_t reason;
  uint8_t window_invalid;
  int8_t threshold;
  uint32_t generation;
  uint32_t sample_rate;
  uint32_t block_size;
  uint32_t buffer_words;
  uint32_t write_index;
  uint32_t total;
  uint32_t trigger_index;
  uint32_t delay;
  uint32_t sai_baseline[2];
  uint32_t sai_first[2 * kSaiBits];
  Totals totals;
};

struct Region {
  Header header;
  Record ring[kRecords];
};

STATIC_ASSERT(sizeof(Record) == 32, trace_record_size);
STATIC_ASSERT(sizeof(Totals) == 144, trace_totals_size);
STATIC_ASSERT(sizeof(Header) == 256, trace_header_size);
STATIC_ASSERT(offsetof(Header, totals) == 112, trace_totals_offset);
STATIC_ASSERT(sizeof(Region) == 24832, trace_region_size);

// The live state, in DTCM: the priority-0 counters are shared with SysTick
// and TIM8; everything else is the codec interrupt's.
struct Live {
  volatile bool active;
  bool open;
  bool begun;
  // Set by Abort until the codec runs a callback again: no request arms then.
  volatile bool blocked;
  uint32_t previous_entry;
  volatile uint32_t p0_cycles;
  volatile uint32_t p0_count;
  uint32_t p0_cycles_mark;
  uint32_t p0_count_mark;
  uint32_t p0_cycles_window;
  uint32_t p0_count_window;
  uint32_t total;
  uint32_t write_index;
  uint32_t trigger_index;
  uint32_t post_trigger;
  uint32_t block_size;
  uint32_t sai_baseline[2];
  uint32_t sai_first[2 * kSaiBits];
  Record record;
  Totals totals;
};

extern volatile Region region;
extern Live live;

struct Snapshot {
  uint32_t cycles;
  uint32_t tx_ndtr;
  uint32_t rx_ndtr;
  uint32_t lisr;
  uint32_t p0_cycles;
  uint32_t p0_count;
  bool long_window;
};

inline volatile uint32_t& Register(uint32_t address) {
  return *reinterpret_cast<volatile uint32_t*>(address);
}

inline uint32_t Mask() {
  uint32_t primask;
  __asm volatile("mrs %0, primask\n\tcpsid i" : "=r" (primask) :: "memory");
  return primask;
}

inline void Unmask(uint32_t primask) {
  __asm volatile("msr primask, %0" :: "r" (primask) : "memory");
}

inline void Take(Snapshot* s) {
  const uint32_t primask = Mask();
  s->cycles = Register(kCyccnt);
  s->tx_ndtr = Register(kTxNdtr);
  s->rx_ndtr = Register(kRxNdtr);
  s->lisr = Register(kDmaLisr);
  s->p0_cycles = live.p0_cycles;
  s->p0_count = live.p0_count;
  s->long_window = Register(kCyccnt) - s->cycles > kSnapshotBound;
  Unmask(primask);
}

// TCIF1 HTIF1 TEIF1 DMEIF1 TCIF0 HTIF0 TEIF0 DMEIF0, from the top bit.
inline uint8_t PackLisr(uint32_t lisr) {
  return ((lisr >> 4) & 0xf0) | ((lisr >> 2) & 0x0f);
}

inline uint8_t Saturate8(uint32_t x) { return x > 0xff ? 0xff : x; }
inline uint16_t Saturate16(uint32_t x) { return x > 0xffff ? 0xffff : x; }

inline void BeginWrite() {
  region.header.sequence = region.header.sequence + 1;
  __asm volatile("dmb" ::: "memory");
}

inline void EndWrite() {
  __asm volatile("dmb" ::: "memory");
  region.header.sequence = region.header.sequence + 1;
}

inline void Publish(uint8_t state, uint8_t reason) {
  BeginWrite();
  region.header.write_index = live.write_index;
  region.header.total = live.total;
  region.header.trigger_index = live.trigger_index;
  for (unsigned i = 0; i < 2 * kSaiBits; ++i) {
    region.header.sai_first[i] = live.sai_first[i];
  }
  const uint32_t* source = reinterpret_cast<const uint32_t*>(&live.totals);
  volatile uint32_t* destination =
      reinterpret_cast<volatile uint32_t*>(&region.header.totals);
  for (unsigned i = 0; i < sizeof(Totals) / 4; ++i) {
    destination[i] = source[i];
  }
  region.header.reason = reason;
  region.header.window_invalid = reason != REASON_WINDOW_END;
  region.header.state = state;
  EndWrite();
}

inline void Init() {
  volatile uint32_t* words = reinterpret_cast<volatile uint32_t*>(&region);
  for (unsigned i = 0; i < sizeof(Header) / 4; ++i) {
    words[i] = 0;
  }
  // Live is in the uninitialized .dtcm section: other RAM addresses stay put.
  uint32_t* state = reinterpret_cast<uint32_t*>(&live);
  for (unsigned i = 0; i < sizeof(Live) / 4; ++i) {
    state[i] = 0;
  }
  region.header.magic = kMagic;
  region.header.version = kVersion;
  region.header.threshold = kThreshold;
  region.header.delay = DEADLINE_TRACE_DELAY;
  region.header.state = STATE_IDLE;
}

// Opens the record of the block being handled from a snapshot.
inline void Open(const Snapshot& s, bool cadence) {
  Record& r = live.record;
  const uint32_t ht_tc = s.lisr & (kTcif1 | kHtif1);
  const uint32_t interval = s.cycles - live.previous_entry;
  live.previous_entry = s.cycles;
  r.t_entry = s.cycles;
  r.t_store_begin = 0;
  r.t_store_end = 0;
  r.tx_ndtr[0] = s.tx_ndtr;
  r.tx_ndtr[1] = 0;
  r.tx_ndtr[2] = 0;
  r.rx_ndtr_entry = s.rx_ndtr;
  r.lisr_entry = PackLisr(s.lisr);
  r.half = ht_tc & kTcif1 ? 1 : 0;
  r.flags = FLAG_NO_STORE;
  if (s.long_window) r.flags |= FLAG_SNAPSHOT_LONG;
  if (ht_tc == (kTcif1 | kHtif1)) r.flags |= FLAG_COALESCED;
  if (!ht_tc) r.flags |= FLAG_SPURIOUS;
  if (cadence &&
      (interval < kBlockCycles / 2 || interval > kBlockCycles * 3 / 2)) {
    r.flags |= FLAG_CADENCE;
  }
  r.p0_cycles_pre = 0;
  r.p0_cycles_store = 0;
  r.p0_count_pre = 0;
  r.p0_count_store = 0;
  live.p0_cycles_mark = s.p0_cycles;
  live.p0_count_mark = s.p0_count;
  live.begun = false;
  live.open = true;
}

// Request 4, from the stats callback that clears the stats: this block is
// record 0, so both windows start here. Its snapshot is late by the callback's
// prologue and its cadence cannot be checked.
inline void Request() {
  if (live.blocked) {
    return;
  }
  live.active = false;
  live.open = false;
  live.total = 0;
  live.write_index = 0;
  live.trigger_index = kNone;
  live.post_trigger = 0;
  live.block_size = 0;
  for (unsigned i = 0; i < 2 * kSaiBits; ++i) {
    live.sai_first[i] = kNone;
  }
  uint32_t* totals = reinterpret_cast<uint32_t*>(&live.totals);
  for (unsigned i = 0; i < sizeof(Totals) / 4; ++i) {
    totals[i] = 0;
  }
  live.totals.min_slack = 0x7fffffff;
  Snapshot s;
  Take(&s);
  live.p0_cycles_window = s.p0_cycles;
  live.p0_count_window = s.p0_count;
  live.sai_baseline[0] = Register(kSaiASr);
  live.sai_baseline[1] = Register(kSaiBSr);
  BeginWrite();
  region.header.generation = region.header.generation + 1;
  region.header.state = STATE_RECORDING;
  region.header.reason = REASON_NONE;
  region.header.window_invalid = 0;
  region.header.sample_rate = 0;
  region.header.block_size = 0;
  region.header.buffer_words = 0;
  region.header.write_index = 0;
  region.header.total = 0;
  region.header.trigger_index = kNone;
  region.header.sai_baseline[0] = live.sai_baseline[0];
  region.header.sai_baseline[1] = live.sai_baseline[1];
  EndWrite();
  Open(s, false);
  live.active = true;
}

// Codec Pause, Stop and Restart, before the callback or the DMA stops; they
// also block requests until the callback is installed again.
inline void Abort(uint8_t reason, bool block = true) {
  const uint32_t primask = Mask();
  live.blocked = live.blocked || block;
  if (live.active) {
    live.active = false;
    live.open = false;
    Publish(STATE_COMPLETE, reason);
  }
  Unmask(primask);
}

// Codec Start, Restart and Resume, just before the callback is installed:
// until then no callback runs, so no request can be refused.
inline void Unblock() {
  live.blocked = false;
  __asm volatile("" ::: "memory");
}

inline void Entry() {
  if (!live.active) {
    return;
  }
  Snapshot s;
  Take(&s);
  Open(s, true);
}

// Codec::Fill: the half actually written. A flag raised between the entry
// snapshot and the dispatch changes it; that block is coalesced.
inline void Dispatch(size_t half) {
  if (!live.open) {
    return;
  }
  Record& r = live.record;
  if (half != r.half) {
    r.half = half;
    r.flags |= FLAG_COALESCED;
  }
}

// Immediately before the output stores; the compile-time delay is Seed3's.
inline void StoreBegin(size_t size, bool silence = false) {
  if (!live.open) {
    return;
  }
#if DEADLINE_TRACE_DELAY > 0
  const uint32_t delay_start = Register(kCyccnt);
  while (Register(kCyccnt) - delay_start < DEADLINE_TRACE_DELAY) { }
#endif  // DEADLINE_TRACE_DELAY
  if (live.block_size != size) {
    if (live.block_size) {
      Abort(REASON_GEOMETRY);
      return;
    }
    live.block_size = size;
    BeginWrite();
    region.header.sample_rate = size * 4000;
    region.header.block_size = size;
    region.header.buffer_words = size * 4;
    EndWrite();
  }
  Snapshot s;
  Take(&s);
  Record& r = live.record;
  r.t_store_begin = s.cycles;
  r.tx_ndtr[1] = s.tx_ndtr;
  live.begun = true;
  if (s.long_window) r.flags |= FLAG_SNAPSHOT_LONG;
  if (silence) r.flags |= FLAG_SILENCE;
  r.p0_cycles_pre = Saturate16(s.p0_cycles - live.p0_cycles_mark);
  r.p0_count_pre = Saturate8(s.p0_count - live.p0_count_mark);
  live.p0_cycles_mark = s.p0_cycles;
  live.p0_count_mark = s.p0_count;
}

// Immediately after the output stores, once they have completed.
inline void StoreEnd() {
  if (!live.open) {
    return;
  }
  __asm volatile("dsb" ::: "memory");
  Snapshot s;
  Take(&s);
  Record& r = live.record;
  r.t_store_end = s.cycles;
  r.tx_ndtr[2] = s.tx_ndtr;
  if (s.long_window) r.flags |= FLAG_SNAPSHOT_LONG;
  if (live.begun) r.flags &= ~FLAG_NO_STORE;
  r.p0_cycles_store = Saturate16(s.p0_cycles - live.p0_cycles_mark);
  r.p0_count_store = Saturate8(s.p0_count - live.p0_count_mark);
}

// Lower-bound slack in DMA words: negative once the half's first word may
// have been read before its store completed.
inline uint8_t Classify(const Record& r, unsigned n, int32_t* slack) {
  if (r.flags & kInvalidFlags) {
    return CLASS_INVALID;
  }
  const unsigned h = n / 2;
  const unsigned base = h * (r.half & 1);
  const unsigned tx_entry = (n - r.tx_ndtr[0]) % n;
  const unsigned tx_begin = (n - r.tx_ndtr[1]) % n;
  const unsigned tx_end = (n - r.tx_ndtr[2]) % n;
  const unsigned entry = h + (tx_entry + 2 * n - base - h) % n;
  const unsigned begin = entry + (tx_begin + n - tx_entry) % n;
  const unsigned end = entry + (tx_end + n - tx_entry) % n;
  *slack = int32_t(n) - 1 - int32_t(end);
  if (*slack >= 0) {
    return CLASS_ON_TIME;
  }
  return begin >= n + 1 ? CLASS_STALE : CLASS_AMBIGUOUS;
}

inline void Exit() {
  if (!live.open) {
    return;
  }
  live.open = false;
  Snapshot s;
  Take(&s);
  Record& r = live.record;
  r.t_exit = s.cycles;
  r.lisr_exit = PackLisr(s.lisr);
  if (s.long_window) r.flags |= FLAG_SNAPSHOT_LONG;
  if (!(r.flags & FLAG_NO_STORE) && r.t_store_end - r.t_entry >= kLapCycles) {
    r.flags |= FLAG_LAP;
  }
  r.sequence = live.total;

  Totals& t = live.totals;
  int32_t slack = 0;
  const uint8_t c = Classify(r, live.block_size * 4, &slack);
  r.half |= c << 1;
  bool trigger = false;
  if (c == CLASS_INVALID) {
    ++t.invalid;
    if (r.flags & FLAG_COALESCED) ++t.coalesced;
    if (r.flags & FLAG_CADENCE) ++t.cadence;
    if (r.flags & FLAG_LAP) ++t.lap;
    if (r.flags & FLAG_SPURIOUS) ++t.spurious;
    if (r.flags & FLAG_NO_STORE) ++t.no_store;
    if (r.flags & FLAG_SNAPSHOT_LONG) ++t.snapshot_long;
    trigger = r.flags & (FLAG_COALESCED | FLAG_CADENCE);
  } else {
    if (c == CLASS_ON_TIME) {
      ++t.on_time;
    } else if (c == CLASS_STALE) {
      ++t.stale;
    } else {
      ++t.ambiguous;
    }
    if (slack < t.min_slack) t.min_slack = slack;
    const int32_t bin = slack < 0 ? 0 : (slack + 1 < int32_t(kSlackBins)
        ? slack + 1 : kSlackBins - 1);
    ++t.histogram[bin];
    trigger = slack <= kThreshold;
  }
  if (r.flags & FLAG_SILENCE) ++t.silence;

  const uint32_t sai[2] = { Register(kSaiASr), Register(kSaiBSr) };
  for (unsigned b = 0; b < 2; ++b) {
    const uint32_t raised = sai[b] & ~live.sai_baseline[b] & kSaiTracked;
    if (raised) {
      for (unsigned i = 0; i < kSaiBits; ++i) {
        if ((raised & (1 << i)) && live.sai_first[b * kSaiBits + i] == kNone) {
          live.sai_first[b * kSaiBits + i] = live.total;
          trigger = trigger || (kSaiFaults & (1 << i));
        }
      }
    }
  }

  t.p0_cycles += s.p0_cycles - live.p0_cycles_window;
  t.p0_count += s.p0_count - live.p0_count_window;
  live.p0_cycles_window = s.p0_cycles;
  live.p0_count_window = s.p0_count;

  if (trigger && live.trigger_index == kNone) {
    r.flags |= FLAG_TRIGGER;
    live.trigger_index = live.total;
    live.post_trigger = kPostTrigger + 1;
  }
  const bool frozen = live.trigger_index != kNone && !live.post_trigger;
  BeginWrite();
  if (r.flags & FLAG_TRIGGER) {
    region.header.trigger_index = live.trigger_index;
  }
  if (!frozen) {
    const uint32_t* source = reinterpret_cast<const uint32_t*>(&r);
    volatile uint32_t* destination = reinterpret_cast<volatile uint32_t*>(
        &region.ring[live.write_index]);
    for (unsigned i = 0; i < sizeof(Record) / 4; ++i) {
      destination[i] = source[i];
    }
    live.write_index = live.write_index + 1 == kRecords
        ? 0 : live.write_index + 1;
    if (live.post_trigger && !--live.post_trigger) {
      region.header.state = STATE_RING_FROZEN;
    }
  }
  ++live.total;
  region.header.total = live.total;
  region.header.write_index = live.write_index;
  EndWrite();
  if (live.total == kWindow) {
    live.active = false;
    Publish(STATE_COMPLETE, REASON_WINDOW_END);
  }
}

// Priority-0 time while the trace records; SysTick and TIM8 cannot nest.
class Preemption {
 public:
  Preemption() : active_(live.active), start_(0) {
    if (active_) {
      start_ = Register(kCyccnt);
    }
  }
  ~Preemption() {
    if (active_) {
      live.p0_cycles = live.p0_cycles + (Register(kCyccnt) - start_);
      live.p0_count = live.p0_count + 1;
    }
  }

 private:
  bool active_;
  uint32_t start_;

  DISALLOW_COPY_AND_ASSIGN(Preemption);
};

}  // namespace trace
}  // namespace dwt_calibration
}  // namespace beads

#endif  // DEADLINE_TRACE

#endif  // BEADS_DEADLINE_TRACE_H_
