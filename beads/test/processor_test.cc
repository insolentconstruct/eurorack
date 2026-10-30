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
// Recording quality and freeze tests through the audio processor.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

#define private public
#include "beads/dsp/processor.h"
#include "beads/settings.h"
#include "beads/ui.h"
#undef private

#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/gain_control.h"

#include "beads/test/firmware_shims.h"

using namespace beads;

bool test_down[4] = { };
uint32_t test_writes = 0;
namespace beads {
}  // namespace beads

struct Rig {
  Processor processor;
  uint8_t banks[3][262144];
  uint16_t reverb[32768];
  Parameters parameters;
  ModulatedParameter density;
  Processor::MixParameters mix;
  FloatFrame output[12];
  bool same_channels;

  void Init(Processor::Media media,
      bool mono) {
    memset(static_cast<void*>(this), 0, sizeof(*this));
    uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
    processor.Init(buffers, reverb);
    processor.Configure(media, mono);
    parameters.size.base = 0.5f;
    parameters.shape.base = 0.5f;
    density.base = 0.9f;
    mix.dry_wet = 1.0f;
  }
  void Block(bool frozen, float value) {
    FloatFrame input[12];
    bool freeze[12];
    const int size = int(processor.sample_rate()) / 4000;
    for (int i = 0; i < size; ++i) {
      input[i].l = value;
      input[i].r = same_channels ? value : -value;
      freeze[i] = frozen;
    }
    processor.Process(SEED_BUTTON_STATE_ON, NULL, freeze, density,
        parameters, mix, input, output, size);
  }
};

static Rig rig;

template<typename Line>
void CheckQuality(Line& line, int capacity, float step) {
  CHECK(line.MAX_DELAY == capacity);
  const int before = line.write_ptr_;
  rig.Block(false, 0.12345f);
  const int count = int(rig.processor.sample_rate()) /
      (4000 * rig.processor.ratio());
  CHECK(line.write_ptr_ == (before + capacity - count) % capacity);
  const FloatFrame stored = line.Load(before);
  // Bound one quantization step at this input level.
  CHECK(fabsf(stored.l - 0.12345f) <= step);
  CHECK(fabsf(stored.r + 0.12345f) <= step);
  if (step < 0.001f) {
    CHECK(stored.l == floorf(0.12345f / step) * step);
  }
}

void TestBrightRecording() {
  test_checker.Start("TestBrightRecording");
  rig.Init(Processor::MEDIA_DIGITAL, false);
  CHECK(rig.processor.sample_rate() == 48000.0f);
  CheckQuality(rig.processor.processor_digital_.stereo_delay_line_,
      192000, 1.0f / 32768.0f);
  rig.Init(Processor::MEDIA_DIGITAL, true);
  CHECK(rig.processor.remaining() == 384000);
}

void TestColdRecording() {
  test_checker.Start("TestColdRecording");
  rig.Init(Processor::MEDIA_VINTAGE_DIGITAL, false);
  CHECK(rig.processor.sample_rate() == 32000.0f);
  CheckQuality(rig.processor.processor_vintage_digital_.stereo_delay_line_,
      256000, 1.0f / 2048.0f);
  rig.Init(Processor::MEDIA_VINTAGE_DIGITAL, true);
  CHECK(rig.processor.remaining() == 512000);
}

void TestSunnyRecording() {
  test_checker.Start("TestSunnyRecording");
  rig.Init(Processor::MEDIA_CLEAN_TAPE, false);
  CHECK(rig.processor.sample_rate() == 48000.0f);
  CHECK(rig.processor.ratio() == 2);
  CHECK(rig.processor.remaining() == 256000);
  for (int i = 0; i < 100; ++i) {
    rig.Block(false, 0.12345f);
  }
  const int pos = *rig.processor.mutable_delay_write_ptr();
  CHECK(pos == 256000 - 600);
  const FloatFrame stored =
      rig.processor.processor_clean_tape_.stereo_delay_line_.Load(pos + 1);
  // Allow two 12-bit steps after the downsampler has settled.
  CHECK(fabsf(stored.l - 0.12345f) < 0.001f);
  rig.mix.dry_wet = 0.0f;
  rig.Block(false, 0.0f);
  rig.Block(false, 0.12345f);
  // Settled dry gain is 0.75, with no recording-rate conversion.
  CHECK(fabsf(rig.output[11].l - 0.75f * 0.12345f) < 0.00001f);
  rig.Init(Processor::MEDIA_CLEAN_TAPE, true);
  CHECK(rig.processor.remaining() == 512000);
}

void TestCassetteRecording() {
  test_checker.Start("TestCassetteRecording");
  rig.Init(Processor::MEDIA_DIRTY_TAPE, false);
  CHECK(rig.processor.sample_rate() == 24000.0f);
  CHECK(rig.processor.wow_and_flutter_.delay_ == 0.0f);
  CheckQuality(rig.processor.processor_dirty_tape_.stereo_delay_line_,
      384000, 0.005f);
  const float delay = rig.processor.wow_and_flutter_.delay_;
  rig.Block(false, 0.12345f);
  CHECK(delay > 0.0f);
  CHECK(rig.processor.wow_and_flutter_.delay_ != delay);
  rig.Init(Processor::MEDIA_DIRTY_TAPE, true);
  CHECK(rig.processor.remaining() == 768000);
}

void TestFreezeGateRecording() {
  test_checker.Start("TestFreezeGateRecording");
  for (int media = 0; media < 4; ++media) {
    rig.Init(Processor::Media(media), false);
    rig.Block(false, 0.25f);
    const int before = *rig.processor.mutable_delay_write_ptr();
    for (int i = 0; i < 100; ++i) {
      rig.Block(true, -0.5f);
    }
    CHECK(*rig.processor.mutable_delay_write_ptr() == before);
    rig.Block(false, -0.5f);
    CHECK(*rig.processor.mutable_delay_write_ptr() < before);
  }
}

void TestFreezeButtonRecording() {
  test_checker.Start("TestFreezeButtonRecording");
  Settings settings;
  GainControl gain;
  Calibrator calibration;
  BackgroundBufferBackup backup;
  Ui ui;
  memset(test_down, 0, sizeof(test_down));
  settings.Init(false);
  ui.Init(&settings, &gain, &calibration, &backup);
  rig.Init(Processor::MEDIA_DIGITAL, false);
  for (int tap = 0; tap < 2; ++tap) {
    for (int down = 1; down >= 0; --down) {
      test_down[SWITCH_FREEZE] = down;
      for (int i = 0; i < 10; ++i) {
        ui.Poll();
        ui.DoEvents();
      }
    }
    CHECK(ui.freeze_button_state() == (tap == 0));
    const int before = *rig.processor.mutable_delay_write_ptr();
    rig.Block(ui.freeze_button_state(), 0.25f);
    CHECK((*rig.processor.mutable_delay_write_ptr() == before) == (tap == 0));
  }
}

void TestFrozenGrains() {
  test_checker.Start("TestFrozenGrains");
  rig.Init(Processor::MEDIA_DIGITAL, false);
  rig.same_channels = true;
  for (int i = 0; i < 17000; ++i) {
    rig.Block(false, 0.25f);
  }
  const int before = *rig.processor.mutable_delay_write_ptr();
  double energy = 0.0;
  double sum = 0.0;
  for (int i = 0; i < 4000; ++i) {
    rig.Block(true, -0.5f);
    for (int j = 0; j < 12; ++j) {
      energy += rig.output[j].l * rig.output[j].l;
      sum += rig.output[j].l;
    }
  }
  CHECK(*rig.processor.mutable_delay_write_ptr() == before);
  // Old positive material survives a full second of negative input.
  CHECK(energy / 48000 > 0.001);
  CHECK(sum / 48000 > 0.02);
}

void TestUnfreezeCrossfade() {
  test_checker.Start("TestUnfreezeCrossfade");
  rig.Init(Processor::MEDIA_DIGITAL, false);
  for (int i = 0; i < 30; ++i) {
    rig.Block(true, 0.25f);
  }
  const int before = *rig.processor.mutable_delay_write_ptr();
  for (int i = 0; i < 23; ++i) {
    rig.Block(false, -0.25f);
  }
  for (int i = 0; i < 276; ++i) {
    const float t = i < 256 ? float(256 - i) / 256.0f : 0.0f;
    const float expected = -0.25f + 0.5f * stmlib::SmoothStep(t);
    const int index = (before + 192000 - i) % 192000;
    const float stored =
        rig.processor.processor_digital_.stereo_delay_line_.Load(index).l;
    // The 16-bit store truncates toward zero.
    CHECK(fabsf(stored - expected) <= 1.0f / 32768.0f);
  }
}

void TestTrimDispatch() {
  test_checker.Start("TestTrimDispatch");
  for (int media = 0; media < 4; ++media) {
    for (int mono = 0; mono < 2; ++mono) {
      rig.Init(Processor::Media(media), mono);
      int* durations[8];
      durations[0] =
          rig.processor.processor_dirty_tape_.mutable_delay_duration(false);
      durations[1] =
          rig.processor.processor_dirty_tape_.mutable_delay_duration(true);
      durations[2] =
          rig.processor.processor_clean_tape_.mutable_delay_duration(false);
      durations[3] =
          rig.processor.processor_clean_tape_.mutable_delay_duration(true);
      durations[4] =
          rig.processor.processor_vintage_digital_.mutable_delay_duration(false);
      durations[5] =
          rig.processor.processor_vintage_digital_.mutable_delay_duration(true);
      durations[6] =
          rig.processor.processor_digital_.mutable_delay_duration(false);
      durations[7] =
          rig.processor.processor_digital_.mutable_delay_duration(true);
      for (int i = 0; i < 8; ++i) {
        *durations[i] = 100 + i;
      }
      rig.processor.Trim();
      for (int i = 0; i < 8; ++i) {
        CHECK(*durations[i] == (i == media * 2 + mono ? 0 : 100 + i));
      }
      CHECK(*rig.processor.mutable_delay_duration() == 0);
      rig.Block(true, 0.25f);
      CHECK(*rig.processor.mutable_delay_duration() == 0);
      rig.Block(false, 0.25f);
      CHECK(*rig.processor.mutable_delay_duration() ==
          int(rig.processor.sample_rate()) / (4000 * rig.processor.ratio()));
    }
  }
}

template<typename Line>
void CheckTrimGrowth(Line& line) {
  const int capacity = line.MAX_DELAY;
  FloatFrame input;
  input.l = input.r = 0.25f;
  line.Trim();
  for (int i = 0; i < capacity + 2; ++i) {
    line.Write(input, true);
    if (!CHECK(line.duration() == std::min(i, capacity))) {
      return;
    }
    line.Write(input, false);
    if (!CHECK(line.duration() == std::min(i + 1, capacity))) {
      return;
    }
  }
}

void TestTrimGrowth() {
  test_checker.Start("TestTrimGrowth");
  rig.Init(Processor::MEDIA_DIGITAL, false);
  CheckTrimGrowth(rig.processor.processor_dirty_tape_.mono_delay_line_);
  CheckTrimGrowth(rig.processor.processor_dirty_tape_.stereo_delay_line_);
  CheckTrimGrowth(rig.processor.processor_clean_tape_.mono_delay_line_);
  CheckTrimGrowth(rig.processor.processor_clean_tape_.stereo_delay_line_);
  CheckTrimGrowth(rig.processor.processor_vintage_digital_.mono_delay_line_);
  CheckTrimGrowth(rig.processor.processor_vintage_digital_.stereo_delay_line_);
  CheckTrimGrowth(rig.processor.processor_digital_.mono_delay_line_);
  CheckTrimGrowth(rig.processor.processor_digital_.stereo_delay_line_);
}

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  TestTrimDispatch();
  TestTrimGrowth();
  TestBrightRecording();
  TestFreezeButtonRecording();
  TestFreezeGateRecording();
  TestFrozenGrains();
  TestColdRecording();
  TestSunnyRecording();
  TestCassetteRecording();
  TestUnfreezeCrossfade();
  return test_checker.failures() ? 1 : 0;
}
