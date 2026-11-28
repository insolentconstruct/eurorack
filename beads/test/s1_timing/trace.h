// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// S1 bench app capture, ABI 1.

#ifndef BEADS_TEST_S1_TIMING_TRACE_H_
#define BEADS_TEST_S1_TIMING_TRACE_H_

#include "stmlib/stmlib.h"

namespace beads {
namespace s1_timing {

const size_t kTraceBytes = 15 * 1024;
const uint32_t kTraceMagic = 0x53315431;
const uint32_t kSnapshotPeriodUs = 20000;
const uint8_t kFlagMiss = 1;
const uint8_t kFlagAssignFeedback = 2;
const uint8_t kFlagSeedEdge = 4;
const uint8_t kFlagRings = 8;
const uint8_t kFlagFreeze = 16;
const uint8_t kFlagSeedPatched = 32;
const uint8_t kFlagDrone = 64;
const uint8_t kFlagDroneUpdate = 128;

struct Frame {
  uint32_t seq;
  uint32_t cycles;
  uint16_t codec_hz;
  uint16_t synth_hz;
  uint8_t block_frames;
  uint8_t sai_a;
  uint8_t sai_b;
  uint8_t flags;
  uint8_t bank;
  uint8_t slot;
  uint8_t quality;
  uint8_t placement;
  uint16_t feedback_base_q16;
  uint16_t paths;
  int8_t models[6];
  uint16_t reserved;
  uint32_t init_serials[6];
};

struct Snapshot {
  uint32_t generation;
  uint32_t abi;
  uint32_t boot_lo;
  uint32_t boot_hi;
  uint32_t tick_us;
  uint32_t tick_hi;
  uint32_t next_seq;
  uint32_t codec_hz;
  uint32_t synth_hz;
  uint32_t block_frames;
  uint32_t core_hz;
  uint32_t sai_a;
  uint32_t sai_b;
  uint32_t bank;
  uint32_t slot;
  uint32_t assign_feedback;
  uint32_t feedback_base_q16;
  uint32_t quality;
  uint32_t placement;
  uint32_t requested_synth;
  uint32_t effective_synth;
  uint32_t wave;
  uint32_t plts;
  uint32_t terr;
  uint32_t cue_armed_since_boot;
  uint32_t cue_count;
};

struct Header {
  uint32_t magic;
  uint32_t boot_lo;
  uint32_t boot_hi;
  uint64_t first_tick_us;
  uint64_t end_tick_us;
  uint32_t count;
  uint32_t dropped;
  uint32_t paused_setup_cycles;
  uint32_t seal;
} __attribute__((packed, aligned(4)));

const size_t kTraceCapacity = (kTraceBytes - sizeof(Header) - 4) / sizeof(Frame);
const uint32_t kSealOffset = kTraceBytes - 4;

struct Chunk {
  Header header;
  Frame frames[kTraceCapacity];
};

struct Mailbox {
  uint32_t request;
  uint32_t count;
  uint32_t codec_hz;
  uint32_t synth_hz;
  uint32_t status;
};

extern "C" {
extern volatile Snapshot s1_snapshot;
extern volatile Mailbox s1_mailbox;
extern uint8_t s1_trace[];
extern volatile uint32_t s1_observer_cycles;
extern volatile uint32_t s1_callback_cycles;
extern volatile uint32_t s1_irq_cycles;
extern volatile uint32_t s1_callback_peak;
extern volatile uint32_t s1_irq_peak;
}

extern Frame frame;
extern uint32_t paused_setup_cycles;

void Init();
void BootStatus(uint32_t wave, uint32_t plts, uint32_t terr);
void Publish(uint32_t requested_synth, uint32_t effective_synth);
uint32_t Poll(bool blanking, bool startup_prepared, bool can_change_mode,
    bool codec_running);
void SetStatus(uint32_t status);
bool armed();
void FaultCue(bool active);
void Begin(uint32_t codec_hz, size_t block_frames);
enum DmaResult {
  kDmaFilled = 1,
  kDmaMiss = 2,
  kDmaLost = 4,
  kDmaLostShift = 3
};
void ResetDmaClock();
uint32_t DmaDiagnostics(uint32_t entry, uint32_t result);
void End(uint32_t irq_cycles, uint32_t dma_result);
extern uint32_t fill_cycles;
extern uint32_t callback_cycles;
extern uint32_t callback_observers;
const uint8_t kPlacementChord = 4;
const uint8_t kPlacementBloomFeedback = 5;

inline uint32_t SubtractCycles(uint32_t cycles, uint32_t overhead) {
  return cycles > overhead ? cycles - overhead : 0;
}
uint32_t codec_override();
uint32_t synth_rate();
// Request 8: the bench's unpatched-inputs posture, and its settled report.
bool no_input();
void Settled(bool reachable, bool settled);
uint32_t SynthCode(uint32_t engine);

inline uint32_t Cycles() {
  return *reinterpret_cast<volatile uint32_t*>(0xe0001004);
}

extern uint32_t observer_cycles;

class Observe {
 public:
  Observe() {
    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(mask_) :: "memory");
    start_ = Stamp();
  }
  ~Observe() {
    const uint32_t end = Stamp();
    __asm__ volatile("msr primask, %0" :: "r"(mask_), "r"(end) : "memory");
    observer_cycles += end - start_;
  }
 private:
  static uint32_t Stamp() {
    uint32_t tick;
    __asm__ volatile("ldr %0, [%1]" : "=r"(tick)
        : "r"(0xe0001004u) : "memory");
    return tick;
  }
  uint32_t mask_;
  uint32_t start_;
};

}  // namespace s1_timing
}  // namespace beads

#endif  // BEADS_TEST_S1_TIMING_TRACE_H_
