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
// Frozen wet feedback: safety and freeze semantics.

#include <cmath>
#include <cstring>

#define private public
#include "beads/dsp/processor.h"
#undef private
#include "beads/test/test_checker.h"

using namespace beads;

void TestFeedbackDiffuser() {
  test_checker.Start("TestFeedbackDiffuser");
  FeedbackDiffuser diffuser;
  diffuser.Init();
  double tail = 0.0;
  for (int i = 0; i < 48000; ++i) {
    FloatFrame frame;
    frame.l = i == 0 ? 0.1f : 0.0f;
    frame.r = 0.0f;
    diffuser.Process(&frame, 1.0f, true, 1);
    CHECK(std::isfinite(frame.l) && std::isfinite(frame.r));
    CHECK(fabsf(frame.l) < 0.2f && fabsf(frame.r) < 0.2f);
    CHECK(frame.r == 0.0f);
    if (i > 500) {
      tail += frame.l * frame.l;
    }
  }
  CHECK(tail > 0.000001);
  diffuser.Init();
  for (int i = 0; i < 100; ++i) {
    FloatFrame frame;
    frame.l = 0.1f;
    frame.r = -0.2f;
    diffuser.Process(&frame, 0.0f, true, 1);
    CHECK(frame.l == 0.1f && frame.r == -0.2f);
  }
}

// The loop runs only while FREEZE is on: when it is off the block is left
// untouched, and the memory is cleared a slice per block after a short fade.
void TestFeedbackIdle() {
  test_checker.Start("TestFeedbackIdle");
  FeedbackDiffuser diffuser;
  diffuser.Init();
  for (int i = 0; i < 4000; ++i) {
    FloatFrame frame[12];
    for (int j = 0; j < 12; ++j) {
      frame[j].l = 0.1f * sinf(float(i * 12 + j) * 0.05f);
      frame[j].r = -frame[j].l;
    }
    diffuser.Process(frame, 0.7f, true, 12);
    CHECK(std::isfinite(frame[11].l) && std::isfinite(frame[11].r));
    CHECK(fabsf(frame[11].l) < 1.0f);
  }
  CHECK(diffuser.engaged_ > 0.99f);
  for (int i = 0; i < 400; ++i) {
    FloatFrame frame[12];
    for (int j = 0; j < 12; ++j) {
      frame[j].l = 0.1f;
      frame[j].r = -0.2f;
    }
    diffuser.Process(frame, 0.7f, false, 12);
  }
  CHECK(diffuser.engaged_ == 0.0f);
  for (size_t i = 0; i < kFeedbackDelayBufferSize; ++i) {
    CHECK(diffuser.delay_buffer_[i] == 0);
  }
  for (size_t i = 0; i < kDiffuserBufferSize; ++i) {
    CHECK(diffuser.diffuser_buffer_[i] == 0);
  }
  FloatFrame frame[12];
  for (int j = 0; j < 12; ++j) {
    frame[j].l = 0.1f;
    frame[j].r = -0.2f;
  }
  diffuser.Process(frame, 0.7f, false, 12);
  for (int j = 0; j < 12; ++j) {
    CHECK(frame[j].l == 0.1f && frame[j].r == -0.2f);
  }
}

void TestFrozenFeedback() {
  test_checker.Start("TestFrozenFeedback");
  static Processor processor;
  static uint8_t banks[3][262144];
  static uint8_t saved[3][262144];
  static uint16_t reverb[32768];
  uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
  for (int quality = 0; quality < 4; ++quality) {
    for (int mono = 0; mono < 2; ++mono) {
      memset(banks, 0, sizeof(banks));
      processor.Init(buffers, reverb);
      processor.Configure(Processor::Media(quality), mono);
      processor.set_evolving_feedback(true);
      Parameters p;
      memset(&p, 0, sizeof(p));
      p.size.base = 1.0f;
      p.shape.base = 0.5f;
      ModulatedParameter d;
      d.base = 0.85f;
      d.cv = 0.0f;
      Processor::MixParameters mix;
      mix.feedback = 1.0f;
      mix.dry_wet = 1.0f;
      mix.reverb_amount = 0.0f;
      mix.output_gate = false;
      const size_t size = size_t(processor.sample_rate()) / 4000;
      for (int block = 0; block < 20000; ++block) {
        // Allow MI's short resume-crossfade tail to finish recording.
        if (block == 17000) {
          memcpy(saved, banks, sizeof(banks));
        }
        FloatFrame in[12], out[12];
        bool freeze[12];
        for (size_t i = 0; i < size; ++i) {
          in[i].l = 0.05f * sinf(float(block * size + i) * 0.03f);
          in[i].r = -in[i].l;
          freeze[i] = block >= 16000;
        }
        processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze,
            d, p, mix, in, out, size);
        for (size_t i = 0; i < size; ++i) {
          CHECK(std::isfinite(out[i].l) && std::isfinite(out[i].r));
          CHECK(fabsf(out[i].l) < 2.0f && fabsf(out[i].r) < 2.0f);
        }
      }
      CHECK(memcmp(saved, banks, sizeof(banks)) == 0);
      CHECK(!processor.feedback_diffuser_.idle());
      // Off while frozen: the loop fades out, clears, then stops running.
      processor.set_evolving_feedback(false);
      for (int block = 0; block < 4000; ++block) {
        FloatFrame in[12], out[12];
        bool freeze[12];
        for (size_t i = 0; i < size; ++i) {
          in[i].l = in[i].r = 0.0f;
          freeze[i] = true;
        }
        processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze,
            d, p, mix, in, out, size);
      }
      CHECK(processor.feedback_diffuser_.idle());
    }
  }
}

int main() {
  TestFeedbackDiffuser();
  TestFeedbackIdle();
  TestFrozenFeedback();
  return test_checker.failures() ? 1 : 0;
}
