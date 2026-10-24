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
// DWT capture counters and synthetic acquisition state.

#ifndef BEADS_DWT_CALIBRATION_H_
#define BEADS_DWT_CALIBRATION_H_

#ifdef DWT_CALIBRATION
#include <algorithm>
#include <stdint.h>

#include "stm32h7xx.h"

#include "stmlib/stmlib.h"

#include "beads/dsp/processor.h"

namespace beads {
namespace dwt_calibration {

const uint32_t kDwtUnlock = 0xc5acce55;
enum BracketIndex {
  BRACKET_CALLBACK, BRACKET_INNER, BRACKET_PREPARATION,
  BRACKET_CALLBACK_MINUS_PREPARATION, BRACKET_LAST
};

const uint8_t kResetPanel = 1;
const uint8_t kResetSynthetic = 3;
const uint32_t kMagic = 0x44575443;
const uint32_t kVersion = 2;
const unsigned kHistogramBinWidth = 2000;
const unsigned kHistogramBins = 64;
const unsigned kSyntheticBlocks = 4096;
const unsigned kPanelBlocks = 240000;
const unsigned kWarmupBlocks = 16000;
const unsigned kStimulusSeed = 0x12345678;
const unsigned kLeftMultiplier = 73;
const unsigned kRightMultiplier = 137;
const unsigned kStimulusPeriod = 2048;
const int kStimulusOffset = 1024;
const float kStimulusScale = 8192.0f;
const unsigned kFrameChannels = 2;

struct Bracket {
  uint32_t maximum;
  uint32_t count;
  uint64_t sum;
  uint32_t histogram[kHistogramBins];
};

// Main-loop spectral frames of one channel. Frame times are DWT cycles from
// before STFT::Buffer to after it: wall includes interrupts, exclusive
// subtracts the callback bracket that interrupted the frame. Discontinuities
// count frames that do not follow the channel's last one (an STFT restart).
struct FrameChannel {
  uint32_t generation;
  uint32_t produced;
  uint32_t completed;
  uint32_t skipped;
  uint32_t backlog_maximum;
  uint32_t wall_maximum;
  uint32_t exclusive_maximum;
  uint32_t done;
  uint32_t discontinuities;
  uint32_t reserved;
  uint64_t wall_sum;
  uint64_t exclusive_sum;
};

// Written by the main loop only, under its own sequence counter.
struct Frames {
  uint32_t sequence;
  uint32_t reserved;
  FrameChannel channel[kFrameChannels];
};

struct Stats {
  uint32_t magic;
  uint32_t version;
  uint32_t sequence;
  uint8_t reset_request;
  uint8_t synthetic;
  uint8_t reserved[2];
  uint32_t generation;
  uint32_t block;
  Bracket bracket[BRACKET_LAST];
  Frames frames;
};

extern volatile Stats stats;

inline uint32_t Read() {
  __asm volatile("dsb\n\tisb" ::: "memory");
  return DWT->CYCCNT;
}

inline void Clear() {
  for (unsigned b = 0; b < BRACKET_LAST; ++b) {
    stats.bracket[b].maximum = 0;
    stats.bracket[b].count = 0;
    stats.bracket[b].sum = 0;
    for (unsigned i = 0; i < kHistogramBins; ++i) {
      stats.bracket[b].histogram[i] = 0;
    }
  }
  stats.synthetic = stats.reset_request == kResetSynthetic;
  stats.block = 0;
  ++stats.generation;
  stats.reset_request = 0;
}

inline void Init() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->LAR = kDwtUnlock;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  stats.magic = kMagic;
  stats.version = kVersion;
  stats.sequence = 1;
  stats.generation = 0;
  stats.block = 0;
  stats.reset_request = kResetPanel;
  stats.synthetic = 0;
  stats.reserved[0] = 0;
  stats.reserved[1] = 0;
  stats.frames.sequence = 0;
  stats.frames.reserved = 0;
  for (unsigned c = 0; c < kFrameChannels; ++c) {
    stats.frames.channel[c].generation = 0;
  }
  Clear();
  stats.sequence = 2;
}

inline void Record(unsigned b, uint32_t elapsed) {
  if (stats.synthetic && stats.block < kWarmupBlocks) {
    return;
  }
  if (stats.bracket[BRACKET_CALLBACK].count >=
      (stats.synthetic ? kSyntheticBlocks : kPanelBlocks)) {
    return;
  }
  volatile Bracket& s = stats.bracket[b];
  if (elapsed > s.maximum) {
    s.maximum = elapsed;
  }
  s.sum += elapsed;
  ++s.count;
  unsigned bin = elapsed / kHistogramBinWidth;
  ++s.histogram[bin < kHistogramBins - 1 ? bin : kHistogramBins - 1];
}

class ScopedBlockTimer {
 public:
  ScopedBlockTimer() {
    const bool reset = stats.reset_request == kResetPanel ||
        stats.reset_request == kResetSynthetic;
    active_ = reset ||
        stats.bracket[BRACKET_CALLBACK].count <
            (stats.synthetic ? kSyntheticBlocks : kPanelBlocks);
    if (!active_) {
      return;
    }
    ++stats.sequence;
    if (reset) {
      Clear();
    }
    preparation_ = 0;
    start_ = Read();
  }
  ~ScopedBlockTimer() {
    if (!active_) {
      return;
    }
    const uint32_t elapsed = Read() - start_;
    Record(BRACKET_CALLBACK_MINUS_PREPARATION, elapsed - preparation_);
    Record(BRACKET_CALLBACK, elapsed);
    ++stats.block;
    __asm volatile("dmb" ::: "memory");
    ++stats.sequence;
  }

  void Preparation(uint32_t elapsed) {
    preparation_ = elapsed;
    Record(BRACKET_PREPARATION, elapsed);
  }
 private:
  uint32_t preparation_;
  uint32_t start_;
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBlockTimer);
};

// Low word of the callback bracket's cycle sum: one atomic read; differences
// are exact while a frame lasts less than 2^32 cycles.
inline uint32_t CallbackCycles() {
  return *reinterpret_cast<volatile uint32_t*>(
      &stats.bracket[BRACKET_CALLBACK].sum);
}

// Main loop: one channel's call to STFT::Buffer. Only panel acquisitions
// record frames, and only frames that start and end inside one.
class FrameTimer {
 public:
  FrameTimer(size_t done) {
    generation_ = stats.generation;
    done_ = done;
    // No callback between the two reads: exclusive time is exact.
    __disable_irq();
    start_ = Read();
    callback_ = CallbackCycles();
    __enable_irq();
  }

  void Stop(unsigned channel, size_t ready, size_t done) {
    __disable_irq();
    const uint32_t callback = CallbackCycles() - callback_;
    const uint32_t wall = Read() - start_;
    __enable_irq();
    const uint32_t exclusive = wall - callback;
    if (done == done_ || stats.generation != generation_ ||
        stats.synthetic ||
        stats.bracket[BRACKET_CALLBACK].count >= kPanelBlocks) {
      return;
    }
    volatile FrameChannel& s = stats.frames.channel[channel];
    ++stats.frames.sequence;
    __asm volatile("dmb" ::: "memory");
    if (s.generation != generation_) {
      // First frame since the reset.
      s.generation = generation_;
      s.completed = 0;
      s.skipped = 0;
      s.backlog_maximum = 0;
      s.wall_maximum = 0;
      s.exclusive_maximum = 0;
      s.discontinuities = 0;
      s.reserved = 0;
      s.wall_sum = 0;
      s.exclusive_sum = 0;
    } else if (done_ != s.done) {
      ++s.discontinuities;
    }
    // Frames STFT::Buffer took in this call: the one it computed and those
    // it skipped as overdue.
    const uint32_t taken = done - done_;
    ++s.completed;
    s.skipped += taken - 1;
    s.produced = s.completed + s.skipped + (ready - done);
    s.done = done;
    if (taken > s.backlog_maximum) {
      s.backlog_maximum = taken;
    }
    if (wall > s.wall_maximum) {
      s.wall_maximum = wall;
    }
    if (exclusive > s.exclusive_maximum) {
      s.exclusive_maximum = exclusive;
    }
    s.wall_sum += wall;
    s.exclusive_sum += exclusive;
    __asm volatile("dmb" ::: "memory");
    ++stats.frames.sequence;
  }

 private:
  uint32_t generation_;
  uint32_t callback_;
  uint32_t start_;
  size_t done_;

  DISALLOW_COPY_AND_ASSIGN(FrameTimer);
};

const float kSyntheticMidpoint = 0.5f;
const float kSyntheticSize = 0.85f;
const float kSyntheticPitch = 24.0f;
const float kSyntheticDensity = 0.95f;
const float kSyntheticMix = 0.8f;
const float kSyntheticReverb = 0.7f;

inline void PrepareSynthetic(
    Processor* processor, uint8_t* delay_buffer_1, size_t delay_buffer_1_size,
    uint8_t* delay_buffer_2, size_t delay_buffer_2_size,
    uint16_t* reverb_buffer, size_t reverb_buffer_size,
    bool clouds_style, bool* synth_mode, Parameters* p, ModulatedParameter* d,
    Processor::MixParameters* m, SeedButtonState* seed_button_state,
    stmlib::GateFlags** seed_input, stmlib::GateFlags* seed, bool* freeze,
    FloatFrame* float_in, size_t size, ScopedBlockTimer* callback) {
  const uint32_t preparation_start = Read();
  const unsigned block = stats.block;
  if (block == 0) {
    uint8_t* buffers[] = {
      delay_buffer_1, delay_buffer_1 + delay_buffer_1_size / 2, delay_buffer_2
    };
    std::fill(delay_buffer_1, delay_buffer_1 + delay_buffer_1_size, 0);
    std::fill(delay_buffer_2, delay_buffer_2 + delay_buffer_2_size, 0);
    std::fill(reverb_buffer, reverb_buffer + reverb_buffer_size, 0);
    processor->Init(buffers, reverb_buffer);
    processor->Configure(Processor::MEDIA_DIGITAL, false);
    stmlib::Random::Seed(kStimulusSeed);
    processor->SetCloudsStyle(clouds_style);
  }
  *synth_mode = false;
  *p = Parameters();
  p->time.base = kSyntheticMidpoint;
  p->size.base = kSyntheticSize;
  p->shape.base = kSyntheticMidpoint;
  p->pitch.base = kSyntheticPitch;
  d->base = kSyntheticDensity;
  d->cv = 0.0f;
  m->feedback = kSyntheticMix;
  m->dry_wet = kSyntheticMix;
  m->reverb_amount = kSyntheticReverb;
  m->output_gate = false;
  *seed_button_state = SEED_BUTTON_STATE_OFF;
  *seed_input = seed;
  for (unsigned i = 0; i < size; ++i) {
    float_in[i].l = float(int((block * size + i) * kLeftMultiplier
        % kStimulusPeriod) - kStimulusOffset) / kStimulusScale;
    float_in[i].r = float(int((block * size + i) * kRightMultiplier
        % kStimulusPeriod) - kStimulusOffset) / kStimulusScale;
    seed[i] = i == 0
        ? stmlib::GateFlags(stmlib::GATE_FLAG_RISING | stmlib::GATE_FLAG_HIGH)
        : stmlib::GATE_FLAG_LOW;
    freeze[i] = false;
  }
  processor->PrepareCalibration(block);
  callback->Preparation(Read() - preparation_start);
}

}  // namespace dwt_calibration
}  // namespace beads
#endif  // DWT_CALIBRATION

#endif  // BEADS_DWT_CALIBRATION_H_
