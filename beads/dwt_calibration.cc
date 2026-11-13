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
// DWT capture storage and the timed spectral frames.

#include "beads/dwt_calibration.h"

#ifdef CPU_CEILING
#include "beads/cpu_ceiling.h"
#include "beads/drivers/codec.h"
#include "beads/resources.h"
#include "stmlib/dsp/dsp.h"
#endif  // CPU_CEILING

#ifdef DWT_CALIBRATION
namespace beads {
namespace dwt_calibration {

volatile Stats stats __attribute__((section(".dwt_calibration"), aligned(8)));

}  // namespace dwt_calibration

// The spectral engine's main-loop frames, timed per channel. Defined here so
// that the release build's phase_vocoder.cc keeps its line numbers.
void PhaseVocoder::Buffer() {
  for (int32_t i = 0; i < num_channels_; ++i) {
    dwt_calibration::FrameTimer calibration_frame(stft_[i].done());
    stft_[i].Buffer();
    calibration_frame.Stop(i, stft_[i].ready(), stft_[i].done());
  }
}


#ifdef CPU_CEILING
namespace cpu_ceiling {

volatile Hold hold __attribute__((section(".cpu_ceiling"), aligned(8)));

namespace {
uint32_t entry;
uint32_t examined;  // the last command word taken or rejected; firmware only
float phase;
}  // namespace

void Init() {
  hold.magic = kMagic;
  hold.version = kVersion;
  hold.command = 0;
  hold.ack = 0;
  hold.rejects = 0;
  hold.armed_blocks = 0;
  hold.frames = 0;
  hold.blocks = 0;
  hold.on_time = 0;
  hold.ambiguous = 0;
  hold.late = 0;
  hold.first_late = 0xffffffff;
  hold.g2 = 0;
  hold.pending = 0;
  hold.max_store = 0;
  hold.state = STATE_IDLE;
  examined = 0;
}

bool Enter(size_t size) {
  entry = dwt_calibration::Read();
  hold.frames = size;
  // The host owns command: read it once, never write it back.
  const uint32_t command = hold.command;
  if (command != hold.ack && command != examined) {
    examined = command;
    if (Valid(command)) {
      hold.armed_blocks = 0;
      hold.blocks = 0;
      hold.on_time = 0;
      hold.ambiguous = 0;
      hold.late = 0;
      hold.first_late = 0xffffffff;
      hold.g2 = 0;
      hold.pending = 0;
      hold.max_store = 0;
      hold.state = Holds(command) ? STATE_ARMED : STATE_IDLE;
      __DMB();
      hold.ack = command;
    } else {
      ++hold.rejects;
    }
  }
  if (hold.state != STATE_ARMED) {
    return false;
  }
  // Observing alone changes nothing audible: only a load, sine or skip expires.
  if ((hold.ack & (kTargetMask | kSine | kSkipDsp)) &&
      ++hold.armed_blocks > kDeadmanBlocks) {
    hold.state = STATE_EXPIRED;
    return false;
  }
  return true;
}

void Output(FloatFrame* frames, Codec::Frame* out, size_t size) {
  if (sine()) {
    // The nominal rate, as the bench's detector expects.
    const float increment = kSineHz / float(size * 4000);
    for (size_t i = 0; i < size; ++i) {
      frames[i].l = frames[i].r =
          kSineLevel * stmlib::Interpolate(lut_sine, phase, 1024.0f);
      phase += increment;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
    }
  }
  const uint32_t hold_cycles = target();
  while (dwt_calibration::Read() - entry < hold_cycles) { }
  const uint32_t stored = dwt_calibration::Read() - entry;
  if (stored > hold.max_store) {
    hold.max_store = stored;
  }
  const uint32_t words = 4 * size;
  const bool first_half =
      out == reinterpret_cast<Codec::Frame*>(DMA1_Stream0->M0AR);
  const uint32_t base = first_half ? 0 : words / 2;
  for (size_t i = 0; i < size; ++i) {
    float l = frames[i].l;
    float r = frames[i].r;
    CONSTRAIN(l, -1.0f, 0.999999f);
    CONSTRAIN(r, -1.0f, 0.999999f);
    if (i == 0) {
      const uint32_t before = DMA1_Stream0->NDTR;
      out[0].l = static_cast<int32_t>(l * 2147483648.0f);
      __DSB();
      const uint32_t after = DMA1_Stream0->NDTR;
      const Store store = Classify(before, after, base, words);
      if (store == STORE_LATE) {
        if (!hold.late) {
          hold.first_late = hold.blocks;
        }
        ++hold.late;
      } else if (store == STORE_AMBIGUOUS) {
        ++hold.ambiguous;
      } else {
        ++hold.on_time;
      }
    } else {
      out[i].l = static_cast<int32_t>(l * 2147483648.0f);
    }
    out[i].r = static_cast<int32_t>(r * 2147483648.0f);
  }
  if (Reached(DMA1_Stream0->NDTR, base, words)) {
    ++hold.g2;
  }
  if (DMA1->LISR & (first_half ? DMA_LISR_TCIF1 : DMA_LISR_HTIF1)) {
    ++hold.pending;
  }
  ++hold.blocks;
}

}  // namespace cpu_ceiling
#endif  // CPU_CEILING

}  // namespace beads
#endif  // DWT_CALIBRATION
