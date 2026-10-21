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
#include "beads/normalization_detector.h"
#undef private

#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/dsp/limiter.h"
#include "beads/gain_control.h"

// Keep the detector in this harness without changing its build recipe.
#include "beads/normalization_detector.cc"

bool test_down[4] = { };
unsigned test_writes = 0;
#include "beads/test/firmware_shims.h"

using namespace beads;

STATIC_ASSERT(sizeof(State) == 24, stat_size);
STATIC_ASSERT(offsetof(State, media) == 0, media);
STATIC_ASSERT(offsetof(State, assignable_cv_destination) == 1, assignment);
STATIC_ASSERT(offsetof(State, assignable_cv_amount) == 2, cv_amounts);
STATIC_ASSERT(offsetof(State, gain) == 5, gain);
STATIC_ASSERT(offsetof(State, gate_output_mode) == 6, gate);
STATIC_ASSERT(offsetof(State, frozen) == 7, frozen);
STATIC_ASSERT(offsetof(State, disable_autosave) == 8, autosave);
STATIC_ASSERT(offsetof(State, monophonic_mode) == 9, mono);
namespace state_layout_test {
STATIC_ASSERT(offsetof(State, diffusion) == 17, diffusion);
STATIC_ASSERT(offsetof(State, padding) == 18, options);
}

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
    CHECK(ui.options_.stage() == Options::STAGE_WAIT_RELEASE);
    Up(SWITCH_ASSIGN);
    CHECK(ui.options_.stage() == Options::STAGE_EDITING);
  }
  void EnterLimiter() {
    Enter();
    Tap();
    Tap();
  }
  void Tap() {
    Down(SWITCH_ASSIGN);
    Up(SWITCH_ASSIGN);
  }
};

void TestTrimGesture() {
  test_checker.Start("TestTrimGesture");
  for (int order = 0; order < 2; ++order) {
    Rig r;
    r.Down(SWITCH_FREEZE);
    r.Up(SWITCH_FREEZE);
    CHECK(r.ui.freeze_button_state_);
    const uint8_t media = r.settings.state().media;
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.ui.trim_flag_);
    CHECK(!r.ui.freeze_button_state_);
    CHECK(r.ui.ignore_release_[SWITCH_MEDIA]);
    CHECK(r.ui.ignore_release_[SWITCH_FREEZE]);
    r.ui.ClearTrimFlag();
    CHECK(!r.ui.trim_flag_);
    r.Up(order ? SWITCH_MEDIA : SWITCH_FREEZE);
    r.Up(order ? SWITCH_FREEZE : SWITCH_MEDIA);
    CHECK(!r.ui.freeze_button_state_);
    CHECK(r.settings.state().media == media);
  }
  Rig reverse;
  const uint8_t media = reverse.settings.state().media;
  reverse.Down(SWITCH_FREEZE);
  reverse.Down(SWITCH_MEDIA);
  CHECK(!reverse.ui.trim_flag_);
  reverse.Up(SWITCH_FREEZE);
  reverse.Up(SWITCH_MEDIA);
  CHECK(reverse.ui.freeze_button_state_);
  CHECK(reverse.settings.state().media == (media + 3) % 4);
  Rig page;
  page.EnterLimiter();
  const uint8_t page_media = page.settings.state().media;
  test_down[SWITCH_MEDIA] = true;
  page.Tick(8, false);
  CHECK(!page.ui.queue_.available());
  test_down[SWITCH_FREEZE] = true;
  page.Tick(8, false);
  CHECK(!page.ui.queue_.available());
  page.ui.DoEvents();
  page.Up(SWITCH_FREEZE);
  page.Up(SWITCH_MEDIA);
  CHECK(!page.ui.trim_flag_);
  CHECK(!page.ui.freeze_button_state_);
  CHECK(page.settings.state().media == page_media);
}

void TestMonophonicGesture() {
  test_checker.Start("TestMonophonicGesture");
  for (int latched = 0; latched < 2; ++latched) {
    for (int order = 0; order < 2; ++order) {
      Rig r;
      CHECK(!r.settings.state().monophonic_mode);
      if (!latched) {
        r.Down(SWITCH_SEED);
        r.Up(SWITCH_SEED);
      }
      const bool seed = r.ui.seed_button_state_;
      const uint8_t media = r.settings.state().media;
      unsigned writes = test_writes;
      for (int toggle = 0; toggle < 2; ++toggle) {
        r.Down(SWITCH_MEDIA);
        r.Down(SWITCH_SEED);
        CHECK(r.settings.state().monophonic_mode == !toggle);
        CHECK(test_writes == ++writes);
        r.Tick(1600);
        r.Up(order ? SWITCH_MEDIA : SWITCH_SEED);
        r.Up(order ? SWITCH_SEED : SWITCH_MEDIA);
        CHECK(test_writes == writes);
        CHECK(r.settings.state().media == media);
        CHECK(r.gain.restarts == 0);
        CHECK(r.ui.seed_button_locked_ == bool(latched));
        CHECK(r.ui.seed_button_state_ == seed);
        Settings loaded;
        CHECK(loaded.Init(false));
        CHECK(loaded.state().monophonic_mode == !toggle);
        r.ui.set_overwrite_reserve(1.0f);
        r.Tick(1);
        Leds::Color color = r.ui.media_palette_[media];
        const float brightness = toggle ? 1.0f : 0.5f;
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
            uint8_t(color.r * brightness * brightness));
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] ==
            uint8_t(color.g * brightness));
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] ==
            uint8_t(color.b * brightness));
      }
    }
  }
  Rig reverse;
  const uint8_t media = reverse.settings.state().media;
  reverse.Down(SWITCH_SEED);
  CHECK(!reverse.ui.seed_button_locked_);
  CHECK(reverse.ui.seed_button_state_);
  reverse.Down(SWITCH_MEDIA);
  reverse.Up(SWITCH_SEED);
  reverse.Up(SWITCH_MEDIA);
  CHECK(!reverse.settings.state().monophonic_mode);
  CHECK(!reverse.ui.seed_button_state_);
  CHECK(reverse.settings.state().media == (media + 3) % 4);
  Rig page;
  page.EnterLimiter();
  page.Down(SWITCH_MEDIA);
  page.Down(SWITCH_SEED);
  page.Up(SWITCH_SEED);
  page.Up(SWITCH_MEDIA);
  CHECK(!page.settings.state().monophonic_mode);
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
      r->Tick(10, false);
      test_down[SWITCH_MEDIA] = true;
      r->Tick(8, false);
      test_down[SWITCH_MEDIA] = false;
      r->Tick(8, false);
    } else {
      r->Tick(1, false);  // After flush check, before options_.active().
    }
  }
}

}  // namespace beads

void TestQueueRace() {
  test_checker.Start("TestQueueRace");
  Rig r;
  r.EnterLimiter();
  r.Down(SWITCH_MEDIA);  // Cancellation enters DRAIN with MEDIA down.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(7, false);  // Debounce 0x7f: release; next tick 0xff exits DRAIN.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
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
  r.ui.options_.stage_ = Options::STAGE_DRAIN;
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
  delayed.Tick(10, false);
  test_down[SWITCH_MEDIA] = true;
  delayed.Tick(8, false);
  test_down[SWITCH_MEDIA] = false;
  delayed.Tick(8, false);
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

struct LegacyState {
  uint8_t media;
  uint8_t assignable_cv_destination;
  uint8_t assignable_cv_amount[3];
  uint8_t gain;  // 0 for auto-gain
  uint8_t gate_output_mode;
  uint8_t frozen;
  uint8_t disable_autosave;
  uint8_t monophonic_mode;
  uint8_t padding[14];
  enum { tag = 0x54415453 };  // STAT
};

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
    s.mutable_state()->output_limiter_mode = i;
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
  LegacyState legacy_state = { };
  PersistentData legacy_calibration = { };
  stmlib::ChunkStorage<15, PersistentData, LegacyState> legacy;
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
  float peak, lp, decay;
  explicit StockLimiter(float d) : peak(.5f), lp(.5f), decay(d) { }
  void Process(FloatFrame* x) {
    const float p = std::min(std::max(fabsf(x->l), fabsf(x->r)), 40.0f);
    SLOPE(peak, p, .1f, decay);
    SLOPE(lp, peak, .5f, decay);
    const float gain = lp <= .9f ? 1.0f : .9f / lp;
    x->l *= gain;
    x->r *= gain;
  }
};
void TestDsp() {
  test_checker.Start("TestDsp");
  const float sample_rates[] = { 24000.0f, 32000.0f, 48000.0f };
  for (size_t j = 0; j < 3; ++j) {
    const float sr = sample_rates[j];
    Limiter stock, fast, off;
    const float decay = 1.0f / (sr * 6.0f);
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
  r.EnterLimiter();
  for (int i = 0; i < 4; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
}

void TestStockAssignAmounts() {
  test_checker.Start("TestStockAssignAmounts");
  for (int i = 0; i < 3; ++i) {
    Rig r;
    r.settings.set_stereo_width(128);
    r.settings.set_width_assigned(true);
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
    CHECK(!loaded.width_assigned());
    CHECK(!r.ui.options_.active());
  }
}

void TestAutosaveChordAndDisplay() {
  test_checker.Start("TestAutosaveChordAndDisplay");
  Rig r;
  for (int i = 0; i < 2; ++i) {
    unsigned writes = test_writes;
    const bool before = r.settings.state().disable_autosave;
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
    const OutputLimiterMode mode = OutputLimiterMode((i / 100) % 3);
    const float input = i % 2 ? 1.0e20f : -1.0e20f;
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

void TestAudioPatchDetection() {
  test_checker.Start("TestAudioPatchDetection");
  NormalizationDetector detector;
  detector.Init(2);
  uint16_t cv[CV_ADC_CHANNEL_LAST] = { };
  for (int patch = 0; patch < 4; ++patch) {
    for (int i = 0; i < 1400; ++i) {
      bool probe = detector.rng_state_ >> 31;
      Codec::Frame input;
      input.l = (patch & 1 ? !probe : probe) ? 1 : -1;
      input.r = (patch & 2 ? !probe : probe) ? 1 : -1;
      detector.Process(Codec::EVENT_NONE, input, cv);
    }
    CHECK(detector.normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT)
        == !(patch & 1));
    CHECK(detector.normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT)
        == !(patch & 2));
    CHECK(detector.mono() == (patch != 3));
    CHECK(detector.no_input() == (patch == 0));
  }
}

void TestFrozenStateChangeGuard() {
  test_checker.Start("TestFrozenStateChangeGuard");
  Rig r;
  CHECK(!r.ui.should_ignore_state_change());
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(r.ui.freeze_button_state());
  r.ui.set_freeze_led_state(true);
  CHECK(r.ui.should_ignore_state_change());
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == 2);
  CHECK(r.ui.should_ignore_state_change());
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  r.ui.set_freeze_led_state(false);
  CHECK(!r.ui.should_ignore_state_change());
  // The callback also merges the external FREEZE gate into this flag.
  r.ui.set_freeze_led_state(true);
  CHECK(r.ui.should_ignore_state_change());
  r.ui.set_freeze_led_state(false);
  CHECK(!r.ui.should_ignore_state_change());
}

void TestGateOutputChordPersistence() {
  test_checker.Start("TestGateOutputChordPersistence");
  Rig r;
  unsigned writes = test_writes;
  const State before = r.settings.state();
  for (int i = 0; i < 2; ++i) {
    r.Down(SWITCH_ASSIGN);
    r.Down(SWITCH_SEED);
    CHECK(r.settings.state().gate_output_mode == !i);
    CHECK(test_writes == writes + i + 1);
    r.Tick(100);
    r.Up(SWITCH_SEED);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + i + 1);
    CHECK(r.settings.state().assignable_cv_destination ==
        before.assignable_cv_destination);
    CHECK(r.ui.seed_button_locked());
    CHECK(!r.ui.seed_button_state());
    CHECK(!r.ui.options_.active());
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().gate_output_mode == !i);
  }
}

void TestOverwriteReserveDisplay() {
  test_checker.Start("TestOverwriteReserveDisplay");
  Rig r;
  const float reserve[] = { 1.0f, 0.5f, 0.0f, 0.5f, 1.0f };
  for (int media = 0; media < 4; ++media) {
    r.settings.mutable_state()->media = media;
    for (int i = 0; i < 5; ++i) {
      r.ui.set_overwrite_reserve(reserve[i]);
      r.Tick(1);
      Leds::Color color = r.ui.media_palette_[media];
      // LED conversion truncates; these binary fractions are exact.
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
          uint8_t(color.r * reserve[i] * reserve[i]));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] ==
          uint8_t(color.g * reserve[i]));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] ==
          uint8_t(color.b * reserve[i]));
    }
  }
}

void TestCorruptCalibrationRewritesOnce() {
  test_checker.Start("TestCorruptCalibrationRewritesOnce");
  Rig r;
  const State defaults = r.settings.state();
  const PersistentData calibration = r.settings.persistent_data();
  r.settings.mutable_state()->media = 0;
  r.settings.SaveState();
  uint8_t* flash = reinterpret_cast<uint8_t*>(0x081e0000);
  flash[8] ^= 1;  // Corrupt CALI payload without changing its checksum.
  unsigned writes = test_writes;
  Settings first;
  CHECK(!first.Init(false));
  CHECK(test_writes > writes);
  CHECK(!memcmp(&first.state(), &defaults, sizeof(State)));
  CHECK(!memcmp(first.persistent_data().adc_calibration,
      calibration.adc_calibration, sizeof(calibration.adc_calibration)));
  writes = test_writes;
  for (int i = 0; i < 3; ++i) {
    Settings next;
    CHECK(next.Init(false));
    CHECK(test_writes == writes);
    CHECK(!memcmp(&next.state(), &defaults, sizeof(State)));
    CHECK(!memcmp(next.persistent_data().adc_calibration,
        calibration.adc_calibration, sizeof(calibration.adc_calibration)));
  }
}

void TestSelfPlayStorage() {
  test_checker.Start("TestSelfPlayStorage");
  Rig r;
  for (int i = 0; i < 256; ++i) {
    r.settings.mutable_state()->self_play = i;
    CHECK(r.settings.self_play_on_request() == (i == 0xa1));
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.self_play_on_request() == (i == 0xa1));
    CHECK(loaded.state().self_play == (i == 0xa1 ? 0xa1 : 0));
  }
  r.settings.set_self_play_on_request(true);
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.self_play_on_request());
  CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  LegacyState legacy_state = { };
  PersistentData calibration = { };
  stmlib::ChunkStorage<15, PersistentData, LegacyState> legacy;
  CHECK(legacy.Init(&calibration, &legacy_state));
  memset(legacy_state.padding, 0, sizeof(legacy_state.padding));
  legacy.SaveState();
  CHECK(loaded.Init(false) && !loaded.self_play_on_request());
  loaded.mutable_state()->self_play = 0xff;
  loaded.chunk_storage_.SaveState();
  CHECK(loaded.Init(false));
  CHECK(loaded.state().self_play == 0);
}

void TestCloudsStorage() {
  test_checker.Start("TestCloudsStorage");
  Rig r;
  for (int value = 0; value < 256; ++value) {
    r.settings.mutable_state()->reverb_style = value;
    CHECK(r.settings.clouds_style() == (value == 0xa2));
    r.settings.mutable_state()->pitch_scale = 0xa7;
    r.settings.mutable_state()->padding[0] = 0xff;
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.clouds_style() == (value == 0xa2));
    CHECK(loaded.state().reverb_style == (value == 0xa2 ? 0xa2 : 0));
    CHECK(loaded.state().pitch_scale == 0xa7);
    CHECK(loaded.pitch_scale() == PITCH_SCALE_PELOG);
    CHECK(loaded.state().padding[0] == 0);
    // Bypass SaveState normalization to exercise load normalization too.
    loaded.mutable_state()->reverb_style = value;
    loaded.chunk_storage_.SaveState();
    CHECK(loaded.Init(false));
    CHECK(loaded.state().reverb_style == (value == 0xa2 ? 0xa2 : 0));
    CHECK(r.settings.Init(false));
  }
}

void TestSelfPlayHold() {
  test_checker.Start("TestSelfPlayHold");
  for (int mode = 0; mode < 2; ++mode) {
    for (int no_input = 0; no_input < 2; ++no_input) {
      for (int frozen = 0; frozen < 2; ++frozen) {
        Rig r;
        r.settings.set_self_play_on_request(mode);
        r.ui.set_no_input(no_input);
        r.ui.freeze_led_state_ = frozen;
        r.settings.mutable_state()->gain = 123;
        unsigned writes = test_writes;
        uint8_t media = r.settings.state().media;
        // While frozen, the hold restarts automatic gain as in stock.
        const bool request = mode && no_input && !frozen;
        for (int hold = 0; hold < 2; ++hold) {
          r.Down(SWITCH_MEDIA);
          uint32_t start = r.ui.press_time_[SWITCH_MEDIA];
          while (stmlib::system_clock.milliseconds() - start < 1499) {
            r.Tick(1);
          }
          CHECK(!r.ui.self_play_requested());
          r.Tick(1);
          CHECK(r.ui.self_play_requested() == request);
          CHECK(r.gain.restarts == (request ? 0 : hold + 1));
          if (request) {
            CHECK(r.settings.state().gain == 123 && test_writes == writes);
          }
          r.ui.ClearSelfPlayRequest();
          CHECK(!r.ui.self_play_requested());
          r.Up(SWITCH_MEDIA);
          CHECK(!r.ui.self_play_requested());
          CHECK(r.settings.state().media == media);
        }
      }
    }
  }
}

// Settled option saves until the sector fills; a power cut anywhere in the
// rollover that erases and rewrites calibration and state together.
const uint32_t kCalibrationChunk = (8 + sizeof(PersistentData) + 31) & ~31u;
const uint32_t kStateChunk = (8 + sizeof(State) + 31) & ~31u;
const int kStateChunks = (0x20000 - kCalibrationChunk) / kStateChunk;

void TestSettingsRollover() {
  test_checker.Start("TestSettingsRollover");
  // Rolling save: erase, four calibration blocks, one state block.
  for (int cut = 0; cut <= 6; ++cut) {
    Rig r;
    const State defaults = r.settings.state();
    const PersistentData factory = r.settings.persistent_data();
    PersistentData* calibration = r.settings.mutable_persistent_data();
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      calibration->adc_calibration[i].scale = -2.5f - 0.125f * i;
      calibration->adc_calibration[i].offset = 0.75f + 0.0625f * i;
    }
    r.settings.SavePersistentData();
    r.settings.set_output_limiter_mode(OUTPUT_LIMITER_FAST);
    r.settings.set_self_play_on_request(true);
    r.settings.set_pitch_scale(PITCH_SCALE_PENTATONIC_MAJOR);
    r.settings.set_clouds_style(false);
    r.settings.set_stereo_width(200);
    r.settings.set_width_assigned(true);
    r.settings.set_delay_mode(DELAY_MODE_WIDE_DETENT);
    r.settings.SaveState();
    const PersistentData old_calibration = r.settings.persistent_data();
    r.Enter();
    CHECK(!r.ui.options_.clouds_preview());
    // Real settled saves: the reverb knob alone flips the style each time.
    const unsigned erases = TestFlashErases();
    for (int i = 0; i < 2; ++i) {
      const unsigned writes = test_writes;
      r.reverb = i ? .1f : .9f;
      r.Tick(1100);
      CHECK(test_writes == writes + 1);
    }
    CHECK(!r.settings.clouds_style());
    while (r.settings.chunk_storage_.next_state_chunk_index_ <=
        kStateChunks) {
      r.settings.SaveState();
    }
    CHECK(TestFlashErases() == erases);
    const State old_state = r.settings.state();
    State new_state = old_state;
    new_state.reverb_style = kOptionTag + 2;

    const unsigned writes = test_writes;
    TestFlashBudget() = cut;
    r.reverb = .9f;
    r.Tick(1100);
    TestFlashBudget() = -1;
    CHECK(r.settings.clouds_style());
    CHECK(TestFlashErases() == erases + (cut ? 1 : 0));
    CHECK(test_writes == writes + (cut > 1 ? cut - 1 : 0));

    Settings loaded;
    const bool valid = loaded.Init(false);
    const State& state = loaded.state();
    const AdcCalibrationData* data = loaded.persistent_data().adc_calibration;
    if (cut == 0) {
      // Power went before the erase: the previous record is untouched.
      CHECK(valid);
      CHECK(!memcmp(&state, &old_state, sizeof(State)));
      CHECK(!memcmp(data, old_calibration.adc_calibration,
          sizeof(old_calibration.adc_calibration)));
    } else if (cut < 5) {
      // Stock stmlib window: from the erase to the last calibration block no
      // copy of the calibration exists, so both come back as the defaults.
      CHECK(!valid);
      CHECK(!memcmp(&state, &defaults, sizeof(State)));
      CHECK(!memcmp(data, factory.adc_calibration,
          sizeof(factory.adc_calibration)));
    } else if (cut == 5) {
      // Stock window: calibration is whole but no state followed it, so the
      // options come back as the defaults while the calibration survives.
      CHECK(!valid);
      CHECK(!memcmp(&state, &defaults, sizeof(State)));
      CHECK(!memcmp(data, old_calibration.adc_calibration,
          sizeof(old_calibration.adc_calibration)));
    } else {
      CHECK(valid);
      CHECK(!memcmp(&state, &new_state, sizeof(State)));
      CHECK(!memcmp(data, old_calibration.adc_calibration,
          sizeof(old_calibration.adc_calibration)));
      CHECK(loaded.chunk_storage_.next_state_chunk_index_ == 2);
      CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_FAST);
      CHECK(loaded.self_play_on_request());
      CHECK(loaded.pitch_scale() == PITCH_SCALE_PENTATONIC_MAJOR);
      CHECK(loaded.clouds_style());
      CHECK(loaded.stereo_width() == 200);
      CHECK(loaded.width_assigned());
      CHECK(loaded.delay_mode() == DELAY_MODE_WIDE_DETENT);
    }
    // Whatever came back is stable: a second boot finds a valid record.
    Settings again;
    CHECK(again.Init(false));
    CHECK(!memcmp(&again.state(), &state, sizeof(State)));
    CHECK(!memcmp(again.persistent_data().adc_calibration, data,
        sizeof(old_calibration.adc_calibration)));
  }
}

void TestPitchStorage() {
  test_checker.Start("TestPitchStorage");
  Rig r;
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.set_self_play_on_request(true);
  for (int i = 0; i < 256; ++i) {
    const PitchScale expected = i >= 0xa1 && i <= 0xa9
        ? PitchScale(i - 0xa0) : PITCH_SCALE_OFF;
    r.settings.mutable_state()->pitch_scale = i;
    CHECK(r.settings.pitch_scale() == expected);
    // Read unnormalized data from flash, then save it canonically.
    r.settings.chunk_storage_.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.pitch_scale() == expected);
    CHECK(loaded.state().pitch_scale == (expected ? i : 0));
    loaded.mutable_state()->padding[0] = 0xff;
    loaded.SaveState();
    CHECK(r.settings.Init(false));
    CHECK(r.settings.pitch_scale() == expected);
    CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    CHECK(r.settings.self_play_on_request());
    CHECK(r.settings.state().padding[0] == 0);
  }
}

void TestPlaybackStorage() {
  test_checker.Start("TestPlaybackStorage");
  for (int raw = 0; raw < 256; ++raw) {
    Rig r;
    r.settings.set_stereo_width(128);
    r.settings.mutable_state()->width_assigned = raw;
    CHECK(r.settings.width_assigned() == (raw == 1));
  }

  Rig r;
  for (int width = 0; width < 256; ++width) {
    for (int delay = 0; delay < 3; ++delay) {
      r.settings.set_pitch_scale(PITCH_SCALE_SHRI);
      r.settings.set_clouds_style(true);
      r.settings.set_stereo_width(StereoWidth(width));
      r.settings.set_width_assigned(width != 0);
      r.settings.set_delay_mode(DelayMode(delay));
      r.settings.SaveState();
      Settings loaded;
      loaded.Init(false);
      CHECK(loaded.stereo_width() == StereoWidth(width));
      CHECK(loaded.state().assignable_cv_destination == 0);
      CHECK(loaded.width_assigned() == (width != 0));
      CHECK(loaded.state().width_assigned == (width ? 1 : 0));
      State stock_view = loaded.state();
      CONSTRAIN(stock_view.assignable_cv_destination, 0, 3);
      CHECK(stock_view.assignable_cv_destination == 0);
      CHECK(loaded.delay_mode() == DelayMode(delay));
      CHECK(loaded.state().pitch_scale == 0xa9);
      CHECK(loaded.state().reverb_style == 0xa2);
      CHECK(loaded.state().stereo_width == width);
      CHECK(loaded.state().delay_mode == (delay ? 0xa0 + delay : 0));
      for (int byte = 7; byte < 14; ++byte) {
        CHECK(reinterpret_cast<const uint8_t*>(
            &loaded.state())[byte + 10] == 0);
      }
    }
  }
}

void TestWidthAssignCycle() {
  test_checker.Start("TestWidthAssignCycle");
  Rig r;
  r.settings.mutable_state()->assignable_cv_destination = 3;
  r.Down(SWITCH_ASSIGN);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.state().assignable_cv_destination == 0);
  for (int enabled = 0; enabled < 2; ++enabled) {
    r.settings.set_stereo_width(enabled ? 128 : 0);
    r.settings.mutable_state()->assignable_cv_destination = 0;
    r.Tick(20);
    const int expected[] = { 1, 2, 4, 0 };
    for (int i = 0; i < (enabled ? 4 : 3); ++i) {
      r.Down(SWITCH_ASSIGN);
      r.Up(SWITCH_ASSIGN);
      CHECK(r.settings.state().assignable_cv_destination <= 3);
      CHECK((r.settings.width_assigned() ? 4 :
          r.settings.state().assignable_cv_destination) ==
          (enabled ? expected[i] : (i + 1) % 3));
    }
  }
}

void TestDiffusionGestureAndMeter() {
  test_checker.Start("TestDiffusionGestureAndMeter");
  Rig r;
  const uint8_t media = r.settings.state().media;
  const uint8_t gain = r.settings.state().gain;
  CHECK(r.settings.state().diffusion == 0);
  CHECK(r.settings.diffusion() == 0.0f);
  unsigned writes = test_writes;
  r.Down(SWITCH_MEDIA);
  const float positions[] = { 0.125f, 0.75f, 0.0f };
  for (int i = 0; i < 3; ++i) {
    r.reverb = positions[i];
    r.Tick(20);
    CHECK(r.settings.state().diffusion ==
        1 + int(positions[i] * 254.0f + 0.5f));
    CHECK(fabsf(r.settings.diffusion() - positions[i]) < 0.003f);
    CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_REVERB) == 0.5f);
    CHECK(r.settings.state().gain == gain);
    CHECK(test_writes == writes);
  }
  r.reverb = 0.5f;
  r.Tick(20);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);  // The meter, not quality.
  r.Tick(1700);  // Holding while turning never restarts the gain.
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  CHECK(r.settings.state().gain == gain);
  CHECK(test_writes == writes + 1);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.state().diffusion == 128);
  // A quality tap without turning REVERB leaves the amount alone.
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  CHECK(r.settings.state().diffusion == 128);
}

void TestDiffusionStorage() {
  test_checker.Start("TestDiffusionStorage");
  for (int clouds = 0; clouds < 2; ++clouds) {
    for (int raw = 0; raw < 256; ++raw) {
      Rig r;
      r.settings.set_clouds_style(clouds);
      r.settings.mutable_state()->diffusion = raw;
      const float expected = raw
          ? float(raw - 1) / 254.0f : (clouds ? kCloudsDiffusion : 0.0f);
      CHECK(r.settings.diffusion() == expected);
      r.settings.mutable_state()->assignable_cv_destination = 2;
      r.settings.set_diffusion_assigned(true);
      // Clouds-style's own amount is not a CV destination until set.
      const bool assignable = raw > 1;
      CHECK(r.settings.diffusion_assigned() == assignable);
      r.settings.SaveState();
      Settings loaded;
      CHECK(loaded.Init(false));
      CHECK(loaded.state().diffusion == raw);
      CHECK(loaded.diffusion_assigned() == assignable);
      CHECK(!loaded.width_assigned());
      CHECK(loaded.state().assignable_cv_destination == (assignable ? 0 : 2));
    }
  }
  Rig r;
  r.settings.set_diffusion(0.5f);
  r.settings.set_stereo_width(128);
  r.settings.set_diffusion_assigned(true);
  r.settings.set_stereo_width(0);  // Width off keeps the diffusion target.
  CHECK(r.settings.diffusion_assigned());
  r.settings.set_diffusion(0.0f);  // No diffusion left to modulate.
  CHECK(!r.settings.diffusion_assigned());
  CHECK(r.settings.state().diffusion == 1);
  r.settings.set_diffusion(1.02f);  // A pot reading past the end.
  CHECK(r.settings.state().diffusion == 255);
}

void TestDiffusionAssignCycle() {
  test_checker.Start("TestDiffusionAssignCycle");
  // Destinations: 0-2 mix knobs, 4 WIDTH, 5 DIFFUSION.
  const int cycles[4][6] = {
    { 1, 2, 0, 1, 2, 0 },
    { 1, 2, 4, 0, 1, 2 },
    { 1, 2, 5, 0, 1, 2 },
    { 1, 2, 4, 5, 0, 1 },
  };
  for (int c = 0; c < 4; ++c) {
    Rig r;
    r.settings.set_stereo_width(c & 1 ? 128 : 0);
    if (c & 2) r.settings.set_diffusion(0.4f);
    r.settings.mutable_state()->assignable_cv_destination = 0;
    r.Tick(20);
    for (int i = 0; i < 6; ++i) {
      r.Down(SWITCH_ASSIGN);
      r.Up(SWITCH_ASSIGN);
      const int destination = r.settings.width_assigned() ? 4
          : r.settings.diffusion_assigned() ? 5
          : r.settings.state().assignable_cv_destination;
      CHECK(destination == cycles[c][i]);
      if (destination == 5) {
        for (int led = 0; led < 3; ++led) {
          CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + led] == 0 ||
              led == Ui::MIX_PARAMETER_REVERB);
        }
      }
    }
  }
  // Clouds-style alone keeps the Beta 1 cycle.
  {
    Rig r;
    r.settings.set_clouds_style(true);
    r.settings.mutable_state()->assignable_cv_destination = 2;
    r.Tap();
    CHECK(!r.settings.diffusion_assigned());
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
  // [M]+knob amounts replace the DIFFUSION target.
  Rig r;
  r.settings.set_diffusion(0.4f);
  r.settings.mutable_state()->assignable_cv_destination = 2;
  r.Tap();
  CHECK(r.settings.diffusion_assigned());
  r.Down(SWITCH_ASSIGN);
  r.feedback = 0.9f;
  r.Tick(20);
  r.Up(SWITCH_ASSIGN);
  CHECK(!r.settings.diffusion_assigned());
  CHECK(r.settings.state().assignable_cv_destination == 3);
}

void TestPendingWidthAssign() {
  test_checker.Start("TestPendingWidthAssign");
  for (int enabled = 0; enabled < 2; ++enabled) {
    Rig r;
    r.settings.set_stereo_width(enabled ? 0 : 128);
    r.settings.mutable_state()->assignable_cv_destination = 2;
    r.settings.set_width_assigned(!enabled);
    r.settings.SaveState();
    r.Tick(30);
    r.Enter();
    r.pot = enabled ? 0.9f : 0.06f;
    r.Tick(10);
    r.Down(SWITCH_MEDIA);
    r.Up(SWITCH_MEDIA);
    CHECK(!r.ui.options_.active());
    CHECK(r.settings.stereo_width() == (enabled ? 0 : 128));
    r.Tap();
    CHECK(r.settings.width_assigned() == bool(enabled));
    CHECK(r.settings.state().assignable_cv_destination == (enabled ? 0 : 1));
    r.Tick(1100);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK((loaded.stereo_width() != 0) == bool(enabled));
    CHECK(loaded.width_assigned() == bool(enabled));
    CHECK(loaded.state().assignable_cv_destination == (enabled ? 0 : 1));
  }
}

void TestOptionsTablesAndLifetime() {
  test_checker.Start("TestOptionsTablesAndLifetime");
  const OptionSetting expected[3][3] = {
    { OPTION_PITCH, OPTION_WIDTH, OPTION_REVERB },
    { OPTION_RESERVED, OPTION_DELAY, OPTION_RESERVED },
    { OPTION_SELF_PLAY, OPTION_LIMITER, OPTION_RESERVED }
  };
  Rig r;
  r.Enter();
  r.Tick(100);  // Entry confirmation has ended.
  for (int page = 0; page < 3; ++page) {
    CHECK(r.ui.options_.page() == page);
    const int expected_leds[3] = { 2, 0, 1 };
    CHECK(kOptionPages[page].led_index == expected_leds[page]);
    for (int knob = 0; knob < 3; ++knob) {
      CHECK(kOptionPages[page].knobs[knob] == expected[page][knob]);
      const int led = LED_ASSIGN_1 + kOptionPages[page].led_index;
      for (int i = LED_ASSIGN_1; i < LED_ASSIGN_1 + 3; ++i) {
        if (i != led) CHECK(r.ui.leds_.intensity_[i] == 0);
      }
    }
    r.Tap();
  }
  CHECK(r.ui.options_.page() == 0);
  r.Tap();
  r.Tick(60000);
  CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  r.Tick(30);
  r.Enter();
  CHECK(r.ui.options_.page() == 1);
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000);
  r.Up(SWITCH_ASSIGN);
  CHECK(!r.ui.options_.active());
  r.Tick(30);
  r.Enter();
  CHECK(r.ui.options_.page() == 1);
  Rig startup(true);
  startup.Tick(3500);
  CHECK(!startup.ui.options_.active());
}

void TestOptionsKnobsAndSettling() {
  test_checker.Start("TestOptionsKnobsAndSettling");
  // Every physical knob, including all three reserved positions.
  for (int page = 0; page < 3; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      Rig r;
      r.Enter();
      for (int i = 0; i < page; ++i) r.Tap();
      float* pot = knob == 0 ? &r.feedback : knob == 1 ? &r.pot : &r.reverb;
      const OptionSetting setting = kOptionPages[page].knobs[knob];
      const OptionSetting selected = r.ui.options_.selected_;
      const State state = r.settings.state();
      const unsigned writes = test_writes;
      *pot = 0.59f;
      r.Tick(1500);
      CHECK(!r.ui.options_.qualified_[knob]);
      CHECK(test_writes == writes);
      *pot = 0.9f;
      r.Tick(100);
      if (setting == OPTION_RESERVED) {
        CHECK(r.ui.options_.selected_ == selected);
        CHECK(!memcmp(&state, &r.settings.state(), sizeof(state)));
        r.Tick(2000);
        CHECK(test_writes == writes);
        continue;
      }
      CHECK(r.ui.options_.selected_ == setting);
      CHECK(r.ui.options_.qualified_[knob]);
      for (int step = 0; step < 20; ++step) {
        *pot = 0.95f - 0.045f * step;
        r.Tick(100);
      }
      *pot = 0.9f;
      r.Tick(999);
      CHECK(test_writes == writes);
      r.Tick(2);
      CHECK(test_writes == writes + 1);
      CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
      CHECK(r.ui.options_.Value(setting) == (setting == OPTION_WIDTH
          ? r.settings.stereo_width() : setting == OPTION_PITCH
          ? int(r.settings.pitch_scale()) : setting == OPTION_DELAY
          ? int(r.settings.delay_mode()) : setting == OPTION_REVERB
          ? r.settings.clouds_style() : setting == OPTION_SELF_PLAY
          ? r.settings.self_play_on_request()
          : int(r.settings.output_limiter_mode())));
      r.Tick(5000);
      CHECK(test_writes == writes + 1);
      CHECK(r.ui.options_.active());
    }
  }
  // A leave button keeps a pending change and waits for its one settled save.
  for (int button = 0; button < SWITCH_LAST; ++button) {
    if (button == SWITCH_ASSIGN) continue;
    Rig r;
    r.Enter();
    const unsigned writes = test_writes;
    r.reverb = .9f;
    r.Tick(10);
    r.Down(Switch(button));
    r.Up(Switch(button));
    CHECK(!r.ui.options_.active());
    r.Tick(1000);
    CHECK(!r.ui.options_.active());
    CHECK(r.settings.clouds_style());
    CHECK(test_writes == writes + 1);
  }
}

void TestOptionsWidthAndSeed() {
  test_checker.Start("TestOptionsWidthAndSeed");
  for (int enabled = 0; enabled < 2; ++enabled) {
    Rig r;
    r.settings.set_stereo_width(enabled ? 180 : 0);
    r.pot = .22f;
    r.Tick(30);
    r.Enter();
    r.pot = .11f;
    r.Tick(1);
    CHECK((r.ui.options_.width_preview() != 0) == bool(enabled));
    r.pot = .14f;
    r.Tick(1);
    CHECK(r.ui.options_.width_preview() != 0);
    r.pot = .9999f;
    r.Tick(1);
    CHECK(r.ui.options_.width_preview() == 255);
    r.pot = .06f;
    r.Tick(1001);
    CHECK(r.settings.stereo_width() == 0);
  }
  Rig saved;
  saved.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
  saved.Enter();
  saved.Down(SWITCH_SEED);
  saved.Up(SWITCH_SEED);
  CHECK(saved.ui.options_.pitch_preview() == PITCH_SCALE_MAJOR);
  CHECK(!saved.ui.options_.qualified_[0]);
  Rig r;
  r.Enter();
  r.feedback = .9f;
  r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  const bool seed_state = r.ui.seed_button_state_;
  const bool seed_locked = r.ui.seed_button_locked_;
  for (int i = 0; i < 8; ++i) {
    test_down[SWITCH_SEED] = true;
    r.Tick(8, false);
    CHECK(!r.ui.queue_.available());
    test_down[SWITCH_SEED] = false;
    r.Tick(8, false);
    CHECK(!r.ui.queue_.available());
    r.ui.DoEvents();
    CHECK(r.ui.seed_button_state_ == seed_state);
    CHECK(r.ui.seed_button_locked_ == seed_locked);
    CHECK(r.ui.options_.pitch_preview() ==
        PitchScale(PITCH_SCALE_FIFTHS + (i + 1) % 8));
    CHECK(r.ui.options_.active());
  }
  r.Down(SWITCH_SEED);
  r.Tick(600);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  r.Tick(1001);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_FIFTHS);
}

void TestOptionsZonesAndSaveBatch() {
  test_checker.Start("TestOptionsZonesAndSaveBatch");
  Rig r;
  r.Enter();
  // Pitch thirds, including hysteresis around both boundaries.
  r.feedback = .1f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_OFF);
  r.feedback = .35f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_OFF);
  r.feedback = .38f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_CHROMATIC);
  r.feedback = .68f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_CHROMATIC);
  r.feedback = .71f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  r.feedback = .65f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  r.feedback = .62f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_CHROMATIC);
  r.feedback = .29f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_OFF);
  // Reverb halves retain a value inside the midpoint dead band.
  r.reverb = .9f; r.Tick(1);
  r.reverb = .5f; r.Tick(1);
  CHECK(r.ui.options_.clouds_preview());
  r.reverb = .46f; r.Tick(1);
  CHECK(!r.ui.options_.clouds_preview());
  r.reverb = .54f; r.Tick(1);
  const unsigned writes = test_writes;
  r.pot = .9f; r.Tick(1001);
  CHECK(test_writes == writes + 1);  // One batch for both knobs.
  CHECK(r.settings.clouds_style());
  CHECK(r.settings.stereo_width() != 0);
  r.settings.set_width_assigned(true);
  r.pot = .01f; r.Tick(1001);
  CHECK(r.settings.stereo_width() == 0);
  CHECK(!r.settings.width_assigned());
  Settings loaded;
  loaded.Init(false);
  CHECK(!loaded.width_assigned());
  CHECK(loaded.clouds_style());
  // Main may be delayed: published batch must stay stable until consumed.
  r.feedback = .9f; r.Tick(1001, false);
  CHECK(r.ui.options_.commit_requested());
  r.feedback = .1f; r.Tick(2000, false);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  // Exit remains responsive even if main is delayed by a flash operation.
  test_down[SWITCH_MEDIA] = true;
  r.Tick(8, false);
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(8, false);
  r.ui.DoEvents();
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.Tick(10);
  CHECK(!r.ui.options_.active());
}

void TestOptionsPalette() {
  test_checker.Start("TestOptionsPalette");
  for (size_t i = 1; i < kNumOptionValues; ++i) {
    const OptionValue& v = kOptionValues[i];
    CHECK(v.color.r || v.color.g || v.color.b);
    if (v.value == 0) {
      CHECK(v.color.r == 0 && v.color.g == 255 &&
          v.color.b == 0 && v.pulses == 0);
    }
    CHECK(v.color.b == 0);
    CHECK((v.color.r == 0 && v.color.g == 255) ||
        (v.color.r == 255 && (v.color.g == 0 || v.color.g == 255)));
    for (size_t j = i + 1; j < kNumOptionValues; ++j) {
      const OptionValue& w = kOptionValues[j];
      if (v.setting == w.setting) {
        CHECK(v.color.r != w.color.r || v.color.g != w.color.g ||
            v.color.b != w.color.b || v.pulses != w.pulses);
      }
    }
  }
  // Renderer consumes the table at every phase of its pulse period.
  Rig r;
  r.Enter();
  for (size_t i = 1; i < kNumOptionValues; ++i) {
    const OptionValue& v = kOptionValues[i];
    r.ui.options_.selected_ = v.setting;
    switch (v.setting) {
      case OPTION_PITCH: r.ui.options_.pitch_preview_ = PitchScale(v.value);
        break;
      case OPTION_WIDTH: r.ui.options_.width_preview_ = v.value ? 255 : 0;
        break;
      case OPTION_REVERB: r.ui.options_.clouds_preview_ = v.value; break;
      case OPTION_DELAY: r.ui.options_.delay_preview_ = DelayMode(v.value);
        break;
      case OPTION_SELF_PLAY: r.ui.options_.self_play_preview_ = v.value; break;
      case OPTION_LIMITER:
        r.ui.options_.limiter_preview_ = OutputLimiterMode(v.value);
        break;
      default: break;
    }
    const OptionValue& indication = r.ui.options_.indication();
    CHECK(&indication == &v);
    CHECK(indication.setting == v.setting && indication.value == v.value);
    CHECK(indication.color.r == v.color.r && indication.color.g == v.color.g &&
        indication.color.b == v.color.b && indication.pulses == v.pulses);
    for (int tick = 0; tick < 2048; ++tick) {
      stmlib::system_clock.Tick();
      r.ui.UpdateLeds();
      const uint32_t now = stmlib::system_clock.milliseconds();
      const uint32_t phase = now % 2048;
      const bool lit = v.pulses == 0 ||
          (v.pulses == 1 && now % 1024 < 512) ||
          (v.pulses == 2 && now % 256 < 128) ||
          (v.pulses == 3 && (phase < 128 || (phase >= 256 && phase < 384)));
      const int red = v.setting == OPTION_WIDTH && v.value ? 255 : v.color.r;
      const int green = v.setting == OPTION_WIDTH && v.value ? 0 : v.color.g;
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == (lit ? red : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == (lit ? green : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == (lit ? v.color.b : 0));
    }
  }
}

void TestOptionsWidthMeter() {
  test_checker.Start("TestOptionsWidthMeter");
  Rig r;
  r.Enter();
  r.ui.options_.selected_ = OPTION_WIDTH;
  for (int width = 1; width <= 255; ++width) {
    r.ui.options_.width_preview_ = width;
    // Compare every code with Beads' meter, independently of the renderer.
    Leds meter;
    meter.Init();
    const float amount = 0.25f + 0.749f * float(width - 1) / 254.0f;
    meter.set_meter(amount);
    for (int tick = 0; tick < 1024; ++tick) {
      stmlib::system_clock.Tick();
      r.ui.UpdateLeds();
      const bool lit = stmlib::system_clock.milliseconds() % 1024 < 512;
      for (int channel = LED_MEDIA_R; channel <= LED_MEDIA_B; ++channel) {
        CHECK(r.ui.leds_.intensity_[channel] ==
            (lit ? meter.intensity_[channel] : 0));
      }
    }
    CHECK(meter.intensity_[LED_MEDIA_B] == 0);
    if (width == 1) {
      CHECK(meter.intensity_[LED_MEDIA_R] == 0);
      CHECK(meter.intensity_[LED_MEDIA_G] == 255);
    }
    if (width == 255) {
      CHECK(meter.intensity_[LED_MEDIA_R] == 255);
      CHECK(meter.intensity_[LED_MEDIA_G] == 0);
    }
  }
}

void TestOptionsConfirmations() {
  test_checker.Start("TestOptionsConfirmations");
  Rig r;
  r.Enter();
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] == 255);
  const uint32_t entry = r.ui.options_flash_time_;
  r.Tick(100);
  CHECK(stmlib::system_clock.milliseconds() - entry >= 100);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] < 255);
  const unsigned writes = test_writes;
  r.reverb = .9f;
  r.Tick(1010);
  CHECK(test_writes == writes + 1);
  CHECK(r.ui.options_flash_time_ != entry);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] == 255);
  const uint32_t saved = r.ui.options_flash_time_;
  r.Tick(1100);
  CHECK(r.ui.options_flash_time_ == saved);  // No write, no confirmation.
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] < 255);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.options_flash_time_ != saved);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] == 255);
  r.Up(SWITCH_FREEZE);
  r.Tick(100);
  CHECK(stmlib::system_clock.milliseconds() - r.ui.options_flash_time_ >= 100);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] == 0);
}

bool IsVolatile(bool*) { return false; }
bool IsVolatile(volatile bool*) { return true; }

void TestCommitPublication() {
  test_checker.Start("TestCommitPublication");
  Rig r;
  CHECK(IsVolatile(&r.ui.options_.dirty_));
  CHECK(IsVolatile(&r.ui.options_.commit_));
}

void TestWidthNoise() {
  test_checker.Start("TestWidthNoise");
  Rig r;
  r.pot = .1f;
  r.Tick(1);
  r.Enter();
  const unsigned writes = test_writes;
  const float boundary = 0.13f + 100.5f * 0.87f / 254.0f;
  r.pot = boundary + 0.002f;
  r.Tick(1);
  uint32_t last_flash = r.ui.options_flash_time_;
  int confirmations = 0;
  for (int i = 0; i < 10000; ++i) {
    r.pot = boundary + (i & 1 ? 0.002f : -0.002f);
    r.Tick(1);
    if (r.ui.options_flash_time_ != last_flash) {
      ++confirmations;
      last_flash = r.ui.options_flash_time_;
    }
  }
  CHECK(confirmations == 1);
  CHECK(test_writes == writes + 1);
  const uint32_t flash = r.ui.options_flash_time_;
  r.Tick(2000);
  CHECK(test_writes == writes + 1);
  CHECK(r.ui.options_flash_time_ == flash);
}

void TestPendingSeed() {
  test_checker.Start("TestPendingSeed");
  for (int hold = 0; hold < 3; ++hold) {
    Rig r;
    r.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
    r.Enter();
    r.reverb = .9f;
    r.Tick(1001, false);
    CHECK(r.ui.options_.commit_requested());
    test_down[SWITCH_SEED] = true;
    r.Tick(hold ? 600 : 20, false);
    if (hold == 2) r.ui.DoEvents();
    test_down[SWITCH_SEED] = false;
    r.Tick(20, false);
    CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
    r.ui.DoEvents();
    r.Tick(1);
    CHECK(r.ui.options_.pitch_preview() ==
        (hold ? PITCH_SCALE_FIFTHS : PITCH_SCALE_MAJOR));
    r.Tick(1001);
    CHECK(r.settings.pitch_scale() ==
        (hold ? PITCH_SCALE_FIFTHS : PITCH_SCALE_MAJOR));
  }
  // Press before publication; release during it, then delay main long enough
  // that using the consumption time would misclassify this as a long hold.
  Rig before;
  before.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
  before.Enter();
  before.reverb = .9f;
  before.Tick(990, false);
  test_down[SWITCH_SEED] = true;
  before.Tick(20, false);
  CHECK(before.ui.options_.commit_requested());
  test_down[SWITCH_SEED] = false;
  before.Tick(620, false);
  before.ui.DoEvents();
  before.Tick(1);
  CHECK(before.ui.options_.pitch_preview() == PITCH_SCALE_MAJOR);
  before.Tick(1001);
  CHECK(before.settings.pitch_scale() == PITCH_SCALE_MAJOR);
}

void TestPendingExitButtons() {
  test_checker.Start("TestPendingExitButtons");
  Rig r;
  r.Enter();
  const unsigned writes = test_writes;
  r.reverb = .9f;
  r.Tick(10);
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(!r.ui.options_.active());
  r.ui.set_overwrite_reserve(1.0f);
  r.Tick(1);
  const Leds::Color normal = r.ui.media_palette_[r.settings.state().media];
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == normal.r);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == normal.g);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == normal.b);
  const bool frozen = r.ui.freeze_button_state_;
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(r.ui.freeze_button_state_ != frozen);
  r.Tick(1001);
  CHECK(r.settings.clouds_style());
  CHECK(test_writes == writes + 1);
}

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x081e0000), 0x20000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x081e0000))) {
    return 1;
  }
  TestCommitPublication();
  TestWidthNoise();
  TestPendingSeed();
  TestPendingExitButtons();
  TestOptionsTablesAndLifetime();
  TestOptionsKnobsAndSettling();
  TestOptionsWidthAndSeed();
  TestOptionsPalette();
  TestOptionsConfirmations();
  TestOptionsWidthMeter();
  TestOptionsZonesAndSaveBatch();
  TestWidthAssignCycle();
  TestPendingWidthAssign();
  TestDiffusionGestureAndMeter();
  TestDiffusionStorage();
  TestDiffusionAssignCycle();
  TestPlaybackStorage();
  TestCloudsStorage();
  TestPitchStorage();
  TestSelfPlayStorage();
  TestSelfPlayHold();
  TestTrimGesture();
  TestMonophonicGesture();
  TestAudioPatchDetection();
  TestFrozenStateChangeGuard();
  TestGateOutputChordPersistence();
  TestOverwriteReserveDisplay();
  TestCorruptCalibrationRewritesOnce();
  TestSettingsRollover();
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
  TestDsp();
  return test_checker.failures() ? 1 : 0;
}
