// Copyright 2026 Insolent Construct.
//
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

#include "beads/test/test_fpu.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>
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
// Host linkage needs definitions for these declared constructors.
CvChannel::CvChannel() { }
CvChannel::~CvChannel() { }
PotChannel::PotChannel() { }
PotChannel::~PotChannel() { }
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
extern volatile bool codec_fade;
extern volatile float codec_fade_gain;

struct Rig;
Rig* current_rig = NULL;
void RigBlock(Rig* r);

// Only the cache warming/timing call WasteCPUCycles is skipped.
void MainLoopStep() {
  ui.DoEvents();
  MonitorStateChange();
  if (!synth_mode) {
    background_buffer_backup.Process(ui.freeze_led_state());
  }
}

// One main loop pass; the audio interrupt runs on while main waits for a
// fade before a codec restart (#53).
void RunMainLoop() {
  MainLoopStep();
  for (int i = 0; i < 64 && current_rig && codec_fade &&
       codec_fade_gain > 0.0f && codec.callback_; ++i) {
    RigBlock(current_rig);
  }
  if (codec_fade && codec_fade_gain == 0.0f) MainLoopStep();
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
  std::vector<int32_t>* peaks;

  Rig(bool unpatched = true)
      : left(!unpatched), right(!unpatched), seed_patched(false), audio(0),
        peaks(NULL) {
    current_rig = this;
    memset(reinterpret_cast<void*>(0x08100000), 255, 0x100000);
    PowerOnObject(&settings);
    settings.Init(false);
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
    CHECK(codec.idle_stops_ == 0);
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
      if (peaks) {
        int32_t peak = 0;
        for (size_t i = 0; i < codec.block_size_; ++i) {
          peak = std::max(peak, std::abs(out[i].l));
        }
        peaks->push_back(peak);
      }
    }
  }

  void Tick(int milliseconds, bool main_loop = true) {
    while (milliseconds--) {
      // The first main step starts the codec; thereafter four callbacks/ms.
      if (main_loop) RunMainLoop();
      for (int i = 0; i < 4; ++i) Block();
      SysTick_Handler();
    }
  }

  // Clock-only waits are used after known silence, or when audio cannot affect
  // the decision. Boundary callbacks still verify the signal and gate state.
  void Advance(int milliseconds) {
    while (milliseconds--) {
      RunMainLoop();
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
    if (consume) RunMainLoop();
  }

  // Eight 32 ms probe windows plus 250 ms of codec blanking.
  void Settle() {
    Tick(600);
    CHECK(codec.idle_stops_ == 0);
  }
};

void RigBlock(Rig* r) { r->Block(); }

std::string Compact(const std::string& source) {
  std::string result;
  for (size_t i = 0; i < source.size(); ++i) {
    if (!isspace(static_cast<unsigned char>(source[i]))) result += source[i];
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
    if (source[end] == '{') ++depth;
    if (source[end] == '}') --depth;
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
  RunMainLoop();
  const int start = synth_mode_start_time;
  r.Advance(9900);
  RunMainLoop();
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
  RunMainLoop();
  CHECK(synth_mode_start_time == 0);
  test_freeze_input = false;
  r.Tick(2);
  r.Advance(9900);
  RunMainLoop();
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

  Rig tail(false);
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
    RunMainLoop();
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





void TestMonoStereo() {
  test_checker.Start("TestMonoStereo");
  Rig r(false);
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
  Rig r(false);
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

// Check the overwrite reserve supplied to the UI.
void TestOverwriteIndicator() {
  test_checker.Start("TestOverwriteIndicator");
  Rig r(false);
  r.Settle();
  test_freeze_input = true;
  r.Tick(1);
  test_freeze_input = false;
  r.Tick(1);
  // Observe the reserve before each callback (the callback feeds UI first).
  float minimum = 1.0f;
  bool passed = false;
  for (int i = 0; i < 17000; ++i) {
    float expected = processor.overwrite_reserve();
    r.Block();
    CHECK(fabsf(ui.overwrite_reserve_ - expected) < 1.0e-6f);
    minimum = std::min(minimum, ui.overwrite_reserve_);
    if (minimum < 0.01f && ui.overwrite_reserve_ == 1.0f) passed = true;
  }
  CHECK(minimum < 0.01f);
  CHECK(passed);

}

void TestTriggerOutput() {
  test_checker.Start("TestTriggerOutput");
  for (int mode = 0; mode < 2; ++mode) {
    Rig r(!mode);
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    r.audio = 0x10000000;
    r.Settle();
    if (!mode) r.StartAutomatic();
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
    Rig r(false);
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) r.cv_patched[i] = true;
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
        if (i == 0) mix[i] += cv_reader.cv(CV_ADC_CHANNEL_ASSIGN) - 0.002f;
        CONSTRAIN(mix[i], 0.0f, 1.0f);
      }
      m.feedback = mix[0];
      m.dry_wet = mix[1];
      m.reverb_amount = mix[2];
      m.output_gate = false;
      FloatFrame in[kMaxCodecBlockSize];
      FloatFrame out[kMaxCodecBlockSize];
      bool freeze[kMaxCodecBlockSize] = { };
      for (size_t i = 0; i < codec.block_size_; ++i) in[i] = 0.0f;
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
    Rig r(mode != 0);
    r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
    r.cv_patched[CV_ADC_CHANNEL_DENSITY] = true;
    PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 65535;
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    r.Settle();
    if (mode) r.StartAutomatic();
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
    if (delta < 0.0f) delta += 1.0f;
    r.cv[CV_ADC_CHANNEL_DENSITY] = 26214;  // 1 V -> 0.2 -> 12 st.
    r.Block();
    const float after = trigger.phase_;
    r.Block();
    float shifted = trigger.phase_ - after;
    if (shifted < 0.0f) shifted += 1.0f;
    // Phase subtraction introduces rounding; this is <0.01 semitone.
    CHECK(fabsf(12.0f * log2f(shifted / delta) - 12.0f) < 0.01f);
  }
}



// A change at the same sample rate keeps the converter clocked (#53): only
// the 24 and 32 kHz qualities stop and restart it.
void TestCodecKeepsClocks() {
  test_checker.Start("TestCodecKeepsClocks");
  Rig r(false);
  r.Settle();
  const Processor::Media media[] = {
    Processor::MEDIA_DIGITAL,
    Processor::MEDIA_CLEAN_TAPE,
    Processor::MEDIA_VINTAGE_DIGITAL,
    Processor::MEDIA_DIRTY_TAPE,
    Processor::MEDIA_DIGITAL,
  };
  for (size_t i = 0; i < sizeof(media) / sizeof(media[0]); ++i) {
    const int previous_rate = codec.sample_rate_;
    const int starts = codec.starts_;
    const int stops = codec.stops_;
    settings.mutable_state()->media = media[i];
    for (int j = 0; j < 200 && processor.media() != media[i]; ++j) {
      r.Tick(1);
    }
    CHECK(processor.media() == media[i]);
    CHECK(codec.callback_ == &Process);
    const int rate = int(processor.sample_rate());
    const int restarts = rate == previous_rate ? 0 : 1;
    CHECK(codec.sample_rate_ == rate);
    CHECK(codec.starts_ == starts + restarts);
    CHECK(codec.stops_ == stops + restarts);
    r.Settle();
  }
}

// Codec restarts fade out first (#53): the output steps down to silence
// over about 10 ms instead of stopping at full level.
void TestCodecRestartFade() {
  test_checker.Start("TestCodecRestartFade");
  Rig r(false);
  r.Settle();
  std::vector<int32_t> peaks;
  r.peaks = &peaks;
  for (int i = 0; i < 400; ++i) {
    r.audio = (i / 2) & 1 ? 0x20000000 : -0x20000000;
    r.Block();
  }
  const int32_t steady = peaks.back();
  CHECK(steady > 0x08000000);
  settings.mutable_state()->media = (settings.state().media + 1) % 4;
  const Processor::Media before = processor.media();
  for (int i = 0; i < 200 && processor.media() == before; ++i) {
    r.audio = (i / 2) & 1 ? 0x20000000 : -0x20000000;
    RunMainLoop();
    r.Block();
  }
  CHECK(processor.media() != before);
  r.peaks = NULL;
  // The last sounding block before the restart is quiet.
  size_t last = peaks.size();
  while (last && peaks[last - 1] == 0) --last;
  CHECK(last > 40);
  printf("Restart fade: steady %d, last %d\n", steady, peaks[last - 1]);
  CHECK(peaks[last - 1] < steady / 8);

  // FREEZE during the fade postpones the restart without leaving silence.
  r.Settle();
  settings.mutable_state()->media = (settings.state().media + 1) % 4;
  MainLoopStep();
  CHECK(codec_fade);
  test_freeze_input = true;
  for (int i = 0; i < 400; ++i) {
    r.Block();
    MainLoopStep();
  }
  CHECK(!codec_fade && codec_fade_gain == 1.0f);
  test_freeze_input = false;
  r.Settle();
}

// Stopping the synth clears the reverb memory it shares.
void TestSynthStopClearsReverb() {
  test_checker.Start("TestSynthStopClearsReverb");
  extern uint16_t reverb_buffer[];
  Rig r;
  PotsAdc::values_[POTS_ADC_CHANNEL_REVERB] = 60000;
  r.StartAutomatic();
  CHECK(synth_mode);
  r.Tick(500);
  bool used = false;
  for (int i = 0; i < 32768 && !used; ++i) used = reverb_buffer[i] != 0;
  CHECK(used);
  r.left = true;
  for (int i = 0; i < 2000 && synth_mode; ++i) r.Tick(1);
  CHECK(!synth_mode && codec_transition_frames > 0);
  bool clear = true;
  for (int i = 0; i < 32768 && clear; ++i) clear = reverb_buffer[i] == 0;
  CHECK(clear);
}

int main() {
  InitTestFpu();
  setbuf(stdout, NULL);
  void* flash = mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (flash == MAP_FAILED) return 2;
  TestMainLoopMatchesHarness();
  TestAutomaticSelfPlay();
  TestMonoStereo();
  TestFrozenQualityLock();
  TestOverwriteIndicator();
  TestTriggerOutput();
  TestStartupBranches();
  TestCallbackCvRouting();
  TestPitchChain();
  TestCodecKeepsClocks();
  TestCodecRestartFade();
  TestSynthStopClearsReverb();
  return test_checker.failures() ? 1 : 0;
}
