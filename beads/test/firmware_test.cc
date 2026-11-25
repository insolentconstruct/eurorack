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
#include <string>
#include <vector>
#include <new>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

#define private public
#define protected public
#include "beads/background_buffer_backup.h"
#include "beads/blob/synth_choice.h"
#include "beads/calibrator.h"
#include "beads/cv_reader.h"
#include "beads/dsp/plaits_synth.h"
#include "beads/dsp/rings_synth.h"
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/engine_transition.h"
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
uint32_t test_writes;
namespace beads {
bool test_probe;
bool test_freeze_input;
bool test_seed_input;
uint16_t CvAdc::values_[CV_ADC_CHANNEL_LAST];
uint16_t PotsAdc::values_[POTS_ADC_CHANNEL_LAST];
}  // namespace beads

extern EngineTransition transition;
SynthEngine& playing_synth = transition.playing_synth_;
SynthEngine& bank_owner = transition.bank_owner_;
int& codec_transition_frames = transition.codec_transition_frames_;
volatile bool& codec_fade = transition.codec_fade_;
float& reverb_style_gain = transition.reverb_style_gain_;
bool& clouds_style = transition.clouds_style_;

extern BackgroundBufferBackup background_buffer_backup;
extern Calibrator calibrator;
extern CvReader cv_reader;
extern Codec codec;
extern FactoryTest factory_test;
extern GainControl gain_control;
extern NormalizationDetector normalization_detector;
extern PlaitsSynth plaits_synth;
extern RingsSynth rings_synth;
extern uint8_t playable_synths;
extern SynthEngine& playing_synth;
extern SynthEngine& bank_owner;
// The Rings stand-in's calls, and whether Process is running.
bool in_audio;
uint32_t configure_calls;
uint32_t render_calls;
uint32_t configure_in_audio;
extern Processor processor;
extern Settings settings;
extern Synth synth;
extern Ui ui;
extern bool synth_mode;
extern int synth_mode_start_time;
extern int& codec_transition_frames;
extern volatile bool ready;
extern GateFlags previous_seed;
extern uint8_t seed_latch_mode;
void Init();
void MonitorStateChange();
void ConfigureAndStartCodec(bool new_synth_mode, bool reset_gain_control);
void SwapPaused(SynthEngine owner, SynthEngine playing);
void Process(const Codec::Event*, const Codec::Frame*, Codec::Frame*, size_t);
extern "C" void SysTick_Handler();

// Only the cache warming/timing call WasteCPUCycles is skipped.
struct Rig;
Rig* current_rig = NULL;
void RigBlock(Rig* r);

void MainLoopStep() {
  ui.DoEvents();
  MonitorStateChange();
  if (!synth_mode && !processor.spectral()) {
    background_buffer_backup.Process(ui.freeze_led_state());
  }
  processor.Buffer();
}

// One main loop pass; the audio interrupt runs on while main waits for a
// fade before a codec restart (#53).
void RunMainLoop() {
  extern volatile bool& codec_fade;
  extern float& reverb_style_gain;
  MainLoopStep();
  for (int i = 0; i < 64 && current_rig && codec_fade &&
       reverb_style_gain > 0.0f && codec.callback_; ++i) {
    RigBlock(current_rig);
  }
  if (codec_fade && reverb_style_gain == 0.0f) {
    MainLoopStep();
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
  std::vector<int32_t>* peaks;

  Rig(bool on_request, bool unpatched)
      : left(!unpatched), right(!unpatched), seed_patched(false), audio(0),
        peaks(NULL) {
    current_rig = this;
    TestFlashRepairAll();
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
    Reboot(false);
  }

  void Reboot(bool seed) {
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
      in_audio = true;
      codec.callback_(events, in, out, codec.block_size_);
      in_audio = false;
      if (peaks) {
        int32_t peak = 0;
        for (size_t i = 0; i < codec.block_size_; ++i) {
          peak = std::max(peak, std::abs(out[i].l));
        }
        peaks->push_back(peak);
      }
    }
  }

  void Tick(int milliseconds, bool main_loop) {
    while (milliseconds--) {
      // The first main step starts the codec; thereafter four callbacks/ms.
      if (main_loop) {
        RunMainLoop();
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
      RunMainLoop();
      SysTick_Handler();
    }
  }

  void StartAutomatic() {
    Settle();
    Advance(10001);
    Settle();
  }

  void HoldMedia(bool consume) {
    test_down[SWITCH_MEDIA] = true;
    Tick(10, true);
    // The input and pots stay fixed during the hold; only the UI clock matters.
    Advance(1497);
    test_down[SWITCH_MEDIA] = false;
    // Dispatch the hold event, leaving MonitorStateChange to the caller.
    Tick(8, false);
    ui.DoEvents();
    if (consume) {
      RunMainLoop();
    }
  }

  // Eight 32 ms probe windows plus 250 ms of codec blanking.
  void Settle() {
    Tick(600, true);
    CHECK(codec.idle_stops_ == 0);
  }
};

void RigBlock(Rig* r) { r->Block(); }

void TestSynthFadeSurvivesTimerReset() {
  test_checker.Start("TestSynthFadeSurvivesTimerReset");
  extern volatile bool& codec_fade;
  extern float& reverb_style_gain;
  Rig r(false, true);
  r.left = false;
  r.Settle();
  CHECK(normalization_detector.no_input() && !synth_mode);
  synth_mode_start_time = system_clock.milliseconds();
  for (int i = 0; i < 10000; ++i) SysTick_Handler();
  reverb_style_gain = 1.0f;
  MonitorStateChange();
  CHECK(codec_fade && !synth_mode);
  synth_mode_start_time = 0;  // A late output peak in the callback.
  reverb_style_gain = 0.0f;
  MonitorStateChange();
  CHECK(synth_mode);
}

void TestSynthEntryCancelsBackup() {
  test_checker.Start("TestSynthEntryCancelsBackup");
  extern float& reverb_style_gain;
  Rig r(false, true);
  r.left = false;
  r.Settle();
  CHECK(normalization_detector.no_input() && !synth_mode);
  synth_mode_start_time = system_clock.milliseconds();
  for (int i = 0; i < 10000; ++i) SysTick_Handler();
  reverb_style_gain = 0.0f;
  background_buffer_backup.previous_freeze_state_ = true;
  background_buffer_backup.remaining_sectors_ = 1;
  MonitorStateChange();
  CHECK(synth_mode);
  CHECK(!background_buffer_backup.busy());
}

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
    r.Tick(1, true);
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
  r.Tick(1, true);
  RunMainLoop();
  CHECK(synth_mode_start_time == 0);
  test_freeze_input = false;
  r.Tick(2, true);
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

  Rig tail(false, false);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 65535;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
  PotsAdc::values_[POTS_ADC_CHANNEL_SIZE] = 50000;
  PotsAdc::values_[POTS_ADC_CHANNEL_SHAPE] = 0;
  for (int i = 0; i < 3000; ++i) {
    tail.audio = i & 1 ? 0x20000000 : -0x20000000;
    tail.Tick(1, true);
  }
  tail.left = tail.right = false;
  tail.Tick(100, true);
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
  RunMainLoop();
  CHECK(synth_mode);
  CHECK(!ui.self_play_requested());
  r.Settle();
  r.HoldMedia(false);
  CHECK(synth_mode);
  RunMainLoop();
  CHECK(!synth_mode);
  r.Settle();
  r.left = true;
  r.audio = 0x10000000;
  r.Settle();
  settings.mutable_state()->gain = 20;
  r.HoldMedia(true);
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
    RunMainLoop();
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
  r.Tick(1, true);
  test_down[SWITCH_MEDIA] = true;
  r.Tick(10, true);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(10, true);
  CHECK(settings.state().media == Processor::MEDIA_VINTAGE_DIGITAL);
  r.right = false;
  r.Settle();
  CHECK(!processor.mono());
  CHECK(processor.media() == Processor::MEDIA_DIGITAL);
  test_freeze_input = false;
  r.Tick(2, true);
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
  r.Tick(1, true);
  test_freeze_input = false;
  r.Tick(1, true);
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
  r.Tick(1, true);
  test_freeze_input = false;
  r.Tick(1, true);
  for (int i = 0; i < 15800; ++i) {
    r.Block();
  }
  r.HoldMedia(false);
  CHECK(processor.overwrite_reserve() < 0.5f);
  RunMainLoop();
  CHECK(synth_mode);
  r.Tick(260, true);
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
  // A new module's blank storage starts the factory test.
  memset(reinterpret_cast<void*>(0x08100000), 255, 0x100000);
  r.Reboot(false);
  CHECK(factory_test.running());
  CHECK(ui.mode_ == Ui::MODE_FACTORY_TEST);
  CHECK(codec.callback_ == &FactoryTest::ProcessFn);
  r.Tick(20, true);
  uint32_t writes = test_writes;
  r.Reboot(false);
  CHECK(!factory_test.running());
  CHECK(ui.mode_ == Ui::MODE_NORMAL);
  CHECK(test_writes == writes);
  // Break the persisted CALI header checksum, not the in-memory object:
  // damaged storage never starts the factory test.
  reinterpret_cast<uint8_t*>(0x081e0000)[6] ^= 1;
  reinterpret_cast<uint8_t*>(0x081c0000)[6] ^= 1;  // And its mirror.
  writes = test_writes;
  r.Reboot(false);
  CHECK(!factory_test.running());
  CHECK(settings.write_locked());
  CHECK(ui.mode_ == Ui::MODE_STORAGE_WARNING);
  CHECK(test_writes == writes);
  r.Settle();
  CHECK(codec.callback_ == &Process);
}

bool AllLeds(int intensity) {
  for (int i = 0; i < LED_LAST; ++i) {
    if (ui.leds_.intensity_[i] != intensity) {
      return false;
    }
  }
  return true;
}

// A module in use whose calibration and its mirror are both lost.
void LoseCalibration() {
  for (int i = 0; i < 4; ++i) {
    settings.mutable_state()->gain = uint8_t(10 + i);
    settings.SaveState();
  }
  TestFlashDamage(0x081e0000 + 32);
  TestFlashDamage(0x081c0000 + 32);
}

void TestSuppressedStorageChord() {
  test_checker.Start("TestSuppressedStorageChord");
  Rig r(false, true);
  LoseCalibration();
  r.Reboot(false);
  test_down[SWITCH_ASSIGN] = true;
  r.Tick(3010, true);
  CHECK(ui.mode_ == Ui::MODE_NORMAL);
  test_down[SWITCH_FREEZE] = true;
  r.Tick(20, true);
  CHECK(ui.freeze_button_state_);
  CHECK(!settings.state().disable_autosave);
}

void TestWarningChordDebounceBoundary() {
  test_checker.Start("TestWarningChordDebounceBoundary");
  Rig r(false, true);
  LoseCalibration();
  r.Reboot(false);
  while (system_clock.milliseconds() - ui.storage_status_time_ < 2998) {
    r.Advance(1);
  }
  CHECK(ui.mode_ == Ui::MODE_STORAGE_WARNING);
  test_down[SWITCH_ASSIGN] = true;
  r.Tick(20, true);
  CHECK(ui.mode_ == Ui::MODE_NORMAL);
  test_down[SWITCH_FREEZE] = true;
  r.Tick(20, true);
  CHECK(ui.freeze_button_state_);
  CHECK(!settings.state().disable_autosave);
}

void TestStorageWarning() {
  test_checker.Start("TestStorageWarning");
  Rig r(false, true);
  settings.mutable_persistent_data()->adc_calibration[0].offset = 0.987f;
  settings.SavePersistentData();
  LoseCalibration();
  static uint8_t sector[0x20000];
  memcpy(sector, reinterpret_cast<void*>(0x081e0000), sizeof(sector));
  const uint32_t writes = test_writes;
  for (int boot = 0; boot < 2; ++boot) {
    r.Reboot(false);
    CHECK(settings.write_locked());
    CHECK(!factory_test.running());
    CHECK(settings.persistent_data().adc_calibration[0].offset != 0.987f);
    CHECK(settings.state().gain == 0);
    // At every power-up, all LEDs blink fast for 3 s, as MODE_PANIC does,
    // and buttons held through it do nothing.
    CHECK(ui.mode_ == Ui::MODE_STORAGE_WARNING);
    test_down[SWITCH_ASSIGN] = true;
    test_down[SWITCH_MEDIA] = true;
    bool lit = false;
    bool dark = false;
    for (int i = 0; i < 2990; ++i) {
      r.Tick(1, true);
      lit = lit || AllLeds(255);
      dark = dark || AllLeds(0);
    }
    CHECK(lit && dark);
    CHECK(ui.mode_ == Ui::MODE_STORAGE_WARNING);
    r.Tick(20, true);
    CHECK(ui.mode_ == Ui::MODE_NORMAL);
    r.Tick(2000, true);
    test_down[SWITCH_ASSIGN] = false;
    test_down[SWITCH_MEDIA] = false;
    r.Tick(100, true);
    CHECK(ui.mode_ == Ui::MODE_NORMAL);
    CHECK(!ui.options_.active());
    CHECK(settings.state().assignable_cv_destination == 0);
    CHECK(settings.state().media == 3);
    // Then normal play; changes apply in RAM and nothing is written.
    test_down[SWITCH_MEDIA] = true;
    r.Tick(50, true);
    test_down[SWITCH_MEDIA] = false;
    r.Tick(50, true);
    CHECK(settings.state().media == 2);
    CHECK(test_writes == writes);
    TestFlashExpose(true);
    CHECK(!memcmp(sector, reinterpret_cast<void*>(0x081e0000),
        sizeof(sector)));
  }
}

void TestStorageRepair() {
  test_checker.Start("TestStorageRepair");
  // A torn calibration with its mirror intact: restored without a word,
  // and no repair touches the frozen buffer.
  {
    Rig r(false, true);
    settings.mutable_persistent_data()->adc_calibration[0].offset = 0.987f;
    settings.SavePersistentData();
    TestFlashDamage(0x081e0000 + 32);
    TestFlashDamage(0x08100000 + 0x30000);
    r.Reboot(false);
    CHECK(!settings.write_locked());
    CHECK(ui.mode_ == Ui::MODE_NORMAL);
    CHECK(settings.persistent_data().adc_calibration[0].offset == 0.987f);
    CHECK(TestFlash().num_damaged == 1);
  }
  // Repaired with the factory calibration: the green mode LED blinks for
  // 3 s with the three ASSIGN LEDs lit, and the frozen buffer is erased.
  {
    Rig r(false, true);
    LoseCalibration();
    TestFlashDamage(0x08100000 + 0x30000);
    r.Reboot(true);
    CHECK(settings.repair() == STORAGE_REPAIR_CALIBRATION_RESET);
    CHECK(!settings.write_locked());
    CHECK(TestFlash().num_damaged == 0);
    CHECK(ui.mode_ == Ui::MODE_REPAIR_CALIBRATION_RESET);
    r.Tick(100, true);
    CHECK(ui.leds_.intensity_[LED_MEDIA_G] == 255);
    CHECK(ui.leds_.intensity_[LED_MEDIA_R] == 0);
    CHECK(ui.leds_.intensity_[LED_ASSIGN_1] == 255);
    CHECK(ui.leds_.intensity_[LED_ASSIGN_2] == 255);
    CHECK(ui.leds_.intensity_[LED_ASSIGN_3] == 255);
    r.Tick(500, true);
    CHECK(ui.leds_.intensity_[LED_MEDIA_G] == 0);
    r.Tick(2420, true);
    CHECK(ui.mode_ == Ui::MODE_NORMAL);
    test_down[SWITCH_SEED] = false;
    r.Reboot(false);
    CHECK(ui.mode_ == Ui::MODE_NORMAL);
    CHECK(!settings.write_locked());
  }
  // Failed: red mode LED and ASSIGN LEDs blinking; still guarded, and the
  // frozen buffer is left alone.
  {
    Rig r(false, true);
    LoseCalibration();
    TestFlashDamage(0x08100000 + 0x30000);
    TestFlashFailures() = 1000;
    r.Reboot(true);
    TestFlashFailures() = 0;
    CHECK(settings.repair() == STORAGE_REPAIR_FAILED);
    CHECK(settings.write_locked());
    CHECK(TestFlash().num_damaged == 3);
    CHECK(ui.mode_ == Ui::MODE_REPAIR_FAILED);
    r.Tick(50, true);
    CHECK(ui.leds_.intensity_[LED_MEDIA_R] == 255);
    CHECK(ui.leds_.intensity_[LED_MEDIA_G] == 0);
    CHECK(ui.leds_.intensity_[LED_ASSIGN_2] == 255);
    r.Tick(100, true);
    CHECK(ui.leds_.intensity_[LED_MEDIA_R] == 0);
    CHECK(ui.leds_.intensity_[LED_ASSIGN_2] == 0);
    r.Tick(2900, true);
    CHECK(ui.mode_ == Ui::MODE_NORMAL);
    test_down[SWITCH_SEED] = false;
    r.Reboot(false);
    CHECK(ui.mode_ == Ui::MODE_STORAGE_WARNING);
    // The lock holds for the frozen buffer's erase too.
    background_buffer_backup.EraseDamagedSectors();
    CHECK(TestFlash().num_damaged == 3);
    TestFlashRepairAll();
  }
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
    r.Tick(30, true);
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
  r.Tick(30, true);
  const float speed = NextGrainSpeed(&r);
  const float expected = stmlib::SemitonesToRatio(7.0f);
  CHECK(fabsf(speed - expected) < 2.0f / 1024.0f);
  settings.SaveState();
  r.Reboot(false);
  CHECK(settings.pitch_scale() == PITCH_SCALE_FIFTHS);
  CHECK(ui.pitch_scale() == PITCH_SCALE_FIFTHS);
}

// Random intervals reaches the grains: with PITCH's attenurandomizer fully
// counter-clockwise, grains differ by fifths and octaves only.
void TestRandomIntervalsChain() {
  test_checker.Start("TestRandomIntervalsChain");
  int off_grid[2] = { 0, 0 };
  for (int intervals = 0; intervals < 2; ++intervals) {
    Rig r(false, true);
    PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 0;
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    settings.set_random_intervals(RandomIntervals(intervals));
    r.Settle();
    CHECK(ui.random_intervals() == bool(intervals));
    float previous = 12.0f * log2f(NextGrainSpeed(&r));
    for (int i = 0; i < 20; ++i) {
      const float pitch = 12.0f * log2f(NextGrainSpeed(&r));
      // Two offsets on fifths and octaves differ by 0, 5 or 7 semitones,
      // within the grain's 1/1024 speed step.
      float step = fmodf(fabsf(pitch - previous) + 0.05f, 12.0f) - 0.05f;
      off_grid[intervals] += fabsf(step) > 0.04f &&
          fabsf(step - 5.0f) > 0.04f && fabsf(step - 7.0f) > 0.04f;
      previous = pitch;
    }
  }
  printf("Random intervals chain: off the grid %d free, %d intervals\n",
      off_grid[0], off_grid[1]);
  CHECK(off_grid[0] > 10 && off_grid[1] == 0);
}

// Recent notes reaches the grains from the main loop: held PITCH CV notes
// become the only offsets counter-clockwise (#220).
void TestRecentNotesChain() {
  test_checker.Start("TestRecentNotesChain");
  const uint16_t held[3] = { 30365, 27000, 34000 };
  int off_notes[2] = { 0, 0 };
  for (int mode = 0; mode < 2; ++mode) {
    Rig r(false, true);
    r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
    PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 0;
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
    settings.set_random_intervals(
        mode ? RANDOM_INTERVALS_RECENT_NOTES : RANDOM_INTERVALS_OFF);
    r.cv[CV_ADC_CHANNEL_PITCH] = held[0];
    r.Settle();
    float notes[3];
    for (int n = 0; n < 3; ++n) {
      r.cv[CV_ADC_CHANNEL_PITCH] = held[n];
      r.Tick(30, true);
      notes[n] = cv_reader.cv(CV_ADC_CHANNEL_PITCH);
    }
    const float base = cv_reader.pot(POTS_ADC_CHANNEL_PITCH);
    int seen[3] = { 0, 0, 0 };
    for (int i = 0; i < 60; ++i) {
      const float offset = 12.0f * log2f(NextGrainSpeed(&r)) - base;
      bool on_note = false;
      for (int n = 0; n < 3; ++n) {
        if (fabsf(offset - notes[n]) < 0.03f) {
          on_note = true;
          ++seen[n];
        }
      }
      off_notes[mode] += !on_note;
    }
    if (mode) {
      CHECK(seen[0] && seen[1] && seen[2]);
    }
  }
  printf("Recent notes chain: off the notes %d free, %d recent\n",
      off_notes[0], off_notes[1]);
  CHECK(off_notes[0] > 30 && off_notes[1] == 0);
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
  r.Reboot(false);
  CHECK(!synth_mode && ui.freeze_button_state());
  CHECK(*processor.mutable_delay_write_ptr() == 1234);
  r.Settle();
  r.HoldMedia(true);
  CHECK(!synth_mode);  // FREEZE prevents starting and leaves backup intact.
  CHECK(settings.state().frozen);
  test_down[SWITCH_FREEZE] = true;
  r.Tick(10, true);
  test_down[SWITCH_FREEZE] = false;
  r.Tick(10, true);
  CHECK(!ui.freeze_button_state());
  CHECK(!settings.state().frozen);
  r.HoldMedia(true);
  CHECK(synth_mode);
  CHECK(!background_buffer_backup.previous_freeze_state_);
  r.Settle();
  const uint32_t writes = test_writes;
  test_freeze_input = true;
  r.Block();
  r.Advance(10100);
  CHECK(test_writes == writes);  // The main loop skips backup in self-play.
  test_freeze_input = false;
  r.Tick(2, true);
  r.HoldMedia(true);
  CHECK(!synth_mode);
  r.Settle();
  CHECK(!settings.state().frozen);
  CHECK(!background_buffer_backup.previous_freeze_state_);
  CHECK(header->tag == 0x5a5a5246);  // Old flash remains until next save.
}

// A frozen buffer whose audio can't be read: the module starts unfrozen,
// with the processor set from the settings, as without a buffer.
void TestFailedBufferLoad() {
  test_checker.Start("TestFailedBufferLoad");
  Rig r(false, false);
  BackgroundBufferBackup::BufferBackupHeader* header =
      reinterpret_cast<BackgroundBufferBackup::BufferBackupHeader*>(0x08100000);
  memset(header, 0, sizeof(*header));
  header->tag = 0x5a5a5246;
  header->write_ptr = 1234;
  header->media = Processor::MEDIA_DIRTY_TAPE;
  header->mono = true;
  settings.mutable_state()->media = Processor::MEDIA_CLEAN_TAPE;
  settings.mutable_state()->frozen = true;
  settings.SaveState();
  TestFlashDamage(0x08100000 + 32 * 1000);
  r.Reboot(false);
  CHECK(!ui.freeze_button_state());
  r.Settle();
  CHECK(!synth_mode);
  CHECK(processor.media() == Processor::MEDIA_CLEAN_TAPE);
  CHECK(!processor.mono());
  CHECK(settings.state().media == Processor::MEDIA_CLEAN_TAPE);
  TestFlashRepairAll();
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
    RunMainLoop();
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

void TestCustomScaleCallbackChain() {
  test_checker.Start("TestCustomScaleCallbackChain");
  uint8_t block[kProfileSize];
  for (size_t i = 0; i < kProfileSize; ++i) {
    block[i] = kProfileBlock[i];
  }
  const size_t records = 16 + 6 * block[5];
  const size_t bytes = 6 * block[5] + kScaleRecordSize;
  block[6] = bytes & 255;
  block[7] = bytes >> 8;
  block[12] = block[13] = 1;
  uint8_t* scale = block + records;
  memset(scale, 0, kScaleRecordSize);
  scale[0] = 15;
  scale[1] = 3;
  memcpy(scale + 12, "Custom callback", 15);
  const float values[] = { 13.0f, 0.0f, 3.25f, 8.125f };
  for (size_t i = 0; i < 4; ++i) {
    uint32_t bits;
    memcpy(&bits, &values[i], 4);
    const size_t offset = i ? 44 + 4 * (i - 1) : 8;
    for (size_t j = 0; j < 4; ++j) {
      scale[offset + j] = bits >> (8 * j);
    }
  }
  const uint32_t crc = Profile::Crc32(block, 16 + bytes);
  for (size_t i = 0; i < 4; ++i) {
    block[16 + bytes + i] = crc >> (8 * i);
  }
  Rig r(true, true);
  r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
  r.cv[CV_ADC_CHANNEL_PITCH] = 30365;  // About 4.4 semitones.
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
  PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 32768;
  for (int boot = 0; boot < 2; ++boot) {
    if (boot) {
      r.Reboot(false);
    }
    // The ROM default is immutable in this host binary. Exercise the same
    // Settings::Init overload with a decoded custom block, retaining flash.
    CHECK(settings.Init(false, block));
    if (!boot) {
      settings.set_pitch_scale(PitchScale(15));
      settings.SaveState();
    }
    CHECK(settings.pitch_scale() == 15);
    ui.Init(&settings, &gain_control, &calibrator, &background_buffer_backup);
    ui.LoadOptions();
    StartSelfPlay(&r, true);
    CHECK(ui.pitch_scale() == 15);
    CHECK(synth.wavetable_granular_player_.pitch_scale_ == 15);
    CHECK(fabsf(NextSynthGrainSemitones(&r) - 3.25f) < 0.01f);
  }
  settings.set_synth(SYNTH_PLAITS);
  r.Settle();
  r.Tick(50, true);
  CHECK(plaits_synth.pitch_scale_ == 15);
  bool plaits_note = false;
  for (int i = 0; i < kNumPlaitsVoices; ++i) {
    plaits_note |= fabsf(plaits_synth.patch_[i].note - kPlaitsRootNote - 3.25f)
        < 0.01f;
  }
  CHECK(plaits_note);
  r.left = r.right = true;
  PotsAdc::values_[POTS_ADC_CHANNEL_PITCH_AMOUNT] = 65535;
  r.Settle();
  r.Tick(30, true);
  CHECK(!synth_mode);
  CHECK(processor.processor_digital_.granular_player_.pitch_scale_ == 15);
  const float speed = NextGrainSpeed(&r);
  CHECK(fabsf(speed - stmlib::SemitonesToRatio(3.25f)) < 2.0f / 1024.0f);
  settings.set_spectral(true);
  r.Settle();
  r.Tick(30, true);
  CHECK(processor.spectral());
  CHECK(processor.spectral_processor_.pitch_scale_ == 15);
  CHECK(fabsf(processor.spectral_processor_.parameters_.pitch - 3.25f) < 0.01f);
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
        r.Reboot(false);  // The saved scale alone must bring it back.
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
  extern bool& clouds_style;
  extern float& reverb_style_gain;
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
    r.Reboot(false);
    CHECK(settings.clouds_style());
    r.Settle();
    CHECK(clouds_style);
  }
}

// Byte 21 selects the synth; leaving Plaits clears its borrowed delay RAM.
// The Rings stand-in stores its target in RAM.

void* HostInit(uint8_t* ram, size_t ram_size, const PlaitsBlobServices* s) {
  CHECK(ram[0] == 0 && ram[ram_size - 1] == 0);
  return ram;
}

bool HostConfigure(void* context, const RingsSynthTarget* target,
                   const RingsSynthControls* controls, bool entry) {
  ++configure_calls;
  configure_in_audio += in_audio;
  *static_cast<RingsSynthTarget*>(context) = *target;
  return true;
}

void HostRender(void* context, RingsSynthControls* controls, float* out,
                float* aux, size_t size) {
  ++render_calls;
  CHECK(bank_owner == SYNTH_RINGS);
  for (size_t i = 0; i < size; ++i) {
    out[i] = aux[i] = (render_calls & 1) ? 0.25f : -0.25f;
  }
}

inline const RingsSynthTarget& CoreTarget(const uint8_t* ram) {
  return *reinterpret_cast<const RingsSynthTarget*>(ram);
}

// An interrupt the moment the codec resumes.
void BlockOnResume() {
  RigBlock(current_rig);
}

// Rings through those entries: bank 0 changes owner only faded out with the
// codec paused; configure never runs in Process.
void TestRingsSynthOwnership() {
  test_checker.Start("TestRingsSynthOwnership");
  extern uint8_t delay_buffer_1[];
  Rig r(false, true);
  settings.set_synth(SYNTH_RINGS);
  settings.SaveState();
  r.Reboot(false);
  rings_synth.set_entries(&HostInit, &HostConfigure, &HostRender);
  playable_synths = SynthMask(true, true, true);
  ui.set_playable_synths(playable_synths);
  playing_synth = EffectiveSynth(ui.synth(), playable_synths);
  CHECK(playing_synth == SYNTH_RINGS);
  configure_calls = render_calls = configure_in_audio = 0;
  // A used bank: Rings must start from zero.
  memset(delay_buffer_1, 0xa5, 256 * 1024);
  r.Settle();
  r.Advance(10001);
  CHECK(synth_mode && bank_owner == SYNTH_RINGS);
  CHECK(configure_calls == 1);
  r.Settle();
  CHECK(render_calls > 0);
  codec.on_resume_ = &BlockOnResume;

  // FEEDBACK into Disastrous peace: configured by main, faded, once.
  PotsAdc::values_[POTS_ADC_CHANNEL_FEEDBACK] = 65535;
  r.Tick(300, true);
  CHECK(configure_calls == 2);
  CHECK(CoreTarget(delay_buffer_1).core == RINGS_CORE_STRING_SYNTH);
  CHECK(codec.callback_ == &Process);

  // Rings -> Plaits takes bank 0; Rings renders no more.
  settings.set_synth(SYNTH_PLAITS);
  ui.LoadOptions();
  // Never a synth on another's bank, even for one block.
  for (int i = 0; i < 400; ++i) {
    RunMainLoop();
    CHECK(playing_synth != SYNTH_PLAITS || bank_owner == SYNTH_PLAITS);
    r.Block();
  }
  CHECK(playing_synth == SYNTH_PLAITS && bank_owner == SYNTH_PLAITS);
  const uint32_t renders = render_calls;
  r.Tick(100, true);
  CHECK(render_calls == renders);
  // Plaits -> Beads keeps Plaits' bank; Beads -> Rings rebuilds Rings.
  settings.set_synth(SYNTH_BEADS);
  ui.LoadOptions();
  r.Tick(20, true);
  CHECK(playing_synth == SYNTH_BEADS && bank_owner == SYNTH_PLAITS);
  settings.set_synth(SYNTH_RINGS);
  ui.LoadOptions();
  for (int i = 0; i < 400; ++i) {
    RunMainLoop();
    CHECK(playing_synth != SYNTH_RINGS || bank_owner == SYNTH_RINGS);
    r.Block();
  }
  CHECK(playing_synth == SYNTH_RINGS && bank_owner == SYNTH_RINGS);
  CHECK(configure_calls == 3);
  CHECK(render_calls > renders);
  CHECK(codec.callback_ == &Process);

  // Polyphony from feature 42, applied off the interrupt.
  settings.SetFeatureValue(FEATURE_RINGS_POLYPHONY, 2);
  r.Tick(300, true);
  CHECK(CoreTarget(delay_buffer_1).polyphony == 4);
  CHECK(configure_in_audio == 0);

  // FREEZE sustains Rings; polyphony and FEEDBACK changes still apply.
  ui.set_freeze_button_state(true);
  r.Tick(20, true);
  CHECK(ui.should_ignore_state_change());
  settings.SetFeatureValue(FEATURE_RINGS_POLYPHONY, 0);
  for (int i = 0; i < 100 && !rings_synth.configure_pending(); ++i) {
    r.Tick(1, true);
  }
  CHECK(rings_synth.configure_pending());
  // A queued self-play request is dropped under FREEZE, as before.
  ui.gestures_.self_play_requested_ = true;
  RunMainLoop();
  CHECK(!ui.self_play_requested());
  r.Tick(300, true);
  CHECK(CoreTarget(delay_buffer_1).polyphony == 1);
  // The unpatched ASSIGN input's probe would move FEEDBACK between zones.
  r.cv_patched[CV_ADC_CHANNEL_ASSIGN] = true;
  PotsAdc::values_[POTS_ADC_CHANNEL_FEEDBACK] = 0;
  r.Tick(1000, true);
  CHECK(CoreTarget(delay_buffer_1).core == RINGS_CORE_PART);
  CHECK(ui.should_ignore_state_change());
  CHECK(configure_in_audio == 0);
  CHECK(codec.callback_ == &Process);
  ui.set_freeze_button_state(false);
  r.Tick(20, true);

  // Without Rings, a Rings selection plays Beads' synth, never silence.
  playable_synths = SynthMask(true, true, false);
  ui.set_playable_synths(playable_synths);
  r.Tick(20, true);
  CHECK(playing_synth == SYNTH_BEADS);
  CHECK(settings.synth() == SYNTH_RINGS);
  rings_synth.set_entries(NULL, NULL, NULL);
  codec.on_resume_ = NULL;
}

// [A]+FREEZE follows the synth that plays, fallbacks included (#353): the
// Plaits bank, Rings' polyphony, else the profile's action (Buffer trim).
void TestFreezeGestureFollowsPlayingSynth() {
  test_checker.Start("TestFreezeGestureFollowsPlayingSynth");
  const struct {
    uint8_t selected;
    uint8_t playable;
    SynthEngine playing;
  } cases[] = {
    { SYNTH_PLAITS, SynthMask(true, false, true), SYNTH_BEADS },
    { SYNTH_PLAITS, SynthMask(false, false, true), SYNTH_SILENT },
    { SYNTH_PLAITS, SynthMask(false, false, false), SYNTH_SILENT },
    { SYNTH_RINGS, SynthMask(true, true, false), SYNTH_BEADS },
    { SYNTH_RINGS, SynthMask(false, true, false), SYNTH_PLAITS },
    { SYNTH_RINGS, SynthMask(false, false, false), SYNTH_SILENT },
    { SYNTH_BEADS, SynthMask(false, true, true), SYNTH_PLAITS },
    { SYNTH_PLAITS, SynthMask(true, true, true), SYNTH_PLAITS },
    { SYNTH_RINGS, SynthMask(true, true, true), SYNTH_RINGS },
    { SYNTH_BEADS, SynthMask(true, true, true), SYNTH_BEADS },
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    Rig r(false, true);
    settings.set_synth(cases[i].selected);
    settings.SaveState();
    r.Reboot(false);
    rings_synth.set_entries(&HostInit, &HostConfigure, &HostRender);
    playable_synths = cases[i].playable;
    ui.set_playable_synths(playable_synths);
    r.StartAutomatic();
    CHECK(synth_mode && playing_synth == cases[i].playing);
    test_down[SWITCH_FREEZE] = true;
    r.Tick(10, true);
    test_down[SWITCH_FREEZE] = false;
    r.Tick(10, true);
    CHECK(ui.freeze_button_state());
    test_down[SWITCH_MEDIA] = true;
    r.Tick(10, true);
    test_down[SWITCH_FREEZE] = true;
    r.Tick(10, true);
    test_down[SWITCH_FREEZE] = test_down[SWITCH_MEDIA] = false;
    r.Tick(50, true);
    const bool plaits = cases[i].playing == SYNTH_PLAITS;
    const bool rings = cases[i].playing == SYNTH_RINGS;
    CHECK(settings.FeatureValue(FEATURE_PLAITS_BANK) == (plaits ? 1 : 0));
    CHECK(settings.FeatureValue(FEATURE_RINGS_POLYPHONY) == (rings ? 1 : 0));
    // Buffer trim releases FREEZE; the steps keep it.
    CHECK(ui.freeze_button_state() == (plaits || rings));
    CHECK(settings.synth() == cases[i].selected);
    // The UI learns of every change before its next event, swaps included.
    // A change waits for FREEZE's release.
    if (ui.freeze_button_state()) {
      test_down[SWITCH_FREEZE] = true;
      r.Tick(10, true);
      test_down[SWITCH_FREEZE] = false;
      r.Tick(10, true);
    }
    settings.set_synth((cases[i].selected + 1) % SYNTH_SILENT);
    ui.LoadOptions();
    for (int j = 0; j < 400; ++j) {
      RunMainLoop();
      CHECK(ui.playing_synth_ == playing_synth);
      r.Block();
    }
    CHECK(playing_synth == EffectiveSynth(settings.synth(), playable_synths));
    rings_synth.set_entries(NULL, NULL, NULL);
  }
}

void TestPlaitsSynthSelection() {
  test_checker.Start("TestPlaitsSynthSelection");
  extern uint8_t delay_buffer_1[];
  extern uint8_t delay_buffer_2[];
  for (int plaits = 0; plaits < 2; ++plaits) {
    Rig r(false, true);
    settings.set_synth(plaits);
    settings.SaveState();
    r.Reboot(false);
    CHECK(ui.synth() == bool(plaits));
    r.Settle();
    CHECK(!synth_mode);
    r.Advance(10001);
    CHECK(synth_mode);
    r.Settle();
    // In synth mode nothing but the voices touches the delay memory.
    static uint8_t bank_1[512 * 1024];
    static uint8_t bank_2[256 * 1024];
    memcpy(bank_1, delay_buffer_1, sizeof(bank_1));
    memcpy(bank_2, delay_buffer_2, sizeof(bank_2));
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
    CHECK(!memcmp(bank_1 + kPlaitsSynthRamSize,
        delay_buffer_1 + kPlaitsSynthRamSize,
        sizeof(bank_1) - kPlaitsSynthRamSize));
    CHECK(!memcmp(bank_2, delay_buffer_2, sizeof(bank_2)));
    r.left = true;
    for (int i = 0; i < 2000 && synth_mode; ++i) {
      RunMainLoop();
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
      RunMainLoop();
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

// Engine fades clear recording; granular restores saved freezes.
// Spectral must never start wavetable synthesis.
void TestSpectralEngine() {
  test_checker.Start("TestSpectralEngine");
  extern float& reverb_style_gain;
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
  const uint32_t writes = test_writes;
  test_freeze_input = true;
  RunSquare(&r, 4);
  CHECK(spectral.parameters_.freeze);
  r.Advance(11000);
  CHECK(test_writes == writes);
  test_freeze_input = false;
  RunSquare(&r, 4);

  // Unpatched inputs do not start the synth, automatically or on a hold.
  // All dry, so the output is silent and the synth's timer runs.
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 0;
  r.left = r.right = false;
  r.Settle();
  r.Advance(11000);
  RunMainLoop();
  CHECK(!synth_mode);
  settings.set_self_play_on_request(true);
  r.HoldMedia(true);
  CHECK(!synth_mode && !ui.self_play_requested());
  r.left = r.right = true;
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 65535;

  // Back to granular: the saved frozen buffer returns, with its own quality
  // even if another was picked meanwhile.
  settings.mutable_state()->media = Processor::MEDIA_CLEAN_TAPE;
  RunSquare(&r, 3000);
  CHECK(processor.spectral() &&
        processor.media() == Processor::MEDIA_CLEAN_TAPE);
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
  r.Reboot(false);
  CHECK(processor.spectral() && !ui.freeze_button_state());
  CHECK(settings.state().frozen && header->tag == 0x5a5a5246);
  RunSquare(&r, 4000);
  CHECK(processor.spectral() && !synth_mode);
}

// Codec restarts fade out first (#53): the output steps down to silence
// over about 10 ms instead of stopping at full level.
void TestCodecRestartFade() {
  test_checker.Start("TestCodecRestartFade");
  Rig r(false, false);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 0;
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
  while (last && peaks[last - 1] == 0) {
    --last;
  }
  CHECK(last > 40);
  printf("Restart fade: steady %d, last %d\n", steady, peaks[last - 1]);
  CHECK(peaks[last - 1] < steady / 8);

  // FREEZE during the fade postpones the restart without leaving silence.
  extern volatile bool& codec_fade;
  extern float& reverb_style_gain;
  r.Settle();
  settings.mutable_state()->media = (settings.state().media + 1) % 4;
  MainLoopStep();
  CHECK(codec_fade);
  test_freeze_input = true;
  for (int i = 0; i < 400; ++i) {
    r.Block();
    MainLoopStep();
  }
  CHECK(!codec_fade && reverb_style_gain == 1.0f);
  test_freeze_input = false;
  r.Settle();
}

// A change at the same sample rate keeps the converter clocked (#53): only
// rate changes use the reset-held restart sequence.
void TestCodecRestartRetry() {
  test_checker.Start("TestCodecRestartRetry");
  Rig r(false, false);
  r.Settle();
  const int restarts = codec.restarts_;
  codec.fail_restart_ = true;
  settings.mutable_state()->media = Processor::MEDIA_VINTAGE_DIGITAL;
  for (int i = 0; i < 200 && codec.running_; ++i) {
    r.Tick(1, true);
  }
  CHECK(!codec.running_ && codec.callback_ == NULL);
  codec.fail_restart_ = false;
  r.Tick(50, true);
  CHECK(!codec.running_);
  r.Tick(350, true);
  CHECK(codec.running_ && codec.callback_ == &Process);
  CHECK(codec.restarts_ == restarts + 1);
  CHECK(codec.previous_sample_rate_ == 0);
  CHECK(codec.sample_rate_ == int(processor.sample_rate()));
  CHECK(codec_transition_frames == 0);
}

void TestCodecKeepsClocks() {
  test_checker.Start("TestCodecKeepsClocks");
  Rig r(false, false);
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
    const int previous_restarts = codec.restarts_;
    settings.mutable_state()->media = media[i];
    for (int j = 0; j < 200 && processor.media() != media[i]; ++j) {
      RunMainLoop();
      r.Block();
    }
    CHECK(processor.media() == media[i]);
    CHECK(codec.callback_ == &Process);
    const int rate = int(processor.sample_rate());
    const int restarts = rate == previous_rate ? 0 : 1;
    CHECK(codec.sample_rate_ == rate);
    CHECK(codec.starts_ == starts);
    CHECK(codec.stops_ == stops);
    CHECK(codec.restarts_ == previous_restarts + restarts);
    if (restarts) {
      CHECK(codec.previous_sample_rate_ == previous_rate);
    }
    CHECK(codec.block_size_ == size_t(rate / 4000));
    CHECK(codec.running_);
    CHECK(codec.idle_stops_ == 0);
    r.Settle();
  }
}

uint8_t factory_profile[kProfileSize];
bool factory_profile_read;

// The factory profile plus Fixed entries, as feature/value pairs.
void BuildProfile(uint8_t* block, const uint8_t* pairs, size_t count) {
  if (!factory_profile_read) {
    for (size_t i = 0; i < kProfileSize; ++i) {
      factory_profile[i] = kProfileBlock[i];
    }
    factory_profile_read = true;
  }
  memcpy(block, factory_profile, kProfileSize);
  for (size_t i = 0; i < count; ++i) {
    const uint8_t entry[] = {
      pairs[2 * i], PROFILE_FIXED, pairs[2 * i + 1], 0, 0, 0
    };
    // A factory entry for the same feature is replaced.
    uint8_t* target = block + 16 + 6 * block[5];
    for (int j = 0; j < block[5]; ++j) {
      if (block[16 + 6 * j] == pairs[2 * i]) {
        target = block + 16 + 6 * j;
      }
    }
    if (target == block + 16 + 6 * block[5]) {
      ++block[5];
    }
    memcpy(target, entry, sizeof(entry));
  }
  const size_t bytes = 6 * block[5];
  block[6] = bytes & 255;
  block[7] = bytes >> 8;
  const uint32_t crc = Profile::Crc32(block, 16 + bytes);
  for (size_t i = 0; i < 4; ++i) {
    block[16 + bytes + i] = crc >> (8 * i);
  }
}

// Writes a profile into the firmware's own block, as a profile update does.
void FlashProfile(const uint8_t* block) {
  const uintptr_t start = reinterpret_cast<uintptr_t>(kProfileBlock);
  const uintptr_t page = start & ~uintptr_t(4095);
  CHECK(!mprotect(reinterpret_cast<void*>(page), start + kProfileSize - page,
      PROT_READ | PROT_WRITE));
  volatile uint8_t* target = const_cast<volatile uint8_t*>(kProfileBlock);
  for (size_t i = 0; i < kProfileSize; ++i) {
    target[i] = block[i];
  }
}

// Powers on through Init() with Fixed entries, then restores the factory
// profile block for later tests.
void BootWithProfile(Rig* r, const uint8_t* pairs, size_t count) {
  uint8_t block[kProfileSize];
  BuildProfile(block, pairs, count);
  FlashProfile(block);
  r->Reboot(false);
  FlashProfile(factory_profile);
}

// Loads the factory profile plus Fixed entries, as feature/value pairs.
void FixedFeaturesProfile(const uint8_t* pairs, size_t count) {
  uint8_t block[kProfileSize];
  BuildProfile(block, pairs, count);
  CHECK(settings.Init(false, block));
  CHECK(!settings.profile_locked());
  ui.Init(&settings, &gain_control, &calibrator, &background_buffer_backup);
  ui.LoadOptions();
  for (size_t i = 0; i < count; ++i) {
    CHECK(settings.FeatureValue(ProfileFeature(pairs[2 * i])) ==
        pairs[2 * i + 1]);
  }
}

// Loads the factory profile plus one Fixed entry with value 1.
void FixedFeatureProfile(ProfileFeature feature) {
  const uint8_t pair[] = { uint8_t(feature), 1 };
  FixedFeaturesProfile(pair, 1);
}

// The level changes only the synth's final audio, leaving gates and input gain.
std::vector<int32_t> SynthLevelOutput(int level, bool gate) {
  Rig r(false, false);
  const uint8_t pair[] = { FEATURE_SYNTH_LEVEL, uint8_t(level) };
  BootWithProfile(&r, pair, 1);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 0;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
  r.Settle();
  settings.mutable_state()->gate_output_mode = gate;
  const int input_gain = settings.state().gain;
  Random::Seed(0x21);
  ConfigureAndStartCodec(true, false);
  CHECK((codec.callback_ == &Process) == (level == 0));
  std::vector<int32_t> samples;
  for (int block = 0; block < 1200; ++block) {
    r.Block();
    if (block >= 1100) {
      for (size_t i = 0; i < codec.block_size_; ++i) {
        samples.push_back(r.out[i].l);
        samples.push_back(r.out[i].r);
      }
    }
  }
  CHECK(settings.state().gain == input_gain);
  SwapPaused(bank_owner, playing_synth);
  CHECK((codec.callback_ == &Process) == (level == 0));
  ConfigureAndStartCodec(false, false);
  CHECK(codec.callback_ == &Process);
  return samples;
}

void TestSynthLevel() {
  test_checker.Start("TestSynthLevel");
  for (int gate = 0; gate < 2; ++gate) {
    const std::vector<int32_t> stock = SynthLevelOutput(0, gate);
    bool audible = false;
    bool has_gate = false;
    for (size_t i = 0; i < stock.size(); i += 2) {
      audible |= stock[i] != 0;
      has_gate |= stock[i + 1] != 0;
    }
    CHECK(audible);
    CHECK(!gate || has_gate);
    for (int level = 1; level < 4; ++level) {
      const std::vector<int32_t> trimmed = SynthLevelOutput(level, gate);
      CHECK(trimmed.size() == stock.size());
      const int divisor = 1 << level;
      bool matches = trimmed.size() == stock.size();
      for (size_t i = 0; i < stock.size() && matches; i += 2) {
        matches = trimmed[i] == stock[i] / divisor &&
            trimmed[i + 1] == stock[i + 1] / (gate ? 1 : divisor);
      }
      CHECK(matches);
    }
  }
}

// Tail handling is a profile choice; the codec transition remains the same.
void SynthTailProfile() {
  FixedFeatureProfile(FEATURE_SYNTH_TAIL);
}

void TestSynthStopClearsReverb() {
  test_checker.Start("TestSynthStopClearsReverb");
  extern uint16_t reverb_buffer[];
  for (int clear = 0; clear < 2; ++clear) {
    for (int plaits = 0; plaits < 2; ++plaits) {
      for (int path = 0; path < 3; ++path) {
        Rig r(path == 1, true);
        if (clear) {
          SynthTailProfile();
          if (!plaits && !path) {
            settings.SaveState();
            CHECK(settings.FeatureValue(FEATURE_SYNTH_TAIL) == 1);
            SynthTailProfile();  // Same profile after a saved-state reload.
          }
        } else {
          CHECK(settings.profile().entry(FEATURE_SYNTH_TAIL).state ==
              PROFILE_OFF);
          CHECK(settings.FeatureValue(FEATURE_SYNTH_TAIL) == 0);
        }
        settings.set_synth(plaits);
        ui.LoadOptions();
        PotsAdc::values_[POTS_ADC_CHANNEL_REVERB] = 60000;
        StartSelfPlay(&r, path == 1);
        CHECK(synth_mode);
        r.Tick(500, true);
        bool used = false;
        for (int i = 0; i < 32768 && !used; ++i) {
          used = reverb_buffer[i] != 0;
        }
        CHECK(used);
        const int starts = codec.starts_;
        const int stops = codec.stops_;
        if (path == 0) {
          r.left = true;
        } else if (path == 1) {
          r.HoldMedia(false);
        } else {
          settings.set_spectral(true);
        }
        for (int i = 0; i < 2000 && synth_mode; ++i) {
          r.Tick(1, true);
        }
        CHECK(!synth_mode && codec_transition_frames > 0);
        CHECK(processor.spectral() == (path == 2));
        CHECK(codec.starts_ == starts && codec.stops_ == stops);
        CHECK(codec.idle_stops_ == 0);
        bool empty = true;
        for (int i = 0; i < 32768 && empty; ++i) {
          empty = reverb_buffer[i] == 0;
        }
        CHECK(empty == bool(clear));
      }
    }
  }
}

void TestPitchNotchLaw() {
  test_checker.Start("TestPitchNotchLaw");
  Rig r(false, false);
  r.Settle();
  settings.SetFeatureValue(FEATURE_PITCH_NOTCH, 1);
  // Boundaries in raw ADC units, both directions, plus both knob ends.
  const uint16_t raw[] = {
    32768, 34580, 34620, 34730, 34580,
    59900, 60130, 61890, 61950, 62060, 61890,
    65535, 65250, 65420, 0, 235, 400
  };
  const bool expected[] = {
    true, true, true, false, true,
    false, true, true, true, false, true,
    true, false, true, true, true, false
  };
  for (int sign = 0; sign < 2; ++sign) {
    cv_reader.pitch_on_notch_ = false;
    for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); ++i) {
      PotsAdc::values_[POTS_ADC_CHANNEL_PITCH] = sign ? 65535 - raw[i] : raw[i];
      MonitorStateChange();
      ui.UpdateLeds();
      CHECK(ui.pitch_on_notch_ == expected[i]);
      CHECK((ui.leds_.intensity_[LED_MEDIA_R] == 0 &&
          ui.leds_.intensity_[LED_MEDIA_G] == 0 &&
          ui.leds_.intensity_[LED_MEDIA_B] == 255) == expected[i]);
    }
  }
}

// Off identity is checked against the release executable's existing audio
// hashes. Comparing two profiles through ApplyProfile is not independent.
void TestSeedLatchPatched() {
  test_checker.Start("TestSeedLatchPatched");
  Rig r(false, false);
  r.seed_patched = true;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 0;
  r.Settle();
  ui.seed_button_locked_ = true;
  for (int mode = 0; mode < 4; ++mode) {
    settings.SetFeatureValue(FEATURE_SEED_LATCH, mode);
    SysTick_Handler();  // The UI publishes the page's or the saved value.
    MonitorStateChange();
    CHECK(seed_latch_mode == mode);
    // Feed actual callback edges, checking the generator selected by the
    // foreground cache and ensuring Ignore cannot light the trigger LED.
    const int ignored_before =
        processor.trigger_generator_.elapsed_since_last_pulse_;
    test_seed_input = true;
    for (int i = 0; i < 30; ++i) {
      r.Block();
    }
    if (mode == 1) {
      CHECK(!ui.seed_led_state_);
      CHECK(processor.trigger_generator_.elapsed_since_last_pulse_ ==
          ignored_before);
    }
    test_seed_input = false;
    for (int i = 0; i < 30; ++i) {
      r.Block();
    }
    if (mode) {
      CHECK(NextGrainSpeed(&r) > 0.0f);
    }
  }
  // Inspect rendered triggers, not an incidental divider counter.
  for (int mode = 0; mode < 4; ++mode) {
    TriggerGenerator generator;
    generator.Init(48000.0f);
    TriggerGenerator::Output out[256];
    GateFlags edges[256];
    std::fill(edges, edges + 256, GATE_FLAG_LOW);
    edges[64] = GateFlags(GATE_FLAG_HIGH | GATE_FLAG_RISING);
    const SeedButtonState state = mode
        ? SeedButtonState(SEED_BUTTON_STATE_IGNORE + mode - 1)
        : SEED_BUTTON_STATE_LOCKED;
    generator.Render(state, edges, 0.0f, 0.0f, 0.0f, out, 256);
    int triggers = 0;
    for (int i = 0; i < 256; ++i) {
      triggers += out[i].trigger;
    }
    CHECK(triggers > 0);
    if (mode == 1) {
      CHECK(!out[64].trigger);
    }
    if (mode == 2) {
      CHECK(out[64].phase < 0.01f && out[64].trigger);
    }
    if (mode == 3) {
      CHECK(out[64].trigger && out[64].phase > 0.1f);
    }
    // At noon Sync and Add insert a grain; Ignore keeps the clock still.
    generator.Init(48000.0f);
    generator.Render(state, edges, 0.5f, 0.0f, 0.0f, out, 256);
    if (mode) {
      CHECK(out[64].trigger == (mode != 1));
    }
  }
  // With no cable, attacks never reach the SEED path.
  r.seed_patched = false;
  r.Settle();
  settings.SetFeatureValue(FEATURE_SEED_LATCH, 3);
  SysTick_Handler();
  MonitorStateChange();
  CHECK(normalization_detector.normalized(NORMALIZATION_DETECTOR_CHANNEL_SEED));
  settings.set_attack_mode(ATTACK_MODE_SNAP_AND_TRIGGER);
  ui.LoadOptions();
  const int before = processor.trigger_generator_.elapsed_since_last_pulse_;
  r.Block();
  CHECK(processor.trigger_generator_.elapsed_since_last_pulse_ == before);
}

// The delay latch fade is a profile choice; Off or omitted keeps stock.
void TestSeedLatchFadeProfile() {
  test_checker.Start("TestSeedLatchFadeProfile");
  Rig r(false, false);
  r.Settle();
  CHECK(settings.profile().entry(FEATURE_SEED_LATCH_FADE).state ==
      PROFILE_OFF);
  const DelayController& delay = processor.delay_controller_;
  processor.set_seed_latch_fade(true);
  MonitorStateChange();
  CHECK(!delay.test_timing_fade());
  FixedFeatureProfile(FEATURE_SEED_LATCH_FADE);
  r.Settle();
  CHECK(delay.test_timing_fade());
  // A quality change turns it off until the next main loop pass.
  processor.Configure(Processor::MEDIA_CLEAN_TAPE, false);
  CHECK(!delay.test_timing_fade());
  r.Settle();
  CHECK(delay.test_timing_fade());
  // The factory profile omits the entry, so a reboot returns to stock.
  r.Reboot(false);
  r.Settle();
  processor.set_seed_latch_fade(true);
  MonitorStateChange();
  CHECK(!delay.test_timing_fade());
}

void RunBlocks(Rig* r, int blocks) {
  while (blocks--) {
    r->Block();
  }
}

// Manual gain at power-on is a profile choice (#19); a saved gain wins.
void TestManualGainProfile() {
  test_checker.Start("TestManualGainProfile");
  Rig r(false, false);
  r.Settle();
  CHECK(settings.state().gain == 0);
  CHECK(gain_control.auto_gain_ && gain_control.gain_ > 1.5f);
  // A blank gain powers on at unity: 1 on the stock law.
  const uint8_t manual[] = { FEATURE_MANUAL_GAIN, 1 };
  BootWithProfile(&r, manual, 1);
  CHECK(settings.state().gain == 1);
  r.Settle();
  CHECK(!gain_control.auto_gain_);
  CHECK(fabsf(gain_control.gain_ - 1.0f) < 0.001f);
  // Holding [A] restarts automatic gain; the next power-on is manual again.
  r.HoldMedia(true);
  CHECK(settings.state().gain == 0);
  RunBlocks(&r, 4);
  CHECK(gain_control.auto_gain_);
  BootWithProfile(&r, manual, 1);
  CHECK(settings.state().gain == 1);
  // Saved gains win, including a saved unity under the factory profile.
  settings.mutable_state()->gain = 200;
  settings.SaveState();
  BootWithProfile(&r, manual, 1);
  CHECK(settings.state().gain == 200);
  settings.mutable_state()->gain = 1;
  settings.SaveState();
  r.Reboot(false);
  CHECK(settings.state().gain == 1);
  // With input attenuation, unity is the middle of its range.
  settings.mutable_state()->gain = 0;
  settings.SaveState();
  const uint8_t pairs[] = { FEATURE_MANUAL_GAIN, 1, FEATURE_ATTENUATION, 1 };
  BootWithProfile(&r, pairs, 2);
  CHECK(settings.state().gain == 128);
  r.Settle();
  CHECK(fabsf(gain_control.gain_ - 1.0f) < 0.001f);
  // The factory profile omits the entry: a blank gain is automatic.
  settings.mutable_state()->gain = 0;
  settings.SaveState();
  r.Reboot(false);
  CHECK(settings.state().gain == 0);
}

// Attack sensitivity is a profile choice (#58); Off is the Beta 2 threshold.
void TestAttackSensitivityProfile() {
  test_checker.Start("TestAttackSensitivityProfile");
  Rig r(false, false);
  r.Settle();
  settings.set_attack_mode(ATTACK_MODE_SNAP);
  settings.SaveState();
  r.Reboot(false);
  CHECK(ui.attack_threshold() == kAttackThreshold);
  r.Settle();
  CHECK(processor.attack_input_.mode() == ATTACK_MODE_SNAP);
  CHECK(processor.attack_input_.threshold_ == kAttackThreshold);
  const float thresholds[] = { 1.5f, 2.0f, 1.3f };
  for (uint8_t value = 0; value < 3; ++value) {
    const uint8_t pair[] = { FEATURE_ATTACK_SENSITIVITY, value };
    BootWithProfile(&r, pair, 1);
    CHECK(settings.attack_mode() == ATTACK_MODE_SNAP);
    r.Settle();
    CHECK(processor.attack_input_.threshold_ == thresholds[value]);
  }
}

int CountGrainBlocks(Rig* r, int blocks) {
  DelayGranularPlayer& player = processor.processor_digital_.granular_player_;
  int count = 0;
  while (blocks--) {
    r->Block();
    count += player.grain_interval_ <= int(codec.block_size_);
  }
  return count;
}

// A hit of four loud blocks, then 300 ms back at the quiet level: the
// blocks where grains start, and whether the hit lit the SEED LED.
int Hit(Rig* r, int* first, bool* led) {
  r->audio = 0x10000000;
  *first = CountGrainBlocks(r, 1);
  *led = ui.seed_led_state_;
  int count = *first + CountGrainBlocks(r, 3);
  r->audio = 0x1000000;
  return count + CountGrainBlocks(r, 1200);
}

// SEED-unpatched snap/trigger hits add grains only in granular mode (#290,
// #294), frozen or not; they never clock synths or delay.
void TestAttackTriggers() {
  test_checker.Start("TestAttackTriggers");
  Rig r(false, false);
  settings.set_attack_mode(ATTACK_MODE_SNAP_AND_TRIGGER);
  settings.SaveState();
  // The delay SEED latch fade makes the delay report which timing it uses.
  const uint8_t fade[] = { FEATURE_SEED_LATCH_FADE, 1 };
  BootWithProfile(&r, fade, 1);
  r.audio = 0x1000000;
  r.Settle();
  CHECK(ui.seed_button_locked());
  CHECK(processor.attack_input_.mode() == ATTACK_MODE_SNAP_AND_TRIGGER);
  const AttackDetector& detector = processor.processor_digital_.attacks_;
  for (int frozen = 0; frozen < 2; ++frozen) {
    ui.set_freeze_button_state(frozen);
    const uint32_t marks = detector.count_;
    // DENSITY at noon plays nothing but the hits.
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
    r.Tick(300, true);
    CHECK(CountGrainBlocks(&r, 800) == 0);
    for (int hit = 0; hit < 3; ++hit) {
      int first;
      bool led;
      CHECK(Hit(&r, &first, &led) == 1);
      CHECK(first == 1 && led);
    }
    // On the periodic side the clock runs on, and each hit adds a grain.
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 8000;
    r.Tick(300, true);
    CHECK(CountGrainBlocks(&r, 1200) > 3);
    for (int hit = 0; hit < 3; ++hit) {
      int first;
      bool led;
      CHECK(Hit(&r, &first, &led) > 3);
      CHECK(first == 1);
    }
    // Frozen hits start grains but leave no marks.
    CHECK(frozen ? detector.count_ == marks : detector.count_ > marks);
  }
  // The options path adds them too.
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
  processor.set_gentle_mix(true);
  r.Tick(300, true);
  for (int hit = 0; hit < 3; ++hit) {
    int first;
    bool led;
    CHECK(Hit(&r, &first, &led) == 1);
    CHECK(first == 1);
  }
  processor.set_gentle_mix(false);
  // A hit while frozen starts one grain; releasing FREEZE on the louder
  // input starts none, and freezing again none either.
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
  r.Tick(300, true);
  CHECK(CountGrainBlocks(&r, 1200) == 0);
  r.audio = 0x10000000;
  CHECK(CountGrainBlocks(&r, 1200) == 1);
  ui.set_freeze_button_state(false);
  CHECK(CountGrainBlocks(&r, 1200) == 0);
  ui.set_freeze_button_state(true);
  CHECK(CountGrainBlocks(&r, 1200) == 0);
  ui.set_freeze_button_state(false);
  r.audio = 0x1000000;
  r.Tick(300, true);
  // Delay mode keeps its knob timing, and plays no hits.
  PotsAdc::values_[POTS_ADC_CHANNEL_SIZE] = 65535;
  r.Tick(2000, true);
  CHECK(processor.delay_active());
  for (int hit = 0; hit < 3; ++hit) {
    int first;
    bool led;
    Hit(&r, &first, &led);
    CHECK(!led);
    CHECK(!processor.delay_controller_.test_synchronized());
  }
  PotsAdc::values_[POTS_ADC_CHANNEL_SIZE] = 32768;
  // The wavetable synth plays its grains, and Plaits drones at noon.
  for (int plaits = 0; plaits < 2; ++plaits) {
    Rig s(false, true);
    settings.set_attack_mode(ATTACK_MODE_SNAP_AND_TRIGGER);
    settings.set_synth(plaits);
    settings.SaveState();
    s.Reboot(false);
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = plaits ? 32768 : 16000;
    s.StartAutomatic();
    CHECK(synth_mode);
    if (plaits) {
      RunBlocks(&s, 100);
      CHECK(plaits_synth.drone_);
      // Hits on an input patched while FREEZE keeps the synth playing.
      ui.set_freeze_button_state(true);
      s.left = s.right = true;
      s.audio = 0x1000000;
      s.Tick(300, true);
      for (int hit = 0; hit < 3; ++hit) {
        s.audio = 0x10000000;
        RunBlocks(&s, 4);
        s.audio = 0x1000000;
        RunBlocks(&s, 1200);
        CHECK(synth_mode && plaits_synth.drone_);
      }
      ui.set_freeze_button_state(false);
    } else {
      WavetableGranularPlayer& player = synth.wavetable_granular_player_;
      int grains = 0;
      for (int i = 0; i < 4000; ++i) {
        s.Block();
        grains += player.grain_interval_ <= int(codec.block_size_);
      }
      CHECK(grains > 3);
    }
  }
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
}

// With SEED patched, a hit adds a grain whatever the latch does with the
// cable, and never reaches the cable's divider or Ignore.
void TestAttackTriggersPatched() {
  test_checker.Start("TestAttackTriggersPatched");
  Rig r(false, false);
  settings.set_attack_mode(ATTACK_MODE_SNAP_AND_TRIGGER);
  settings.SaveState();
  r.Reboot(false);
  r.seed_patched = true;
  r.audio = 0x1000000;
  r.Settle();
  ui.seed_button_locked_ = true;
  for (int mode = 0; mode < 4; ++mode) {
    settings.SetFeatureValue(FEATURE_SEED_LATCH, mode);
    r.Tick(300, true);
    CHECK(seed_latch_mode == mode);
    const int pulses = processor.trigger_generator_.pulse_count_;
    for (int hit = 0; hit < 3; ++hit) {
      int first;
      bool led;
      CHECK(Hit(&r, &first, &led) == 1);
      CHECK(first == 1 && led);
    }
    CHECK(processor.trigger_generator_.pulse_count_ == pulses);
  }
  settings.SetFeatureValue(FEATURE_SEED_LATCH, 0);
}

// SEED can start unlatched (#41): grains wait for SEED, the latch modes
// apply once latched, and a delay latch fade does not fade at power-on.
void TestSeedStartProfile() {
  test_checker.Start("TestSeedStartProfile");
  Rig r(false, false);
  r.Settle();
  CHECK(ui.seed_button_locked());
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 65535;
  r.Tick(50, true);
  CHECK(CountGrainBlocks(&r, 2000) > 10);
  const uint8_t unlatched[] = { FEATURE_SEED_START, 1 };
  BootWithProfile(&r, unlatched, 1);
  CHECK(!ui.seed_button_locked());
  r.Settle();
  CHECK(CountGrainBlocks(&r, 2000) == 0);
  // A SEED gate still starts grains; Ignore applies only once latched.
  settings.SetFeatureValue(FEATURE_SEED_LATCH, 1);
  r.seed_patched = true;
  r.Tick(600, true);
  test_seed_input = true;
  CHECK(CountGrainBlocks(&r, 200) > 0);
  CHECK(ui.seed_led_state_);
  test_seed_input = false;
  ui.seed_button_locked_ = true;
  MonitorStateChange();
  r.Tick(50, true);
  test_seed_input = true;
  RunBlocks(&r, 30);
  CHECK(!ui.seed_led_state_);
  test_seed_input = false;
  // With the delay fade, delay mode starts in tapped timing at full level,
  // as after the processor's power-on configuration.
  const uint8_t pairs[] = {
    FEATURE_SEED_START, 1, FEATURE_SEED_LATCH_FADE, 1
  };
  r.seed_patched = false;
  FixedFeaturesProfile(pairs, 2);
  CHECK(!ui.seed_button_locked());
  PotsAdc::values_[POTS_ADC_CHANNEL_SIZE] = 65535;
  r.Tick(2000, true);
  CHECK(processor.delay_active());
  processor.Configure(Processor::Media(settings.state().media), false);
  MonitorStateChange();
  const DelayController& delay = processor.delay_controller_;
  for (int i = 0; i < 200; ++i) {
    r.Block();
    CHECK(delay.test_timing_change_gain() >= 1.0f);
  }
  CHECK(delay.test_timing_fade() && delay.test_synchronized());
  CHECK(delay.test_timing_change_gain() == 1.0f);
  PotsAdc::values_[POTS_ADC_CHANNEL_SIZE] = 32768;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
  // The factory profile omits the entry: SEED starts latched.
  r.Reboot(false);
  CHECK(ui.seed_button_locked());
}

// The smooth PITCH law is a profile choice (#43), set before the codec can
// start; it has no notch cue and leaves the PITCH CV alone.
void TestPitchLawProfile() {
  test_checker.Start("TestPitchLawProfile");
  Rig r(false, false);
  r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
  r.cv[CV_ADC_CHANNEL_PITCH] = 26214;
  r.Settle();
  // Inside the first plateau above noon.
  PotsAdc::values_[POTS_ADC_CHANNEL_PITCH] = 33300;
  settings.SetFeatureValue(FEATURE_PITCH_NOTCH, 1);
  r.Tick(100, true);
  const float cv = cv_reader.cv(CV_ADC_CHANNEL_PITCH);
  CHECK(cv_reader.pot(POTS_ADC_CHANNEL_PITCH) == 0.0f);
  CHECK(ui.pitch_on_notch_);
  const uint8_t pairs[] = { FEATURE_PITCH_LAW, 1, FEATURE_PITCH_NOTCH, 1 };
  BootWithProfile(&r, pairs, 2);
  CHECK(cv_reader.pot_channel_[POTS_ADC_CHANNEL_PITCH].law() ==
      PotChannel::LAW_DETUNE_SMOOTH);
  r.Settle();
  r.Tick(100, true);
  CHECK(cv_reader.pot(POTS_ADC_CHANNEL_PITCH) > 0.05f);
  CHECK(!ui.pitch_on_notch_);
  CHECK(cv_reader.cv(CV_ADC_CHANNEL_PITCH) == cv);
  // The factory profile omits the entry: the notches return.
  r.Reboot(false);
  CHECK(cv_reader.pot_channel_[POTS_ADC_CHANNEL_PITCH].law() ==
      PotChannel::LAW_DETUNE);
  r.Settle();
  r.Tick(100, true);
  CHECK(cv_reader.pot(POTS_ADC_CHANNEL_PITCH) == 0.0f);
}

// Installs the factory profile with the Plaits bank starting elsewhere.
void FlashPlaitsBankProfile(uint8_t bank) {
  uint8_t block[kProfileSize];
  BuildProfile(block, NULL, 0);
  uint8_t* entry = block + 16;
  while (entry[0] != FEATURE_PLAITS_BANK) {
    entry += 6;
  }
  entry[2] = bank;
  const size_t bytes = 6 * block[5];
  const uint32_t crc = Profile::Crc32(block, 16 + bytes);
  for (size_t i = 0; i < 4; ++i) {
    block[16 + bytes + i] = crc >> (8 * i);
  }
  FlashProfile(block);
}

// Powers on and waits for the synth to start.
void StartPlaits(Rig* r) {
  r->Reboot(false);
  r->Settle();
  r->Advance(10001);
  r->Settle();
}

// The model the drone plays with FEEDBACK at this pot value.
int PlaitsModel(Rig* r, uint16_t feedback) {
  PotsAdc::values_[POTS_ADC_CHANNEL_FEEDBACK] = feedback;
  r->Tick(50, true);
  CHECK(synth_mode && plaits_synth.drone_);
  return plaits_synth.patch_[plaits_synth.active_voice_].model;
}

// Plaits banks (#190): a blank sector plays green; [A]+FREEZE steps red,
// yellow, green, keeps FREEZE, survives power cycles; profiles pick the start.
void TestPlaitsBankProfile() {
  test_checker.Start("TestPlaitsBankProfile");
  Rig r(false, true);
  // ASSIGN CV at 0 V: the host probe would otherwise modulate FEEDBACK.
  r.cv_patched[CV_ADC_CHANNEL_ASSIGN] = true;
  settings.set_synth(SYNTH_PLAITS);
  settings.SaveState();
  StartPlaits(&r);
  CHECK(PlaitsModel(&r, 65535) == 15);
  CHECK(PlaitsModel(&r, 0) == 8);
  test_down[SWITCH_FREEZE] = true;
  r.Tick(10, true);
  test_down[SWITCH_FREEZE] = false;
  r.Tick(10, true);
  CHECK(ui.freeze_button_state());
  const int last[] = { 23, 7, 15, 23 };
  for (int step = 0; step < 4; ++step) {
    test_down[SWITCH_MEDIA] = true;
    r.Tick(10, true);
    test_down[SWITCH_FREEZE] = true;
    r.Tick(10, true);
    test_down[SWITCH_FREEZE] = test_down[SWITCH_MEDIA] = false;
    r.Tick(10, true);
    CHECK(settings.FeatureValue(FEATURE_PLAITS_BANK) == (step + 1) % 3);
    CHECK(ui.freeze_button_state() && !ui.trim_flag());
    CHECK(PlaitsModel(&r, 65535) == last[step]);
    CHECK(PlaitsModel(&r, 0) == last[step] - 7);
  }
  // Saved: red returns after a power cycle.
  StartPlaits(&r);
  CHECK(settings.FeatureValue(FEATURE_PLAITS_BANK) == 1);
  CHECK(PlaitsModel(&r, 65535) == 23);
  // A profile's starting bank applies to a blank or stock sector.
  FlashPlaitsBankProfile(2);
  Rig blank(false, true);
  blank.cv_patched[CV_ADC_CHANNEL_ASSIGN] = true;
  settings.set_synth(SYNTH_PLAITS);
  settings.SaveState();
  StartPlaits(&blank);
  CHECK(settings.FeatureValue(FEATURE_PLAITS_BANK) == 2);
  CHECK(PlaitsModel(&blank, 65535) == 7);
  FlashProfile(factory_profile);
}

// Output hash over one second, with DRY/WET at 9 o'clock and REVERB up.
uint64_t MixOptionsHash(Rig* r) {
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 13107;
  PotsAdc::values_[POTS_ADC_CHANNEL_REVERB] = 52000;
  uint64_t hash = 14695981039346656037ull;
  for (int ms = 0; ms < 1000; ++ms) {
    r->audio = int32_t(4.0e8f * sinf(float(ms) * 0.7f));
    r->Tick(1, true);
    for (size_t i = 0; i < codec.block_size_; ++i) {
      hash = (hash ^ uint32_t(r->out[i].l)) * 1099511628211ull;
      hash = (hash ^ uint32_t(r->out[i].r)) * 1099511628211ull;
    }
  }
  return hash;
}

// Mix law and Reverb input are power-on profile choices (#21, #20); they
// change the granular mix only, never either synth.
void TestMixOptionsProfile() {
  test_checker.Start("TestMixOptionsProfile");
  const uint8_t both[] = { FEATURE_MIX_LAW, 1, FEATURE_REVERB_INPUT, 1 };
  uint64_t hashes[3][2];
  for (int engine = 0; engine < 3; ++engine) {
    for (int on = 0; on < 2; ++on) {
      Rig r(false, engine > 0);
      settings.set_synth(engine == 2);
      settings.SaveState();
      if (on) {
        BootWithProfile(&r, both, 2);
        CHECK(processor.options_ == (Processor::OPTION_FLAG_GENTLE_MIX |
            Processor::OPTION_FLAG_REVERB_ON_GRAINS));
      } else {
        r.Reboot(false);
        CHECK(processor.options_ == 0);
      }
      Random::Seed(1);
      if (engine) {
        r.StartAutomatic();
        CHECK(synth_mode);
      } else {
        r.Settle();
      }
      hashes[engine][on] = MixOptionsHash(&r);
      CHECK(synth_mode == (engine > 0));
    }
  }
  CHECK(hashes[0][0] != hashes[0][1]);
  CHECK(hashes[1][0] == hashes[1][1]);
  CHECK(hashes[2][0] == hashes[2][1]);
  // Each choice sets its own path, and TIME scan from the main loop keeps it.
  Rig r(false, false);
  const uint8_t law[] = { FEATURE_MIX_LAW, 1 };
  BootWithProfile(&r, law, 1);
  r.Settle();
  CHECK(processor.options_ == Processor::OPTION_FLAG_GENTLE_MIX);
  const uint8_t input[] = { FEATURE_REVERB_INPUT, 1 };
  BootWithProfile(&r, input, 1);
  settings.SetFeatureValue(FEATURE_TIME_SCAN, 1);
  r.Settle();
  CHECK(processor.options_ == (Processor::OPTION_FLAG_REVERB_ON_GRAINS |
      Processor::OPTION_FLAG_TIME_SCAN));
  settings.SetFeatureValue(FEATURE_TIME_SCAN, 0);
  r.Settle();
  CHECK(processor.options_ == Processor::OPTION_FLAG_REVERB_ON_GRAINS);
  // The factory profile omits both entries.
  r.Reboot(false);
  CHECK(processor.options_ == 0);
}

void TestSendMixProfile() {
  test_checker.Start("TestSendMixProfile");
  Rig r(false, false);
  const uint8_t send[] = { FEATURE_MIX_LAW, 2 };
  BootWithProfile(&r, send, 1);
  r.Settle();
  CHECK(processor.options_ == Processor::OPTION_FLAG_SEND_MIX);
  CHECK(!(processor.options_ & Processor::OPTION_FLAG_GENTLE_MIX));
  r.Reboot(false);
  r.Settle();
  CHECK(processor.options_ == 0);
}

void TestOutputDivisionProfile() {
  test_checker.Start("TestOutputDivisionProfile");
  Rig r(false, false);
  for (int value = 0; value < 5; ++value) {
    const uint8_t division[] = { FEATURE_OUTPUT_DIVISION, uint8_t(value) };
    BootWithProfile(&r, division, 1);
    r.Settle();
    CHECK(processor.output_division_ == (1 << value));
    CHECK(bool(processor.options_ & Processor::OPTION_FLAG_OUTPUT_DIVISION)
        == (value != 0));
  }
  r.Reboot(false);
  r.Settle();
  CHECK(processor.output_division_ == 1);
  CHECK(processor.options_ == 0);
}

// Grain stealing is a power-on profile choice (#237): 0, the default, resorts
// the grains from 5 ms as in 2021/2024; 1 keeps the 2026 source's 25 ms.
void TestGrainStealingProfile() {
  test_checker.Start("TestGrainStealingProfile");
  const uint8_t stealing_2026[] = { FEATURE_GRAIN_STEALING, 1 };
  const uint8_t stealing_2024[] = { FEATURE_GRAIN_STEALING, 0 };
  const uint8_t beta3[] = { FEATURE_REVERB_INPUT, 1 };
  // Factory profile, explicit 2021/2024, a profile without ID 34, then 2026.
  for (int boot = 0; boot < 4; ++boot) {
    Rig r(false, false);
    if (boot == 0) {
      r.Reboot(false);
    } else {
      BootWithProfile(&r, boot == 1 ? stealing_2024 :
          (boot == 2 ? beta3 : stealing_2026), 1);
    }
    const int value = boot == 3;
    const int scale = value ? 5 : 1;
    CHECK(processor.processor_digital_.granular_player_.
        simple_scheduler_threshold_ == 240 * scale);
    CHECK(processor.processor_vintage_digital_.granular_player_.
        simple_scheduler_threshold_ == 160 * scale);
    CHECK(processor.processor_clean_tape_.granular_player_.
        simple_scheduler_threshold_ == 120 * scale);
    CHECK(processor.processor_dirty_tape_.granular_player_.
        simple_scheduler_threshold_ == 120 * scale);
    CHECK(synth.wavetable_granular_player_.
        simple_scheduler_threshold_ == 240 * scale);
    CHECK(settings.FeatureValue(FEATURE_GRAIN_STEALING) == value);
  }
  // Nothing is saved: after play and a save, the factory profile is 2021/2024.
  Rig r(false, false);
  BootWithProfile(&r, stealing_2026, 1);
  r.Settle();
  CHECK(processor.processor_digital_.granular_player_.
      simple_scheduler_threshold_ == 1200);
  settings.SavePersistentData();
  settings.SaveState();
  r.Reboot(false);
  CHECK(settings.FeatureValue(FEATURE_GRAIN_STEALING) == 0);
  CHECK(processor.processor_digital_.granular_player_.
      simple_scheduler_threshold_ == 240);
}

// Delay feedback and Delay time are power-on profile choices (#10, #30);
// granular playback and both synths are unchanged.
void TestDelayOptionsProfile() {
  test_checker.Start("TestDelayOptionsProfile");
  const uint8_t both[] = {
    FEATURE_UNITY_FEEDBACK, 1, FEATURE_DELAY_CROSSFADE, 1
  };
  for (int engine = 0; engine < 3; ++engine) {
    uint64_t hashes[2];
    for (int on = 0; on < 2; ++on) {
      Rig r(false, engine > 0);
      settings.set_synth(engine == 2);
      settings.SaveState();
      if (on) {
        BootWithProfile(&r, both, 2);
        CHECK(processor.options_ == (Processor::OPTION_FLAG_UNITY_FEEDBACK |
            Processor::OPTION_FLAG_DELAY_CROSSFADE));
      } else {
        r.Reboot(false);
        CHECK(processor.options_ == 0);
      }
      Random::Seed(1);
      if (engine) {
        r.StartAutomatic();
        CHECK(synth_mode);
      } else {
        r.Settle();
      }
      hashes[on] = MixOptionsHash(&r);
    }
    CHECK(hashes[0] == hashes[1]);
  }
  Rig r(false, false);
  const uint8_t unity[] = { FEATURE_UNITY_FEEDBACK, 1 };
  BootWithProfile(&r, unity, 1);
  r.Settle();
  CHECK(processor.options_ == Processor::OPTION_FLAG_UNITY_FEEDBACK);
  const uint8_t crossfade[] = { FEATURE_DELAY_CROSSFADE, 1 };
  BootWithProfile(&r, crossfade, 1);
  r.Settle();
  CHECK(processor.options_ == Processor::OPTION_FLAG_DELAY_CROSSFADE);
  // The factory profile omits both entries.
  r.Reboot(false);
  CHECK(processor.options_ == 0);
}

// The resonator's energy: the first batch's filter state.
float ResonatorEnergy() {
  float energy = 0.0f;
  for (int i = 0; i < plaits::kModeBatchSize; ++i) {
    energy += fabsf(processor.resonator_.odd_[0].state_1_[i]);
    energy += fabsf(processor.resonator_.even_[0].state_1_[i]);
  }
  return energy;
}

// The resonator is a power-on profile choice; both synths are unchanged.
void TestResonatorProfile() {
  test_checker.Start("TestResonatorProfile");
  const uint16_t expected[] = {
    0,
    Processor::OPTION_FLAG_RESONATOR_GRAINS,
    Processor::OPTION_FLAG_RESONATOR_INPUT,
    Processor::OPTION_FLAG_RESONATOR_GRAINS |
        Processor::OPTION_FLAG_RESONATOR_INPUT,
    Processor::OPTION_FLAG_RESONATOR_GRAINS |
        Processor::OPTION_FLAG_RESONATOR_CHORD
  };
  for (int engine = 0; engine < 3; ++engine) {
    uint64_t hashes[5];
    for (int placement = 0; placement < 5; ++placement) {
      Rig r(false, engine > 0);
      settings.set_synth(engine == 2);
      settings.SaveState();
      if (placement) {
        const uint8_t entry[] = { FEATURE_RESONATOR, uint8_t(placement) };
        BootWithProfile(&r, entry, 1);
      } else {
        r.Reboot(false);
      }
      CHECK(processor.options_ == expected[placement]);
      Random::Seed(1);
      if (engine) {
        r.StartAutomatic();
        CHECK(synth_mode);
      } else {
        r.Settle();
      }
      if (!engine) {
        CHECK((codec.callback_ != &Process) == (placement == RESONATOR_CHORD));
      }
      hashes[placement] = MixOptionsHash(&r);
    }
    if (engine) {
      CHECK(hashes[1] == hashes[0]);
      CHECK(hashes[2] == hashes[0]);
      CHECK(hashes[3] == hashes[0]);
      CHECK(hashes[4] == hashes[0]);
    }
  }
  // Through the audio callback: the input rings it before the buffer, and
  // a Snap and trigger hit strikes it after the silent grains, also in the
  // feedback.
  for (int placement = 1; placement < 4; ++placement) {
    Rig r(false, false);
    settings.set_synth(SYNTH_BEADS);
    settings.set_attack_mode(ATTACK_MODE_SNAP_AND_TRIGGER);
    settings.SaveState();
    const uint8_t entry[] = { FEATURE_RESONATOR, uint8_t(placement) };
    BootWithProfile(&r, entry, 1);
    PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
    // Quiet input, as for Hit(); before the buffer it would ring on it.
    r.audio = placement != RESONATOR_BEFORE_BUFFER ? 0x1000000 : 0;
    r.Settle();
    r.Tick(300, true);
    CHECK(ResonatorEnergy() == 0.0f);
    int first;
    bool led;
    Hit(&r, &first, &led);
    CHECK(first == 1 && led);
    CHECK(ResonatorEnergy() > 0.0f);
  }
  settings.set_attack_mode(ATTACK_MODE_OFF);
  settings.SaveState();
  // The factory profile omits it.
  Rig r(false, false);
  r.Reboot(false);
  CHECK(processor.options_ == 0);
}

// Tap [A]: a short press, released at once.
void TapAssign(Rig* r) {
  test_down[SWITCH_ASSIGN] = true;
  r->Tick(30, true);
  test_down[SWITCH_ASSIGN] = false;
  r->Tick(30, true);
}

bool ControlsEqual(const ResonatorControls& c, float structure,
    float brightness, float damping, float position, float amount) {
  return c.structure == structure && c.brightness == brightness &&
      c.damping == damping && c.position == position && c.amount == amount;
}

// Page 5 moves the resonator live and saves; Hygrostat's exact values win.
void TestResonatorPage() {
  test_checker.Start("TestResonatorPage");
  Rig r(false, false);
  r.Settle();
  const Profile& profile = settings.profile();
  CHECK(profile.Page(13) == FEATURE_RESONATOR);
  CHECK(profile.Page(14) == FEATURE_RESONATOR_MATERIAL);
  CHECK(profile.Page(15) == FEATURE_RESONATOR_RING);
  CHECK(settings.FeatureValue(FEATURE_RESONATOR) == RESONATOR_OFF);
  CHECK(settings.FeatureValue(FEATURE_RESONATOR_MATERIAL) == 1);
  CHECK(settings.FeatureValue(FEATURE_RESONATOR_RING) == 1);
  CHECK(processor.options_ == 0);
  CHECK(!processor.resonator_controls_set_);
  const uint16_t flags[] = {
    0,
    Processor::OPTION_FLAG_RESONATOR_GRAINS,
    Processor::OPTION_FLAG_RESONATOR_INPUT,
    Processor::OPTION_FLAG_RESONATOR_GRAINS |
        Processor::OPTION_FLAG_RESONATOR_INPUT,
    Processor::OPTION_FLAG_RESONATOR_GRAINS |
        Processor::OPTION_FLAG_RESONATOR_CHORD
  };
  const int restarts = codec.restarts_;
  for (int placement = 1; placement < 8; ++placement) {
    settings.SetFeatureValue(FEATURE_RESONATOR, placement % 5);
    r.Tick(2, true);
    CHECK(processor.options_ == flags[placement % 5]);
    CHECK((codec.callback_ != &Process) == (placement % 5 == RESONATOR_CHORD));
  }
  CHECK(codec.restarts_ == restarts);
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_AFTER_GRAINS);
  for (int material = 0; material < kNumResonatorMaterials; ++material) {
    for (int ring = 0; ring < 4; ++ring) {
      settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, material);
      settings.SetFeatureValue(FEATURE_RESONATOR_RING, ring);
      r.Tick(2, true);
      CHECK(ControlsEqual(processor.resonator_controls_,
          kResonatorMaterials[material].structure,
          kResonatorMaterials[material].brightness,
          kResonatorRingTimes[ring], kResonatorPosition, 1.0f));
      CHECK(processor.resonator_controls_.modes ==
          kResonatorMaterials[material].modes);
    }
  }
  // Off, the controls are left alone.
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_OFF);
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 1);
  r.Tick(2, true);
  CHECK(processor.options_ == 0);
  CHECK(processor.resonator_controls_.modes ==
      kResonatorMaterials[kNumResonatorMaterials - 1].modes);
  // Saved: a power cycle brings it back, before the buffer from the start.
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_BEFORE_BUFFER);
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 3);
  settings.SetFeatureValue(FEATURE_RESONATOR_RING, 0);
  settings.SaveOptions();
  r.Reboot(false);
  CHECK(settings.FeatureValue(FEATURE_RESONATOR) == RESONATOR_BEFORE_BUFFER);
  CHECK(settings.FeatureValue(FEATURE_RESONATOR_MATERIAL) == 3);
  CHECK(settings.FeatureValue(FEATURE_RESONATOR_RING) == 0);
  CHECK(processor.options_ == Processor::OPTION_FLAG_RESONATOR_INPUT);
  r.Settle();
  CHECK(ControlsEqual(processor.resonator_controls_,
      kResonatorMaterials[3].structure, kResonatorMaterials[3].brightness,
      kResonatorRingTimes[0], kResonatorPosition, 1.0f));
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_OFF);
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 1);
  settings.SetFeatureValue(FEATURE_RESONATOR_RING, 1);
  settings.SaveOptions();
  // Exact values replace the curated ones; the others stay curated.
  const uint8_t exact[] = {
    FEATURE_RESONATOR, RESONATOR_AFTER_GRAINS,
    FEATURE_RESONATOR_STRUCTURE, 64,
    FEATURE_RESONATOR_DAMPING, 10,
    FEATURE_RESONATOR_POSITION, 255,
    FEATURE_RESONATOR_AMOUNT, 32
  };
  BootWithProfile(&r, exact, 5);
  CHECK(settings.profile().Page(13) == FEATURE_NONE);
  r.Settle();
  CHECK(processor.options_ == Processor::OPTION_FLAG_RESONATOR_GRAINS);
  CHECK(ControlsEqual(processor.resonator_controls_, 0.25f,
      kResonatorMaterials[1].brightness, 10.0f / 256.0f, 255.0f / 256.0f,
      0.25f));
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 3);
  settings.SetFeatureValue(FEATURE_RESONATOR_RING, 3);
  r.Tick(2, true);
  CHECK(ControlsEqual(processor.resonator_controls_, 0.25f,
      kResonatorMaterials[3].brightness, 10.0f / 256.0f, 255.0f / 256.0f,
      0.25f));
  // An exact structure is Rings' curve under Drizzlet's objects too.
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 5);
  r.Tick(2, true);
  CHECK(processor.resonator_controls_.structure == 0.25f);
  CHECK(processor.resonator_controls_.brightness ==
      kResonatorMaterials[5].brightness);
  CHECK(processor.resonator_controls_.modes == NULL);
  const uint8_t bright[] = {
    FEATURE_RESONATOR, RESONATOR_BEFORE_BUFFER,
    FEATURE_RESONATOR_BRIGHTNESS, 0
  };
  BootWithProfile(&r, bright, 2);
  r.Settle();
  CHECK(ControlsEqual(processor.resonator_controls_,
      kResonatorMaterials[1].structure, 0.0f, kResonatorRingTimes[1],
      kResonatorPosition, 1.0f));
  // An exact brightness keeps the object's modes.
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 5);
  r.Tick(2, true);
  CHECK(processor.resonator_controls_.brightness == 0.0f);
  CHECK(processor.resonator_controls_.modes == kResonatorMaterials[5].modes);
}

// [A] taps reach resonator damping after the other destinations, while the
// resonator is on; the CV then moves damping and no mix knob.
void TestResonatorDampingCv() {
  test_checker.Start("TestResonatorDampingCv");
  Rig r(false, false);
  r.cv_patched[CV_ADC_CHANNEL_ASSIGN] = true;
  r.Settle();
  // Off, the cycle is stock.
  for (int tap = 0; tap < 4; ++tap) {
    TapAssign(&r);
    CHECK(settings.state().assignable_cv_destination == (tap + 1) % 3);
    CHECK(!ui.resonator_assigned());
  }
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_AFTER_GRAINS);
  r.Tick(2, true);
  TapAssign(&r);
  CHECK(settings.state().assignable_cv_destination == 2);
  TapAssign(&r);
  CHECK(ui.resonator_assigned());
  CHECK(settings.state().assignable_cv_destination == 0);
  CHECK(!settings.width_assigned() && !settings.diffusion_assigned());
  // The CV adds to Ring time's damping; FEEDBACK does not move.
  float feedback[2];
  for (int step = 0; step < 2; ++step) {
    r.cv[CV_ADC_CHANNEL_ASSIGN] = step ? 20000 : 32768;
    r.Tick(200, true);
    const float cv = cv_reader.cv(CV_ADC_CHANNEL_ASSIGN) - 0.002f;
    float damping = kResonatorRingTimes[1] + cv;
    CONSTRAIN(damping, 0.0f, 0.9995f);
    CHECK(processor.resonator_controls_.damping == damping);
    CHECK(!step || fabsf(damping - kResonatorRingTimes[1]) > 0.05f);
    feedback[step] = processor.processor_digital_.feedback_;
  }
  CHECK(feedback[0] == feedback[1]);
  // Held below the end of Rings' damping table.
  r.cv[CV_ADC_CHANNEL_ASSIGN] = 0;
  r.Tick(200, true);
  CHECK(processor.resonator_controls_.damping == 0.9995f);
  // The LEDs show page 5's: FEEDBACK and REVERB pulse, DRY/WET is dark.
  bool lit = false;
  for (int ms = 0; ms < 1100; ++ms) {
    r.Tick(1, false);
    const uint8_t* led = &ui.leds_.intensity_[LED_ASSIGN_1];
    CHECK(led[Ui::MIX_PARAMETER_DRY_WET] == 0);
    CHECK(led[Ui::MIX_PARAMETER_FEEDBACK] == led[Ui::MIX_PARAMETER_REVERB]);
    lit = lit || led[Ui::MIX_PARAMETER_FEEDBACK] > 128;
  }
  CHECK(lit);
  // Next tap: FEEDBACK again, and the CV moves it.
  TapAssign(&r);
  CHECK(!ui.resonator_assigned());
  CHECK(settings.state().assignable_cv_destination == 0);
  r.Tick(200, true);
  CHECK(processor.processor_digital_.feedback_ != feedback[0]);
  // The synths, where it does not run, return the CV to FEEDBACK.
  TapAssign(&r);
  TapAssign(&r);
  TapAssign(&r);
  CHECK(ui.resonator_assigned());
  ui.set_synth_mode(true);
  SysTick_Handler();
  CHECK(!ui.resonator_assigned());
  ui.set_synth_mode(false);
  // Turning the resonator off returns the CV to FEEDBACK.
  TapAssign(&r);
  TapAssign(&r);
  TapAssign(&r);
  CHECK(ui.resonator_assigned());
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_OFF);
  r.Tick(2, true);
  CHECK(!ui.resonator_assigned());
  CHECK(settings.state().assignable_cv_destination == 0);
  // Not saved: a power cycle starts at FEEDBACK.
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_AFTER_GRAINS);
  r.Tick(2, true);
  TapAssign(&r);
  TapAssign(&r);
  TapAssign(&r);
  CHECK(ui.resonator_assigned());
  r.Reboot(false);
  CHECK(!ui.resonator_assigned());
  CHECK(settings.state().assignable_cv_destination == 0);
}

// Through the audio callback, Attacks Off: a SEED press strikes it; the
// press that unlatches SEED at power-up does not.
void TestResonatorSeedCallback() {
  test_checker.Start("TestResonatorSeedCallback");
  Rig r(false, false);
  settings.set_attack_mode(ATTACK_MODE_OFF);
  settings.SaveState();
  const uint8_t entry[] = { FEATURE_RESONATOR, RESONATOR_AFTER_GRAINS };
  BootWithProfile(&r, entry, 1);
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
  r.audio = 0;
  r.Settle();
  CHECK(ui.seed_button_locked());
  CHECK(ResonatorEnergy() == 0.0f);
  for (int press = 0; press < 2; ++press) {
    test_down[SWITCH_SEED] = true;
    r.Tick(20, true);
    test_down[SWITCH_SEED] = false;
    r.Tick(20, true);
    CHECK((ResonatorEnergy() > 0.0f) == bool(press));
  }
  // Patched and latched, each edge at the jack strikes it.
  r.seed_patched = true;
  BootWithProfile(&r, entry, 1);
  r.Settle();
  CHECK(processor.options_ == Processor::OPTION_FLAG_RESONATOR_GRAINS);
  CHECK(ui.seed_button_locked());
  CHECK(ResonatorEnergy() == 0.0f);
  test_seed_input = true;
  r.Tick(20, true);
  CHECK(ResonatorEnergy() > 0.0f);
  test_seed_input = false;
}

// The factory profile with one feature's action slot moved, as a Hygrostat
// layout can place it.
void MoveAction(uint8_t* block, uint8_t feature, uint8_t action) {
  for (int i = 0; i < block[5]; ++i) {
    uint8_t* entry = block + 16 + 6 * i;
    if (entry[0] == feature) {
      entry[5] = action;
    }
  }
  const size_t bytes = 6 * block[5];
  const uint32_t crc = Profile::Crc32(block, 16 + bytes);
  for (size_t i = 0; i < 4; ++i) {
    block[16 + bytes + i] = crc >> (8 * i);
  }
}

// Reject profiles whose [A] hold prevents returning to automatic gain
// (#291); the standard controls' hold restores it.
void TestHoldActionRestartsAutoGain() {
  test_checker.Start("TestHoldActionRestartsAutoGain");
  const uint8_t features[] = { FEATURE_MONOPHONIC, FEATURE_TRIM };
  const uint8_t synth_start[] = { ACTION_SEED, ACTION_FREEZE };
  for (int i = 0; i < 2; ++i) {
    Rig r(false, false);
    uint8_t block[kProfileSize];
    BuildProfile(block, NULL, 0);
    MoveAction(block, FEATURE_SYNTH_START, synth_start[i]);
    MoveAction(block, features[i], ACTION_HOLD);
    FlashProfile(block);
    r.Reboot(false);
    CHECK(settings.profile_locked());
    CHECK(settings.profile().Action(ACTION_HOLD) == FEATURE_SYNTH_START);
    settings.mutable_state()->gain = 200;
    r.audio = 0x10000000;
    r.Settle();
    r.HoldMedia(true);
    CHECK(settings.state().gain == 0);
    FlashProfile(factory_profile);
  }
  // Monophonic and Buffer trim may still swap their own gestures.
  Rig r(false, false);
  uint8_t block[kProfileSize];
  BuildProfile(block, NULL, 0);
  MoveAction(block, FEATURE_MONOPHONIC, ACTION_FREEZE);
  MoveAction(block, FEATURE_TRIM, ACTION_SEED);
  FlashProfile(block);
  r.Reboot(false);
  CHECK(!settings.profile_locked());
  CHECK(settings.profile().Action(ACTION_SEED) == FEATURE_TRIM);
  FlashProfile(factory_profile);
}

// Synth start on an options page with no gesture is refused (#292): Manual
// would leave nothing to start the synth. The standard controls' hold does.
void TestManualSynthStartNeedsGesture() {
  test_checker.Start("TestManualSynthStartNeedsGesture");
  Rig r(true, true);
  uint8_t block[kProfileSize];
  BuildProfile(block, NULL, 0);
  MoveAction(block, FEATURE_SYNTH_START, ACTION_NONE);
  FlashProfile(block);
  r.Reboot(false);
  CHECK(settings.profile_locked());
  CHECK(settings.self_play_on_request());
  CHECK(settings.profile().Action(ACTION_HOLD) == FEATURE_SYNTH_START);
  r.Settle();
  r.Advance(11000);
  RunMainLoop();
  CHECK(!synth_mode);
  r.HoldMedia(true);
  r.Settle();
  CHECK(synth_mode);
  FlashProfile(factory_profile);
}

// A slow [A] gesture is not pre-empted by the hold (#289): the hold acts on
// release, and only when nothing else was done while [A] was down.
void TestSlowMediaGestures() {
  test_checker.Start("TestSlowMediaGestures");
  Rig r(false, false);
  r.Settle();
  settings.mutable_state()->gain = 200;
  settings.SaveState();
  // Width, then diffusion, two seconds into the hold: the gain stays.
  const PotsAdcChannel knobs[] = {
    POTS_ADC_CHANNEL_DRY_WET, POTS_ADC_CHANNEL_REVERB
  };
  for (int k = 0; k < 2; ++k) {
    test_down[SWITCH_MEDIA] = true;
    r.Tick(2000, true);
    CHECK(settings.state().gain == 200);
    PotsAdc::values_[knobs[k]] = 40000;
    r.Tick(100, true);
    test_down[SWITCH_MEDIA] = false;
    r.Tick(50, true);
    CHECK(settings.state().gain == 200);
    PotsAdc::values_[knobs[k]] = 0;
    r.Tick(50, true);
  }
  CHECK(settings.stereo_width() != 0 && settings.diffusion_set());
  // FREEZE two seconds in trims, clearing the FREEZE latch, and keeps the
  // gain.
  ui.set_freeze_button_state(true);
  test_down[SWITCH_MEDIA] = true;
  r.Tick(2000, true);
  test_down[SWITCH_FREEZE] = true;
  r.Tick(20, true);
  test_down[SWITCH_FREEZE] = test_down[SWITCH_MEDIA] = false;
  r.Tick(50, true);
  CHECK(!ui.freeze_button_state() && !ui.trim_flag());
  CHECK(settings.state().gain == 200);
  // A hold alone acts on release, however long.
  const uint8_t media = settings.state().media;
  test_down[SWITCH_MEDIA] = true;
  r.Tick(3000, true);
  CHECK(settings.state().gain == 200);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(50, true);
  CHECK(settings.state().gain == 0 && settings.state().media == media);
  // In Plaits synth play with Manual start, a slow [A]+FREEZE steps the
  // bank and keeps the synth.
  Rig s(true, true);
  settings.set_synth(SYNTH_PLAITS);
  settings.SaveState();
  s.Reboot(false);
  StartSelfPlay(&s, true);
  const uint8_t bank = settings.FeatureValue(FEATURE_PLAITS_BANK);
  test_down[SWITCH_MEDIA] = true;
  s.Tick(2000, true);
  test_down[SWITCH_FREEZE] = true;
  s.Tick(20, true);
  test_down[SWITCH_FREEZE] = test_down[SWITCH_MEDIA] = false;
  s.Tick(50, true);
  s.Settle();
  CHECK(synth_mode);
  CHECK(settings.FeatureValue(FEATURE_PLAITS_BANK) == (bank + 1) % 3);
}

// A Processor change while frozen keeps the current engine playing until
// FREEZE is released, then fades and switches (#288).
void TestEngineChangeWhileFrozen() {
  test_checker.Start("TestEngineChangeWhileFrozen");
  extern float& reverb_style_gain;
  Rig r(false, false);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 65535;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 8000;
  // Grains from the last moment recorded.
  PotsAdc::values_[POTS_ADC_CHANNEL_TIME] = 0;
  const double live = RunSquare(&r, 2400);
  ui.set_freeze_button_state(true);
  const double frozen = RunSquare(&r, 400);
  settings.set_spectral(true);
  SysTick_Handler();
  CHECK(ui.spectral() && !processor.spectral());
  double pending = 1e9;
  for (int i = 0; i < 8; ++i) {
    pending = std::min(pending, RunSquare(&r, 400));
    CHECK(reverb_style_gain == 1.0f && !processor.spectral());
  }
  printf("Engine change while frozen: live %.1f, frozen %.1f, pending %.1f\n",
      live, frozen, pending);
  CHECK(frozen > 1.0 && pending > frozen * 0.25);
  ui.set_freeze_button_state(false);
  for (int i = 0; i < 400 && !processor.spectral(); ++i) {
    RunSquare(&r, 1);
  }
  CHECK(processor.spectral());
  // Back to granular with a FREEZE gate high: spectral plays on, then fades.
  RunSquare(&r, 2000);
  test_freeze_input = true;
  RunSquare(&r, 400);
  settings.set_spectral(false);
  SysTick_Handler();
  pending = 1e9;
  for (int i = 0; i < 8; ++i) {
    pending = std::min(pending, RunSquare(&r, 400));
    CHECK(reverb_style_gain == 1.0f && processor.spectral());
  }
  CHECK(pending > 1.0);
  test_freeze_input = false;
  bool faded = false;
  for (int i = 0; i < 400 && processor.spectral(); ++i) {
    RunSquare(&r, 1);
    faded |= reverb_style_gain == 0.0f;
  }
  CHECK(faded && !processor.spectral());
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 0;
  PotsAdc::values_[POTS_ADC_CHANNEL_DENSITY] = 32768;
  PotsAdc::values_[POTS_ADC_CHANNEL_TIME] = 32768;
}

// In calibration, holding [A] confirms a step only on release, once.
void TestCalibrationStepOnRelease() {
  test_checker.Start("TestCalibrationStepOnRelease");
  Rig r(false, false);
  r.Settle();
  test_down[SWITCH_ASSIGN] = true;
  r.Advance(20);
  test_down[SWITCH_MEDIA] = true;
  r.Advance(20);
  test_down[SWITCH_ASSIGN] = test_down[SWITCH_MEDIA] = false;
  r.Advance(20);
  CHECK(ui.mode_ == Ui::MODE_CALIBRATION_1);
  test_down[SWITCH_MEDIA] = true;
  r.Advance(2000);
  CHECK(ui.mode_ == Ui::MODE_CALIBRATION_1);
  test_down[SWITCH_MEDIA] = false;
  r.Advance(20);
  CHECK(ui.mode_ == Ui::MODE_CALIBRATION_2);
  r.Advance(2000);
  CHECK(ui.mode_ == Ui::MODE_CALIBRATION_2);
}

// Each channel's scale and offset, to within the CV filter's settling.
bool SameCalibration(const PersistentData& a, const PersistentData& b) {
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    if (fabsf(a.adc_calibration[i].scale - b.adc_calibration[i].scale) >
            1.0e-3f ||
        fabsf(a.adc_calibration[i].offset - b.adc_calibration[i].offset) >
            1.0e-3f) {
      return false;
    }
  }
  return true;
}

// A recalibration out of range is not saved: the stored calibration stays,
// in use too, and all LEDs blink as for a rejected step.
void TestRejectedCalibrationCue() {
  test_checker.Start("TestRejectedCalibrationCue");
  for (int wild = 0; wild < 2; ++wild) {
    Rig r(false, false);
    // DENSITY CV reading 0.2 of the ADC: an offset near the factory one.
    r.cv_patched[CV_ADC_CHANNEL_DENSITY] = true;
    r.cv[CV_ADC_CHANNEL_DENSITY] = 13107;
    r.Settle();
    const PersistentData stored = settings.persistent_data();
    // A DENSITY step whose scale passes, as a calibration with 1 V and 3 V.
    const float u = cv_reader.unscaled_cv(CV_ADC_CHANNEL_DENSITY);
    calibrator.calibrator_[1].Acquire(0, u + 0.2f, 0.2f);
    PersistentData attempt = stored;
    attempt.adc_calibration[CV_ADC_CHANNEL_DENSITY].scale = -2.0f;
    attempt.adc_calibration[CV_ADC_CHANNEL_DENSITY].offset = 0.6f + 2.0f * u;
    // The run's values stay in the calibrator until it completes.
    calibrator.Start();
    if (wild) {
      calibrator.calibration_data_[CV_ADC_CHANNEL_TIME].offset *= 3.0f;
    }
    CHECK(Settings::CalibrationPlausible(attempt, settings.factory_));
    // The last event over a second ago, as after a slow flash rewrite.
    for (int i = 0; i < 400 && ui.queue_.idle_time() <= 1500; ++i) {
      r.Advance(10);
    }
    CHECK(ui.queue_.idle_time() > 1500);
    ui.mode_ = Ui::MODE_CALIBRATION_4;
    ui.NextCalibrationStep();
    CHECK(ui.mode_ == (wild ? Ui::MODE_PANIC : Ui::MODE_NORMAL));
    CHECK(SameCalibration(settings.persistent_data(), wild ? stored : attempt));
    if (wild) {
      r.Advance(500);
      CHECK(ui.mode_ == Ui::MODE_PANIC);
      r.Advance(1000);
      CHECK(ui.mode_ == Ui::MODE_NORMAL);
    }
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(SameCalibration(loaded.persistent_data(), wild ? stored : attempt));
  }
}

// A button, or a chord with the first held, pressed and released.
void Press(Rig* r, int first, int second) {
  test_down[first] = true;
  r->Advance(20);
  test_down[second] = true;
  r->Advance(20);
  test_down[first] = test_down[second] = false;
  r->Advance(20);
}

// The next save rewrites the settings sector, as when it is full.
void RewriteOnNextSave() {
  settings.chunk_storage_.next_state_chunk_index_ = 0xffff;
  settings.SaveState();
}

// Incomplete calibration changes neither live nor saved values (#346),
// including rejected offsets/pitch and exit via the autosave chord.
void TestCalibrationExitResumesNormalization() {
  test_checker.Start("TestCalibrationExitResumesNormalization");
  Rig r(false, false);
  r.Settle();
  Press(&r, SWITCH_ASSIGN, SWITCH_MEDIA);
  CHECK(ui.mode_ == Ui::MODE_CALIBRATION_1);
  Press(&r, SWITCH_ASSIGN, SWITCH_FREEZE);
  r.left = r.right = false;
  r.Settle();
  CHECK(normalization_detector.no_input());
}

void TestUnfinishedCalibrationKept() {
  test_checker.Start("TestUnfinishedCalibrationKept");
  for (int exit = 0; exit < 3; ++exit) {
    Rig r(false, false);
    // Cables left in TIME, PITCH and DENSITY.
    r.cv_patched[CV_ADC_CHANNEL_TIME] = true;
    r.cv_patched[CV_ADC_CHANNEL_PITCH] = true;
    r.cv_patched[CV_ADC_CHANNEL_DENSITY] = true;
    r.cv[CV_ADC_CHANNEL_TIME] = 20000;
    r.cv[CV_ADC_CHANNEL_PITCH] = 40000;
    r.cv[CV_ADC_CHANNEL_DENSITY] = 13107;
    r.Settle();
    const PersistentData stored = settings.persistent_data();
    Press(&r, SWITCH_ASSIGN, SWITCH_MEDIA);
    CHECK(ui.mode_ == Ui::MODE_CALIBRATION_1);
    Press(&r, SWITCH_MEDIA, SWITCH_MEDIA);
    CHECK(ui.mode_ == Ui::MODE_CALIBRATION_2);
    CHECK(fabsf(calibrator.calibration_data_[CV_ADC_CHANNEL_TIME].offset -
        stored.adc_calibration[CV_ADC_CHANNEL_TIME].offset) > 0.1f);
    RewriteOnNextSave();
    if (exit == 0) {
      Press(&r, SWITCH_MEDIA, SWITCH_MEDIA);  // PITCH unchanged: rejected.
    } else if (exit == 1) {
      r.cv[CV_ADC_CHANNEL_PITCH] -= 13107;  // 2 V higher: accepted.
      r.Settle();
      Press(&r, SWITCH_MEDIA, SWITCH_MEDIA);
      CHECK(ui.mode_ == Ui::MODE_CALIBRATION_3);
      CHECK(fabsf(calibrator.calibration_data_[CV_ADC_CHANNEL_PITCH].offset -
          stored.adc_calibration[CV_ADC_CHANNEL_PITCH].offset) > 1.0f);
      Press(&r, SWITCH_MEDIA, SWITCH_MEDIA);
      CHECK(ui.mode_ == Ui::MODE_CALIBRATION_4);
      Press(&r, SWITCH_MEDIA, SWITCH_MEDIA);  // DENSITY unchanged: rejected.
    } else {
      Press(&r, SWITCH_ASSIGN, SWITCH_FREEZE);
      CHECK(ui.mode_ == Ui::MODE_DISPLAY_AUTOSAVE);
    }
    CHECK(exit == 2 || ui.mode_ == Ui::MODE_PANIC);
    CHECK(SameCalibration(settings.persistent_data(), stored));
    RewriteOnNextSave();
    Settings loaded;
    PersistentData mirror;
    CHECK(loaded.Init(false));
    CHECK(SameCalibration(loaded.persistent_data(), stored));
    CHECK(loaded.chunk_storage_.ReadMirror(&mirror));
    CHECK(SameCalibration(mirror, stored));
  }
}

// The factory test may repeat its last step: the completed run stays in the
// calibrator, so the repeat saves the same offsets and pitch again.
void TestFactoryCalibrationRepeat() {
  test_checker.Start("TestFactoryCalibrationRepeat");
  Rig r(false, false);
  // Readings that give a calibration within 25% of the factory one.
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    r.cv_patched[i] = true;
    r.cv[i] = 30000;
  }
  r.cv[CV_ADC_CHANNEL_PITCH] = 28000;
  r.cv[CV_ADC_CHANNEL_DENSITY] = 22938;
  r.Settle();
  const PersistentData stored = settings.persistent_data();
  CHECK(factory_test.Calibrate(3) && factory_test.Calibrate(4));
  r.cv[CV_ADC_CHANNEL_PITCH] -= 13107;
  r.Settle();
  CHECK(factory_test.Calibrate(5) && factory_test.Calibrate(6));
  r.cv[CV_ADC_CHANNEL_DENSITY] -= 13107;
  r.Settle();
  CHECK(factory_test.Calibrate(7));
  const PersistentData measured = settings.persistent_data();
  CHECK(!SameCalibration(measured, stored));
  CHECK(factory_test.Calibrate(7));
  CHECK(SameCalibration(settings.persistent_data(), measured));
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(SameCalibration(loaded.persistent_data(), measured));
}

// #347: with the profile refused, nothing can be saved: a recalibration
// shows the rejection blink, and the stored calibration stays in use.
void TestLockedCalibrationCue() {
  test_checker.Start("TestLockedCalibrationCue");
  Rig r(false, false);
  uint8_t block[kProfileSize];
  BuildProfile(block, NULL, 0);
  block[0] ^= 0xff;
  FlashProfile(block);
  r.Reboot(false);
  FlashProfile(factory_profile);
  CHECK(settings.profile_locked() && settings.write_locked());
  r.cv_patched[CV_ADC_CHANNEL_DENSITY] = true;
  r.cv[CV_ADC_CHANNEL_DENSITY] = 13107;
  r.Settle();
  const PersistentData stored = settings.persistent_data();
  const float u = cv_reader.unscaled_cv(CV_ADC_CHANNEL_DENSITY);
  calibrator.Start();
  calibrator.calibrator_[1].Acquire(0, u + 0.19f, 0.2f);  // Not the stored.
  ui.mode_ = Ui::MODE_CALIBRATION_4;
  ui.NextCalibrationStep();
  CHECK(ui.mode_ == Ui::MODE_PANIC);
  CHECK(SameCalibration(settings.persistent_data(), stored));
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(SameCalibration(loaded.persistent_data(), stored));
}

// [A]+SEED as a Processor toggle takes the options page's path: it saves,
// fades and swaps the engine both ways, clearing the recording for spectral.
void TestProcessorToggle() {
  test_checker.Start("TestProcessorToggle");
  extern float& reverb_style_gain;
  extern uint8_t delay_buffer_1[];
  Rig r(false, false);
  uint8_t block[kProfileSize];
  BuildProfile(block, NULL, 0);
  MoveAction(block, FEATURE_MONOPHONIC, ACTION_NONE);
  MoveAction(block, FEATURE_PROCESSOR, ACTION_SEED);
  FlashProfile(block);
  r.Reboot(false);
  CHECK(!settings.profile_locked());
  CHECK(settings.profile().Action(ACTION_SEED) == FEATURE_PROCESSOR);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 65535;
  for (int pass = 0; pass < 2; ++pass) {
    const bool spectral = pass == 0;
    RunSquare(&r, 2400);
    CHECK(processor.spectral() == !spectral);
    bool recorded = false;
    for (size_t i = 0; i < 512 * 1024; ++i) {
      recorded = recorded || delay_buffer_1[i];
    }
    CHECK(recorded || !spectral);
    const uint32_t writes = test_writes;
    bool faded = false;
    for (int step = 0; step < 3; ++step) {
      test_down[SWITCH_MEDIA] = step < 2;
      test_down[SWITCH_SEED] = step == 1;
      for (int i = 0; i < 20; ++i) {
        r.Tick(1, true);
        faded |= reverb_style_gain == 0.0f;
      }
    }
    CHECK(settings.spectral() == spectral && ui.spectral() == spectral);
    CHECK(test_writes == writes + 2);  // One options record, no state.
    CHECK(!settings.state().monophonic_mode);
    for (int i = 0; i < 400 && processor.spectral() != spectral; ++i) {
      RunSquare(&r, 1);
      faded |= reverb_style_gain == 0.0f;
    }
    CHECK(faded && processor.spectral() == spectral);
    if (spectral) {
      bool cleared = true;
      for (size_t i = 0; i < 512 * 1024; ++i) {
        cleared = cleared && delay_buffer_1[i] == 0;
      }
      CHECK(cleared);
    }
  }
  // The saved engine returns after a restart.
  settings.set_spectral(true);
  settings.SaveState();
  r.Reboot(false);
  CHECK(processor.spectral());
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 0;
  FlashProfile(factory_profile);
}

// Characterization of the transitions and option fan-out before they move
// out of beads.cc: exact block traces, codec call order and setter timing.

// Blocks: P full, D fading, Z held silent, B blanking, U rising, - paused.
// Codec calls: s start, R restart, x stop, p pause, r resume (* gain reset).
struct TransitionTrace {
  std::string blocks;
  int calls;
  int paused;
  bool silent;
};

TransitionTrace* transition_trace = NULL;
// A quarter second of 4 kHz blocks, as beads.cc's kBlockRate / 4.
const int kBlankingBlocks = 1000;

void ObserveCodec(Codec::Call call) {
  if (!transition_trace) {
    return;
  }
  extern volatile bool& codec_fade;
  extern float& reverb_style_gain;
  const char letters[] = "sRxpr";
  transition_trace->blocks += ' ';
  transition_trace->blocks += letters[call];
  ++transition_trace->calls;
  // What the pause and the restart frame: synth, engine, quality, mono.
  const int configuration = synth_mode + 2 * processor.spectral() +
      4 * processor.media() + 16 * processor.mono();
  if (call == Codec::CALL_PAUSE) {
    // Pauses only once faded: no block is cut at full level.
    CHECK(reverb_style_gain == 0.0f);
    transition_trace->paused = configuration;
  } else if (call == Codec::CALL_RESUME || call == Codec::CALL_RESTART) {
    // Reconfigured while paused, then blanking first.
    CHECK(configuration != transition_trace->paused);
    CHECK(codec_transition_frames == kBlankingBlocks);
    CHECK(!codec_fade);
    if (gain_control.gain_ == 1.0f && gain_control.peak_ == 0.5f &&
        gain_control.time_ == 0) {
      transition_trace->blocks += '*';
    }
  }
  transition_trace->blocks += ' ';
}

void TraceBlock(Rig* r) {
  extern float& reverb_style_gain;
  const float before = reverb_style_gain;
  const int blanking = codec_transition_frames;
  char letter = '-';
  if (codec.callback_) {
    r->Block();
    bool silent = true;
    for (size_t i = 0; i < codec.block_size_; ++i) {
      silent = silent && !r->out[i].l && !r->out[i].r;
    }
    const float after = reverb_style_gain;
    letter = blanking ? 'B' : (after < before ? 'D'
        : (after > before ? 'U' : (after == 0.0f ? 'Z' : 'P')));
    if (letter == 'B' || letter == 'Z') {
      transition_trace->silent = transition_trace->silent && silent;
    }
  }
  transition_trace->blocks += letter;
}

// Main loop, four callbacks and a SysTick per millisecond, as Rig::Tick.
void TraceTick(Rig* r, int milliseconds) {
  while (milliseconds--) {
    MainLoopStep();
    for (int i = 0; i < 4; ++i) {
      TraceBlock(r);
    }
    SysTick_Handler();
  }
}

// Runs of one block letter become "P12"; codec calls stay as they are.
std::string Compress(const std::string& blocks) {
  std::string out;
  size_t i = 0;
  while (i < blocks.size()) {
    if (blocks[i] == ' ') {
      ++i;
      continue;
    }
    size_t j = i + 1;
    if (strchr("PDZBU-", blocks[i])) {
      while (j < blocks.size() && blocks[j] == blocks[i]) {
        ++j;
      }
      char run[16];
      snprintf(run, sizeof(run), "%c%d", blocks[i], int(j - i));
      out += out.empty() ? "" : " ";
      out += run;
    } else {
      while (j < blocks.size() && blocks[j] == '*') {
        ++j;
      }
      out += out.empty() ? "" : " ";
      out += blocks.substr(i, j - i);
    }
    i = j;
  }
  return out;
}

void StartTrace(TransitionTrace* trace) {
  trace->blocks.clear();
  trace->calls = 0;
  trace->paused = -1;
  trace->silent = true;
  transition_trace = trace;
  codec.observer_ = &ObserveCodec;
}

std::string EndTrace(TransitionTrace* trace, const char* name) {
  transition_trace = NULL;
  codec.observer_ = NULL;
  const std::string compressed = Compress(trace->blocks);
  printf("Trace %s: %s\n", name, compressed.c_str());
  CHECK(trace->silent);
  return compressed;
}

// Without the full-level runs at either end, whose length follows the
// normalization probe's phase.
std::string Core(const std::string& trace) {
  size_t begin = trace[0] == 'P' ? trace.find(' ') + 1 : 0;
  size_t end = trace.rfind(' ');
  end = trace[end + 1] == 'P' ? end : trace.size();
  return trace.substr(begin, end - begin);
}

bool SameTrace(const std::string& trace, const char* expected) {
  if (trace != expected) {
    printf("  expected: %s\n", expected);
    return false;
  }
  return true;
}

// Settles until full level with no transition pending for 100 ms.
void Quiet(Rig* r) {
  extern volatile bool& codec_fade;
  extern float& reverb_style_gain;
  r->Settle();
  int quiet = 0;
  for (int i = 0; i < 5000 && quiet < 100; ++i) {
    r->Tick(1, true);
    quiet = reverb_style_gain == 1.0f && !codec_transition_frames &&
        !codec_fade ? quiet + 1 : 0;
  }
  CHECK(quiet == 100);
}

// Fades, codec pause and restart order, synth entry and exit, quality and
// rate changes, the reverb style and Processor changes, block by block.
void TestTransitionTraces() {
  test_checker.Start("TestTransitionTraces");
  extern bool& clouds_style;
  TransitionTrace t;

  // Quality at the same rate: fade, pause, resume, blanking, fade in.
  {
    Rig r(false, false);
    r.audio = 0x10000000;
    Quiet(&r);
    CHECK(processor.media() == Processor::MEDIA_DIGITAL);
    StartTrace(&t);
    settings.mutable_state()->media = Processor::MEDIA_CLEAN_TAPE;
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "quality"),
        "D41 Z3 p r B1000 U41 P115"));
    CHECK(processor.media() == Processor::MEDIA_CLEAN_TAPE);

    // Another rate restarts the converter.
    Quiet(&r);
    StartTrace(&t);
    settings.mutable_state()->media = Processor::MEDIA_VINTAGE_DIGITAL;
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "rate"),
        "D41 Z3 p R B1000 U41 P115"));
    CHECK(codec.sample_rate_ == 32000 && codec.block_size_ == 8);

    // Mono: the gain control restarts too.
    Quiet(&r);
    StartTrace(&t);
    r.right = false;
    TraceTick(&r, 600);
    CHECK(SameTrace(Core(EndTrace(&t, "mono")),
        "D41 Z3 p r* B1000 U41"));
    CHECK(processor.mono());

    // FREEZE during the fade postpones the change and fades back in.
    r.right = true;
    Quiet(&r);
    StartTrace(&t);
    settings.mutable_state()->media = Processor::MEDIA_DIGITAL;
    TraceTick(&r, 5);
    test_freeze_input = true;
    TraceTick(&r, 30);
    CHECK(SameTrace(EndTrace(&t, "frozen fade"),
        "D24 U24 P92"));
    CHECK(processor.media() == Processor::MEDIA_VINTAGE_DIGITAL);
    test_freeze_input = false;
    Quiet(&r);
    CHECK(processor.media() == Processor::MEDIA_DIGITAL);
  }

  // The reverb style switches in one silent block, with no codec call.
  for (int synth = 0; synth < 2; ++synth) {
    Rig r(false, synth != 0);
    r.audio = 0x10000000;
    if (synth) {
      r.StartAutomatic();
    }
    Quiet(&r);
    StartTrace(&t);
    settings.set_clouds_style(true);
    TraceTick(&r, 30);
    CHECK(SameTrace(EndTrace(&t, synth ? "synth style" : "style"),
        "P4 D41 Z1 U41 P33"));
    CHECK(clouds_style && t.calls == 0);
  }

  // Granular to spectral and back.
  {
    Rig r(false, false);
    r.audio = 0x10000000;
    Quiet(&r);
    StartTrace(&t);
    settings.set_spectral(true);
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "spectral"),
        "P4 D41 Z3 p r B1000 U41 P111"));
    CHECK(processor.spectral());
    Quiet(&r);
    StartTrace(&t);
    settings.set_spectral(false);
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "granular"),
        "P4 D41 Z3 p r B1000 U41 P111"));
    CHECK(!processor.spectral());
  }

  // A Processor change while frozen waits for FREEZE's release.
  {
    Rig r(false, false);
    r.audio = 0x10000000;
    Quiet(&r);
    ui.set_freeze_button_state(true);
    r.Tick(10, true);
    StartTrace(&t);
    settings.set_spectral(true);
    TraceTick(&r, 20);
    ui.set_freeze_button_state(false);
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "frozen engine"),
        "P80 D41 Z3 p r B1000 U41 P115"));
    CHECK(processor.spectral());
  }

  // Synth entry 10 s after the inputs empty, and exit when a cable returns.
  for (int plaits = 0; plaits < 2; ++plaits) {
    Rig r(false, true);
    settings.set_synth(plaits ? SYNTH_PLAITS : SYNTH_BEADS);
    ui.LoadOptions();
    Quiet(&r);
    CHECK(!synth_mode);
    synth_mode_start_time = 0;
    RunMainLoop();
    r.Advance(9990);
    StartTrace(&t);
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, plaits ? "plaits entry" : "synth entry"),
        "P40 D41 Z3 p r* B1000 U41 P75"));
    CHECK(synth_mode);
    Quiet(&r);
    PotsAdc::values_[POTS_ADC_CHANNEL_REVERB] = 0;
    StartTrace(&t);
    r.left = true;
    TraceTick(&r, 600);
    CHECK(SameTrace(Core(EndTrace(&t, plaits ? "plaits exit" : "synth exit")),
        "D41 Z3 p r* B1000 U41"));
    CHECK(!synth_mode);
  }

  // Synth entry and exit on request: a MEDIA hold each way.
  {
    Rig r(true, true);
    Quiet(&r);
    StartTrace(&t);
    r.HoldMedia(false);
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "requested entry"),
        "D41 Z3 p r* B1000 U41 P115"));
    CHECK(synth_mode);
    Quiet(&r);
    StartTrace(&t);
    r.HoldMedia(false);
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "requested exit"),
        "D41 Z3 p r* B1000 U41 P115"));
    CHECK(!synth_mode);
  }

  // Synth to spectral: the engine change ends synth play.
  {
    Rig r(false, true);
    r.StartAutomatic();
    Quiet(&r);
    StartTrace(&t);
    settings.set_spectral(true);
    TraceTick(&r, 300);
    CHECK(SameTrace(EndTrace(&t, "synth to spectral"),
        "P4 D41 Z3 p r* B1000 U41 P111"));
    CHECK(!synth_mode && processor.spectral());
  }
}

// The fade's level, block by block, on a dry signal.
void TestTransitionFadeLevels() {
  test_checker.Start("TestTransitionFadeLevels");
  extern float& reverb_style_gain;
  Rig r(false, false);
  r.audio = 0x20000000;
  r.Settle();
  r.Block();
  const double steady = fabs(double(r.out[codec.block_size_ - 1].l));
  CHECK(steady > 1.0e8);
  settings.mutable_state()->media = Processor::MEDIA_CLEAN_TAPE;
  MainLoopStep();
  int fading = 0;
  float gain = 1.0f;
  while (reverb_style_gain > 0.0f && fading < 100) {
    r.Block();
    ++fading;
    // The gain ramps across the block: one step in from the last block's.
    const double first = fabs(double(r.out[0].l));
    const double last = fabs(double(r.out[codec.block_size_ - 1].l));
    const float ramp = gain + (reverb_style_gain - gain) / codec.block_size_;
    CHECK(fabs(first - steady * ramp) < steady * 0.01);
    CHECK(fabs(last - steady * reverb_style_gain) < steady * 0.01);
    CHECK(reverb_style_gain == std::max(0.0f, gain - 0.025f));
    gain = reverb_style_gain;
  }
  // 1 - 40 * 0.025 in float is just above zero: one more step.
  CHECK(fading == 41);
  MainLoopStep();
  CHECK(codec_transition_frames == kBlankingBlocks);
  for (int i = 0; i < kBlankingBlocks; ++i) {
    r.Block();
  }
  int rising = 0;
  gain = 0.0f;
  while (reverb_style_gain < 1.0f && rising < 100) {
    r.Block();
    ++rising;
    const float next = std::min(1.0f, gain + 0.025f);
    const float ramp = gain + (next - gain) / codec.block_size_;
    const double first = fabs(double(r.out[0].l));
    const double last = fabs(double(r.out[codec.block_size_ - 1].l));
    CHECK(reverb_style_gain == next);
    CHECK(fabs(first - steady * ramp) < steady * 0.01);
    CHECK(fabs(last - steady * next) < steady * 0.01);
    gain = next;
  }
  CHECK(rising == 41);
}

// Each option's setter tick: main loop (frozen or blanking too), SysTick then
// the next callback, or power-on only.
void TestOptionFanOut() {
  test_checker.Start("TestOptionFanOut");
  Rig r(false, false);
  r.Settle();

  // Main loop: one pass applies them, even while frozen or blanking. SEED
  // latch and TIME scan come from the page, which SysTick publishes first.
  for (int pass = 0; pass < 3; ++pass) {
    if (pass == 1) {
      ui.set_freeze_button_state(true);
    } else if (pass == 2) {
      ui.set_freeze_button_state(false);
      codec_transition_frames = 10;
    }
    for (int value = 1; value >= 0; --value) {
      settings.SetFeatureValue(FEATURE_SEED_LATCH, value ? 2 : 0);
      settings.SetFeatureValue(FEATURE_ATTENUATION, value);
      settings.SetFeatureValue(FEATURE_TIME_SCAN, value);
      settings.SetFeatureValue(FEATURE_SEED_LATCH_FADE, value);
      settings.SetFeatureValue(FEATURE_PLAITS_BANK, value ? 2 : 0);
      CHECK(seed_latch_mode == (value ? 0 : 2));
      CHECK(gain_control.attenuation_ == !value);
      CHECK(bool(processor.options_ & Processor::OPTION_FLAG_TIME_SCAN) ==
          !value);
      CHECK((processor.delay_controller_.timing_gain_ !=
          DelayController::TIMING_FADE_OFF) == !value);
      CHECK(plaits_synth.first_model_ == (value ? 8 : 0));
      SysTick_Handler();
      CHECK(seed_latch_mode == (value ? 0 : 2));
      CHECK(bool(processor.options_ & Processor::OPTION_FLAG_TIME_SCAN) ==
          !value);
      MainLoopStep();
      CHECK(seed_latch_mode == (value ? 2 : 0));
      CHECK(gain_control.attenuation_ == bool(value));
      CHECK(bool(processor.options_ & Processor::OPTION_FLAG_TIME_SCAN) ==
          bool(value));
      CHECK((processor.delay_controller_.timing_gain_ !=
          DelayController::TIMING_FADE_OFF) == bool(value));
      // Bank 2 starts at model 0, bank 0 at model 8.
      CHECK(plaits_synth.first_model_ == (value ? 0 : 8));
    }
  }
  codec_transition_frames = 0;
  r.Settle();

  // Modulation display: only with the pitch notch or live assign shown.
  r.cv_patched[CV_ADC_CHANNEL_ASSIGN] = true;
  r.cv[CV_ADC_CHANNEL_ASSIGN] = 20000;
  r.Settle();
  ui.assign_modulation_ = -9.0f;
  MainLoopStep();
  CHECK(ui.assign_modulation_ == -9.0f);
  const ProfileFeature shown[] = { FEATURE_PITCH_NOTCH, FEATURE_LIVE_ASSIGN };
  for (int i = 0; i < 2; ++i) {
    settings.SetFeatureValue(shown[i], 1);
    ui.assign_modulation_ = -9.0f;
    MainLoopStep();
    CHECK(ui.assign_modulation_ == cv_reader.cv(CV_ADC_CHANNEL_ASSIGN));
    settings.SetFeatureValue(shown[i], 0);
  }
  r.cv_patched[CV_ADC_CHANNEL_ASSIGN] = false;
  r.Settle();

  // UI options: SysTick reads them, the next callback applies them.
  DelayGranularPlayer& player =
      processor.processor_digital_.granular_player_;
  settings.set_output_limiter_mode(OUTPUT_LIMITER_FAST);
  settings.set_pitch_scale(PitchScale(1));
  settings.set_random_intervals(RandomIntervals(1));
  settings.set_delay_mode(DELAY_MODE_NEVER);
  settings.set_evolving_feedback(true);
  settings.set_attack_mode(ATTACK_MODE_SNAP);
  settings.set_stereo_width(StereoWidth(2));
  settings.set_diffusion(0.5f);
  // Diffusion is read from the settings in the callback; the rest wait for
  // SysTick to publish them.
  r.Block();
  CHECK(processor.diffusion_ == settings.diffusion());
  processor.set_diffusion(0.0f);
  CHECK(processor.output_limiter_mode_ != OUTPUT_LIMITER_FAST);
  CHECK(player.pitch_scale_ == PITCH_SCALE_OFF && !player.random_intervals_);
  CHECK(processor.delay_mode_ == DELAY_MODE_AUTO);
  CHECK(!processor.evolving_feedback_ && !player.width_enabled_);
  CHECK(processor.attack_input()->mode() == ATTACK_MODE_OFF);
  SysTick_Handler();
  CHECK(processor.output_limiter_mode_ != OUTPUT_LIMITER_FAST);
  CHECK(player.pitch_scale_ == PITCH_SCALE_OFF && !player.random_intervals_);
  CHECK(processor.delay_mode_ == DELAY_MODE_AUTO);
  CHECK(!processor.evolving_feedback_ && !player.width_enabled_);
  CHECK(processor.attack_input()->mode() == ATTACK_MODE_OFF);
  CHECK(processor.diffusion_ == 0.0f);
  r.Block();
  CHECK(processor.output_limiter_mode_ == OUTPUT_LIMITER_FAST);
  CHECK(synth.output_limiter_mode_ == OUTPUT_LIMITER_FAST);
  CHECK(player.pitch_scale_ == PitchScale(1) && player.random_intervals_);
  CHECK(processor.delay_mode_ == DELAY_MODE_NEVER);
  CHECK(processor.evolving_feedback_ && player.width_enabled_);
  CHECK(processor.attack_input()->mode() == ATTACK_MODE_SNAP);
  CHECK(processor.diffusion_ == settings.diffusion());
  CHECK(processor.diffusion_ > 0.49f && processor.diffusion_ < 0.51f);

  // Blanking and held-silent blocks return early: nothing reaches the DSP.
  settings.set_pitch_scale(PitchScale(2));
  codec_transition_frames = 2;
  SysTick_Handler();
  r.Block();
  r.Block();
  CHECK(player.pitch_scale_ == PitchScale(1));
  r.Block();
  CHECK(player.pitch_scale_ == PitchScale(2));
  extern float& reverb_style_gain;
  settings.set_clouds_style(true);
  SysTick_Handler();
  for (int i = 0; i < 41; ++i) {
    r.Block();
  }
  CHECK(reverb_style_gain == 0.0f);
  settings.set_pitch_scale(PitchScale(1));
  SysTick_Handler();
  r.Block();  // Held silent: the style switches, nothing else is applied.
  CHECK(player.pitch_scale_ == PitchScale(2));
  r.Block();
  CHECK(player.pitch_scale_ == PitchScale(1));
  settings.set_clouds_style(false);
  r.Settle();

  // Resonator: SysTick previews the page; main publishes controls and bound.
  // Material and Ring time only apply while it is on.
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 3);
  SysTick_Handler();
  CHECK(!processor.resonator_controls_set_);
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_AFTER_GRAINS);
  MainLoopStep();
  CHECK(!(processor.options_ & Processor::OPTION_FLAG_RESONATOR_GRAINS));
  SysTick_Handler();
  CHECK(!processor.resonator_controls_set_);
  CHECK(!(processor.options_ & Processor::OPTION_FLAG_RESONATOR_GRAINS));
  MainLoopStep();
  CHECK(processor.resonator_controls_set_);
  CHECK(processor.resonator_controls_.structure ==
      kResonatorMaterials[3].structure);
  CHECK(processor.options_ & Processor::OPTION_FLAG_RESONATOR_GRAINS);
  settings.SetFeatureValue(FEATURE_RESONATOR_RING, 3);
  SysTick_Handler();
  MainLoopStep();
  CHECK(processor.resonator_controls_.damping == kResonatorRingTimes[3]);
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_OFF);
  SysTick_Handler();
  MainLoopStep();
  CHECK(!(processor.options_ & Processor::OPTION_FLAG_RESONATOR_GRAINS));
  r.Settle();

  // Power-on choices: runtime changes wait for the next power-on.
  const uint8_t boot[] = {
    FEATURE_PITCH_LAW, 1, FEATURE_MIX_LAW, 1, FEATURE_REVERB_INPUT, 1,
    FEATURE_GRAIN_STEALING, 1, FEATURE_UNITY_FEEDBACK, 1,
    FEATURE_DELAY_CROSSFADE, 1
  };
  const size_t count = sizeof(boot) / 2;
  for (size_t i = 0; i < count; ++i) {
    settings.SetFeatureValue(ProfileFeature(boot[2 * i]), boot[2 * i + 1]);
  }
  const uint8_t options = processor.options_;
  const PotChannel::Law law =
      cv_reader.pot_channel_[POTS_ADC_CHANNEL_PITCH].law_;
  const int threshold = player.simple_scheduler_threshold_;
  r.Settle();
  CHECK(processor.options_ == options);
  CHECK(cv_reader.pot_channel_[POTS_ADC_CHANNEL_PITCH].law_ == law);
  CHECK(player.simple_scheduler_threshold_ == threshold);
  BootWithProfile(&r, boot, count);
  CHECK(cv_reader.pot_channel_[POTS_ADC_CHANNEL_PITCH].law_ ==
      PotChannel::LAW_DETUNE_SMOOTH);
  CHECK(processor.options_ == (Processor::OPTION_FLAG_GENTLE_MIX |
      Processor::OPTION_FLAG_REVERB_ON_GRAINS |
      Processor::OPTION_FLAG_UNITY_FEEDBACK |
      Processor::OPTION_FLAG_DELAY_CROSSFADE));
  CHECK(player.simple_scheduler_threshold_ != threshold);
}

// Opens the options pages with [M] held 3 s, then taps to a page.
void OpenOptionsPage(Rig* r, int page) {
  test_down[SWITCH_ASSIGN] = true;
  r->Tick(3010, true);
  test_down[SWITCH_ASSIGN] = false;
  r->Tick(20, true);
  while (page--) {
    test_down[SWITCH_ASSIGN] = true;
    r->Tick(20, true);
    test_down[SWITCH_ASSIGN] = false;
    r->Tick(20, true);
  }
  CHECK(ui.options_.stage() == Options::STAGE_EDITING);
}

// SEED latch and TIME scan follow their page while it is open, as the other
// page settings do; the page's save then keeps them.
void TestPagePreviewSeedLatchAndTimeScan() {
  test_checker.Start("TestPagePreviewSeedLatchAndTimeScan");
  Rig r(false, false);
  r.Settle();
  CHECK(seed_latch_mode == 0);
  // Page 4, DRY/WET fully clockwise: Add.
  OpenOptionsPage(&r, 3);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 65535;
  r.Tick(100, true);
  CHECK(ui.options_.Value(FEATURE_SEED_LATCH) == 3);
  CHECK(settings.FeatureValue(FEATURE_SEED_LATCH) == 0);
  CHECK(seed_latch_mode == 3);
  r.Tick(1100, true);
  CHECK(settings.FeatureValue(FEATURE_SEED_LATCH) == 3);
  CHECK(seed_latch_mode == 3);

  // TIME scan on page 6, FEEDBACK, in a profile.
  uint8_t block[kProfileSize];
  const uint8_t scan[] = { FEATURE_TIME_SCAN, 0 };
  BuildProfile(block, scan, 1);
  uint8_t* entry = block + 16 + 6 * (block[5] - 1);
  CHECK(entry[0] == FEATURE_TIME_SCAN);
  entry[1] = PROFILE_ADJUSTABLE;
  entry[3] = 16;
  const size_t size = 16 + 6 * block[5];
  const uint32_t crc = Profile::Crc32(block, size);
  for (size_t i = 0; i < 4; ++i) {
    block[size + i] = crc >> (8 * i);
  }
  FlashProfile(block);
  PotsAdc::values_[POTS_ADC_CHANNEL_DRY_WET] = 0;
  r.Reboot(false);
  FlashProfile(factory_profile);
  r.Settle();
  CHECK(settings.profile().Page(16) == FEATURE_TIME_SCAN);
  CHECK(!(processor.options_ & Processor::OPTION_FLAG_TIME_SCAN));
  OpenOptionsPage(&r, 5);
  CHECK(ui.options_.page() == 5);
  PotsAdc::values_[POTS_ADC_CHANNEL_FEEDBACK] = 65535;
  r.Tick(100, true);
  CHECK(ui.options_.Value(FEATURE_TIME_SCAN) == 1);
  CHECK(settings.FeatureValue(FEATURE_TIME_SCAN) == 0);
  CHECK(processor.options_ & Processor::OPTION_FLAG_TIME_SCAN);
  r.Tick(1100, true);
  CHECK(settings.FeatureValue(FEATURE_TIME_SCAN) == 1);
  CHECK(processor.options_ & Processor::OPTION_FLAG_TIME_SCAN);
  PotsAdc::values_[POTS_ADC_CHANNEL_FEEDBACK] = 0;
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
  TestCustomScaleCallbackChain();
  TestCloudsSwitch();
  TestAutomaticSelfPlay();
  TestSynthFadeSurvivesTimerReset();
  TestSynthEntryCancelsBackup();
  TestRequestedSelfPlay();
  TestSelfPlayRequestLifecycle();
  TestMonoStereo();
  TestFrozenQualityLock();
  TestOverwriteIndicator();
  TestTriggerOutput();
  TestStartupBranches();
  TestStorageWarning();
  TestSuppressedStorageChord();
  TestWarningChordDebounceBoundary();
  TestStorageRepair();
  TestFailedBufferLoad();
  TestCallbackCvRouting();
  TestPitchChain();
  TestQuantizedPitchChain();
  TestRandomIntervalsChain();
  TestRecentNotesChain();
  TestSelfPlayFrozenBackup();
  TestPlaitsSynthSelection();
  TestRingsSynthOwnership();
  TestFreezeGestureFollowsPlayingSynth();
  TestSpectralEngine();
  TestCodecRestartFade();
  TestCodecRestartRetry();
  TestCodecKeepsClocks();
  TestSynthLevel();
  TestSynthStopClearsReverb();
  TestPitchNotchLaw();
  TestSeedLatchPatched();
  TestAttackTriggers();
  TestAttackTriggersPatched();
  TestSeedLatchFadeProfile();
  TestManualGainProfile();
  TestAttackSensitivityProfile();
  TestSeedStartProfile();
  TestPitchLawProfile();
  TestPlaitsBankProfile();
  TestMixOptionsProfile();
  TestSendMixProfile();
  TestOutputDivisionProfile();
  TestGrainStealingProfile();
  TestDelayOptionsProfile();
  TestResonatorProfile();
  TestResonatorPage();
  TestResonatorDampingCv();
  TestResonatorSeedCallback();
  TestHoldActionRestartsAutoGain();
  TestManualSynthStartNeedsGesture();
  TestSlowMediaGestures();
  TestEngineChangeWhileFrozen();
  TestProcessorToggle();
  TestCalibrationStepOnRelease();
  TestCalibrationExitResumesNormalization();
  TestRejectedCalibrationCue();
  TestUnfinishedCalibrationKept();
  TestLockedCalibrationCue();
  TestFactoryCalibrationRepeat();
  TestTransitionTraces();
  TestTransitionFadeLevels();
  TestOptionFanOut();
  TestPagePreviewSeedLatchAndTimeScan();
  return test_checker.failures() ? 1 : 0;
}
