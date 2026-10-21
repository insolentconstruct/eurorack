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
// The firmware entry point, driven by host hardware.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

#define private public
#define protected public
#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/cv_reader.h"
#include "beads/dsp/plaits_synth.h"
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/factory_test.h"
#include "beads/gain_control.h"
#include "beads/ui.h"
#undef protected
#undef private

#define BEADS_TEST_CV_SHIMS
#include "beads/test/firmware_shims.h"

using namespace beads;
using namespace stmlib;

bool test_down[4];
unsigned test_writes;
namespace beads {
bool test_probe;
bool test_freeze_input;
bool test_seed_input;
uint16_t CvAdc::values_[CV_ADC_CHANNEL_LAST];
uint16_t PotsAdc::values_[POTS_ADC_CHANNEL_LAST];
}  // namespace beads

extern BackgroundBufferBackup background_buffer_backup;
extern Calibrator calibrator;
extern CvReader cv_reader;
extern Codec codec;
extern FactoryTest factory_test;
extern GainControl gain_control;
extern NormalizationDetector normalization_detector;
extern PlaitsSynth plaits_synth;
extern Processor processor;
extern Settings settings;
extern Synth synth;
extern Ui ui;
extern bool synth_mode;
extern int synth_mode_start_time;
extern int codec_transition_frames;
extern volatile bool ready;
extern GateFlags previous_seed;
void Init();
void MonitorStateChange();
void Process(const Codec::Event*, const Codec::Frame*, Codec::Frame*, size_t);
extern "C" void SysTick_Handler();

// Only the cache warming/timing call WasteCPUCycles is skipped.
void MainLoopStep() {
  ui.DoEvents();
  MonitorStateChange();
  if (!synth_mode && !processor.spectral()) {
    background_buffer_backup.Process(ui.freeze_led_state());
  }
  processor.Buffer();
}

template<typename T>
void PowerOnObject(T* object) {
  object->~T();
  memset(static_cast<void*>(object), 0, sizeof(T));
  new (object) T;
}

struct Rig {
  bool left;
  bool right;
  bool seed_patched;
  bool cv_patched[CV_ADC_CHANNEL_LAST];
  uint16_t cv[CV_ADC_CHANNEL_LAST];
  int32_t audio;
  Codec::Frame out[kMaxCodecBlockSize];

  Rig(bool on_request, bool unpatched)
      : left(!unpatched), right(!unpatched), seed_patched(false), audio(0) {
    memset(reinterpret_cast<void*>(0x08100000), 255, 0x100000);
    PowerOnObject(&settings);
    settings.Init(false);
    settings.set_self_play_on_request(on_request);
    settings.SaveState();
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      cv_patched[i] = false;
      cv[i] = 32768;
    }
    std::fill(PotsAdc::values_, PotsAdc::values_ + POTS_ADC_CHANNEL_LAST,
        32768);
    PotsAdc::values_[POTS_ADC_CHANNEL_FEEDBACK] = 0;
    PotsAdc::values_[POTS_ADC_CHANNEL_REVERB] = 0;
    PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 0;
    Reboot();
  }

  void Reboot(bool seed = false) {
    ready = false;
    PowerOnObject(&background_buffer_backup);
    PowerOnObject(&calibrator);
    PowerOnObject(&cv_reader);
    PowerOnObject(&codec);
    PowerOnObject(&factory_test);
    PowerOnObject(&gain_control);
    PowerOnObject(&normalization_detector);
    PowerOnObject(&plaits_synth);
    PowerOnObject(&processor);
    PowerOnObject(&settings);
    PowerOnObject(&synth);
    PowerOnObject(&ui);
    synth_mode = false;
    synth_mode_start_time = codec_transition_frames = 0;
    previous_seed = GATE_FLAG_LOW;
    std::fill(test_down, test_down + 4, false);
    test_down[SWITCH_SEED] = seed;
    test_freeze_input = test_seed_input = false;
    Init();
  }

  void Block() {
    Codec::Frame in[kMaxCodecBlockSize];
    Codec::Event events[kMaxCodecBlockSize];
    // The normalization probe reaches open jacks; patched signals replace it.
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      CvAdc::values_[i] = cv_patched[i] ? cv[i] :
          (test_probe ? 16000 : 32768);
    }
    for (size_t i = 0; i < codec.block_size_; ++i) {
      in[i].l = left ? audio : (test_probe ? 1 : -1);
      in[i].r = right ? audio : (test_probe ? 1 : -1);
      events[i] = Codec::Event(
          ((seed_patched ? test_seed_input : test_probe)
              ? Codec::EVENT_SEED : 0)
          | (test_freeze_input ? Codec::EVENT_FREEZE : 0));
    }
    if (codec.callback_) {
      codec.callback_(events, in, out, codec.block_size_);
    }
  }

  void Tick(int milliseconds, bool main_loop = true) {
    while (milliseconds--) {
      // The first main step starts the codec; thereafter four callbacks/ms.
      if (main_loop) {
        MainLoopStep();
      }
      for (int i = 0; i < 4; ++i) {
        Block();
      }
      SysTick_Handler();
    }
  }

  // Clock-only waits are used after known silence, or when audio cannot affect
  // the decision. Boundary callbacks still verify the signal and gate state.
  void Advance(int milliseconds) {
    while (milliseconds--) {
      MainLoopStep();
      SysTick_Handler();
    }
  }

  void StartAutomatic() {
    Settle();
    Advance(10001);
    Settle();
  }

  void HoldMedia(bool consume = true) {
    test_down[SWITCH_MEDIA] = true;
    Tick(10);
    // The input and pots stay fixed during the hold; only the UI clock matters.
    Advance(1497);
    test_down[SWITCH_MEDIA] = false;
    // Dispatch the hold event, leaving MonitorStateChange to the caller.
    Tick(8, false);
    ui.DoEvents();
    if (consume) {
      MainLoopStep();
    }
  }

  // Eight 32 ms probe windows plus 250 ms of codec blanking.
  void Settle() { Tick(600); }
};

void TestAutomaticSelfPlay() {
  test_checker.Start("TestAutomaticSelfPlay");
  Rig r(false, true);
  r.StartAutomatic();
  CHECK(synth_mode);
  r.left = true;
  r.Settle();
  CHECK(!synth_mode);
  r.left = false;
  r.Settle();
  // Silence: no grains or reverb tail from the initial synth.
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
  // Detection settles within a few hundred milliseconds.
  for (int i = 0; i < 2000 && !normalization_detector.no_input(); ++i) {
    r.Tick(1);
  }
  if (!CHECK(normalization_detector.no_input())) {
    return;
  }
  synth_mode_start_time = 0;
  MainLoopStep();
  const int start = synth_mode_start_time;
  r.Advance(9900);
  MainLoopStep();
  CHECK(!synth_mode);
  CHECK(synth_mode_start_time == start);
  r.Advance(101);
  CHECK(synth_mode);

  r.left = true;
  r.Settle();
  r.left = false;
  r.Settle();
  r.Advance(5000);
  CHECK(synth_mode_start_time != 0);
  test_freeze_input = true;
  r.Tick(1);
  MainLoopStep();
  CHECK(synth_mode_start_time == 0);
  test_freeze_input = false;
  r.Tick(2);
  r.Advance(9900);
  MainLoopStep();
  CHECK(!synth_mode);
  r.Advance(102);
  CHECK(synth_mode);

  r.left = r.right = true;
  r.audio = 0x20000000;
  r.Settle();
  // The callback resets the idle timer when actual OUT exceeds 0.01.
  synth_mode_start_time = 123;
  r.Block();
  CHECK(fabs(double(r.out[codec.block_size_ - 1].l)) > 21474836);
  CHECK(synth_mode_start_time == 0);

  Rig tail(false, false);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 65535;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
  PotsAdc::values_[POTS_ADC_CHANNEL_SIZE] = 50000;
  PotsAdc::values_[POTS_ADC_CHANNEL_SHAPE] = 0;
  for (int i = 0; i < 3000; ++i) {
    tail.audio = i & 1 ? 0x20000000 : -0x20000000;
    tail.Tick(1);
  }
  tail.left = tail.right = false;
  tail.Tick(100);
  CHECK(normalization_detector.no_input());
  bool heard = false;
  for (int i = 0; i < 1200; ++i) {
    MainLoopStep();
    tail.Block();
    for (size_t j = 0; j < codec.block_size_; ++j) {
      if (fabs(double(tail.out[j].l)) > 21474836.0 ||
          fabs(double(tail.out[j].r)) > 21474836.0) {
        heard = true;
        CHECK(synth_mode_start_time == 0);
      }
    }
  }
  CHECK(heard);  // Audible grains delay self-play even with both jacks open.
}

void TestRequestedSelfPlay() {
  test_checker.Start("TestRequestedSelfPlay");
  Rig r(true, true);
  CHECK(codec.callback_ == NULL);  // No synth audio at power-on.
  r.Settle();
  CHECK(!synth_mode);
  r.Advance(11000);
  CHECK(!synth_mode);
  r.HoldMedia(false);
  CHECK(ui.self_play_requested());
  CHECK(!synth_mode);
  MainLoopStep();
  CHECK(synth_mode);
  CHECK(!ui.self_play_requested());
  r.Settle();
  r.HoldMedia(false);
  CHECK(synth_mode);
  MainLoopStep();
  CHECK(!synth_mode);
  r.Settle();
  r.left = true;
  r.audio = 0x10000000;
  r.Settle();
  settings.mutable_state()->gain = 20;
  r.HoldMedia();
  CHECK(!synth_mode);
  CHECK(settings.state().gain == 0);
  CHECK(gain_control.time_ == gain_control.ignore_time_);
}

void TestSelfPlayRequestLifecycle() {
  test_checker.Start("TestSelfPlayRequestLifecycle");
  for (int blocker = 0; blocker < 5; ++blocker) {
    Rig r(true, true);
    r.Settle();
    r.HoldMedia(false);
    CHECK(ui.self_play_requested());
    if (blocker == 0) {
      test_freeze_input = true;
      r.Block();
    } else if (blocker == 1) {
      ui.mode_ = Ui::MODE_CALIBRATION_1;
    } else if (blocker == 2) {
      ui.set_factory_test(true);
    } else if (blocker == 3) {
      codec_transition_frames = 1;
    } else {
      r.left = true;
      // Settle detection without consuming the already raised request.
      r.Tick(350, false);
    }
    MainLoopStep();
    CHECK(!synth_mode);
    CHECK(!ui.self_play_requested());
    test_freeze_input = false;
    ui.mode_ = Ui::MODE_NORMAL;
    r.Settle();
    CHECK(!synth_mode);
  }
}

void TestMonoStereo() {
  test_checker.Start("TestMonoStereo");
  Rig r(false, false);
  r.Settle();
  CHECK(!processor.mono());
  r.right = false;
  r.Settle();
  CHECK(processor.mono());
  r.right = true;
  r.Settle();
  CHECK(!processor.mono());
  r.left = false;
  r.Settle();
  CHECK(processor.mono());
}

void TestFrozenQualityLock() {
  test_checker.Start("TestFrozenQualityLock");
  Rig r(false, false);
  r.Settle();
  CHECK(processor.media() == Processor::MEDIA_DIGITAL);
  test_freeze_input = true;
  r.Tick(1);
  test_down[SWITCH_MEDIA] = true;
  r.Tick(10);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(10);
  CHECK(settings.state().media == Processor::MEDIA_VINTAGE_DIGITAL);
  r.right = false;
  r.Settle();
  CHECK(!processor.mono());
  CHECK(processor.media() == Processor::MEDIA_DIGITAL);
  test_freeze_input = false;
  r.Tick(2);
  r.Settle();
  CHECK(processor.mono());
  CHECK(processor.media() == Processor::MEDIA_VINTAGE_DIGITAL);
}

// The reserve fed to Ui here; its LED colours are checked in options_test.cc
// (TestOverwriteReserveDisplay). PWM output is driver code, not tested.
void TestOverwriteIndicator() {
  test_checker.Start("TestOverwriteIndicator");
  Rig r(false, false);
  r.Settle();
  test_freeze_input = true;
  r.Tick(1);
  test_freeze_input = false;
  r.Tick(1);
  // Observe the reserve before each callback (the callback feeds UI first).
  float minimum = 1.0f;
  bool passed = false;
  for (int i = 0; i < 17000; ++i) {
    const float expected = processor.overwrite_reserve();
    r.Block();
    CHECK(fabsf(ui.overwrite_reserve_ - expected) < 1.0e-6f);
    minimum = std::min(minimum, ui.overwrite_reserve_);
    if (minimum < 0.01f && ui.overwrite_reserve_ == 1.0f) {
      passed = true;
    }
  }
  CHECK(minimum < 0.01f);
  CHECK(passed);
  // Enter self-play with a low granular reserve, so the override is visible.
  settings.set_self_play_on_request(true);
  r.left = r.right = false;
  r.Settle();
  test_freeze_input = true;
  r.Tick(1);
  test_freeze_input = false;
  r.Tick(1);
  for (int i = 0; i < 15800; ++i) {
    r.Block();
  }
  r.HoldMedia(false);
  CHECK(processor.overwrite_reserve() < 0.5f);
  MainLoopStep();
  CHECK(synth_mode);
  r.Tick(260);
  r.Block();
  CHECK(processor.overwrite_reserve() < 0.5f);
  CHECK(ui.overwrite_reserve_ == 1.0f);
}

void TestTriggerOutput() {
  test_checker.Start("TestTriggerOutput");
  for (int mode = 0; mode < 2; ++mode) {
    Rig r(false, !mode);
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    r.audio = 0x10000000;
    r.Settle();
    if (!mode) {
      r.StartAutomatic();
    }
    settings.mutable_state()->gate_output_mode = 1;
    bool high = false;
    bool low = false;
    for (int j = 0; j < 800; ++j) {
      r.Block();
      for (size_t i = 0; i < codec.block_size_; ++i) {
        const int32_t gate = r.out[i].r;
        // Integer conversion of -0.8f is exact at this scale.
        CHECK(gate == 0 || gate == -1717986944);
        high |= gate < 0;
        low |= gate == 0;
      }
    }
    CHECK(high && low);
    settings.mutable_state()->gate_output_mode = 0;
    bool audio = false;
    for (int j = 0; j < 100; ++j) {
      r.Block();
      for (size_t i = 0; i < codec.block_size_; ++i) {
        audio |= r.out[i].r != 0 && r.out[i].r != -1717986944;
      }
    }
    CHECK(audio);
  }
}

void TestStartupBranches() {
  test_checker.Start("TestStartupBranches");
  Rig r(false, true);
  settings.mutable_persistent_data()->adc_calibration[0].offset = 0.987f;
  settings.SavePersistentData();
  settings.mutable_state()->media = 1;
  settings.mutable_state()->gain = 22;
  settings.SaveState();
  r.Reboot(true);
  CHECK(settings.state().media == 3 && settings.state().gain == 0);
  CHECK(settings.persistent_data().adc_calibration[0].offset == 0.987f);
  CHECK(!factory_test.running());
  // Break the persisted CALI header checksum, not the in-memory object.
  reinterpret_cast<uint8_t*>(0x081e0000)[6] ^= 1;
  r.Reboot();
  CHECK(factory_test.running());
  CHECK(ui.mode_ == Ui::MODE_FACTORY_TEST);
  CHECK(codec.callback_ == &FactoryTest::ProcessFn);
  r.Tick(20);
  const unsigned writes = test_writes;
  r.Reboot();
  CHECK(!factory_test.running());
  CHECK(ui.mode_ == Ui::MODE_NORMAL);
  CHECK(test_writes == writes);
  r.Settle();
  CHECK(codec.callback_ == &Process);
}

// Independent DSP invocation: the expected Parameters route one ADC channel
// each. Comparing full rendered samples catches both missing and cross routing.
void TestCallbackCvRouting() {
  test_checker.Start("TestCallbackCvRouting");
  static Processor expected;
  static uint8_t memory_1[512 * 1024];
  static uint8_t memory_2[256 * 1024];
  static uint16_t reverb[32768];
  extern uint8_t delay_buffer_1[];
  extern uint8_t delay_buffer_2[];
  extern uint16_t reverb_buffer[];
  for (int channel = 0; channel <= CV_ADC_CHANNEL_LAST; ++channel) {
    const bool width = channel == CV_ADC_CHANNEL_LAST;
    Rig r(false, false);
    if (width) {
      settings.set_stereo_width(128);
      settings.set_width_assigned(true);
      settings.set_delay_mode(DELAY_MODE_WIDE_DETENT);
    }
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      r.cv_patched[i] = true;
    }
    // Every channel starts at zero; one channel alone then receives a step.
    for (int i = POTS_ADC_CHANNEL_TIME_AMOUNT;
         i <= POTS_ADC_CHANNEL_PITCH_AMOUNT; ++i) {
      PotsAdc::values_[i] = 65535;
    }
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 40000;
    r.Settle();
    uint8_t* buffers[] = { delay_buffer_1, delay_buffer_1 + 256 * 1024,
        delay_buffer_2 };
    uint8_t* reference[] = { memory_1, memory_1 + 256 * 1024, memory_2 };
    PowerOnObject(&processor);
    PowerOnObject(&expected);
    processor.Init(buffers, reverb_buffer);
    expected.Init(reference, reverb);
    // Identical, non-silent recording makes TIME/SIZE/SHAPE observable.
    for (size_t i = 0; i < kDelayBufferSizeBits / 8; ++i) {
      const uint8_t sample = uint8_t(i * 37 + i / 256);
      *processor.delay_buffer_raw_bytes(i) = sample;
      *expected.delay_buffer_raw_bytes(i) = sample;
    }
    r.cv[width ? CV_ADC_CHANNEL_ASSIGN : channel] = 30000;
    bool nonzero = false;
    for (int block = 0; block < 500; ++block) {
      const uint32_t random = Random::state();
      r.Block();
      Parameters p;
      RandomizedParameter* parameters[] = { &p.time, &p.size, &p.shape,
          &p.pitch };
      const PotsAdcChannel knobs[] = { POTS_ADC_CHANNEL_TIME,
          POTS_ADC_CHANNEL_SIZE, POTS_ADC_CHANNEL_SHAPE,
          POTS_ADC_CHANNEL_PITCH };
      const CvAdcChannel cvs[] = { CV_ADC_CHANNEL_TIME, CV_ADC_CHANNEL_SIZE,
          CV_ADC_CHANNEL_SHAPE, CV_ADC_CHANNEL_PITCH };
      for (int i = 0; i < 4; ++i) {
        parameters[i]->base = cv_reader.pot(knobs[i]);
        parameters[i]->attenuverter = cv_reader.pot(
            PotsAdcChannel(POTS_ADC_CHANNEL_TIME_AMOUNT + i));
        parameters[i]->has_cv = true;
        parameters[i]->cv = cv_reader.cv(cvs[i]);
      }
      p.monophonic_mode = false;
      expected.set_pitch_scale(ui.pitch_scale());
      expected.set_delay_mode(ui.delay_mode());
      expected.set_stereo_width(ModulatedStereoWidth(ui.stereo_width(),
          width ? cv_reader.cv(CV_ADC_CHANNEL_ASSIGN) - 0.002f : 0.0f),
          width);
      if (width) {
        CHECK(processor.delay_mode_ == DELAY_MODE_WIDE_DETENT);
        CHECK(processor.processor_digital_.filter_mode_.wide_detent);
        CHECK(processor.processor_vintage_digital_.filter_mode_.wide_detent);
        CHECK(processor.processor_clean_tape_.filter_mode_.wide_detent);
        CHECK(processor.processor_dirty_tape_.filter_mode_.wide_detent);
        CHECK(processor.processor_digital_.granular_player_.width_enabled_);
        CHECK(processor.processor_vintage_digital_.
            granular_player_.width_enabled_);
        CHECK(processor.processor_clean_tape_.granular_player_.width_enabled_);
        CHECK(processor.processor_dirty_tape_.granular_player_.width_enabled_);
        CHECK(processor.processor_digital_.granular_player_.stereo_width_ ==
            expected.processor_digital_.granular_player_.stereo_width_);
        CHECK(processor.processor_vintage_digital_.
            granular_player_.stereo_width_ ==
            expected.processor_digital_.granular_player_.stereo_width_);
        CHECK(processor.processor_clean_tape_.granular_player_.stereo_width_
            == expected.processor_digital_.granular_player_.stereo_width_);
        CHECK(processor.processor_dirty_tape_.granular_player_.stereo_width_
            == expected.processor_digital_.granular_player_.stereo_width_);
      }
      ModulatedParameter d;
      d.base = cv_reader.pot(POTS_ADC_CHANNEL_DENSITY);
      d.cv = cv_reader.cv(CV_ADC_CHANNEL_DENSITY);
      Processor::MixParameters m;
      float mix[3];
      for (int i = 0; i < 3; ++i) {
        mix[i] = ui.mix_parameter(Ui::MixParameter(i)) * 1.005f;
        if (i == 0 && !width) {
          mix[i] += cv_reader.cv(CV_ADC_CHANNEL_ASSIGN) - 0.002f;
        }
        CONSTRAIN(mix[i], 0.0f, 1.0f);
      }
      m.feedback = mix[0];
      m.dry_wet = mix[1];
      m.reverb_amount = mix[2];
      m.output_gate = false;
      FloatFrame in[kMaxCodecBlockSize];
      FloatFrame out[kMaxCodecBlockSize];
      bool freeze[kMaxCodecBlockSize] = { };
      for (size_t i = 0; i < codec.block_size_; ++i) {
        in[i] = 0.0f;
      }
      Random::Seed(random);
      expected.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze, d, p, m,
          in, out, codec.block_size_);
      // Feedback changes the recording before that audio reaches a grain.
      CHECK(fabsf(processor.processor_digital_.feedback_ -
          expected.processor_digital_.feedback_) < 1.0e-6f);
      for (size_t i = 0; i < codec.block_size_; ++i) {
        float l = out[i].l;
        float right = out[i].r;
        CONSTRAIN(l, -1.0f, 0.999999f);
        CONSTRAIN(right, -1.0f, 0.999999f);
        // One float ULP at full scale, plus integer conversion rounding.
        CHECK(fabs(double(r.out[i].l) - double(l * 2147483648.0f)) <= 256);
        CHECK(fabs(double(r.out[i].r) - double(right * 2147483648.0f)) <= 256);
        nonzero |= fabs(double(r.out[i].l)) > 10000;
      }
    }
    CHECK(nonzero);
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      const AdcCalibrationData& c =
          settings.persistent_data().adc_calibration[i];
      const float voltage = float(r.cv[i]) / 65536.0f * c.scale + c.offset;
      // All CV filters have settled after 500 callbacks.
      CHECK(fabsf(cv_reader.cv(CvAdcChannel(i)) - voltage) < 1.0e-5f);
    }
  }
}

float NextGrainSpeed(Rig* rig) {
  DelayGranularPlayer& player = processor.processor_digital_.granular_player_;
  for (int i = 0; i < 100; ++i) {
    DelayGrain* grain = player.new_grain();
    rig->Block();
    if (player.grain_interval_ <= int(codec.block_size_)) {
      CHECK(!grain->done());
      return fabsf(float(grain->speed_)) / 1024.0f;
    }
  }
  CHECK(false);  // No grain was triggered.
  return 0.0f;
}

// Default ADC transfer only; real-module calibration accuracy is out of scope.
void TestPitchChain() {
  test_checker.Start("TestPitchChain");
  for (int mode = 0; mode < 2; ++mode) {
    Rig r(false, mode != 0);
    r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
    r.cv_patched[CV_ADC_CHANNEL_DENSITY] = true;
    PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 65535;
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    r.Settle();
    if (mode) {
      r.StartAutomatic();
    }
    const float base = mode ? synth.wavetable_voice_.frequency_ :
        NextGrainSpeed(&r);
    r.cv[CV_ADC_CHANNEL_PITCH] = 26214;  // 1 V: 1/10 ADC range.
    r.Tick(30);
    const float octave = mode ? synth.wavetable_voice_.frequency_ :
        NextGrainSpeed(&r);
    // ADC, grain speed quantization and the pot endpoint give <0.01 st error.
    CHECK(fabsf(12.0f * log2f(octave / base) - 12.0f) < 0.01f);
    if (!mode) {
      bool octave_grain = false;
      DelayGranularPlayer& player =
          processor.processor_digital_.granular_player_;
      for (int i = 0; i < 32; ++i) {
        if (!player.grain_[i].done()) {
          octave_grain |= abs(player.grain_[i].speed_ - 2048) <= 2;
        }
      }
      CHECK(octave_grain);
    }
    // At the DENSITY knob endpoint, CV goes to exponential grain FM.
    TriggerGenerator& trigger = mode ? synth.trigger_generator_ :
        processor.trigger_generator_;
    const float before = trigger.phase_;
    r.Block();
    float delta = trigger.phase_ - before;
    if (delta < 0.0f) {
      delta += 1.0f;
    }
    r.cv[CV_ADC_CHANNEL_DENSITY] = 26214;  // 1 V -> 0.2 -> 12 st.
    r.Block();
    const float after = trigger.phase_;
    r.Block();
    float shifted = trigger.phase_ - after;
    if (shifted < 0.0f) {
      shifted += 1.0f;
    }
    // Phase subtraction introduces rounding; this is <0.01 semitone.
    CHECK(fabsf(12.0f * log2f(shifted / delta) - 12.0f) < 0.01f);
  }
}

void TestQuantizedPitchChain() {
  test_checker.Start("TestQuantizedPitchChain");
  Rig r(false, true);
  r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
  PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 65535;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
  settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
  r.Settle();
  CHECK(ui.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.cv[CV_ADC_CHANNEL_PITCH] = 30365;  // About 0.367 V, or 4.4 st.
  r.Tick(30);
  const float speed = NextGrainSpeed(&r);
  const float expected = stmlib::SemitonesToRatio(7.0f);
  CHECK(fabsf(speed - expected) < 2.0f / 1024.0f);
  settings.SaveState();
  r.Reboot();
  CHECK(settings.pitch_scale() == PITCH_SCALE_FIFTHS);
  CHECK(ui.pitch_scale() == PITCH_SCALE_FIFTHS);
}

void TestSelfPlayFrozenBackup() {
  test_checker.Start("TestSelfPlayFrozenBackup");
  Rig r(true, true);
  // A stock-format frozen buffer, with no CRC extension.
  BackgroundBufferBackup::BufferBackupHeader* header =
      reinterpret_cast<BackgroundBufferBackup::BufferBackupHeader*>(0x08100000);
  memset(header, 0, sizeof(*header));
  header->tag = 0x5a5a5246;
  header->write_ptr = 1234;
  header->media = Processor::MEDIA_DIGITAL;
  header->mono = false;
  settings.mutable_state()->frozen = true;
  settings.SaveState();
  r.Reboot();
  CHECK(!synth_mode && ui.freeze_button_state());
  CHECK(*processor.mutable_delay_write_ptr() == 1234);
  r.Settle();
  r.HoldMedia();
  CHECK(!synth_mode);  // FREEZE prevents starting and leaves backup intact.
  CHECK(settings.state().frozen);
  test_down[SWITCH_FREEZE] = true;
  r.Tick(10);
  test_down[SWITCH_FREEZE] = false;
  r.Tick(10);
  CHECK(!ui.freeze_button_state());
  CHECK(!settings.state().frozen);
  r.HoldMedia();
  CHECK(synth_mode);
  CHECK(!background_buffer_backup.previous_freeze_state_);
  r.Settle();
  const unsigned writes = test_writes;
  test_freeze_input = true;
  r.Block();
  r.Advance(10100);
  CHECK(test_writes == writes);  // The main loop skips backup in self-play.
  test_freeze_input = false;
  r.Tick(2);
  r.HoldMedia();
  CHECK(!synth_mode);
  r.Settle();
  CHECK(!settings.state().frozen);
  CHECK(!background_buffer_backup.previous_freeze_state_);
  CHECK(header->tag == 0x5a5a5246);  // Old flash remains until next save.
}

// Semitones above the synth's root note of the grain the next block starts.
float NextSynthGrainSemitones(Rig* rig) {
  WavetableGranularPlayer& player = synth.wavetable_granular_player_;
  for (int i = 0; i < 400; ++i) {
    WavetableGrain* grain = player.new_grain();
    rig->Block();
    if (player.grain_interval_ <= int(codec.block_size_)) {
      return 12.0f * log2f(
          grain->frequency_ * kSynthSampleRate / kWavetableRootNote);
    }
  }
  CHECK(false);  // No grain was triggered.
  return 0.0f;
}

// Distance in semitones to the nearest note of the scale, in any octave.
float DistanceToScale(float semitones, PitchScale scale) {
  const PitchScaleNotes& notes = kPitchScales[scale - 1];
  const float octave = floorf(semitones / 12.0f) * 12.0f;
  float distance = fabsf(semitones - octave - 12.0f);
  for (int i = 0; i < notes.size; ++i) {
    distance = std::min(distance, fabsf(
        semitones - octave - kPitchScaleNotes[notes.offset + i]));
  }
  return distance;
}

void StartSelfPlay(Rig* r, bool requested) {
  if (requested) {
    r->Settle();
    r->HoldMedia(false);
    MainLoopStep();
  } else {
    r->StartAutomatic();
  }
  CHECK(synth_mode);
  r->Settle();
}

// Grains of 32 triggers: how many are off the scale, and how many pitches.
void SurveySynthGrains(Rig* r, PitchScale scale, int* off_scale,
    int* distinct) {
  float seen[32];
  *off_scale = *distinct = 0;
  for (int i = 0; i < 32; ++i) {
    const float semitones = NextSynthGrainSemitones(r);
    if (DistanceToScale(semitones, scale) > 0.01f) {
      ++*off_scale;
    }
    bool known = false;
    for (int j = 0; j < *distinct; ++j) {
      known |= fabsf(seen[j] - semitones) < 0.05f;
    }
    if (!known) {
      seen[(*distinct)++] = semitones;
    }
  }
}

void TestSynthPitchScaleChain() {
  test_checker.Start("TestSynthPitchScaleChain");
  const PitchScale scale = PITCH_SCALE_PENTATONIC_MAJOR;
  for (int requested = 0; requested < 2; ++requested) {
    Rig r(requested != 0, true);
    r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    settings.set_pitch_scale(scale);
    settings.SaveState();
    for (int boot = 0; boot < 2; ++boot) {
      if (boot) {
        r.Reboot();  // The saved scale alone must bring it back.
      }
      CHECK(settings.pitch_scale() == scale);
      // Full randomness with no CV: the spread is wide, the grains on scale.
      r.cv[CV_ADC_CHANNEL_PITCH] = 32768;
      r.cv_patched[CV_ADC_CHANNEL_PITCH] = false;
      PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 65535;
      StartSelfPlay(&r, requested != 0);
      CHECK(ui.pitch_scale() == scale);
      int off_scale, distinct;
      SurveySynthGrains(&r, scale, &off_scale, &distinct);
      CHECK(off_scale == 0 && distinct >= 3);
      // Without a scale the same randomness leaves most grains between notes.
      settings.set_pitch_scale(PITCH_SCALE_OFF);
      r.Settle();
      SurveySynthGrains(&r, scale, &off_scale, &distinct);
      CHECK(off_scale >= 16);
      settings.set_pitch_scale(scale);
      r.Settle();

      // Pitch CV alone: the voice follows it exactly, the grains snap.
      r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
      PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 32768;
      const float cv_semitones[2] = { 4.4f, 5.8f };
      const float snapped[2] = { 3.9996f, 6.9996f };
      for (int i = 0; i < 2; ++i) {
        r.cv[CV_ADC_CHANNEL_PITCH] = uint16_t(
            (60.0f - cv_semitones[i]) * 65536.0f / 120.0f + 0.5f);
        r.Settle();
        const float voice = 12.0f * log2f(
            synth.wavetable_voice_.frequency_ * kSynthSampleRate /
            kWavetableRootNote);
        CHECK(fabsf(voice - cv_semitones[i]) < 0.02f);
        for (int grain = 0; grain < 4; ++grain) {
          CHECK(fabsf(NextSynthGrainSemitones(&r) - snapped[i]) < 0.02f);
        }
      }
    }
  }
}

void TestSynthWidthCallback() {
  test_checker.Start("TestSynthWidthCallback");
  Rig r(false, true);
  settings.set_stereo_width(128);
  settings.set_width_assigned(true);
  r.StartAutomatic();
  CHECK(synth_mode);
  r.cv_patched[CV_ADC_CHANNEL_ASSIGN] = true;
  r.cv[CV_ADC_CHANNEL_ASSIGN] = 30000;
  r.Settle();
  CHECK(synth.wavetable_granular_player_.width_enabled_);
  CHECK(synth.wavetable_granular_player_.stereo_width_ ==
      ModulatedStereoWidth(128,
          cv_reader.cv(CV_ADC_CHANNEL_ASSIGN) - 0.002f));
}

void TestCloudsSwitch() {
  test_checker.Start("TestCloudsSwitch");
  extern bool clouds_style;
  extern float reverb_style_gain;
  for (int mode = 0; mode < 2; ++mode) {
    Rig r(false, mode != 0);
    r.audio = 0x20000000;
    PotsAdc::values_[POTS_ADC_CHANNEL_REVERB] = 40000;
    r.Settle();
    if (mode) {
      r.StartAutomatic();
    }
    CHECK(synth_mode == (mode != 0));
    for (int change = 0; change < 2; ++change) {
      const bool requested = change == 0;
      settings.set_clouds_style(requested);
      SysTick_Handler();
      bool switched = false;
      bool faded = false;
      for (int block = 0; block < 100; ++block) {
        const bool previous = clouds_style;
        const float gain = reverb_style_gain;
        r.Block();
        CHECK(fabsf(reverb_style_gain - gain) <= 0.025001f);
        if (previous != clouds_style) {
          switched = true;
          CHECK(gain == 0.0f && reverb_style_gain == 0.0f);
          for (size_t i = 0; i < codec.block_size_; ++i) {
            CHECK(r.out[i].l == 0 && r.out[i].r == 0);
          }
          CHECK(processor.clouds_style_ == requested);
          CHECK(synth.clouds_style_ == requested);
          extern uint16_t reverb_buffer[];
          for (int i = 0; i < 32768; ++i) {
            CHECK(reverb_buffer[i] == 0);
          }
          for (int i = 0; i < 2048; ++i) {
            CHECK(processor.diffuser_buffer_[i] == 0);
          }
        }
        faded |= reverb_style_gain == 0.0f;
      }
      CHECK(switched && faded && clouds_style == requested);
      CHECK(reverb_style_gain == 1.0f);
    }
    settings.set_clouds_style(true);
    settings.SaveState();
    r.Reboot();
    CHECK(settings.clouds_style());
    r.Settle();
    CHECK(clouds_style);
  }
}

// Byte 21 picks the synth. The Plaits voices borrow the start of the first
// delay bank only in synth mode; leaving it clears what they used.
void TestPlaitsSynthSelection() {
  test_checker.Start("TestPlaitsSynthSelection");
  extern uint8_t delay_buffer_1[];
  extern uint8_t delay_buffer_2[];
  for (int plaits = 0; plaits < 2; ++plaits) {
    Rig r(false, true);
    settings.set_plaits_synth(plaits);
    settings.SaveState();
    r.Reboot();
    CHECK(ui.plaits_synth() == bool(plaits));
    r.Settle();
    CHECK(!synth_mode);
    // The granular processor has stopped writing when synth mode starts.
    memset(delay_buffer_1, 0x11, 512 * 1024);
    memset(delay_buffer_2, 0x22, 256 * 1024);
    r.Advance(10001);
    CHECK(synth_mode);
    r.Settle();
    bool loud = false;
    for (int i = 0; i < 2000; ++i) {
      r.Block();
      for (size_t j = 0; j < codec.block_size_; ++j) {
        loud = loud || fabs(double(r.out[j].l)) > 21474836.0;
      }
    }
    CHECK(loud);
    // Only the selected synth runs.
    const int model = plaits_synth.voice_[plaits_synth.active_voice_].model();
    CHECK(plaits ? model >= 0 : model == -1);
    for (size_t i = kPlaitsSynthRamSize; i < 512 * 1024; ++i) {
      if (delay_buffer_1[i] != 0x11) {
        CHECK(false);
        break;
      }
    }
    for (size_t i = 0; i < 256 * 1024; ++i) {
      if (delay_buffer_2[i] != 0x22) {
        CHECK(false);
        break;
      }
    }
    r.left = true;
    for (int i = 0; i < 2000 && synth_mode; ++i) {
      MainLoopStep();
      SysTick_Handler();
      if (synth_mode) {
        r.Block();
      }
    }
    CHECK(!synth_mode);
    for (size_t i = 0; i < kPlaitsSynthRamSize; ++i) {
      if (delay_buffer_1[i]) {
        CHECK(false);
        break;
      }
    }
  }
}

// A square wave in, with a main loop step and a SysTick every four blocks.
double RunSquare(Rig* r, int blocks) {
  double energy = 0.0;
  for (int i = 0; i < blocks; ++i) {
    r->audio = (i / 2) & 1 ? 0x10000000 : -0x10000000;
    if (i % 4 == 0) {
      MainLoopStep();
    }
    r->Block();
    if (i % 4 == 3) {
      SysTick_Handler();
    }
    for (size_t j = 0; j < codec.block_size_; ++j) {
      const double l = r->out[j].l / 2147483648.0;
      const double rr = r->out[j].r / 2147483648.0;
      energy += l * l + rr * rr;
    }
  }
  return energy;
}

// Engine changes fade out, clear the recording and fade back in. A frozen
// buffer saved to flash is kept, and returns with the granular engine.
// Spectral never starts the wavetable synth.
void TestSpectralEngine() {
  test_checker.Start("TestSpectralEngine");
  extern float reverb_style_gain;
  extern uint8_t delay_buffer_1[];
  extern uint8_t delay_buffer_2[];
  Rig r(false, false);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 65535;
  BackgroundBufferBackup::BufferBackupHeader* header =
      reinterpret_cast<BackgroundBufferBackup::BufferBackupHeader*>(0x08100000);
  memset(header, 0, sizeof(*header));
  header->tag = 0x5a5a5246;
  header->write_ptr = 1234;
  header->media = Processor::MEDIA_DIGITAL;
  header->mono = false;
  settings.mutable_state()->frozen = true;
  RunSquare(&r, 2400);
  CHECK(!synth_mode && !processor.spectral() && !ui.freeze_button_state());

  settings.set_spectral(true);
  SysTick_Handler();
  CHECK(ui.spectral() && !processor.spectral());
  float gain = reverb_style_gain;
  bool faded = false;
  for (int i = 0; i < 400 && !processor.spectral(); ++i) {
    RunSquare(&r, 1);
    CHECK(reverb_style_gain <= gain);
    gain = reverb_style_gain;
    faded |= gain == 0.0f;
  }
  CHECK(faded && processor.spectral() && codec_transition_frames > 0);
  bool cleared = true;
  for (size_t i = 0; i < 512 * 1024; ++i) {
    cleared = cleared && delay_buffer_1[i] == 0;
  }
  for (size_t i = 0; i < 256 * 1024; ++i) {
    cleared = cleared && delay_buffer_2[i] == 0;
  }
  CHECK(cleared);
  CHECK(settings.state().frozen && header->tag == 0x5a5a5246);

  // Silent while the codec restarts, then back to full level.
  CHECK(RunSquare(&r, 990) == 0.0);
  CHECK(RunSquare(&r, 4000) > 1.0);
  CHECK(reverb_style_gain == 1.0f);
  const SpectralProcessor& spectral = processor.spectral_processor_;
  CHECK(std::isfinite(spectral.previous_sample_.l) &&
        std::isfinite(spectral.previous_sample_.r));

  // FREEZE holds the spectrum; nothing is saved to flash.
  const unsigned writes = test_writes;
  test_freeze_input = true;
  RunSquare(&r, 4);
  CHECK(spectral.parameters_.freeze);
  r.Advance(11000);
  CHECK(test_writes == writes);
  test_freeze_input = false;
  RunSquare(&r, 4);

  // Unpatched inputs do not start the synth, automatically or on a hold.
  r.left = r.right = false;
  r.Settle();
  r.Advance(11000);
  MainLoopStep();
  CHECK(!synth_mode);
  settings.set_self_play_on_request(true);
  r.HoldMedia();
  CHECK(!synth_mode && !ui.self_play_requested());
  r.left = r.right = true;

  // Back to granular: the saved frozen buffer returns, with its own quality
  // even if another was picked meanwhile.
  settings.mutable_state()->media = Processor::MEDIA_CLEAN_TAPE;
  RunSquare(&r, 3000);
  CHECK(processor.spectral() && processor.media() == Processor::MEDIA_CLEAN_TAPE);
  settings.set_spectral(false);
  SysTick_Handler();
  for (int i = 0; i < 400 && processor.spectral(); ++i) {
    RunSquare(&r, 1);
  }
  CHECK(!processor.spectral() && !synth_mode);
  CHECK(ui.freeze_button_state());
  CHECK(*processor.mutable_delay_write_ptr() == 1234);
  RunSquare(&r, 2000);
  CHECK(reverb_style_gain == 1.0f);
  CHECK(processor.media() == Processor::MEDIA_DIGITAL);
  CHECK(settings.state().media == Processor::MEDIA_DIGITAL);
  CHECK(*processor.mutable_delay_write_ptr() == 1234);

  // Starting in spectral leaves the saved buffer in flash.
  settings.set_spectral(true);
  settings.SaveState();
  r.Reboot();
  CHECK(processor.spectral() && !ui.freeze_button_state());
  CHECK(settings.state().frozen && header->tag == 0x5a5a5246);
  RunSquare(&r, 4000);
  CHECK(processor.spectral() && !synth_mode);
}

int main() {
  setbuf(stdout, NULL);
  void* flash = mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (flash == MAP_FAILED) {
    return 2;
  }
  TestSynthWidthCallback();
  TestSynthPitchScaleChain();
  TestCloudsSwitch();
  TestAutomaticSelfPlay();
  TestRequestedSelfPlay();
  TestSelfPlayRequestLifecycle();
  TestMonoStereo();
  TestFrozenQualityLock();
  TestOverwriteIndicator();
  TestTriggerOutput();
  TestStartupBranches();
  TestCallbackCvRouting();
  TestPitchChain();
  TestQuantizedPitchChain();
  TestSelfPlayFrozenBackup();
  TestPlaitsSynthSelection();
  TestSpectralEngine();
  return test_checker.failures() ? 1 : 0;
}
