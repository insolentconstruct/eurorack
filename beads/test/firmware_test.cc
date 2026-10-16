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
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

#define private public
#define protected public
#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/cv_reader.h"
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/factory_test.h"
#include "beads/gain_control.h"
#include "beads/ui.h"
#undef protected
#undef private

using namespace beads;
using namespace stmlib;

bool test_down[4];
unsigned test_writes;
namespace stmlib {
SystemClock system_clock;
}  // namespace stmlib
namespace beads {
// The firmware does not define these constructors.
CvChannel::CvChannel() { }
CvChannel::~CvChannel() { }
PotChannel::PotChannel() { }
PotChannel::~PotChannel() { }
void TestOptionsDispatchInterrupt() { }
bool test_probe;
bool test_freeze_input;
bool test_seed_input;
uint16_t CvAdc::values_[CV_ADC_CHANNEL_LAST];
uint16_t PotsAdc::values_[POTS_ADC_CHANNEL_LAST];
bool Leds::Init() {
  Clear();
  return true;
}
void Leds::Clear() { std::fill(intensity_, intensity_ + LED_LAST, 0); }
void Leds::Write() { }
}  // namespace beads

extern BackgroundBufferBackup background_buffer_backup;
extern Calibrator calibrator;
extern CvReader cv_reader;
extern Codec codec;
extern FactoryTest factory_test;
extern GainControl gain_control;
extern NormalizationDetector normalization_detector;
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
  if (!synth_mode) {
    background_buffer_backup.Process(ui.freeze_led_state());
  }
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

  Rig(bool on_request = false, bool unpatched = true)
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

std::string Compact(const std::string& source) {
  std::string result;
  for (size_t i = 0; i < source.size(); ++i) {
    if (!isspace(static_cast<unsigned char>(source[i]))) {
      result += source[i];
    }
  }
  return result;
}

// Returns the body of the block that opens at the first `opening` after
// `anchor` in a source file, or an empty string.
std::string Body(const char* path, const char* anchor, const char* opening) {
  std::ifstream file(path);
  std::string source((std::istreambuf_iterator<char>(file)),
      std::istreambuf_iterator<char>());
  size_t start = source.find(anchor);
  if (start == std::string::npos) {
    return std::string();
  }
  start = source.find(opening, start);
  if (start == std::string::npos) {
    return std::string();
  }
  start = source.find('{', start) + 1;
  size_t end = start;
  int depth = 1;
  for (; end < source.size() && depth; ++end) {
    if (source[end] == '{') {
      ++depth;
    }
    if (source[end] == '}') {
      --depth;
    }
  }
  return depth ? std::string() : source.substr(start, end - start - 1);
}

void TestMainLoopMatchesHarness() {
  test_checker.Start("TestMainLoopMatchesHarness");
  // main()'s loop body must equal MainLoopStep() plus the timing call.
  const std::string firmware = Body(
      "beads/beads.cc", "int main(void)", "while (1) {");
  const std::string harness = Body(
      "beads/test/firmware_test.cc", "void MainLoopStep() {", "{");
  CHECK(!firmware.empty() && !harness.empty());
  CHECK(Compact(firmware) ==
      Compact(harness + "processor.WasteCPUCycles(16);"));
}

void TestAutomaticSelfPlay() {
  test_checker.Start("TestAutomaticSelfPlay");
  Rig r;
  r.StartAutomatic();
  CHECK(synth_mode);
  r.left = true;
  r.Settle();
  CHECK(!synth_mode);
  r.left = false;
  r.Settle();
  // Silence now, with no grains or reverb tail from the initial synth.
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
  Rig r(true);
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
    Rig r(true);
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
  Rig r;
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
  for (int channel = 0; channel < CV_ADC_CHANNEL_LAST; ++channel) {
    Rig r(false, false);
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
    r.cv[channel] = 30000;
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
      ModulatedParameter d;
      d.base = cv_reader.pot(POTS_ADC_CHANNEL_DENSITY);
      d.cv = cv_reader.cv(CV_ADC_CHANNEL_DENSITY);
      Processor::MixParameters m;
      float mix[3];
      for (int i = 0; i < 3; ++i) {
        mix[i] = ui.mix_parameter(Ui::MixParameter(i)) * 1.005f;
        if (i == 0) {
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

void TestSelfPlayFrozenBackup() {
  test_checker.Start("TestSelfPlayFrozenBackup");
  Rig r(true);
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

int main() {
  setbuf(stdout, NULL);
  void* flash = mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (flash == MAP_FAILED) {
    return 2;
  }
  TestCloudsSwitch();
  TestMainLoopMatchesHarness();
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
  TestSelfPlayFrozenBackup();
  return test_checker.failures() ? 1 : 0;
}
