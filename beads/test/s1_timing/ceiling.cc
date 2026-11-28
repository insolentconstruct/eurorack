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

#include "beads/test/s1_timing/ceiling.h"

#include "beads/resources.h"
#include "stmlib/dsp/dsp.h"

namespace beads {
namespace cpu_ceiling {

volatile Hold s1_ceiling;
Block block;

void Init() {
  volatile Hold& hold = s1_ceiling;
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
  block.examined = 0;
  block.held = false;
  block.store = STORE_ON_TIME;
}

bool Enter(size_t size) {
  block.entry = s1_timing::Cycles();
  s1_timing::Observe observation;
  volatile Hold& hold = s1_ceiling;
  hold.frames = size;
  // The host owns command: read it once, never write it back.
  const uint32_t command = hold.command;
  if (command != hold.ack && command != block.examined) {
    block.examined = command;
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

void Output(FloatFrame* frames, Codec::Frame* out, size_t size,
    bool gate_output) {
  if (sine()) {
    // The nominal rate, as the bench's detector expects.
    const float increment = kSineHz / float(size * 4000);
    for (size_t i = 0; i < size; ++i) {
      const float sample =
          kSineLevel * stmlib::Interpolate(lut_sine, block.phase, 1024.0f);
      frames[i].l = sample;
      if (!gate_output) {
        frames[i].r = sample;
      }
      block.phase += increment;
      if (block.phase >= 1.0f) {
        block.phase -= 1.0f;
      }
    }
  }
  const uint32_t hold_cycles = target();
  while (s1_timing::Cycles() - block.entry < hold_cycles) { }
  const uint32_t stored = s1_timing::Cycles() - block.entry;
  if (stored > s1_ceiling.max_store) {
    s1_ceiling.max_store = stored;
  }
  for (size_t i = 0; i < size; ++i) {
    float l = frames[i].l;
    float r = frames[i].r;
    CONSTRAIN(l, -1.0f, 0.999999f);
    CONSTRAIN(r, -1.0f, 0.999999f);
    out[i].l = static_cast<int32_t>(l * 2147483648.0f);
    out[i].r = static_cast<int32_t>(r * 2147483648.0f);
  }
  block.held = true;
}

}  // namespace cpu_ceiling
}  // namespace beads
