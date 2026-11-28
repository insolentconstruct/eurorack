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

#include "beads/test/s1_timing/trace.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>

#include <stm32h7xx_hal.h>

namespace beads {
namespace s1_timing {

STATIC_ASSERT(sizeof(Snapshot) == 104, s1_snapshot_size);
STATIC_ASSERT(sizeof(Header) == 44, s1_header_size);
STATIC_ASSERT(offsetof(Header, first_tick_us) == 12, s1_first_tick_offset);
STATIC_ASSERT(sizeof(Frame) == 56, s1_frame_size);
STATIC_ASSERT(offsetof(Frame, init_serials) == 32, s1_serials_offset);
STATIC_ASSERT(sizeof(Chunk) <= kSealOffset, s1_chunk_fits);

volatile Snapshot s1_snapshot;
volatile Mailbox s1_mailbox;
volatile uint32_t s1_observer_cycles;
volatile uint32_t s1_callback_cycles;
volatile uint32_t s1_irq_cycles;
volatile uint32_t s1_callback_peak;
volatile uint32_t s1_irq_peak;
extern "C" {
volatile uint32_t s1_callback_gate_cycles;
volatile uint32_t s1_irq_gate_cycles;
volatile uint32_t s1_callback_gate_peak;
volatile uint32_t s1_irq_gate_peak;
}
Frame frame;
uint32_t observer_cycles;
uint32_t paused_setup_cycles;

static Frame latest;
static uint32_t next_seq;
static uint32_t last_publish;
static uint32_t last_tick;
static uint32_t tick_hi;
static uint32_t boot_lo;
static uint32_t boot_hi;
static uint32_t codec_hz;
static volatile bool cue_active;
static volatile uint32_t cue_count;
static volatile uint32_t synth_hz = 48000;
static volatile uint32_t target;
static uint32_t seal;
static volatile bool flush_pending;
// A late request 6 still reports this seal, until the next arm or startup
// preparation starts a new acquisition.
static bool published;
static uint64_t begin_tick;
// A sealed capture is a ring of `ring` callbacks; a latched one keeps `pre`
// callbacks before its trigger flag and seals once `stop` are written.
static uint32_t ring;
static uint32_t pre;
static uint8_t trigger;
static uint32_t stop;
static uint32_t written;
static uint32_t ticks[kTraceCapacity];
static uint8_t lost[kTraceCapacity];
static volatile bool forced_no_input;
static bool settle_pending;

static Chunk& chunk = *reinterpret_cast<Chunk*>(s1_trace);
static volatile uint32_t& external_seal =
    *reinterpret_cast<volatile uint32_t*>(s1_trace + kSealOffset);

static uint64_t Now() {
  const uint32_t mask = __get_PRIMASK();
  __disable_irq();
  const uint32_t tick = TIM2->CNT;
  if (tick < last_tick) {
    ++tick_hi;
  }
  last_tick = tick;
  const uint32_t hi = tick_hi;
  __set_PRIMASK(mask);
  return (uint64_t(hi) << 32) | tick;
}

static uint32_t RandomWord() {
  const uint32_t start = Cycles();
  while (Cycles() - start < SystemCoreClock / 500) {
    const uint32_t status = RNG->SR;
    if (status & (RNG_SR_CECS | RNG_SR_SECS)) {
      break;
    }
    if (status & RNG_SR_DRDY) {
      return RNG->DR;
    }
  }
  return 0;
}

void Init() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  __HAL_RCC_TIM2_CLK_ENABLE();
  TIM2->CR1 = 0;
  TIM2->PSC = HAL_RCC_GetPCLK1Freq() * 2 / 1000000 - 1;
  TIM2->ARR = 0xffffffff;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 = TIM_CR1_CEN;

  __HAL_RCC_HSI48_ENABLE();
  const uint32_t hsi_start = Cycles();
  while (!__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) &&
      Cycles() - hsi_start < SystemCoreClock / 500) { }
  __HAL_RCC_RNG_CONFIG(RCC_RNGCLKSOURCE_HSI48);
  __HAL_RCC_RNG_CLK_ENABLE();
  RNG->CR = RNG_CR_RNGEN;
  boot_lo = RandomWord();
  boot_hi = RandomWord();
  RNG->CR = 0;
  __HAL_RCC_RNG_CLK_DISABLE();
  __HAL_RCC_HSI48_DISABLE();

  published = false;
  external_seal = 0;
  SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(
      s1_trace + kTraceBytes - 32), 32);
  memset(latest.models, -1, sizeof(latest.models));
  s1_snapshot.cue_armed_since_boot = 1;
}

void BootStatus(uint32_t wave, uint32_t plts, uint32_t terr) {
  s1_snapshot.wave = wave;
  s1_snapshot.plts = plts;
  s1_snapshot.terr = terr;
}

uint32_t SynthCode(uint32_t engine) {
  return (engine + 1) & 3;
}

void Publish(uint32_t requested_synth, uint32_t effective_synth) {
  const uint64_t tick = Now();
  if (!(boot_lo || boot_hi) || (s1_snapshot.generation &&
      uint32_t(tick) - last_publish < kSnapshotPeriodUs)) {
    return;
  }
  last_publish = uint32_t(tick);
  __disable_irq();
  const Frame sample = latest;
  const uint32_t next = next_seq;
  __enable_irq();

  ++s1_snapshot.generation;
  __DMB();
  s1_snapshot.abi = 1;
  s1_snapshot.boot_lo = boot_lo;
  s1_snapshot.boot_hi = boot_hi;
  s1_snapshot.tick_us = tick;
  s1_snapshot.tick_hi = tick >> 32;
  s1_snapshot.next_seq = next;
  s1_snapshot.codec_hz = sample.codec_hz;
  s1_snapshot.synth_hz = sample.synth_hz;
  s1_snapshot.block_frames = sample.block_frames;
  s1_snapshot.core_hz = SystemCoreClock;
  s1_snapshot.sai_a = SAI1_Block_A->SR;
  s1_snapshot.sai_b = SAI1_Block_B->SR;
  s1_snapshot.cue_count = cue_count;
  s1_snapshot.bank = sample.bank & 0x7f;
  s1_snapshot.slot = sample.slot;
  s1_snapshot.assign_feedback = bool(sample.flags & kFlagAssignFeedback);
  s1_snapshot.feedback_base_q16 = sample.feedback_base_q16;
  s1_snapshot.quality = sample.quality;
  s1_snapshot.placement = sample.placement;
  s1_snapshot.requested_synth = SynthCode(requested_synth);
  s1_snapshot.effective_synth = SynthCode(effective_synth);
  __DMB();
  ++s1_snapshot.generation;
}

static bool ValidRate(uint32_t rate) {
  return rate == 24000 || rate == 32000 || rate == 48000;
}

// Puts the retained callbacks in order and completes the header.
static void Seal() {
  Header& header = chunk.header;
  const uint32_t count = std::min(written, ring);
  const uint32_t first = (written - count) % ring;
  std::rotate(chunk.frames, chunk.frames + first, chunk.frames + ring);
  std::rotate(ticks, ticks + first, ticks + ring);
  std::rotate(lost, lost + first, lost + ring);
  uint32_t dropped = lost[0];
  for (uint32_t i = 1; i < count; ++i) {
    dropped += lost[i] + (ticks[i] - ticks[i - 1] >= 375 ? 1 : 0);
  }
  header.first_tick_us = header.end_tick_us - 250 -
      (ticks[count - 1] - ticks[0]);
  header.count = count;
  header.dropped = dropped;
  seal += 2;
  header.seal = seal;
}

static void Arm(uint32_t count, uint32_t before, uint32_t status) {
  published = false;
  external_seal = 0;
  SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(
      s1_trace + kTraceBytes - 32), 32);
  chunk.header.magic = kTraceMagic;
  chunk.header.boot_lo = boot_lo;
  chunk.header.boot_hi = boot_hi;
  chunk.header.count = 0;
  chunk.header.dropped = 0;
  chunk.header.paused_setup_cycles = 0;
  chunk.header.seal = 0;
  ring = count;
  pre = before;
  trigger = status == 9 ? kFlagMiss : kFlagSeedEdge;
  stop = before ? 0 : count;
  written = 0;
  s1_callback_peak = 0;
  s1_irq_peak = 0;
  s1_callback_gate_peak = 0;
  s1_irq_gate_peak = 0;
  s1_mailbox.status = status;
  __DMB();
  target = count;
}

static void Flush() {
  Seal();
  const size_t bytes = sizeof(Header) + chunk.header.count * sizeof(Frame);
  SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(s1_trace), bytes);
  external_seal = seal;
  SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(
      s1_trace + kTraceBytes - 32), 32);
  flush_pending = false;
  published = true;
  s1_mailbox.status = 2;
}

uint32_t Poll(bool blanking, bool startup_prepared, bool can_force_no_input) {
  const uint32_t request = s1_mailbox.request;
  if (request) {
    settle_pending = false;
  }
  if (request == 6) {
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    const bool publish = flush_pending;
    target = 0;
    __set_PRIMASK(mask);
    if (publish) {
      Flush();
    }
    if (!published) {
      return request;
    }
    SetStatus(2);
    return 0;
  }
  if (flush_pending) {
    Flush();
  }
  if (!request) {
    return 0;
  }
  const uint32_t count = s1_mailbox.count;
  const uint32_t codec = s1_mailbox.codec_hz;
  const uint32_t synth = s1_mailbox.synth_hz;
  const bool idle = !target && !flush_pending;
  const bool capture = idle && !blanking && (boot_lo || boot_hi) &&
      count >= 2 && count <= kTraceCapacity;
  if (request == 1 && capture) {
    Arm(count, 0, 1);
  } else if ((request == 7 || request == 9) && capture && codec >= 1 &&
      codec < count) {
    Arm(count, codec, request);
  } else if (request == 8 && (count == 0 ||
      (count == 1 && idle && !startup_prepared && can_force_no_input))) {
    forced_no_input = count;
    if (count == 1 || (idle && s1_mailbox.status != 2)) {
      // Startup preparation has paused the codec. Clear the policy now;
      // the regular mode machinery will apply it after startup resumes.
      settle_pending = !startup_prepared;
      s1_mailbox.status = settle_pending ? 8 : 9;
    }
  } else if (request == 2 && !target && !flush_pending && !startup_prepared &&
      (!codec || ValidRate(codec)) && ValidRate(synth)) {
    codec_hz = codec;
    synth_hz = synth;
    s1_mailbox.status = 3;
  } else if ((request == 4 && !target && !flush_pending) ||
      (request == 5 && target && !flush_pending)) {
    return request;
  } else {
    s1_mailbox.status = 0xffffffff;
  }
  __DMB();
  s1_mailbox.request = 0;
  return 0;
}

void SetStatus(uint32_t status) {
  if (status == 4) {
    published = false;
  }
  s1_mailbox.status = status;
  __DMB();
  s1_mailbox.request = 0;
}

bool armed() { return target != 0; }

void FaultCue(bool active) {
  if (active && !cue_active) {
    ++cue_count;
  }
  cue_active = active;
}

uint32_t codec_override() { return codec_hz; }
uint32_t synth_rate() { return synth_hz; }
bool no_input() { return forced_no_input; }

void Settled(bool settled) {
  if (settle_pending && settled && !s1_mailbox.request) {
    settle_pending = false;
    s1_mailbox.status = 9;
  }
}

void Begin(uint32_t rate, size_t block_frames) {
  memset(&frame, 0, sizeof(frame));
  memset(frame.models, -1, sizeof(frame.models));
  frame.seq = next_seq++;
  frame.codec_hz = rate;
  frame.block_frames = block_frames;
  observer_cycles = 0;
  if (target) {
    begin_tick = Now();
  }
}

uint32_t fill_cycles;
uint32_t callback_cycles;
uint32_t callback_observers;

void End(uint32_t irq_cycles, uint32_t dma_result) {
  frame.cycles = SubtractCycles(fill_cycles, observer_cycles);
  s1_observer_cycles = observer_cycles;
  s1_callback_cycles = callback_cycles;
  s1_irq_cycles = irq_cycles;
  const uint32_t callback_gate =
      SubtractCycles(callback_cycles, callback_observers);
  const uint32_t irq_gate =
      SubtractCycles(irq_cycles, observer_cycles);
  s1_callback_gate_cycles = callback_gate;
  s1_irq_gate_cycles = irq_gate;
  if (target) {
    if (callback_cycles > s1_callback_peak) {
      s1_callback_peak = callback_cycles;
    }
    if (irq_cycles > s1_irq_peak) {
      s1_irq_peak = irq_cycles;
    }
    if (callback_gate > s1_callback_gate_peak) {
      s1_callback_gate_peak = callback_gate;
    }
    if (irq_gate > s1_irq_gate_peak) {
      s1_irq_gate_peak = irq_gate;
    }
  }
  frame.sai_a = SAI1_Block_A->SR;
  frame.sai_b = SAI1_Block_B->SR;
  frame.paths |= cue_active ? 1 << 6 : 0;
  latest = frame;
  if (!target) {
    return;
  }
  Header& header = chunk.header;
  if (frame.paths & (1 << 7)) {
    header.paused_setup_cycles = paused_setup_cycles;
  }
  const uint32_t index = written % ring;
  Frame& record = chunk.frames[index];
  record = frame;
  if (dma_result & kDmaMiss) {
    record.flags |= kFlagMiss;
  }
  ticks[index] = uint32_t(begin_tick);
  lost[index] = (dma_result & kDmaLost) != 0;
  ++written;
  if (!stop && (record.flags & trigger)) {
    stop = written - 1 + ring - pre;
  }
  if (written == stop) {
    header.end_tick_us = begin_tick + 250;
    flush_pending = true;
    __DMB();
    target = 0;
  }
}

}  // namespace s1_timing
}  // namespace beads
