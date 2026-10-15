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
// Guarded page, UI and STAT storage tests.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

#define private public
#include "beads/settings.h"
#include "beads/ui.h"
#undef private

#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/dsp/limiter.h"
#include "beads/gain_control.h"

bool test_down[4] = { };
unsigned test_writes = 0;
namespace stmlib {

SystemClock system_clock;

}  // namespace stmlib

namespace beads {

bool Leds::Init() {
  Clear();
  return true;
}
void Leds::Clear() { std::fill(intensity_, intensity_ + LED_LAST, 0); }
void Leds::Write() { }

}  // namespace beads

using namespace beads;

static_assert(sizeof(State) == 24, "STAT size");
static_assert(offsetof(State, media) == 0, "media");
static_assert(offsetof(State, assignable_cv_destination) == 1, "assignment");
static_assert(offsetof(State, assignable_cv_amount) == 2, "CV amounts");
static_assert(offsetof(State, gain) == 5, "gain");
static_assert(offsetof(State, gate_output_mode) == 6, "gate");
static_assert(offsetof(State, frozen) == 7, "frozen");
static_assert(offsetof(State, disable_autosave) == 8, "autosave");
static_assert(offsetof(State, monophonic_mode) == 9, "mono");
static_assert(offsetof(State, padding) == 10, "options");

struct Rig {
  Settings settings;
  GainControl gain;
  Calibrator calibration;
  BackgroundBufferBackup backup;
  Ui ui;
  float pot;
  float feedback;
  float reverb;

  Rig(bool startup_assign = false)
      : pot(0.5f), feedback(0.5f), reverb(0.5f) {
    std::fill(test_down, test_down + 4, false);
    test_down[SWITCH_ASSIGN] = startup_assign;
    memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
    settings.Init(false);
    ui.Init(&settings, &gain, &calibration, &backup);
    Tick(40);
  }
  void Tick(int n, bool events = true) {
    while (n--) {
      ui.ProcessPot(Ui::MIX_PARAMETER_FEEDBACK, feedback);
      ui.ProcessPot(Ui::MIX_PARAMETER_DRY_WET, pot);
      ui.ProcessPot(Ui::MIX_PARAMETER_REVERB, reverb);
      ui.Poll();
      if (events) {
        ui.DoEvents();
      }
    }
  }
  void Down(Switch s) {
    test_down[s] = true;
    Tick(8);
  }
  void Up(Switch s) {
    test_down[s] = false;
    Tick(8);
  }
  void Enter() {
    Down(SWITCH_ASSIGN);
    Tick(2999);
    CHECK(ui.options_.stage() == Options::WAIT_RELEASE);
    Up(SWITCH_ASSIGN);
    CHECK(ui.options_.stage() == Options::EDITING);
  }
  void Tap() {
    Down(SWITCH_ASSIGN);
    Up(SWITCH_ASSIGN);
  }
};

void TestUi() {
  test_checker.Start("TestUi");
  Rig r;
  unsigned writes = test_writes;
  r.Down(SWITCH_ASSIGN);
  r.Tick(2992);
  CHECK(!r.ui.options_.active());
  r.Tick(8);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.ui.options_.stage() == Options::EDITING);
  // Deliberate movement below qualification refreshes inactivity only.
  r.Tick(4000);
  r.pot = .54f;
  r.Tick(1500);
  CHECK(r.ui.options_.stage() == Options::EDITING);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.pot = .9f;
  r.Tick(10);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  ++writes;  // Preview must not leak into unrelated saves.
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000, false);
  CHECK(r.ui.options_.commit_requested());
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.ui.DoEvents();
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  CHECK(test_writes == writes + 1);
  r.Tick(30);
  r.Enter();
  r.pot = .1f;
  r.Tick(10);
  r.Tick(5000);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  CHECK(test_writes == writes + 2);  // Timeout saves a changed value.
  r.Tick(30);
  r.Enter();
  r.Tick(5010);
  CHECK(!r.ui.options_.active());
  CHECK(test_writes == writes + 2);  // Timeout without a change writes nothing.
  r.Tick(30);
  r.Enter();
  CHECK(r.ui.options_.page() == kLimiterPage);  // Opens on dry/wet.
  r.Tap();
  CHECK(r.ui.options_.page() == 2);
  r.pot = .9f;
  r.Tick(10);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);  // Reserved.
  r.Tap();
  r.Tap();
  CHECK(r.ui.options_.page() == kLimiterPage);
  r.pot = .5f;
  r.Tick(10);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_FAST);
  r.Down(SWITCH_MEDIA);
  CHECK(r.calibration.starts == 0);  // Active page consumes stock chords.
  Rig startup(true);
  startup.Tick(3500);
  CHECK(!startup.ui.options_.active());
  Rig chord;
  chord.Down(SWITCH_ASSIGN);
  chord.Down(SWITCH_MEDIA);
  CHECK(chord.calibration.starts == 1);
  puts("gesture/preview/commit: PASS");
}

static Rig* interrupted_rig;
static bool interrupt_session;
namespace beads {

void TestOptionsDispatchInterrupt() {
  if (interrupted_rig) {
    Rig* r = interrupted_rig;
    interrupted_rig = NULL;
    if (interrupt_session) {
      test_down[SWITCH_ASSIGN] = true;
      r->Tick(3010, false);
      CHECK(r->ui.options_.active());
      test_down[SWITCH_ASSIGN] = false;
      r->Tick(5010, false);
    } else {
      r->Tick(1, false);  // After flush check, before options_.active().
    }
  }
}

}  // namespace beads

void TestQueueRace() {
  test_checker.Start("TestQueueRace");
  Rig r;
  r.Enter();
  r.Down(SWITCH_MEDIA);  // Cancellation enters DRAIN with MEDIA down.
  CHECK(r.ui.options_.stage() == Options::DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(7, false);  // Debounce 0x7f: release; next tick 0xff exits DRAIN.
  CHECK(r.ui.options_.stage() == Options::DRAIN);
  uint8_t media = r.settings.state().media;
  unsigned writes = test_writes;
  interrupted_rig = &r;
  r.ui.DoEvents();
  if (interrupted_rig) { // Fixed ISR did not queue a release to dispatch.
    interrupted_rig = NULL;
    r.Tick(1, false);
  }
  printf("DRAIN release interleaving: media %u -> %u, saves %u\n",
      media, r.settings.state().media, test_writes - writes);
  fflush(stdout);
  CHECK(!r.ui.options_.active());
  CHECK(r.ui.queue_.available() == 0);
  CHECK(r.settings.state().media == media && test_writes == writes);

  // Retain the read-pointer regression: interrupt between available and pull.
  r.ui.options_.stage_ = Options::DRAIN;
  r.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 1);
  CHECK(r.ui.queue_.available() == 1);
  r.Tick(1, false);
  r.ui.queue_.PullEvent();
  // An ISR Flush would create 15 phantom events.
  CHECK(r.ui.queue_.available() == 0);

  // Pre-entry events remain pending through an entire session without DoEvents.
  Rig delayed;
  delayed.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 1);
  test_down[SWITCH_ASSIGN] = true;
  delayed.Tick(3010, false);
  CHECK(delayed.ui.options_.active());
  test_down[SWITCH_ASSIGN] = false;
  delayed.Tick(5010, false);  // Release, inactivity cancellation, drain exit.
  CHECK(!delayed.ui.options_.active());
  media = delayed.settings.state().media;
  writes = test_writes;
  delayed.ui.DoEvents();
  CHECK(delayed.ui.queue_.available() == 0);
  CHECK(delayed.settings.state().media == media && test_writes == writes);

  delayed.Tick(30);  // Re-arm after the preceding drain exit.
  // An event already pulled when entry/exit preempts main is rejected too.
  delayed.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 1);
  interrupt_session = true;
  interrupted_rig = &delayed;
  delayed.ui.DoEvents();
  interrupt_session = false;
  CHECK(!interrupted_rig && !delayed.ui.options_.active());
  CHECK(delayed.settings.state().media == media && test_writes == writes);
  delayed.Down(SWITCH_MEDIA);
  delayed.Up(SWITCH_MEDIA);
  CHECK(delayed.settings.state().media != media && test_writes == writes + 1);
  puts("queue interrupt/pre-entry/normal dispatch: PASS");
}

void TestStorage() {
  test_checker.Start("TestStorage");
  Settings s;
  memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
  CHECK(!s.Init(false));
  CHECK(s.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings old_record;
  CHECK(old_record.Init(false));
  CHECK(old_record.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  for (int i = 0; i < 256; ++i) {
    s.mutable_state()->padding[0] = i;
    CHECK(s.output_limiter_mode() == (i == 0xa1 ? OUTPUT_LIMITER_FAST :
        i == 0xa2 ? OUTPUT_LIMITER_OFF : OUTPUT_LIMITER_STOCK));
  }
  for (int i = 0; i < 3; ++i) {
    s.set_output_limiter_mode(OutputLimiterMode(i));
    s.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.output_limiter_mode() == i);
    CHECK(loaded.state().media == 3);
  }
  // The legacy layout reads the new record with the same tag and size.
  State legacy_state = { };
  PersistentData legacy_calibration = { };
  stmlib::ChunkStorage<15, PersistentData, State> legacy;
  CHECK(legacy.Init(&legacy_calibration, &legacy_state));
  CHECK(legacy_state.media == 3 && legacy_state.padding[0] == 0xa2);
  memset(legacy_state.padding, 0, sizeof(legacy_state.padding));
  legacy.SaveState();
  Settings downgrade;
  CHECK(downgrade.Init(false));
  CHECK(downgrade.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  puts("STAT persistence/compatibility: PASS");
}

// Frozen released limiter arithmetic, independent of ProcessOutput dispatch.
struct StockLimiter {
  float peak = .5f, lp = .5f, decay;
  explicit StockLimiter(float d) : decay(d) { }
  void Process(FloatFrame* x) {
    float p = std::min(std::max(fabsf(x->l), fabsf(x->r)), 40.0f);
    SLOPE(peak, p, .1f, decay);
    SLOPE(lp, peak, .5f, decay);
    float gain = lp <= .9f ? 1.0f : .9f / lp;
    x->l *= gain;
    x->r *= gain;
  }
};
void TestDsp() {
  test_checker.Start("TestDsp");
  for (float sr : {24000.0f, 32000.0f, 48000.0f}) {
    Limiter stock, fast, off;
    float decay = 1.0f / (sr * 6.0f);
    stock.Init(decay, .9f);
    fast.Init(decay, .9f);
    off.Init(decay, .9f);
    StockLimiter reference(decay);
    for (int i = 0; i < 4096; ++i) {
      FloatFrame x = {sinf(i * .17f) * 4.0f, cosf(i * .13f) * 2.0f};
      FloatFrame y = x, z = x;
      stock.ProcessOutput(&y, 1, OUTPUT_LIMITER_STOCK, sr);
      reference.Process(&z);
      CHECK(!memcmp(&y, &z, sizeof(y)));
      off.ProcessOutput(&x, 1, OUTPUT_LIMITER_OFF, sr);
      CHECK(x.l == sinf(i * .17f) * 4.0f && x.r == cosf(i * .13f) * 2.0f);
      fast.ProcessOutput(&x, 1, OUTPUT_LIMITER_FAST, sr);
      CHECK(std::isfinite(x.l) && std::isfinite(x.r));
    }
    for (int i = 0; i < int(sr); ++i) {
      FloatFrame x = {.1f, -.1f}, y = x;
      stock.ProcessOutput(&x, 1, OUTPUT_LIMITER_STOCK, sr);
      fast.ProcessOutput(&y, 1, OUTPUT_LIMITER_FAST, sr);
      if (i == int(sr) - 1) {
        CHECK(y.l > x.l && y.l >= .099f);
      }
    }
  }
  puts("DSP mode dispatch: PASS");
}
void TestStockAssignRouting() {
  test_checker.Start("TestStockAssignRouting");
  Rig r;
  r.settings.mutable_state()->assignable_cv_destination = 0;
  for (int i = 1; i <= 6; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == i % 3);
  }
  r.Tick(30);
  r.Enter();
  for (int i = 0; i < 4; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
}

void TestStockAssignAmounts() {
  test_checker.Start("TestStockAssignAmounts");
  for (int i = 0; i < 3; ++i) {
    Rig r;
    const State before = r.settings.state();
    unsigned writes = test_writes;
    r.Down(SWITCH_ASSIGN);
    if (i == 0) {
      r.feedback = 0.25f;
    }
    if (i == 1) {
      r.pot = 0.75f;
    }
    if (i == 2) {
      r.reverb = 0.125f;
    }
    r.Tick(3100);
    CHECK(!r.ui.options_.active());
    const int expected[] = { 64, 192, 32 };
    CHECK(r.settings.state().assignable_cv_amount[i] == expected[i]);
    CHECK(r.settings.state().assignable_cv_destination == 3);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + 1);
    Settings loaded;
    CHECK(loaded.Init(false));
    for (int j = 0; j < 3; ++j) {
      CHECK(loaded.state().assignable_cv_amount[j] ==
          (i == j ? expected[i] : before.assignable_cv_amount[j]));
    }
    CHECK(loaded.state().assignable_cv_destination == 3);
    CHECK(!r.ui.options_.active());
  }
}

void TestAutosaveChordAndDisplay() {
  test_checker.Start("TestAutosaveChordAndDisplay");
  Rig r;
  for (int i = 0; i < 2; ++i) {
    unsigned writes = test_writes;
    bool before = r.settings.state().disable_autosave;
    r.Down(SWITCH_ASSIGN);
    r.Down(SWITCH_FREEZE);
    r.Tick(200);
    CHECK(r.settings.state().disable_autosave == !before);
    CHECK(test_writes == writes + 1);
    CHECK(r.ui.leds_.intensity_[LED_FREEZE] == (!before ? 16 : 64));
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().disable_autosave == !before);
    r.Tick(4300);
    CHECK(r.settings.state().disable_autosave == !before);
    CHECK(test_writes == writes + 1);
    r.Up(SWITCH_FREEZE);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + 1);
    CHECK(!r.ui.freeze_button_state_);
    r.Tick(1100);
  }
}

void TestSeedLatchGestures() {
  test_checker.Start("TestSeedLatchGestures");
  Rig r;
  r.Down(SWITCH_SEED);
  uint32_t start = r.ui.press_time_[SWITCH_SEED];
  while (stmlib::system_clock.milliseconds() - start < 3999) {
    r.Tick(1);
  }
  CHECK(!r.ui.seed_button_locked_);
  r.Tick(1);
  CHECK(r.ui.seed_button_locked_ && !r.ui.seed_button_state_);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.seed_button_locked_);
  r.Down(SWITCH_SEED);
  r.Up(SWITCH_SEED);
  CHECK(!r.ui.seed_button_locked_ && !r.ui.seed_button_state_);
  r.Down(SWITCH_SEED);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.seed_button_locked_ && !r.ui.seed_button_state_);
  CHECK(r.ui.ignore_release_[SWITCH_SEED]);
  CHECK(r.ui.ignore_release_[SWITCH_FREEZE]);
  r.Up(SWITCH_SEED);
  r.Up(SWITCH_FREEZE);
  CHECK(r.ui.seed_button_locked_ && !r.ui.freeze_button_state_);
}

void TestGainHoldThresholdAndReload() {
  test_checker.Start("TestGainHoldThresholdAndReload");
  Rig r;
  uint8_t media = r.settings.state().media;
  r.Down(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  r.settings.mutable_state()->gain = 123;
  r.settings.SaveState();
  unsigned writes = test_writes;
  media = r.settings.state().media;
  r.Down(SWITCH_MEDIA);
  uint32_t start = r.ui.press_time_[SWITCH_MEDIA];
  while (stmlib::system_clock.milliseconds() - start < 1499) {
    r.Tick(1);
  }
  CHECK(r.settings.state().gain == 123 && test_writes == writes);
  r.Tick(1);
  CHECK(r.settings.state().gain == 0 && test_writes == writes + 1);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media && test_writes == writes + 1);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.state().gain == 0 && loaded.state().media == media);
}

void TestBackupLedAndRestore() {
  test_checker.Start("TestBackupLedAndRestore");
  Rig r;
  r.ui.freeze_led_state_ = true;
  r.backup.busy_ = true;
  r.Tick(1);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 128);
  r.backup.busy_ = false;
  r.Tick(1);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 255);
  r.Down(SWITCH_ASSIGN);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 16);
  r.Up(SWITCH_FREEZE);
  r.Up(SWITCH_ASSIGN);
  r.Tick(1100);
  CHECK(r.ui.mode_ == Ui::MODE_NORMAL);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 255);
  const State& state = r.settings.state();
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 +
      state.assignable_cv_destination] == 128);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
      r.ui.media_palette_[state.media].r);
}

void TestLimiterLinkedGainAndModeSwitch() {
  test_checker.Start("TestLimiterLinkedGainAndModeSwitch");
  const OutputLimiterMode modes[] = {
    OUTPUT_LIMITER_STOCK, OUTPUT_LIMITER_FAST
  };
  for (int m = 0; m < 2; ++m) {
    Limiter limiter;
    Limiter fresh;
    limiter.Init(1.0f / (48000.0f * 6.0f), 0.9f);
    fresh.Init(1.0f / (48000.0f * 6.0f), 0.9f);
    FloatFrame block[32];
    for (int n = 0; n < 10; ++n) {
      for (int i = 0; i < 32; ++i) {
        block[i].l = 20.0f;
        block[i].r = -5.0f;
      }
      limiter.ProcessOutput(block, 32, modes[m], 48000.0f);
    }
    CHECK(fabsf(block[31].l) < 1.0f);
    for (int n = 0; n < 4; ++n) {
      for (int i = 0; i < 32; ++i) {
        block[i].l = 0.1f;
        block[i].r = -0.025f;
      }
      limiter.ProcessOutput(block, 32, OUTPUT_LIMITER_OFF, 48000.0f);
    }
    for (int n = 0; n < 4; ++n) {
      FloatFrame expected[32];
      for (int i = 0; i < 32; ++i) {
        block[i].l = expected[i].l = 0.1f;
        block[i].r = expected[i].r = -0.025f;
      }
      limiter.ProcessOutput(block, 32, modes[m], 48000.0f);
      fresh.ProcessOutput(expected, 32, modes[m], 48000.0f);
      for (int i = 0; i < 32; ++i) {
        CHECK(block[i].l == expected[i].l);
        CHECK(block[i].r == expected[i].r);
      }
    }
  }
  Limiter limiter;
  limiter.Init(1.0f / (48000.0f * 6.0f), 0.9f);
  for (int i = 0; i < 3000; ++i) {
    OutputLimiterMode mode = OutputLimiterMode((i / 100) % 3);
    float input = i % 2 ? 1.0e20f : -1.0e20f;
    FloatFrame x = { input, -input * 0.25f };
    limiter.ProcessOutput(&x, 1, mode, 48000.0f);
    CHECK(std::isfinite(x.l) && std::isfinite(x.r));
    // The detector cap does not bound the output to the audio range.
    CHECK(fabsf(x.l) > 1.0f && fabsf(x.l) <= fabsf(input));
    CHECK(x.r == -x.l * 0.25f);
    if (mode == OUTPUT_LIMITER_OFF) {
      CHECK(x.l == input);
    }
    FloatFrame small = { 0.1f, -0.025f };
    limiter.ProcessOutput(&small, 1, mode, 48000.0f);
    CHECK(std::isfinite(small.l) && std::isfinite(small.r));
    CHECK(fabsf(small.l) <= 0.1f && small.r == -small.l * 0.25f);
    if (mode == OUTPUT_LIMITER_OFF) {
      CHECK(small.l == 0.1f);
    }
  }
}

// Panel quality, gain and assignment displays.
void TestQualityTapCycle() {
  test_checker.Start("TestQualityTapCycle");
  Rig r;
  const int media[] = { 2, 1, 0, 3 };
  unsigned writes = test_writes;
  for (int i = 0; i < 4; ++i) {
    r.Down(SWITCH_MEDIA);
    CHECK(r.settings.state().media == (i == 0 ? 3 : media[i - 1]));
    r.Up(SWITCH_MEDIA);
    CHECK(r.settings.state().media == media[i]);
    CHECK(test_writes == writes + i + 1);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().media == media[i]);
  }
}

void TestManualGainGestureAndMeter() {
  test_checker.Start("TestManualGainGestureAndMeter");
  Rig r;
  const float positions[] = { 0.125f, 0.375f, 0.75f };
  const int gains[] = { 32, 96, 192 };
  const int red[] = { 0, 160, 255 };
  const int green[] = { 172, 255, 121 };
  const uint8_t media = r.settings.state().media;
  unsigned writes = test_writes;
  r.Down(SWITCH_MEDIA);
  for (int i = 0; i < 3; ++i) {
    r.feedback = positions[i];
    r.Tick(20);
    CHECK(r.settings.state().gain == gains[i]);
    CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_FEEDBACK) == 0.5f);
    CHECK(r.settings.state().media == media);
    CHECK(test_writes == writes);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == red[i]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == green[i]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  }
  r.Tick(1700);
  CHECK(r.settings.state().gain == 192 && test_writes == writes);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  CHECK(test_writes == writes + 1);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == 192);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == 192);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 208);
}

void TestSeedStartupLatch() {
  test_checker.Start("TestSeedStartupLatch");
  Rig r;
  CHECK(r.ui.seed_button_locked());
  CHECK(!r.ui.seed_button_state());
  CHECK(!r.ui.freeze_button_state());
}

void TestAssignableAmountLeds() {
  test_checker.Start("TestAssignableAmountLeds");
  Rig r;
  State* state = r.settings.mutable_state();
  state->assignable_cv_destination = 3;
  const uint8_t amounts[][3] = {
    { 0, 1, 255 }, { 255, 128, 64 }, { 32, 254, 129 }
  };
  for (int j = 0; j < 3; ++j) {
    memcpy(state->assignable_cv_amount, amounts[j], 3);
    r.Tick(1);
    for (int i = 0; i < 3; ++i) {
      CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + i] == amounts[j][i] / 2);
    }
  }
  for (int j = 0; j < 3; ++j) {
    state->assignable_cv_destination = j;
    r.Tick(1);
    for (int i = 0; i < 3; ++i) {
      CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + i] == (i == j ? 128 : 0));
    }
  }
}

void TestAgcAdjustmentBlink() {
  test_checker.Start("TestAgcAdjustmentBlink");
  Rig r;
  r.gain.adjusting_ = true;
  for (int i = 0; i < 256; ++i) {
    r.Tick(1);
    CHECK(r.ui.leds_.intensity_[LED_VU_METER] ==
        ((stmlib::system_clock.milliseconds() & 128) ? 128 : 0));
  }
  r.gain.adjusting_ = false;
  r.gain.meter_ = 73;
  r.Tick(1);
  CHECK(r.ui.leds_.intensity_[LED_VU_METER] == 73);
}

void TestRecoveryPreservesCalibration() {
  test_checker.Start("TestRecoveryPreservesCalibration");
  Rig r;
  const State defaults = r.settings.state();
  PersistentData* data = r.settings.mutable_persistent_data();
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    data->adc_calibration[i].scale = -3.0f - i;
    data->adc_calibration[i].offset = 0.25f + i;
  }
  r.settings.SavePersistentData();
  const PersistentData calibration = r.settings.persistent_data();
  memset(r.settings.mutable_state(), 1, sizeof(State));
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  Settings recovered;
  CHECK(recovered.Init(true));
  CHECK(!memcmp(&recovered.state(), &defaults, sizeof(State)));
  CHECK(!memcmp(&recovered.persistent_data(), &calibration,
      sizeof(PersistentData)));
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(!memcmp(&loaded.state(), &defaults, sizeof(State)));
  CHECK(!memcmp(&loaded.persistent_data(), &calibration,
      sizeof(PersistentData)));
}

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x081e0000), 0x20000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x081e0000))) {
    return 1;
  }
  TestQualityTapCycle();
  TestManualGainGestureAndMeter();
  TestSeedStartupLatch();
  TestAssignableAmountLeds();
  TestAgcAdjustmentBlink();
  TestRecoveryPreservesCalibration();
  TestStockAssignRouting();
  TestStockAssignAmounts();
  TestAutosaveChordAndDisplay();
  TestSeedLatchGestures();
  TestGainHoldThresholdAndReload();
  TestBackupLedAndRestore();
  TestLimiterLinkedGainAndModeSwitch();
  TestQueueRace();
  TestStorage();
  TestUi();
  TestDsp();
  return test_checker.failures() ? 1 : 0;
}
