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
#include <string>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

#define private public
#include "beads/blob/synth_choice.h"
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
uint32_t test_writes = 0;
#include "beads/test/firmware_shims.h"

using namespace beads;

// Every CV target the instrumented UI leaves between two of its calls.
const Settings* watched_settings = NULL;
uint32_t watched_targets = 0;

extern "C" {

void __cyg_profile_func_enter(void*, void*)
    __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void*, void*)
    __attribute__((no_instrument_function));

void __cyg_profile_func_enter(void*, void*) {
  if (watched_settings) {
    watched_targets |= 1 << watched_settings->cv_target_;
  }
}

void __cyg_profile_func_exit(void*, void*) {
  if (watched_settings) {
    watched_targets |= 1 << watched_settings->cv_target_;
  }
}

}  // extern "C"

// A dimmed quality LED channel (0 red, 1 green, 2 blue) at percent: red *
// level^1.5; lit green and blue at the level plus 12 * (1 - level), rounded.
int DimmedMedia(int channel, uint8_t c, int percent) {
  const double p = percent / 100.0;
  if (percent == 100) {
    return c;
  } else if (channel == 0) {
    return int(c * pow(p, 1.5));
  }
  return c * percent / 100 + (c ? int(12.0 * (1.0 - p) + 0.5) : 0);
}

STATIC_ASSERT(sizeof(State) == 24, stat_size);
STATIC_ASSERT(offsetof(State, media) == 0, media);
STATIC_ASSERT(offsetof(State, assignable_cv_destination) == 1, assignment);
STATIC_ASSERT(offsetof(State, assignable_cv_amount) == 2, cv_amounts);
STATIC_ASSERT(offsetof(State, gain) == 5, gain);
STATIC_ASSERT(offsetof(State, gate_output_mode) == 6, gate);
STATIC_ASSERT(offsetof(State, frozen) == 7, frozen);
STATIC_ASSERT(offsetof(State, disable_autosave) == 8, autosave);
STATIC_ASSERT(offsetof(State, monophonic_mode) == 9, mono);
STATIC_ASSERT(offsetof(State, padding) == 10, padding);
STATIC_ASSERT(sizeof(OptionsRecord) == 56, record_size);

struct Rig {
  Settings settings;
  GainControl gain;
  Calibrator calibration;
  BackgroundBufferBackup backup;
  Ui ui;
  float pot;
  float feedback;
  float reverb;

  explicit Rig(bool startup_assign)
      : pot(0.5f), feedback(0.5f), reverb(0.5f) {
    std::fill(test_down, test_down + 4, false);
    test_down[SWITCH_ASSIGN] = startup_assign;
    memset(reinterpret_cast<void*>(0x081c0000), 255, 0x40000);
    settings.Init(false);
    // A new module's first save also fills in its record; do it here so
    // the tests count only their own writes.
    settings.SaveOptions();
    ui.Init(&settings, &gain, &calibration, &backup);
    ui.LoadOptions();
    Tick(40, true);
  }
  void Tick(int n, bool events) {
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
    Tick(8, true);
  }
  void Up(Switch s) {
    test_down[s] = false;
    Tick(8, true);
  }
  void Enter() {
    Down(SWITCH_ASSIGN);
    Tick(2999, true);
    CHECK(ui.options_.stage() == Options::STAGE_WAIT_RELEASE);
    Up(SWITCH_ASSIGN);
    CHECK(ui.options_.stage() == Options::STAGE_EDITING);
  }
  void EnterPage(int page) {
    Enter();
    while (page--) {
      Tap();
    }
  }
  void EnterLimiter() {
    EnterPage(3);
  }
  void Tap() {
    Down(SWITCH_ASSIGN);
    Up(SWITCH_ASSIGN);
  }
};

void TestTrimGesture() {
  test_checker.Start("TestTrimGesture");
  for (int order = 0; order < 2; ++order) {
    Rig r(false);
    r.Down(SWITCH_FREEZE);
    r.Up(SWITCH_FREEZE);
    CHECK(r.ui.freeze_button_state_);
    const uint8_t media = r.settings.state().media;
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.ui.gestures_.trim_flag_);
    CHECK(!r.ui.freeze_button_state_);
    CHECK(r.ui.ignore_release_[SWITCH_MEDIA]);
    CHECK(r.ui.ignore_release_[SWITCH_FREEZE]);
    r.ui.ClearTrimFlag();
    CHECK(!r.ui.gestures_.trim_flag_);
    r.Up(order ? SWITCH_MEDIA : SWITCH_FREEZE);
    r.Up(order ? SWITCH_FREEZE : SWITCH_MEDIA);
    CHECK(!r.ui.freeze_button_state_);
    CHECK(r.settings.state().media == media);
  }
  Rig reverse(false);
  const uint8_t media = reverse.settings.state().media;
  reverse.Down(SWITCH_FREEZE);
  reverse.Down(SWITCH_MEDIA);
  CHECK(!reverse.ui.gestures_.trim_flag_);
  reverse.Up(SWITCH_FREEZE);
  reverse.Up(SWITCH_MEDIA);
  CHECK(reverse.ui.freeze_button_state_);
  CHECK(reverse.settings.state().media == (media + 3) % 4);
  Rig page(false);
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
  CHECK(!page.ui.gestures_.trim_flag_);
  CHECK(!page.ui.freeze_button_state_);
  CHECK(page.settings.state().media == page_media);
}

// An [A] knob edit is saved when an [A] gesture consumes the release.
void TestAmountSavedByAction() {
  test_checker.Start("TestAmountSavedByAction");
  const Switch buttons[] = { SWITCH_FREEZE, SWITCH_SEED };
  for (int b = 0; b < 2; ++b) {
    for (int knob = 0; knob < 3; ++knob) {
      Rig r(false);
      const int before[] = {
        r.settings.state().gain,
        r.settings.stereo_width(),
        r.settings.FeatureValue(FEATURE_DIFFUSION)
      };
      r.Down(SWITCH_MEDIA);
      float* pots[] = { &r.feedback, &r.pot, &r.reverb };
      *pots[knob] = 0.8f;
      r.Tick(20, true);
      r.Down(buttons[b]);
      r.Up(buttons[b]);
      r.Up(SWITCH_MEDIA);
      CHECK(r.ui.ignore_release_[SWITCH_MEDIA]);
      Settings loaded;
      CHECK(loaded.Init(false));
      CHECK(loaded.state().gain == r.settings.state().gain);
      CHECK(loaded.stereo_width() == r.settings.stereo_width());
      CHECK(loaded.FeatureValue(FEATURE_DIFFUSION) ==
          r.settings.FeatureValue(FEATURE_DIFFUSION));
      const int after[] = {
        r.settings.state().gain,
        r.settings.stereo_width(),
        r.settings.FeatureValue(FEATURE_DIFFUSION)
      };
      CHECK(after[knob] != before[knob]);
    }
  }
}

void TestMonophonicGesture() {
  test_checker.Start("TestMonophonicGesture");
  for (int latched = 0; latched < 2; ++latched) {
    for (int order = 0; order < 2; ++order) {
      Rig r(false);
      CHECK(!r.settings.state().monophonic_mode);
      if (!latched) {
        r.Down(SWITCH_SEED);
        r.Up(SWITCH_SEED);
      }
      const bool seed = r.ui.seed_button_state_;
      const uint8_t media = r.settings.state().media;
      uint32_t writes = test_writes;
      for (int toggle = 0; toggle < 2; ++toggle) {
        r.Down(SWITCH_MEDIA);
        r.Down(SWITCH_SEED);
        CHECK(r.settings.state().monophonic_mode == !toggle);
        CHECK(test_writes == ++writes);
        r.Tick(1600, true);
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
        r.Tick(1, true);
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
  Rig reverse(false);
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
  Rig page(false);
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
  Rig r(false);
  r.EnterLimiter();
  r.Down(SWITCH_MEDIA);  // Cancellation enters DRAIN with MEDIA down.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(7, false);  // Debounce 0x7f: release; next tick 0xff exits DRAIN.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  uint8_t media = r.settings.state().media;
  uint32_t writes = test_writes;
  interrupted_rig = &r;
  r.ui.DoEvents();
  if (interrupted_rig) {  // Fixed ISR did not queue a release to dispatch.
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
  Rig delayed(false);
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

  delayed.Tick(30, true);  // Re-arm after the preceding drain exit.
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

// An options record takes two flash words; a state takes one.
const uint32_t kRecordWords = 2;

// A feature's byte in the record held in RAM, and in the one last stored.
uint8_t* RecordByte(Settings* s, ProfileFeature feature) {
  return reinterpret_cast<uint8_t*>(&s->record_) + kRecordOffsets[feature];
}

uint8_t StoredByte(const Settings& s, ProfileFeature feature) {
  return reinterpret_cast<const uint8_t*>(
      &s.stored_record_)[kRecordOffsets[feature]];
}

// Appends a raw byte, as a damaged or later record may hold it.
void SaveRaw(Settings* s, ProfileFeature feature, uint8_t raw) {
  *RecordByte(s, feature) = raw;
  CHECK(s->chunk_storage_.SaveRecord());
}

// A STAT as stock firmware writes it (padding zero), or with the bytes
// 10-23 an older Drizzlet packed its options into.
void SaveStockState(Settings* s, uint8_t padding) {
  s->stored_state_ = s->state();
  memset(s->stored_state_.padding, padding, sizeof(s->stored_state_.padding));
  s->chunk_storage_.SaveState();
}

// Every raw byte of a saved feature: a value loads, a byte past them gives
// the profile's default; the next save writes the value back.
void CheckRecordByte(ProfileFeature feature, int values) {
  Rig r(false);
  const uint8_t fallback = r.settings.profile().entry(feature).value;
  for (int raw = 0; raw < 256; ++raw) {
    SaveRaw(&r.settings, feature, raw);
    Settings loaded;
    CHECK(loaded.Init(false));
    const uint8_t expected = raw < values ? raw : fallback;
    CHECK(loaded.FeatureValue(feature) == expected);
    loaded.SaveState();
    CHECK(StoredByte(loaded, feature) == expected);
  }
}

void TestStorage() {
  test_checker.Start("TestStorage");
  CheckRecordByte(FEATURE_LIMITER, 3);
  Settings s;
  memset(reinterpret_cast<void*>(0x081c0000), 255, 0x40000);
  CHECK(!s.Init(false));
  CHECK(s.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings old_record;
  CHECK(old_record.Init(false));
  CHECK(old_record.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  for (int i = 0; i < 3; ++i) {
    s.set_output_limiter_mode(OutputLimiterMode(i));
    s.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.output_limiter_mode() == i);
    CHECK(loaded.state().media == 3);
    for (size_t j = 0; j < sizeof(loaded.stored_state_.padding); ++j) {
      CHECK(loaded.stored_state_.padding[j] == 0);  // Written as stock.
    }
  }
  // Stock or an older Drizzlet saves State only: the record keeps the
  // options, and the bytes after monophonic mode are ignored, then zeroed.
  for (int padding = 0; padding < 2; ++padding) {
    Settings older;
    CHECK(older.Init(false));
    SaveStockState(&older, padding ? 0xa1 : 0);
    Settings upgrade;
    CHECK(upgrade.Init(false));
    CHECK(upgrade.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    CHECK(upgrade.state().padding[0] == 0);
    upgrade.mutable_state()->media = 1;
    upgrade.SaveState();
    CHECK(upgrade.stored_state_.padding[sizeof(State) - 11] == 0);
  }
  puts("STAT persistence/compatibility: PASS");
}

// Frozen released limiter arithmetic, independent of ProcessOutput dispatch.
struct StockLimiter {
  float peak;
  float lp;
  float decay;
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
      FloatFrame x;
      x.l = sinf(i * .17f) * 4.0f;
      x.r = cosf(i * .13f) * 2.0f;
      FloatFrame y = x;
      FloatFrame z = x;
      stock.ProcessOutput(&y, 1, OUTPUT_LIMITER_STOCK, sr);
      reference.Process(&z);
      CHECK(!memcmp(&y, &z, sizeof(y)));
      off.ProcessOutput(&x, 1, OUTPUT_LIMITER_OFF, sr);
      CHECK(x.l == sinf(i * .17f) * 4.0f && x.r == cosf(i * .13f) * 2.0f);
      fast.ProcessOutput(&x, 1, OUTPUT_LIMITER_FAST, sr);
      CHECK(std::isfinite(x.l) && std::isfinite(x.r));
    }
    for (int i = 0; i < int(sr); ++i) {
      FloatFrame x;
      x.l = .1f;
      x.r = -.1f;
      FloatFrame y = x;
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
  Rig r(false);
  r.settings.mutable_state()->assignable_cv_destination = 0;
  for (int i = 1; i <= 6; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == i % 3);
  }
  r.Tick(30, true);
  r.EnterLimiter();
  for (int i = 0; i < 4; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
}

void TestStockAssignAmounts() {
  test_checker.Start("TestStockAssignAmounts");
  for (int i = 0; i < 3; ++i) {
    Rig r(false);
    r.settings.set_stereo_width(128);
    r.settings.set_width_assigned(true);
    const State before = r.settings.state();
    uint32_t writes = test_writes;
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
    r.Tick(3100, true);
    CHECK(!r.ui.options_.active());
    const int expected[] = { 64, 192, 32 };
    CHECK(r.settings.state().assignable_cv_amount[i] == expected[i]);
    CHECK(r.settings.state().assignable_cv_destination == 3);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + kRecordWords + 1);
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
  Rig r(false);
  for (int i = 0; i < 2; ++i) {
    uint32_t writes = test_writes;
    const bool before = r.settings.state().disable_autosave;
    r.Down(SWITCH_ASSIGN);
    r.Down(SWITCH_FREEZE);
    r.Tick(200, true);
    CHECK(r.settings.state().disable_autosave == !before);
    CHECK(test_writes == writes + 1);
    CHECK(r.ui.leds_.intensity_[LED_FREEZE] == (!before ? 16 : 64));
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().disable_autosave == !before);
    r.Tick(4300, true);
    CHECK(r.settings.state().disable_autosave == !before);
    CHECK(test_writes == writes + 1);
    r.Up(SWITCH_FREEZE);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + 1);
    CHECK(!r.ui.freeze_button_state_);
    r.Tick(1100, true);
  }
}

void TestSeedLatchGestures() {
  test_checker.Start("TestSeedLatchGestures");
  Rig r(false);
  r.Down(SWITCH_SEED);
  uint32_t start = r.ui.press_time_[SWITCH_SEED];
  while (stmlib::system_clock.milliseconds() - start < 3999) {
    r.Tick(1, true);
  }
  CHECK(!r.ui.seed_button_locked_);
  r.Tick(1, true);
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
  Rig r(false);
  uint8_t media = r.settings.state().media;
  r.Down(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  r.settings.mutable_state()->gain = 123;
  r.settings.SaveState();
  uint32_t writes = test_writes;
  media = r.settings.state().media;
  // The hold acts on release (#289): a release before 1.5 s is a tap.
  r.Down(SWITCH_MEDIA);
  uint32_t start = r.ui.press_time_[SWITCH_MEDIA];
  while (stmlib::system_clock.milliseconds() - start < 1480) {
    r.Tick(1, true);
  }
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().gain == 123 && test_writes == writes + 1);
  CHECK(r.settings.state().media == (media + 3) % 4);
  media = r.settings.state().media;
  writes = test_writes;
  r.Down(SWITCH_MEDIA);
  start = r.ui.press_time_[SWITCH_MEDIA];
  while (stmlib::system_clock.milliseconds() - start < 1495) {
    r.Tick(1, true);
  }
  CHECK(r.settings.state().gain == 123 && test_writes == writes);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().gain == 0 && test_writes == writes + 1);
  CHECK(r.settings.state().media == media);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.state().gain == 0 && loaded.state().media == media);
}

void TestBackupLedAndRestore() {
  test_checker.Start("TestBackupLedAndRestore");
  Rig r(false);
  r.ui.freeze_led_state_ = true;
  r.backup.busy_ = true;
  r.Tick(1, true);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 128);
  r.backup.busy_ = false;
  r.Tick(1, true);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 255);
  r.Down(SWITCH_ASSIGN);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 16);
  r.Up(SWITCH_FREEZE);
  r.Up(SWITCH_ASSIGN);
  r.Tick(1100, true);
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
    FloatFrame x;
    x.l = input;
    x.r = -input * 0.25f;
    limiter.ProcessOutput(&x, 1, mode, 48000.0f);
    CHECK(std::isfinite(x.l) && std::isfinite(x.r));
    // The detector cap does not bound the output to the audio range.
    CHECK(fabsf(x.l) > 1.0f && fabsf(x.l) <= fabsf(input));
    CHECK(x.r == -x.l * 0.25f);
    if (mode == OUTPUT_LIMITER_OFF) {
      CHECK(x.l == input);
    }
    FloatFrame small;
    small.l = 0.1f;
    small.r = -0.025f;
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
  Rig r(false);
  const int media[] = { 2, 1, 0, 3 };
  uint32_t writes = test_writes;
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
  Rig r(false);
  const float positions[] = { 0.125f, 0.375f, 0.75f };
  const int gains[] = { 32, 96, 192 };
  const int red[] = { 0, 160, 255 };
  const int green[] = { 172, 255, 121 };
  const uint8_t media = r.settings.state().media;
  uint32_t writes = test_writes;
  r.Down(SWITCH_MEDIA);
  for (int i = 0; i < 3; ++i) {
    r.feedback = positions[i];
    r.Tick(20, true);
    CHECK(r.settings.state().gain == gains[i]);
    CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_FEEDBACK) == 0.5f);
    CHECK(r.settings.state().media == media);
    CHECK(test_writes == writes);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == red[i]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == green[i]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  }
  r.Tick(1700, true);
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
  Rig r(false);
  CHECK(r.ui.seed_button_locked());
  CHECK(!r.ui.seed_button_state());
  CHECK(!r.ui.freeze_button_state());
}

void TestAssignableAmountLeds() {
  test_checker.Start("TestAssignableAmountLeds");
  Rig r(false);
  State* state = r.settings.mutable_state();
  state->assignable_cv_destination = 3;
  const uint8_t amounts[][3] = {
    { 0, 1, 255 }, { 255, 128, 64 }, { 32, 254, 129 }
  };
  for (int j = 0; j < 3; ++j) {
    memcpy(state->assignable_cv_amount, amounts[j], 3);
    r.Tick(1, true);
    for (int i = 0; i < 3; ++i) {
      CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + i] == amounts[j][i] / 2);
    }
  }
  for (int j = 0; j < 3; ++j) {
    state->assignable_cv_destination = j;
    r.Tick(1, true);
    for (int i = 0; i < 3; ++i) {
      CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + i] == (i == j ? 128 : 0));
    }
  }
}

void TestAgcAdjustmentBlink() {
  test_checker.Start("TestAgcAdjustmentBlink");
  Rig r(false);
  r.gain.adjusting_ = true;
  for (int i = 0; i < 256; ++i) {
    r.Tick(1, true);
    CHECK(r.ui.leds_.intensity_[LED_VU_METER] ==
        ((stmlib::system_clock.milliseconds() & 128) ? 128 : 0));
  }
  r.gain.adjusting_ = false;
  r.gain.meter_ = 73;
  r.Tick(1, true);
  CHECK(r.ui.leds_.intensity_[LED_VU_METER] == 73);
}

void TestRecoveryPreservesCalibration() {
  test_checker.Start("TestRecoveryPreservesCalibration");
  Rig r(false);
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
  Rig r(false);
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
  Rig r(false);
  uint32_t writes = test_writes;
  const State before = r.settings.state();
  for (int i = 0; i < 2; ++i) {
    r.Down(SWITCH_ASSIGN);
    r.Down(SWITCH_SEED);
    CHECK(r.settings.state().gate_output_mode == !i);
    CHECK(test_writes == writes + i + 1);
    r.Tick(100, true);
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
  Rig r(false);
  const float reserve[] = { 1.0f, 0.5f, 0.0f, 0.5f, 1.0f };
  for (int media = 0; media < 4; ++media) {
    r.settings.mutable_state()->media = media;
    for (int i = 0; i < 5; ++i) {
      r.ui.set_overwrite_reserve(reserve[i]);
      r.Tick(1, true);
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

void TestCorruptCalibrationLocks() {
  test_checker.Start("TestCorruptCalibrationLocks");
  Rig r(false);
  const State defaults = r.settings.state();
  const PersistentData calibration = r.settings.persistent_data();
  r.settings.mutable_state()->media = 0;
  r.settings.SaveState();
  uint8_t* flash = reinterpret_cast<uint8_t*>(0x081e0000);
  flash[8] ^= 1;  // Corrupt CALI payload without changing its checksum.
  reinterpret_cast<uint8_t*>(0x081c0000)[8] ^= 1;  // And its mirror.
  static uint8_t sector[0x20000];
  memcpy(sector, flash, sizeof(sector));
  const uint32_t writes = test_writes;
  // Stock firmware rewrites the factory values and runs the factory test;
  // here every boot plays on the defaults and writes nothing.
  for (int i = 0; i < 3; ++i) {
    Settings next;
    CHECK(next.Init(false));
    CHECK(next.write_locked());
    CHECK(!memcmp(&next.state(), &defaults, sizeof(State)));
    CHECK(!memcmp(next.persistent_data().adc_calibration,
        calibration.adc_calibration, sizeof(calibration.adc_calibration)));
    next.mutable_state()->media = 1;
    next.SaveState();
    CHECK(test_writes == writes);
    CHECK(!memcmp(sector, flash, sizeof(sector)));
  }
}

void TestSelfPlayStorage() {
  test_checker.Start("TestSelfPlayStorage");
  Rig r(false);
  CheckRecordByte(FEATURE_SYNTH_START, 2);
  r.settings.set_self_play_on_request(true);
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.self_play_on_request());
  CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  // A stock State save keeps the choice in the record.
  SaveStockState(&loaded, 0);
  CHECK(loaded.Init(false) && loaded.self_play_on_request());
}

void TestAttackStorage() {
  test_checker.Start("TestAttackStorage");
  // One threshold per Attack sensitivity value.
  CHECK(kFeatureDefinitions[FEATURE_ATTACK_SENSITIVITY].values ==
      sizeof(kAttackThresholds) / sizeof(kAttackThresholds[0]));
  CheckRecordByte(FEATURE_ATTACKS, 3);
  // Page 1, REVERB, in thirds.
  // Far enough from the entry position (0.5) to take over.
  const float positions[] = { 0.05f, 0.62f, 0.95f };
  for (int zone = 0; zone < 3; ++zone) {
    Rig s(false);
    s.Enter();
    s.reverb = positions[zone];
    s.Tick(20, true);
    CHECK(s.ui.attack_mode() == AttackMode(zone));
    s.Tick(1100, true);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.attack_mode() == AttackMode(zone));
  }
}

void TestFrozenFeedbackStorage() {
  test_checker.Start("TestFrozenFeedbackStorage");
  CheckRecordByte(FEATURE_FROZEN, 2);
  // Page 2, FEEDBACK: upper half is Evolving, saved when the page settles.
  Rig s(false);
  s.Enter();
  s.Tap();
  s.feedback = 0.9f;
  s.Tick(20, true);
  CHECK(s.ui.evolving_feedback());
  s.Tick(1100, true);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.evolving_feedback());
}

void TestCloudsStorage() {
  test_checker.Start("TestCloudsStorage");
  CheckRecordByte(FEATURE_REVERB, 2);
  // Its neighbour in the record is kept.
  Rig r(false);
  r.settings.set_pitch_scale(PITCH_SCALE_PELOG);
  for (int value = 0; value < 2; ++value) {
    r.settings.set_clouds_style(value);
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.clouds_style() == bool(value));
    CHECK(loaded.pitch_scale() == PITCH_SCALE_PELOG);
  }
}

void TestSynthEngineStorage() {
  test_checker.Start("TestSynthEngineStorage");
  Rig r(false);
  CHECK(!r.settings.synth() && !r.ui.synth());
  CheckRecordByte(FEATURE_SYNTH, 3);
  // Page 3, DRY/WET knob: two halves with a dead band, previewed live.
  r.settings.set_synth(SYNTH_BEADS);
  r.settings.SaveState();
  r.ui.LoadOptions();
  r.EnterPage(2);
  r.pot = .9f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_SYNTH) && r.ui.synth());
  CHECK(!r.settings.synth());
  r.pot = .5f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_SYNTH));
  r.pot = .46f;
  r.Tick(1, true);
  CHECK(!r.ui.options_.Value(FEATURE_SYNTH) && !r.ui.synth());
  r.pot = .54f;
  r.Tick(1001, true);
  CHECK(r.settings.synth());
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.synth());
  CHECK(StoredByte(loaded, FEATURE_SYNTH) == 1);
}

// Synth edits select playable values in order, using three zones or two
// halves; a lone value fills the knob. Unplayable saved values stay unedited.
void TestSynthPlayableSets() {
  test_checker.Start("TestSynthPlayableSets");
  const uint8_t all = SynthMask(true, true, true);
  struct Case {
    uint8_t playable;
    uint8_t low;
    uint8_t middle;
    uint8_t high;
  } cases[] = {
    { all, SYNTH_BEADS, SYNTH_PLAITS, SYNTH_RINGS },
    { SynthMask(true, true, false), SYNTH_BEADS, SYNTH_PLAITS, SYNTH_PLAITS },
    { SynthMask(true, false, true), SYNTH_BEADS, SYNTH_RINGS, SYNTH_RINGS },
    { SynthMask(false, true, true), SYNTH_PLAITS, SYNTH_RINGS, SYNTH_RINGS },
    { SynthMask(false, false, true), SYNTH_RINGS, SYNTH_RINGS, SYNTH_RINGS },
    { SynthMask(true, false, false), SYNTH_BEADS, SYNTH_BEADS, SYNTH_BEADS },
  };
  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
    Rig r(false);
    r.ui.set_playable_synths(cases[c].playable);
    r.EnterPage(2);
    const uint32_t writes = test_writes;
    // Low, then the middle third (a dead band for halves), then high.
    r.pot = .1f;
    r.Tick(1, true);
    CHECK(r.ui.synth() == cases[c].low);
    r.pot = .45f;
    r.Tick(1, true);
    r.pot = .5f;
    r.Tick(1, true);
    const bool halves = cases[c].middle == cases[c].high &&
        cases[c].low != cases[c].high;
    CHECK(r.ui.synth() == (halves ? cases[c].low : cases[c].middle));
    r.pot = .9f;
    r.Tick(1, true);
    CHECK(r.ui.options_.Value(FEATURE_SYNTH) == cases[c].high);
    CHECK(r.ui.synth() == cases[c].high);
    CHECK(r.settings.synth() == SYNTH_BEADS);
    r.Tick(1001, true);
    CHECK(r.settings.synth() == cases[c].high);
    // Saved Beads: only a new value writes.
    CHECK((test_writes == writes) == (cases[c].high == SYNTH_BEADS));
    // The indication is the value's colour: green, yellow, red.
    const OptionValue& indication = r.ui.options_.indication();
    CHECK(indication.setting == FEATURE_SYNTH);
    CHECK(indication.value == cases[c].high);
    CHECK(!memcmp(&indication.color, &kOptionColors[cases[c].high],
        sizeof(Leds::Color)));
    Settings loaded;
    CHECK(loaded.Init(false) && loaded.synth() == cases[c].high);
  }
  // Saved Rings on a unit without it: kept through a page commit.
  Rig r(false);
  r.settings.set_synth(SYNTH_RINGS);
  r.settings.SaveOptions();
  r.ui.LoadOptions();
  CHECK(r.ui.synth() == SYNTH_RINGS);
  r.EnterPage(2);
  r.feedback = .9f;
  r.Tick(1001, true);
  CHECK(r.settings.spectral());
  CHECK(r.settings.synth() == SYNTH_RINGS);
  CHECK(StoredByte(r.settings, FEATURE_SYNTH) == SYNTH_RINGS);
  // With Plaits alone, the saved Rings stays until the knob selects Plaits.
  r.ui.set_playable_synths(SynthMask(false, true, false));
  r.Tick(1001, true);
  CHECK(r.settings.synth() == SYNTH_RINGS);
  r.pot = .1f;
  r.Tick(1001, true);
  CHECK(r.settings.synth() == SYNTH_PLAITS);
}

// Spectral and the synth start are separate record bytes.
void TestEngineStorage() {
  test_checker.Start("TestEngineStorage");
  CheckRecordByte(FEATURE_PROCESSOR, 2);
  Rig r(false);
  for (int i = 0; i < 4; ++i) {
    r.settings.set_spectral(i & 1);
    r.settings.set_self_play_on_request(i & 2);
    CHECK(r.settings.spectral() == bool(i & 1));
    CHECK(r.settings.self_play_on_request() == bool(i & 2));
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.spectral() == bool(i & 1));
    CHECK(loaded.self_play_on_request() == bool(i & 2));
    CHECK(StoredByte(loaded, FEATURE_PROCESSOR) == (i & 1));
    CHECK(StoredByte(loaded, FEATURE_SYNTH_START) == (i & 2) / 2);
  }
  // Packed bytes an older Drizzlet left in State are ignored; a stock
  // State save keeps Spectral.
  r.settings.set_spectral(true);
  r.settings.SaveState();
  SaveStockState(&r.settings, 0x12);
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.spectral());
  CHECK(!loaded.evolving_feedback() && !loaded.attack_mode());
  CHECK(loaded.self_play_on_request());
  SaveStockState(&loaded, 0);
  CHECK(loaded.Init(false) && loaded.spectral());
}

// Page 3, FEEDBACK: Granular / Spectral. The saved processor reaches the
// audio code only once the page has closed.
void TestEngineOption() {
  test_checker.Start("TestEngineOption");
  const float positions[2] = { 0.15f, 0.85f };
  for (int target = 0; target < 2; ++target) {
    Rig r(false);
    r.settings.set_spectral(!target);
    r.settings.set_self_play_on_request(true);
    r.ui.LoadOptions();
    const bool was_spectral = r.settings.spectral();
    r.EnterPage(2);
    CHECK(r.ui.options_.page() == 2);
    CHECK(r.ui.options_.Value(FEATURE_PROCESSOR) == was_spectral);
    r.feedback = target ? 0.05f : 0.95f;
    r.Tick(50, true);
    for (int step = 0; step < 20; ++step) {
      const float from = target ? 0.05f : 0.95f;
      r.feedback = from + (positions[target] - from) * (step + 1) / 20.0f;
      r.Tick(10, true);
    }
    CHECK(r.ui.options_.Value(FEATURE_PROCESSOR) == bool(target));
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == FEATURE_PROCESSOR && v.value == target);
    CHECK(v.color.r == (target ? 255 : 0) && v.color.g == 255);
    CHECK(v.color.b == 0 && v.pulses == PULSE_STEADY);
    r.Tick(1100, true);
    CHECK(r.settings.spectral() == bool(target));
    CHECK(r.settings.self_play_on_request());  // Synth start is separate.
    CHECK(r.ui.spectral() == was_spectral);
    r.Down(SWITCH_ASSIGN);
    r.Tick(3000, true);
    r.Up(SWITCH_ASSIGN);
    r.Tick(50, true);
    CHECK(r.ui.options_.stage() == Options::STAGE_INACTIVE);
    CHECK(r.ui.spectral() == bool(target));
    Settings loaded;
    CHECK(loaded.Init(false) && loaded.spectral() == bool(target));
    CHECK(loaded.self_play_on_request());
  }
}

// Random intervals. A byte past its values reads as Free (Stock).
void TestIntervalsStorage() {
  test_checker.Start("TestIntervalsStorage");
  CheckRecordByte(FEATURE_INTERVALS, 3);
}

// Page 1, DRY/WET: Free / Fifths and octaves / Recent notes, independent of
// the scale.
void TestIntervalsOption() {
  test_checker.Start("TestIntervalsOption");
  const float positions[3] = { 0.05f, 0.5f, 0.95f };
  const uint8_t red[3] = { 0, 255, 255 };
  const uint8_t green[3] = { 255, 255, 0 };
  for (int target = 0; target < 3; ++target) {
    const RandomIntervals from = RandomIntervals((target + 1) % 3);
    Rig r(false);
    r.settings.set_pitch_scale(PITCH_SCALE_MAJOR);
    r.settings.set_random_intervals(from);
    r.ui.LoadOptions();
    r.Enter();
    CHECK(r.ui.options_.Value(FEATURE_INTERVALS) == from);
    r.pot = positions[from];
    r.Tick(50, true);
    r.pot = positions[target];
    r.Tick(50, true);
    CHECK(r.ui.options_.Value(FEATURE_INTERVALS) == target);
    CHECK(r.ui.intervals_mode() == target);
    CHECK(r.ui.random_intervals() == (target != 0));
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == FEATURE_INTERVALS && v.value == target);
    CHECK(v.color.r == red[target] && v.color.g == green[target]);
    r.Tick(1100, true);
    CHECK(r.settings.random_intervals() == target);
    CHECK(r.settings.pitch_scale() == PITCH_SCALE_MAJOR);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.random_intervals() == target);
    CHECK(StoredByte(loaded, FEATURE_INTERVALS) == target);
  }
}

// Page 3, REVERB: Automatic / Manual synth start, independent of the
// processor.
void TestSynthStartOption() {
  test_checker.Start("TestSynthStartOption");
  for (int target = 0; target < 2; ++target) {
    Rig r(false);
    r.settings.set_spectral(true);
    r.settings.set_self_play_on_request(!target);
    r.ui.LoadOptions();
    r.EnterPage(2);
    CHECK(r.ui.options_.Value(FEATURE_SYNTH_START) == !target);
    r.reverb = target ? 0.05f : 0.95f;
    r.Tick(50, true);
    r.reverb = target ? 0.85f : 0.15f;
    r.Tick(50, true);
    CHECK(r.ui.options_.Value(FEATURE_SYNTH_START) == bool(target));
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == FEATURE_SYNTH_START && v.value == target);
    r.Tick(1100, true);
    CHECK(r.settings.self_play_on_request() == bool(target));
    CHECK(r.settings.spectral());
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.self_play_on_request() == bool(target));
    CHECK(StoredByte(loaded, FEATURE_SYNTH_START) == target);
  }
}

void TestSelfPlayHold() {
  test_checker.Start("TestSelfPlayHold");
  for (int mode = 0; mode < 2; ++mode) {
    for (int no_input = 0; no_input < 2; ++no_input) {
      for (int frozen = 0; frozen < 2; ++frozen) {
        Rig r(false);
        r.settings.set_self_play_on_request(mode);
        r.ui.set_no_input(no_input);
        r.ui.freeze_led_state_ = frozen;
        r.settings.mutable_state()->gain = 123;
        uint32_t writes = test_writes;
        uint8_t media = r.settings.state().media;
        // While frozen, the hold restarts automatic gain as in stock.
        const bool request = mode && no_input && !frozen;
        for (int hold = 0; hold < 2; ++hold) {
          // The hold acts on release (#289), however long it lasts.
          r.Down(SWITCH_MEDIA);
          uint32_t start = r.ui.press_time_[SWITCH_MEDIA];
          while (stmlib::system_clock.milliseconds() - start < 1600) {
            r.Tick(1, true);
          }
          CHECK(!r.ui.self_play_requested());
          CHECK(r.gain.restarts == (request ? 0 : hold));
          r.Up(SWITCH_MEDIA);
          CHECK(r.ui.self_play_requested() == request);
          CHECK(r.gain.restarts == (request ? 0 : hold + 1));
          if (request) {
            CHECK(r.settings.state().gain == 123 && test_writes == writes);
          }
          r.ui.ClearSelfPlayRequest();
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
  // Rolling save: erase, four calibration blocks, one state block. The
  // options live in the mirror sector, which the rollover leaves alone.
  for (int cut = 0; cut <= 6; ++cut) {
    Rig r(false);
    const State defaults = r.settings.state();
    PersistentData* calibration = r.settings.mutable_persistent_data();
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      calibration->adc_calibration[i].scale *= 1.1f;
      calibration->adc_calibration[i].offset *= 0.9f;
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
    r.EnterPage(1);
    CHECK(!r.ui.options_.Value(FEATURE_REVERB));
    // Real settled saves: the REVERB knob alone flips the style each time,
    // writing a record (two flash words) and no state.
    const uint32_t erases = TestFlashErases();
    const uint16_t index = r.settings.chunk_storage_.next_state_chunk_index_;
    for (int i = 0; i < 2; ++i) {
      const uint32_t writes = test_writes;
      r.reverb = i ? .1f : .9f;
      r.Tick(1100, true);
      CHECK(test_writes == writes + 2);
    }
    CHECK(!r.settings.clouds_style());
    CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == index);
    r.reverb = .9f;
    r.Tick(1100, true);
    CHECK(r.settings.clouds_style());
    while (r.settings.chunk_storage_.next_state_chunk_index_ <=
        kStateChunks) {
      r.settings.SaveState();
    }
    CHECK(TestFlashErases() == erases);
    const State old_state = r.settings.state();
    State new_state = old_state;
    new_state.media = (old_state.media + 1) % 4;

    const uint32_t writes = test_writes;
    TestFlashBudget() = cut;
    r.settings.mutable_state()->media = new_state.media;
    r.settings.SaveState();
    TestFlashBudget() = -1;
    CHECK(TestFlashErases() == erases + (cut ? 1 : 0));
    CHECK(test_writes == writes + (cut > 1 ? cut - 1 : 0));

    Settings loaded;
    // Never the factory test, never locked.
    CHECK(loaded.Init(false));
    CHECK(!loaded.write_locked());
    const State& state = loaded.state();
    const AdcCalibrationData* data = loaded.persistent_data().adc_calibration;
    CHECK(!memcmp(data, old_calibration.adc_calibration,
        sizeof(old_calibration.adc_calibration)));
    if (cut == 0) {
      // Power went before the erase: the previous record is untouched.
      CHECK(!memcmp(&state, &old_state, sizeof(State)));
    } else if (cut <= 5) {
      // Erased, or partly or wholly rewritten without a state: the
      // calibration from the mirror, the state reset.
      CHECK(!memcmp(&state, &defaults, sizeof(State)));
    } else {
      CHECK(!memcmp(&state, &new_state, sizeof(State)));
      CHECK(loaded.chunk_storage_.next_state_chunk_index_ == 2);
    }
    CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_FAST);
    CHECK(loaded.self_play_on_request());
    CHECK(loaded.pitch_scale() == PITCH_SCALE_PENTATONIC_MAJOR);
    CHECK(loaded.clouds_style());
    CHECK(loaded.stereo_width() == 200);
    CHECK(loaded.width_assigned());
    CHECK(loaded.delay_mode() == DELAY_MODE_WIDE_DETENT);
    // Whatever came back is stable.
    Settings again;
    CHECK(again.Init(false));
    CHECK(again.write_locked() == loaded.write_locked());
    CHECK(!memcmp(&again.state(), &state, sizeof(State)));
    CHECK(!memcmp(again.persistent_data().adc_calibration, data,
        sizeof(old_calibration.adc_calibration)));
    CHECK(again.clouds_style());
  }
}

// Settings saves until the next one rewrites the sector.
void FillSettingsSector(Settings* settings) {
  while (settings->chunk_storage_.next_state_chunk_index_ <= kStateChunks) {
    settings->SaveState();
  }
}

bool SettingsLoad(const State& state, const PersistentData& calibration) {
  Settings loaded;
  return loaded.Init(false) && \
      !memcmp(&loaded.state(), &state, sizeof(State)) && \
      !memcmp(loaded.persistent_data().adc_calibration,
          calibration.adc_calibration, sizeof(calibration.adc_calibration));
}

const uint32_t kStaleFlag = 0x00040000;  // PGSERR

void TestStorageStaleErrorFlag() {
  test_checker.Start("TestStorageStaleErrorFlag");
  // A stale error flag: stmlib's rewrite skips the erase and programs over
  // the old records. The masked reader clears the ECC flags it raises.
  {
    Rig r(false);
    FillSettingsSector(&r.settings);
    PersistentData calibration = r.settings.persistent_data();
    State state = r.settings.state();
    stmlib::ChunkStorage<15, PersistentData, State> stock;
    stock.Init(&calibration, &state);
    const uint32_t overwrites = TestFlashOverwrites();
    TestFlashErrors() = kStaleFlag;
    stock.SavePersistentData();
    CHECK(TestFlashOverwrites() > overwrites);
  }
  // The Beads storage clears the flag first: one erase, no overwrite.
  Rig r(false);
  FillSettingsSector(&r.settings);
  r.settings.mutable_state()->media = 1;
  const uint32_t overwrites = TestFlashOverwrites();
  const uint32_t erases = TestFlashErases();
  const uint32_t refusals = TestFlashRefusals();
  TestFlashErrors() = kStaleFlag;
  r.settings.SaveState();
  CHECK(TestFlashErrors() == 0);
  CHECK(TestFlashRefusals() == refusals);  // Cleared before the erase.
  CHECK(TestFlashOverwrites() == overwrites);
  CHECK(TestFlashErases() == erases + 1);
  CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
  CHECK(SettingsLoad(r.settings.state(), r.settings.persistent_data()));
  // The same on an ordinary save, which appends a record.
  r.settings.mutable_state()->media = 2;
  TestFlashErrors() = kStaleFlag;
  r.settings.SaveState();
  CHECK(TestFlashRefusals() == refusals);  // Cleared before the program.
  CHECK(TestFlashOverwrites() == overwrites);
  CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 3);
  CHECK(SettingsLoad(r.settings.state(), r.settings.persistent_data()));
}

void TestStorageFailedErase() {
  test_checker.Start("TestStorageFailedErase");
  for (int stuck = 0; stuck < 2; ++stuck) {
    Rig r(false);
    FillSettingsSector(&r.settings);
    const State old_state = r.settings.state();
    const PersistentData calibration = r.settings.persistent_data();
    r.settings.mutable_state()->media = 1;
    const uint32_t overwrites = TestFlashOverwrites();
    // Every attempt refused, or reported done with the sector unchanged.
    if (stuck) {
      TestFlashStuckErases() = 100;
    } else {
      TestFlashFailures() = 100;
    }
    r.settings.SaveState();
    TestFlashStuckErases() = 0;
    TestFlashFailures() = 0;
    // Nothing programmed over the old records, which still load.
    CHECK(TestFlashOverwrites() == overwrites);
    CHECK(SettingsLoad(old_state, calibration));
    // The settings stay in RAM and the next save rewrites the sector.
    r.settings.SaveState();
    CHECK(TestFlashOverwrites() == overwrites);
    CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
    CHECK(SettingsLoad(r.settings.state(), calibration));
  }
}

void TestStorageNonBlankSlot() {
  test_checker.Start("TestStorageNonBlankSlot");
  // A record is never programmed over a free chunk that isn't blank.
  Rig r(false);
  r.settings.SaveState();
  const uint16_t index = r.settings.chunk_storage_.next_state_chunk_index_;
  uint8_t* slot = reinterpret_cast<uint8_t*>(
      0x081e0000 + kCalibrationChunk + kStateChunk * (index - 1));
  slot[5] = 0;
  r.settings.mutable_state()->media = 1;
  const uint32_t overwrites = TestFlashOverwrites();
  r.settings.SaveState();
  CHECK(TestFlashOverwrites() == overwrites);
  CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
  CHECK(SettingsLoad(r.settings.state(), r.settings.persistent_data()));
}

void TestStorageFailedProgram() {
  test_checker.Start("TestStorageFailedProgram");
  for (int failures = 1; failures <= 100; failures += 99) {
    Rig r(false);
    r.settings.mutable_state()->media = 1;
    r.settings.SaveState();
    const State old_state = r.settings.state();
    const PersistentData calibration = r.settings.persistent_data();
    const uint16_t index = r.settings.chunk_storage_.next_state_chunk_index_;
    r.settings.mutable_state()->media = 2;
    const uint32_t overwrites = TestFlashOverwrites();
    TestFlashFailures() = failures;
    r.settings.SaveState();
    TestFlashFailures() = 0;
    CHECK(TestFlashOverwrites() == overwrites);
    if (failures == 1) {
      // A refused program is retried in place.
      CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == index + 1);
      CHECK(SettingsLoad(r.settings.state(), calibration));
    } else {
      // Refused every time: the previous record still loads.
      CHECK(SettingsLoad(old_state, calibration));
      r.settings.SaveState();
      CHECK(TestFlashOverwrites() == overwrites);
      CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
      CHECK(SettingsLoad(r.settings.state(), calibration));
    }
  }
}

void TestStorageMatchesStmlib() {
  test_checker.Start("TestStorageMatchesStmlib");
  // Normal saves leave the sector byte for byte as stmlib's storage does.
  static uint8_t ours[0x20000];
  uint8_t* sector = reinterpret_cast<uint8_t*>(0x081e0000);
  for (int pass = 0; pass < 2; ++pass) {
    memset(reinterpret_cast<void*>(0x081c0000), 255, 0x40000);
    PersistentData calibration;
    State state;
    memset(&calibration, 0, sizeof(calibration));
    memset(&state, 0, sizeof(state));
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      calibration.adc_calibration[i].scale = -2.5f - 0.125f * i;
      calibration.adc_calibration[i].offset = 0.75f + 0.0625f * i;
    }
    if (pass == 0) {
      OptionsRecord record;
      memset(&record, 0, sizeof(record));
      ChunkStorage<15, 14, PersistentData, State, OptionsRecord> storage;
      storage.Init(&calibration, &state, &record, false);
      for (int i = 0; i < kStateChunks + 10; ++i) {
        state.media = i & 3;
        state.gain = uint8_t(i);
        storage.SaveState();
      }
      storage.SavePersistentData();
      memcpy(ours, sector, sizeof(ours));
    } else {
      stmlib::ChunkStorage<15, PersistentData, State> storage;
      storage.Init(&calibration, &state);
      for (int i = 0; i < kStateChunks + 10; ++i) {
        state.media = i & 3;
        state.gain = uint8_t(i);
        storage.SaveState();
      }
      storage.SavePersistentData();
      CHECK(!memcmp(ours, sector, sizeof(ours)));
    }
  }
}

void TestPitchStorage() {
  test_checker.Start("TestPitchStorage");
  // The factory profile's scales end at Raag Shri; IDs 10-15 are custom.
  CheckRecordByte(FEATURE_PITCH, PITCH_SCALE_SHRI + 1);
  Rig r(false);
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.set_self_play_on_request(true);
  for (int i = 0; i <= PITCH_SCALE_SHRI; ++i) {
    r.settings.set_pitch_scale(PitchScale(i));
    r.settings.SaveState();
    CHECK(r.settings.Init(false));
    CHECK(r.settings.pitch_scale() == i);
    CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    CHECK(r.settings.self_play_on_request());
    CHECK(r.settings.state().padding[0] == 0);
  }
}

void TestPlaybackStorage() {
  test_checker.Start("TestPlaybackStorage");
  // Only a saved target of 1 assigns the width.
  for (int raw = 0; raw < 256; ++raw) {
    Rig r(false);
    r.settings.set_stereo_width(128);
    r.settings.SaveState();
    r.settings.record_.cv_target = raw;
    CHECK(r.settings.chunk_storage_.SaveRecord());
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.width_assigned() == (raw == 1));
    CHECK(!loaded.diffusion_assigned());
  }

  Rig r(false);
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
      CHECK(loaded.stored_record_.cv_target == (width ? 1 : 0));
      CHECK(loaded.delay_mode() == DelayMode(delay));
      CHECK(StoredByte(loaded, FEATURE_PITCH) == PITCH_SCALE_SHRI);
      CHECK(StoredByte(loaded, FEATURE_REVERB) == 1);
      CHECK(StoredByte(loaded, FEATURE_WIDTH) == width);
      CHECK(StoredByte(loaded, FEATURE_DELAY) == delay);
      for (size_t byte = 0; byte < sizeof(State) - 10; ++byte) {
        CHECK(loaded.state().padding[byte] == 0);
      }
    }
  }
}

void TestWidthAssignCycle() {
  test_checker.Start("TestWidthAssignCycle");
  Rig r(false);
  r.settings.mutable_state()->assignable_cv_destination = 3;
  r.Down(SWITCH_ASSIGN);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.state().assignable_cv_destination == 0);
  for (int enabled = 0; enabled < 2; ++enabled) {
    r.settings.set_stereo_width(enabled ? 128 : 0);
    r.settings.mutable_state()->assignable_cv_destination = 0;
    r.Tick(20, true);
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
  Rig r(false);
  const uint8_t media = r.settings.state().media;
  const uint8_t gain = r.settings.state().gain;
  CHECK(r.settings.FeatureValue(FEATURE_DIFFUSION) == 0);
  CHECK(r.settings.diffusion() == 0.0f);
  uint32_t writes = test_writes;
  r.Down(SWITCH_MEDIA);
  const float positions[] = { 0.125f, 0.75f, 0.0f };
  for (int i = 0; i < 3; ++i) {
    r.reverb = positions[i];
    r.Tick(20, true);
    CHECK(r.settings.FeatureValue(FEATURE_DIFFUSION) ==
        1 + int(positions[i] * 254.0f + 0.5f));
    CHECK(fabsf(r.settings.diffusion() - positions[i]) < 0.003f);
    CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_REVERB) == 0.5f);
    CHECK(r.settings.state().gain == gain);
    CHECK(test_writes == writes);
  }
  r.reverb = 0.5f;
  r.Tick(20, true);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);  // The meter, not quality.
  r.Tick(1700, true);  // Holding while turning never restarts the gain.
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  CHECK(r.settings.state().gain == gain);
  CHECK(test_writes == writes + kRecordWords + 1);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.FeatureValue(FEATURE_DIFFUSION) == 128);
  // A quality tap without turning REVERB leaves the amount alone.
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  CHECK(r.settings.FeatureValue(FEATURE_DIFFUSION) == 128);
}

void TestDiffusionStorage() {
  test_checker.Start("TestDiffusionStorage");
  for (int clouds = 0; clouds < 2; ++clouds) {
    for (int raw = 0; raw < 256; ++raw) {
      Rig r(false);
      r.settings.set_clouds_style(clouds);
      r.settings.values_[FEATURE_DIFFUSION] = raw;
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
      CHECK(loaded.FeatureValue(FEATURE_DIFFUSION) == raw);
      CHECK(loaded.diffusion_assigned() == assignable);
      CHECK(!loaded.width_assigned());
      CHECK(loaded.state().assignable_cv_destination == (assignable ? 0 : 2));
    }
  }
  Rig r(false);
  r.settings.set_diffusion(0.5f);
  r.settings.set_stereo_width(128);
  r.settings.set_diffusion_assigned(true);
  r.settings.set_stereo_width(0);  // Width off keeps the diffusion target.
  CHECK(r.settings.diffusion_assigned());
  r.settings.set_diffusion(0.0f);  // No diffusion left to modulate.
  CHECK(!r.settings.diffusion_assigned());
  CHECK(r.settings.FeatureValue(FEATURE_DIFFUSION) == 1);
  r.settings.set_diffusion(1.02f);  // A pot reading past the end.
  CHECK(r.settings.FeatureValue(FEATURE_DIFFUSION) == 255);
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
    Rig r(false);
    r.settings.set_stereo_width(c & 1 ? 128 : 0);
    if (c & 2) {
      r.settings.set_diffusion(0.4f);
    }
    r.settings.mutable_state()->assignable_cv_destination = 0;
    r.Tick(20, true);
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
    Rig r(false);
    r.settings.set_clouds_style(true);
    r.settings.mutable_state()->assignable_cv_destination = 2;
    r.Tap();
    CHECK(!r.settings.diffusion_assigned());
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
  // [M]+knob amounts replace the DIFFUSION target.
  Rig r(false);
  r.settings.set_diffusion(0.4f);
  r.settings.mutable_state()->assignable_cv_destination = 2;
  r.Tap();
  CHECK(r.settings.diffusion_assigned());
  r.Down(SWITCH_ASSIGN);
  r.feedback = 0.9f;
  r.Tick(20, true);
  r.Up(SWITCH_ASSIGN);
  CHECK(!r.settings.diffusion_assigned());
  CHECK(r.settings.state().assignable_cv_destination == 3);
}

// [M] moves the CV from width to diffusion in one store: the audio interrupt
// never sees the target cleared in between.
void TestWidthToDiffusionTarget() {
  test_checker.Start("TestWidthToDiffusionTarget");
  Rig r(false);
  r.settings.set_stereo_width(128);
  r.settings.set_diffusion(0.4f);
  r.settings.mutable_state()->assignable_cv_destination = 2;
  r.Tick(20, true);
  r.Tap();
  CHECK(r.settings.width_assigned());
  watched_settings = &r.settings;
  watched_targets = 0;
  r.Tap();
  watched_settings = NULL;
  CHECK(r.settings.diffusion_assigned());
  CHECK(watched_targets == ((1 << 1) | (1 << 2)));
  // Without a diffusion amount, width hands over to FEEDBACK as before.
  Rig none(false);
  none.settings.set_stereo_width(128);
  none.settings.mutable_state()->assignable_cv_destination = 2;
  none.Tick(20, true);
  none.Tap();
  CHECK(none.settings.width_assigned());
  none.Tap();
  CHECK(!none.settings.width_assigned());
  CHECK(!none.settings.diffusion_assigned());
  CHECK(none.settings.state().assignable_cv_destination == 0);
}

// [A]+DRY/WET sets the stereo width as Beads' [A]+FEEDBACK sets the gain:
// fully CCW is stock, and the amount saves when [A] is released.
void TestWidthGestureAndMeter() {
  test_checker.Start("TestWidthGestureAndMeter");
  Rig r(false);
  const uint8_t media = r.settings.state().media;
  r.settings.set_stereo_width(128);
  r.settings.mutable_state()->assignable_cv_destination = 2;
  r.settings.set_width_assigned(true);
  const uint32_t writes = test_writes;
  r.Down(SWITCH_MEDIA);
  r.pot = .9f;
  r.Tick(20, true);
  CHECK(r.settings.stereo_width() ==
      1 + int((.9f - .13f) / .87f * 254.0f + .5f));
  CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_DRY_WET) == 0.5f);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);  // The meter, not quality.
  // Every code on Beads' meter, below its wrap at 1.0.
  for (int width = 0; width <= 255; ++width) {
    r.settings.values_[FEATURE_WIDTH] = width;
    r.ui.UpdateLeds();
    Leds meter;
    meter.Init();
    meter.set_meter(width ? .25f + .749f * float(width - 1) / 254.0f : .05f);
    for (int channel = LED_MEDIA_R; channel <= LED_MEDIA_B; ++channel) {
      CHECK(r.ui.leds_.intensity_[channel] == meter.intensity_[channel]);
    }
  }
  r.pot = .1f;  // Inside the dead band: still on.
  r.Tick(20, true);
  CHECK(r.settings.stereo_width() == 1);
  CHECK(r.settings.width_assigned());
  r.pot = .06f;
  r.Tick(20, true);
  CHECK(r.settings.stereo_width() == 0);
  CHECK(!r.settings.width_assigned());
  r.pot = .1f;  // Off stays off inside the dead band.
  r.Tick(20, true);
  CHECK(r.settings.stereo_width() == 0);
  r.pot = .5f;
  r.Tick(1700, true);  // Holding while turning never restarts the gain.
  CHECK(test_writes == writes);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  CHECK(r.gain.restarts == 0);
  CHECK(test_writes == writes + kRecordWords + 1);
  const StereoWidth width = r.settings.stereo_width();
  CHECK(width > 1);
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.stereo_width() == width);
  // A quality tap without turning DRY/WET leaves the width alone.
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  CHECK(r.settings.stereo_width() == width);
  // The options page no longer holds the width.
  r.Tick(30, true);
  r.Enter();
  for (int page = 0; page < kNumOptionPages; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      CHECK(kOptionPages[page].knobs[knob] != FEATURE_NONE ||
          page == 0 || page >= 3);
    }
  }
  r.pot = .02f;
  r.Tick(1100, true);
  CHECK(r.settings.stereo_width() == width);
}

void TestOptionsTablesAndLifetime() {
  test_checker.Start("TestOptionsTablesAndLifetime");
  const ProfileFeature expected[5][3] = {
    { FEATURE_PITCH, FEATURE_INTERVALS, FEATURE_ATTACKS },
    { FEATURE_FROZEN, FEATURE_DELAY, FEATURE_REVERB },
    { FEATURE_PROCESSOR, FEATURE_SYNTH, FEATURE_SYNTH_START },
    { FEATURE_BRIGHTNESS, FEATURE_SEED_LATCH, FEATURE_LIMITER },
    { FEATURE_RESONATOR, FEATURE_RESONATOR_MATERIAL, FEATURE_RESONATOR_RING }
  };
  CHECK(kNumOptionPages == 7);
  // Pages 1-3 light one LED, then two together, then all three.
  const uint8_t masks[7] = { 1, 2, 4, 3, 5, 6, 7 };
  for (int page = 0; page < kNumOptionPages; ++page) {
    CHECK(kOptionPages[page].leds == masks[page]);
    for (int knob = 0; knob < 3; ++knob) {
      CHECK((kOptionPages[page].knobs[knob] == FEATURE_NONE) == (page >= 5));
    }
  }
  Rig r(false);
  r.Enter();
  r.Tick(100, true);  // Entry confirmation has ended.
  for (int page = 0; page < 5; ++page) {
    CHECK(r.ui.options_.page() == page);
    const int preview = 0;
    CHECK(r.ui.options_.preview_knob() == preview);
    for (int knob = 0; knob < 3; ++knob) {
      CHECK(kOptionPages[page].knobs[knob] == expected[page][knob]);
      const int led = LED_ASSIGN_1 + knob;
      if (knob == preview) {
        CHECK(r.ui.leds_.intensity_[led] == 255);
      } else if (!(masks[page] & (1 << knob))) {
        CHECK(r.ui.leds_.intensity_[led] == 0);
      }
    }
    r.Tap();
  }
  CHECK(r.ui.options_.page() == 0);
  r.Tap();
  r.Tick(60000, true);
  CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  r.Tick(30, true);
  r.Enter();
  CHECK(r.ui.options_.page() == 1);
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000, true);
  r.Up(SWITCH_ASSIGN);
  CHECK(!r.ui.options_.active());
  r.Tick(30, true);
  r.Enter();
  CHECK(r.ui.options_.page() == 1);
  Rig startup(true);
  startup.Tick(3500, true);
  CHECK(!startup.ui.options_.active());
}

void TestOptionsKnobsAndSettling() {
  test_checker.Start("TestOptionsKnobsAndSettling");
  // Every physical knob of the four factory pages.
  for (int page = 0; page < 4; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      Rig r(false);
      r.Enter();
      for (int i = 0; i < page; ++i) {
        r.Tap();
      }
      float* pot = knob == 0 ? &r.feedback : knob == 1 ? &r.pot : &r.reverb;
      const ProfileFeature setting = kOptionPages[page].knobs[knob];
      const State state = r.settings.state();
      const uint32_t writes = test_writes;
      *pot = 0.59f;
      r.Tick(1500, true);
      CHECK(!r.ui.options_.qualified_[knob]);
      CHECK(test_writes == writes);
      *pot = 0.9f;
      r.Tick(100, true);
      if (setting == FEATURE_NONE) {
        CHECK(!r.ui.options_.qualified_[knob]);
        CHECK(!memcmp(&state, &r.settings.state(), sizeof(state)));
        r.Tick(2000, true);
        CHECK(test_writes == writes);
        continue;
      }
      CHECK(r.ui.options_.selected_ == setting);
      CHECK(r.ui.options_.qualified_[knob]);
      for (int step = 0; step < 20; ++step) {
        *pot = 0.95f - 0.045f * step;
        r.Tick(100, true);
      }
      *pot = 0.9f;
      r.Tick(999, true);
      CHECK(test_writes == writes);
      r.Tick(2, true);
      CHECK(test_writes == writes + kRecordWords);
      CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
      CHECK(r.ui.options_.Value(setting) == (setting == FEATURE_PITCH
          ? int(r.settings.pitch_scale()) : setting == FEATURE_DELAY
          ? int(r.settings.delay_mode()) : setting == FEATURE_REVERB
          ? r.settings.clouds_style() : setting == FEATURE_PROCESSOR
          ? r.settings.spectral() : setting == FEATURE_SYNTH_START
          ? r.settings.self_play_on_request() : setting == FEATURE_INTERVALS
          ? int(r.settings.random_intervals()) : setting == FEATURE_FROZEN
          ? r.settings.evolving_feedback() : setting == FEATURE_SYNTH
          ? r.settings.synth() : setting == FEATURE_ATTACKS
          ? int(r.settings.attack_mode()) : setting == FEATURE_BRIGHTNESS
          ? r.settings.FeatureValue(FEATURE_BRIGHTNESS)
          : setting == FEATURE_SEED_LATCH
          ? r.settings.FeatureValue(FEATURE_SEED_LATCH)
          : int(r.settings.output_limiter_mode())));
      r.Tick(5000, true);
      CHECK(test_writes == writes + kRecordWords);
      CHECK(r.ui.options_.active());
    }
  }
  // A leave button keeps a pending change and waits for its one settled save.
  for (int button = 0; button < SWITCH_LAST; ++button) {
    if (button == SWITCH_ASSIGN) {
      continue;
    }
    Rig r(false);
    r.Enter();
    const uint32_t writes = test_writes;
    r.reverb = .9f;
    r.Tick(10, true);
    r.Down(Switch(button));
    r.Up(Switch(button));
    CHECK(!r.ui.options_.active());
    r.Tick(1000, true);
    CHECK(!r.ui.options_.active());
    CHECK(r.settings.attack_mode() == ATTACK_MODE_SNAP_AND_TRIGGER);
    CHECK(test_writes == writes + kRecordWords);
  }
}

void TestOptionsSeed() {
  test_checker.Start("TestOptionsSeed");
  Rig saved(false);
  saved.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
  saved.Enter();
  saved.Down(SWITCH_SEED);
  saved.Up(SWITCH_SEED);
  CHECK(saved.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_MAJOR);
  CHECK(!saved.ui.options_.qualified_[0]);
  Rig r(false);
  r.Enter();
  r.feedback = .9f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_FIFTHS);
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
    CHECK(r.ui.options_.Value(FEATURE_PITCH) ==
        PitchScale(PITCH_SCALE_FIFTHS + (i + 1) % 8));
    CHECK(r.ui.options_.active());
  }
  r.Down(SWITCH_SEED);
  r.Tick(600, true);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_FIFTHS);
  r.Tick(1001, true);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_FIFTHS);
}

void TestOptionsZonesAndSaveBatch() {
  test_checker.Start("TestOptionsZonesAndSaveBatch");
  Rig r(false);
  r.Enter();
  // Pitch thirds, including hysteresis around both boundaries.
  r.feedback = .1f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_OFF);
  r.feedback = .35f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_OFF);
  r.feedback = .38f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_CHROMATIC);
  r.feedback = .68f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_CHROMATIC);
  r.feedback = .71f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_FIFTHS);
  r.feedback = .65f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_FIFTHS);
  r.feedback = .62f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_CHROMATIC);
  r.feedback = .29f;
  r.Tick(1, true);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_OFF);
  // Halves retain a value inside the midpoint dead band; two knobs
  // settle in one batch.
  {
    Rig halves(false);
    halves.EnterPage(1);
    halves.reverb = .9f;
    halves.Tick(1, true);
    halves.reverb = .5f;
    halves.Tick(1, true);
    CHECK(halves.ui.options_.Value(FEATURE_REVERB));
    halves.reverb = .46f;
    halves.Tick(1, true);
    CHECK(!halves.ui.options_.Value(FEATURE_REVERB));
    halves.reverb = .54f;
    halves.Tick(1, true);
    const uint32_t writes = test_writes;
    halves.feedback = .9f;
    halves.Tick(1001, true);
    CHECK(test_writes == writes + kRecordWords);
    CHECK(halves.settings.clouds_style());
    CHECK(halves.settings.evolving_feedback());
    Settings loaded;
    loaded.Init(false);
    CHECK(loaded.clouds_style() && loaded.evolving_feedback());
  }
  // Main may be delayed: published batch must stay stable until consumed.
  r.feedback = .9f;
  r.Tick(1001, false);
  CHECK(r.ui.options_.commit_requested());
  r.feedback = .1f;
  r.Tick(2000, false);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_FIFTHS);
  // Exit remains responsive even if main is delayed by a flash operation.
  test_down[SWITCH_MEDIA] = true;
  r.Tick(8, false);
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(8, false);
  r.ui.DoEvents();
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.Tick(10, true);
  CHECK(!r.ui.options_.active());
}

void TestOptionsPalette() {
  test_checker.Start("TestOptionsPalette");
  // Every page value lights, shared colours included; pitch up to Raag Shri.
  OptionValue values[64];
  size_t count = 0;
  for (int i = 0; i < kNumPageFeatures; ++i) {
    const ProfileFeature f = kPageFeatures[i];
    const int n = f == FEATURE_PITCH
        ? int(PITCH_SCALE_LAST) : int(kFeatureDefinitions[f].values);
    for (int value = 0; value < n && count < 64; ++value) {
      const OptionValue v = FindOptionValue(f, value);
      CHECK(v.setting == f && v.value == value);
      values[count++] = v;
    }
  }
  CHECK(count > kNumOptionValues);
  CHECK(FindOptionValue(FEATURE_PITCH, PITCH_SCALE_LAST).setting ==
      FEATURE_NONE);
  // Only page values light, and only the values a feature has.
  CHECK(FindOptionValue(FEATURE_REVERB, 2).setting == FEATURE_NONE);
  CHECK(FindOptionValue(FEATURE_ATTACK_SENSITIVITY, 0).setting ==
      FEATURE_NONE);
  for (size_t i = 0; i < count; ++i) {
    const OptionValue& v = values[i];
    CHECK(v.color.r || v.color.g || v.color.b);
    if (v.value == 0) {
      CHECK(v.color.r == 0 && v.color.g == 255 &&
          v.color.b == 0 && v.pulses == 0);
    }
    if (v.setting != FEATURE_PITCH) {
      CHECK(v.color.b == 0);
      CHECK((v.color.r == 0 && v.color.g == 255) ||
          (v.color.r == 255 && (v.color.g == 0 || v.color.g == 255)));
    }
    for (size_t j = i + 1; j < count; ++j) {
      const OptionValue& w = values[j];
      if (v.setting == w.setting) {
        CHECK(v.color.r != w.color.r || v.color.g != w.color.g ||
            v.color.b != w.color.b || v.pulses != w.pulses);
      }
    }
  }
  // Renderer consumes the table at every phase of its pulse period.
  Rig r(false);
  r.Enter();
  for (size_t i = 0; i < count; ++i) {
    const OptionValue& v = values[i];
    r.ui.options_.selected_ = v.setting;
    r.ui.options_.preview_[v.setting] = v.value;
    const OptionValue& indication = r.ui.options_.indication();
    CHECK(indication.setting == v.setting && indication.value == v.value);
    CHECK(indication.color.r == v.color.r && indication.color.g == v.color.g &&
        indication.color.b == v.color.b && indication.pulses == v.pulses);
    for (int tick = 0; tick < 2048; ++tick) {
      stmlib::system_clock.Tick();
      r.ui.UpdateLeds();
      // Blinks start with the setting they show.
      const uint32_t now = stmlib::system_clock.milliseconds() -
          r.ui.options_.indication_time();
      const uint32_t phase = now % 2048;
      const bool lit = v.pulses == 0 ||
          (v.pulses == 1 && now % 1024 < 512) ||
          (v.pulses == 2 && now % 256 < 128) ||
          (v.pulses == 3 && (phase < 128 || (phase >= 256 && phase < 384)));
      const uint8_t level = r.ui.options_.Value(FEATURE_BRIGHTNESS);
      const int percent = level == 0 ? 100 : level == 1 ? 66 : 33;
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
          (lit ? DimmedMedia(0, v.color.r, percent) : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] ==
          (lit ? DimmedMedia(1, v.color.g, percent) : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] ==
          (lit ? DimmedMedia(2, v.color.b, percent) : 0));
    }
  }
}

void TestOptionsConfirmations() {
  test_checker.Start("TestOptionsConfirmations");
  Rig r(false);
  r.Enter();
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  const uint32_t entry = r.ui.options_flash_time_;
  const uint32_t writes = test_writes;
  r.reverb = .9f;  // Ends the preview.
  r.Tick(1010, true);
  CHECK(r.ui.options_.preview_knob() == -1);
  CHECK(test_writes == writes + kRecordWords);
  CHECK(r.ui.options_flash_time_ != entry);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  const uint32_t saved = r.ui.options_flash_time_;
  r.Tick(1100, true);
  CHECK(r.ui.options_flash_time_ == saved);  // No write, no confirmation.
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] < 255);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] == 0);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.options_flash_time_ != saved);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  r.Up(SWITCH_FREEZE);
  r.Tick(100, true);
  CHECK(stmlib::system_clock.milliseconds() - r.ui.options_flash_time_ >= 100);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] < 255);
  // Page four flashes FEEDBACK and DRY/WET.
  Rig last(false);
  last.EnterLimiter();
  last.reverb = .9f;
  last.Tick(1010, true);
  CHECK(last.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  CHECK(last.ui.leds_.intensity_[LED_ASSIGN_2] == 255);
  CHECK(last.ui.leds_.intensity_[LED_ASSIGN_3] == 0);
  CHECK(last.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
}

// Opening a page shows each stored value in turn, left to right, with its
// knob lit; touching a knob ends it.
void TestOptionsPreview() {
  test_checker.Start("TestOptionsPreview");
  Rig r(false);
  r.settings.set_delay_mode(DELAY_MODE_WIDE_DETENT);
  r.settings.set_clouds_style(true);
  r.settings.set_evolving_feedback(false);
  r.ui.LoadOptions();
  r.EnterPage(1);
  const ProfileFeature order[3] = {
    FEATURE_FROZEN, FEATURE_DELAY, FEATURE_REVERB
  };
  const int values[3] = { 0, DELAY_MODE_WIDE_DETENT, 1 };
  for (int knob = 0; knob < 3; ++knob) {
    CHECK(r.ui.options_.preview_knob() == knob);
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == order[knob] && v.value == values[knob]);
    r.Tick(1, true);
    CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + knob] == 255);
    r.Tick(2048 - 1, true);
  }
  CHECK(r.ui.options_.preview_knob() == -1);
  CHECK(r.ui.options_.indication().setting == FEATURE_FROZEN);
  // Page four previews brightness, latch and limiter; a knob ends preview.
  r.Tap();
  r.Tap();
  CHECK(r.ui.options_.page() == 3);
  CHECK(r.ui.options_.preview_knob() == 0);
  r.Tap();
  CHECK(r.ui.options_.page() == 4);
  r.Tap();
  CHECK(r.ui.options_.page() == 0);
  CHECK(r.ui.options_.preview_knob() == 0);
  r.Tick(2048, true);
  CHECK(r.ui.options_.preview_knob() == 1);
  r.reverb = .9f;
  r.Tick(1, true);
  CHECK(r.ui.options_.preview_knob() == -1);
  CHECK(r.ui.options_.indication().setting == FEATURE_ATTACKS);
  const uint32_t writes = test_writes;
  r.Tick(1100, true);
  CHECK(test_writes == writes + kRecordWords);
  // Each shown value starts its blink pattern, even on the same setting.
  Rig phase(false);
  phase.Enter();
  phase.Tick(500, true);
  for (int page = 0; page < kNumOptionPages; ++page) {
    phase.Tap();
    CHECK(stmlib::system_clock.milliseconds() -
        phase.ui.options_.indication_time() <= 8);
    phase.Tick(500, true);
  }
  // The preview alone never writes.
  Rig idle(false);
  const uint32_t idle_writes = test_writes;
  idle.Enter();
  idle.Tick(10000, true);
  CHECK(test_writes == idle_writes);
}

bool IsVolatile(bool*) { return false; }
bool IsVolatile(volatile bool*) { return true; }

void TestCommitPublication() {
  test_checker.Start("TestCommitPublication");
  Rig r(false);
  CHECK(IsVolatile(&r.ui.options_.dirty_));
  CHECK(IsVolatile(&r.ui.options_.commit_));
}

void TestPendingSeed() {
  test_checker.Start("TestPendingSeed");
  for (int hold = 0; hold < 3; ++hold) {
    Rig r(false);
    r.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
    r.Enter();
    r.reverb = .9f;
    r.Tick(1001, false);
    CHECK(r.ui.options_.commit_requested());
    test_down[SWITCH_SEED] = true;
    r.Tick(hold ? 600 : 20, false);
    if (hold == 2) {
      r.ui.DoEvents();
    }
    test_down[SWITCH_SEED] = false;
    r.Tick(20, false);
    CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_FIFTHS);
    r.ui.DoEvents();
    r.Tick(1, true);
    CHECK(r.ui.options_.Value(FEATURE_PITCH) ==
        (hold ? PITCH_SCALE_FIFTHS : PITCH_SCALE_MAJOR));
    r.Tick(1001, true);
    CHECK(r.settings.pitch_scale() ==
        (hold ? PITCH_SCALE_FIFTHS : PITCH_SCALE_MAJOR));
  }
  // Press before publication; release during it, then delay main long enough
  // that using the consumption time would misclassify this as a long hold.
  Rig before(false);
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
  before.Tick(1, true);
  CHECK(before.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_MAJOR);
  before.Tick(1001, true);
  CHECK(before.settings.pitch_scale() == PITCH_SCALE_MAJOR);
}

// A brightness chosen just before the pages close holds until it is saved.
void TestBrightnessHoldsAfterClose() {
  test_checker.Start("TestBrightnessHoldsAfterClose");
  Rig r(false);
  r.EnterPage(3);
  r.feedback = 0.9f;  // 33%.
  r.Tick(40, true);
  CHECK(r.ui.options_.Value(FEATURE_BRIGHTNESS) == 2);
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(!r.ui.options_.active());
  r.ui.set_freeze_led_state(true);
  for (int t = 0; t < 1100; ++t) {
    r.Tick(1, true);
    CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 255 * 33 / 100);
  }
  CHECK(r.ui.options_.stage() == Options::STAGE_INACTIVE);
  CHECK(r.settings.FeatureValue(FEATURE_BRIGHTNESS) == 2);
}

void TestPendingExitButtons() {
  test_checker.Start("TestPendingExitButtons");
  Rig r(false);
  r.Enter();
  const uint32_t writes = test_writes;
  r.reverb = .9f;
  r.Tick(10, true);
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(!r.ui.options_.active());
  r.ui.set_overwrite_reserve(1.0f);
  r.Tick(1, true);
  const Leds::Color normal = r.ui.media_palette_[r.settings.state().media];
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == normal.r);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == normal.g);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == normal.b);
  const bool frozen = r.ui.freeze_button_state_;
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(r.ui.freeze_button_state_ != frozen);
  r.Tick(1001, true);
  CHECK(r.settings.attack_mode() == ATTACK_MODE_SNAP_AND_TRIGGER);
  CHECK(test_writes == writes + kRecordWords);
}

void CustomProfile(uint8_t* b, int id, int page, int amount, int action) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  b[5] = id ? 1 : 0;
  b[6] = id ? 6 : 0;
  b[16] = id;
  b[17] = PROFILE_ADJUSTABLE;
  b[18] = id == FEATURE_SYNTH_START ? 1 : 0;
  b[19] = page;
  b[20] = amount;
  b[21] = action;
  const size_t size = 16 + b[6];
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

void ApplyCustomProfile(Rig* r, const uint8_t* b) {
  CHECK(r->settings.Init(false, b));
  CHECK(!r->settings.profile_locked());
  r->ui.Init(&r->settings, &r->gain, &r->calibration, &r->backup);
  r->ui.LoadOptions();
  r->Tick(40, true);
}

// Appends an entry with no slots to a profile without scales.
void AppendEntry(uint8_t* b, ProfileFeature feature, int state, int value) {
  CHECK(!b[13]);
  uint8_t* entry = b + 16 + 6 * b[5];
  entry[0] = feature;
  entry[1] = state;
  entry[2] = value;
  entry[3] = entry[4] = entry[5] = 0;
  ++b[5];
  const size_t bytes = 6 * b[5];
  b[6] = bytes & 255;
  b[7] = bytes >> 8;
  const uint32_t crc = Profile::Crc32(b, 16 + bytes);
  for (int i = 0; i < 4; ++i) {
    b[16 + bytes + i] = crc >> (8 * i);
  }
}

void AppendIdleTimeout(uint8_t* b, int state, int value) {
  AppendEntry(b, FEATURE_IDLE_TIMEOUT, state, value);
}

void IdleProfile(uint8_t* b, int state, int value) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  AppendIdleTimeout(b, state, value);
}

bool Idle(Rig* r, int ticks) {
  r->Tick(ticks, true);
  return r->ui.options_.stage() == Options::STAGE_EDITING;
}

void TestOptionsIdleTimeout() {
  test_checker.Start("TestOptionsIdleTimeout");
  CHECK(kFeatureDefinitions[FEATURE_IDLE_TIMEOUT].values == kNumIdleTimeouts);
  CHECK(kFeatureDefinitions[FEATURE_IDLE_TIMEOUT].field == kNoField);
  CHECK(!kFeatureDefinitions[FEATURE_IDLE_TIMEOUT].page &&
      !kFeatureDefinitions[FEATURE_IDLE_TIMEOUT].amount &&
      !kFeatureDefinitions[FEATURE_IDLE_TIMEOUT].action);
  uint8_t b[kProfileSize];
  for (int state = PROFILE_OFF; state <= PROFILE_FIXED; ++state) {
    for (int value = 0; value < kNumIdleTimeouts; ++value) {
      Rig r(false);
      IdleProfile(b, state, value);
      ApplyCustomProfile(&r, b);
      CHECK(r.ui.options_.idle_timeout_ ==
          (state == PROFILE_FIXED ? kOptionsIdleTimeouts[value] : 0));
    }
  }
  // Absent, Off and Fixed Off keep the page open indefinitely.
  for (int kind = 0; kind < 3; ++kind) {
    Rig r(false);
    if (kind) {
      IdleProfile(b, kind == 1 ? PROFILE_OFF : PROFILE_FIXED,
          kind == 1 ? kNumIdleTimeouts - 1 : 0);
      ApplyCustomProfile(&r, b);
    }
    r.Enter();
    CHECK(Idle(&r, 200000));
  }
  IdleProfile(b, PROFILE_FIXED, 1);
  CHECK(kOptionsIdleTimeouts[1] == 5000);
  // Counts from the entry release; previews and unchanged exit write nothing.
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    const uint32_t writes = test_writes;
    r.Enter();
    CHECK(Idle(&r, 3000));
    CHECK(r.ui.options_.preview_knob() == 1);
    CHECK(Idle(&r, 1980));
    CHECK(!Idle(&r, 20));
    CHECK(r.ui.options_flash_time_);
    r.Tick(10, true);
    CHECK(r.ui.options_.stage() == Options::STAGE_INACTIVE);
    CHECK(test_writes == writes);
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      CHECK(r.ui.assignable_cv_pot_[i].state_ != POT_STATE_LOCKING);
    }
    // Reopen restarts the timer; the next button press is a normal event.
    r.Tick(100, true);
    r.Enter();
    CHECK(Idle(&r, 4900));
    CHECK(!Idle(&r, 120));
    CHECK(test_writes == writes);
    const bool freeze = r.ui.freeze_button_state_;
    r.Down(SWITCH_FREEZE);
    r.Up(SWITCH_FREEZE);
    CHECK(r.ui.freeze_button_state_ != freeze);
  }
  // Page taps and knob turns before pickup restart the timer.
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.Enter();
    CHECK(Idle(&r, 4000));
    r.Tap();
    CHECK(r.ui.options_.page() == 1);
    CHECK(Idle(&r, 4000));
    r.reverb = 0.55f;
    CHECK(Idle(&r, 4900));
    CHECK(!r.ui.options_.qualified_[2]);
    CHECK(!Idle(&r, 120));
  }
  // Pot jitter below 2% never keeps the page open.
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.Enter();
    for (int i = 0; i < 4900; ++i) {
      r.feedback = (i & 1) ? 0.508f : 0.492f;
      r.pot = (i & 2) ? 0.51f : 0.49f;
      r.Tick(1, true);
    }
    CHECK(r.ui.options_.active());
    for (int i = 0; i < 120; ++i) {
      r.feedback = (i & 1) ? 0.508f : 0.492f;
      r.Tick(1, true);
    }
    CHECK(!r.ui.options_.active());
  }
  // Reserved knobs do nothing on a page, so they do not keep it open.
  {
    Rig r(false);
    uint8_t custom[kProfileSize];
    CustomProfile(custom, FEATURE_PITCH, 1, 0, 0);
    AppendIdleTimeout(custom, PROFILE_FIXED, 1);
    ApplyCustomProfile(&r, custom);
    r.Enter();
    for (int i = 0; i < 4900; ++i) {
      r.pot = (i / 100) & 1 ? 0.9f : 0.1f;
      r.Tick(1, true);
    }
    CHECK(r.ui.options_.active());
    CHECK(!Idle(&r, 120));
  }
  // A turn slower than 2% per second counts once it adds up to 2%.
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.Enter();
    for (int i = 0; i < 4900; ++i) {
      r.reverb = 0.5f + 0.03f * float(i) / 4900.0f;
      r.Tick(1, true);
    }
    CHECK(Idle(&r, 3200));
    CHECK(!Idle(&r, 300));
  }
  // A slow movement smaller than 2% in total never counts.
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.Enter();
    for (int i = 0; i < 4900; ++i) {
      r.reverb = 0.5f + 0.015f * float(i) / 4900.0f;
      r.Tick(1, true);
    }
    CHECK(r.ui.options_.active());
    CHECK(!Idle(&r, 120));
  }
  // Idle expiry never interrupts a hold and counts from the release, across
  // clock wrap; holding [M] 3 seconds still closes as before.
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    stmlib::system_clock.count_ = 0xffffffff - 10000;
    r.Enter();
    CHECK(Idle(&r, 4000));
    r.Down(SWITCH_ASSIGN);
    CHECK(Idle(&r, 2500));
    r.Up(SWITCH_ASSIGN);
    CHECK(r.ui.options_.page() == 1);
    CHECK(stmlib::system_clock.milliseconds() > 0xffffffff - 5000);
    CHECK(Idle(&r, 4900));
    CHECK(stmlib::system_clock.milliseconds() < 20000);
    CHECK(!Idle(&r, 120));
  }
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
    r.Enter();
    CHECK(r.ui.options_.seed_allowed());
    r.Down(SWITCH_SEED);
    CHECK(Idle(&r, 30000));
    r.Up(SWITCH_SEED);
    CHECK(Idle(&r, 4900));
    CHECK(!Idle(&r, 120));
    CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_FIFTHS);
  }
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.Enter();
    CHECK(Idle(&r, 1000));
    r.Down(SWITCH_ASSIGN);
    CHECK(!Idle(&r, 3000));
  }
  // A pending edit is kept and saved once through the normal drain.
  {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    const uint32_t writes = test_writes;
    const AttackMode mode = r.settings.attack_mode();
    r.Enter();
    r.reverb = 0.9f;
    r.Tick(5100, false);
    CHECK(!r.ui.options_.active());
    CHECK(r.ui.options_.commit_requested());
    CHECK(test_writes == writes);
    r.ui.DoEvents();
    r.Tick(100, true);
    CHECK(r.ui.options_.stage() == Options::STAGE_INACTIVE);
    CHECK(r.settings.attack_mode() != mode);
    CHECK(test_writes == writes + kRecordWords);
    r.Tick(60000, true);
    CHECK(test_writes == writes + kRecordWords);
  }
}

void TestPageTwoKnobRoutingAndSavedMeaning() {
  test_checker.Start("TestPageTwoKnobRoutingAndSavedMeaning");
  for (int explicit_old = 0; explicit_old < 2; ++explicit_old) {
    uint8_t b[kProfileSize];
    for (size_t i = 0; i < kProfileSize; ++i) {
      b[i] = kProfileBlock[i];
    }
    if (explicit_old) {
      for (size_t i = 0; i < b[5]; ++i) {
        uint8_t* entry = b + 16 + 6 * i;
        if (entry[0] == FEATURE_REVERB) {
          entry[3] = 5;
        } else if (entry[0] == FEATURE_DELAY) {
          entry[3] = 4;
        } else if (entry[0] == FEATURE_FROZEN) {
          entry[3] = 6;
        }
      }
      const size_t size = 16 + b[6];
      const uint32_t crc = Profile::Crc32(b, size);
      for (int i = 0; i < 4; ++i) {
        b[size + i] = crc >> (8 * i);
      }
    }
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.EnterPage(1);
    float* pots[] = { &r.feedback, &r.pot, &r.reverb };
    for (int knob = 0; knob < 3; ++knob) {
      const ProfileFeature feature = r.settings.profile().Page(4 + knob);
      const uint8_t value = kFeatureDefinitions[feature].values - 1;
      *pots[knob] = 0.9f;
      r.Tick(1100, true);
      CHECK(r.settings.FeatureValue(feature) == value);
    }
    CHECK(r.settings.evolving_feedback());
    CHECK(r.settings.clouds_style());
    CHECK(r.settings.delay_mode() == DELAY_MODE_WIDE_DETENT);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.evolving_feedback() && loaded.clouds_style());
    CHECK(loaded.delay_mode() == DELAY_MODE_WIDE_DETENT);
    CHECK(loaded.profile().Page(4) == FEATURE_FROZEN);
  }
}

void TestDefaultPageFourControls() {
  test_checker.Start("TestDefaultPageFourControls");
  Rig r(false);
  CHECK(r.settings.FeatureValue(FEATURE_BRIGHTNESS) == 0);
  CHECK(r.settings.FeatureValue(FEATURE_SEED_LATCH) == 0);
  r.EnterPage(3);
  r.feedback = 0.15f;
  r.Tick(1100, true);
  r.feedback = 0.5f;
  r.Tick(1100, true);
  CHECK(r.settings.FeatureValue(FEATURE_BRIGHTNESS) == 1);
  r.pot = 0.65f;
  r.Tick(1100, true);
  CHECK(r.settings.FeatureValue(FEATURE_SEED_LATCH) == 2);
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.FeatureValue(FEATURE_BRIGHTNESS) == 1);
  CHECK(loaded.FeatureValue(FEATURE_SEED_LATCH) == 2);
  r.feedback = 0.1f;
  r.pot = 0.1f;
  r.Tick(1100, true);
  CHECK(r.settings.FeatureValue(FEATURE_BRIGHTNESS) == 0);
  CHECK(r.settings.FeatureValue(FEATURE_SEED_LATCH) == 0);
}

void ScaleFloat(uint8_t* p, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  for (size_t i = 0; i < 4; ++i) {
    p[i] = bits >> (8 * i);
  }
}

void ScaleChecksum(uint8_t* b) {
  const size_t size = 16 + uint16_t(b[6]) + (uint16_t(b[7]) << 8);
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

void TestCustomScalePersistenceAndLeds() {
  test_checker.Start("TestCustomScalePersistenceAndLeds");
  Rig r(false);
  uint8_t b[kProfileSize];
  CustomProfile(b, FEATURE_PITCH, 1, 0, 0);
  b[6] = uint8_t(6 + 2 * kScaleRecordSize);
  b[7] = (6 + 2 * kScaleRecordSize) >> 8;
  b[12] = 1;
  b[13] = 2;
  for (int i = 0; i < 2; ++i) {
    uint8_t* p = b + 22 + i * kScaleRecordSize;
    memset(p, 0, kScaleRecordSize);
    p[0] = i ? 3 : 15;
    p[1] = 2;
    p[2] = i ? 17 : 29;
    p[3] = 83;
    p[4] = 199;
    p[5] = i ? PULSE_FAST : PULSE_DOUBLE;
    memcpy(p + 12, "Custom", 6);
    ScaleFloat(p + 8, 13.0f);
    ScaleFloat(p + 48, 4.125f);
  }
  ScaleChecksum(b);
  ApplyCustomProfile(&r, b);
  r.settings.set_pitch_scale(PitchScale(15));
  r.settings.SaveState();
  CHECK(r.settings.pitch_scale() == 15);
  CHECK(StoredByte(r.settings, FEATURE_PITCH) == 15);  // A plain byte.
  // Cycle order is independent of the permanent saved ID.
  uint8_t record[kScaleRecordSize];
  memcpy(record, b + 22, sizeof(record));
  memcpy(b + 22, b + 22 + sizeof(record), sizeof(record));
  memcpy(b + 22 + sizeof(record), record, sizeof(record));
  ScaleChecksum(b);
  ApplyCustomProfile(&r, b);
  CHECK(r.settings.pitch_scale() == 15);
  CHECK(r.settings.profile().FirstScale() == 3);
  r.Enter();
  r.Down(SWITCH_SEED);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.options_.Value(FEATURE_PITCH) == PITCH_SCALE_MAJOR);
  const OptionValue& indication = r.ui.options_.indication();
  CHECK(indication.value == 3 && indication.color.r == 17);
  CHECK(indication.color.g == 83 && indication.color.b == 199);
  CHECK(indication.pulses == PULSE_FAST);
  r.Tick(1100, true);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_MAJOR);
  // Removing the saved ID cannot make that nibble select another scale.
  r.settings.set_pitch_scale(PitchScale(15));
  r.settings.SaveState();
  b[6] = 6 + kScaleRecordSize;
  b[7] = 0;
  b[13] = 1;
  ScaleChecksum(b);
  ApplyCustomProfile(&r, b);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_OFF);
  CHECK(r.settings.profile().FirstScale() == PITCH_SCALE_MAJOR);
}

void TestProfilePages() {
  test_checker.Start("TestProfilePages");
  uint8_t b[kProfileSize] = { 0 };
  Rig invalid(false);
  invalid.settings.Init(false, b);
  invalid.ui.Init(&invalid.settings, &invalid.gain,
      &invalid.calibration, &invalid.backup);
  invalid.ui.LoadOptions();
  invalid.ui.ShowStorageStatus();
  CHECK(invalid.ui.mode_ == Ui::MODE_NORMAL);
  for (int id = FEATURE_PITCH; id < FEATURE_LAST; ++id) {
    if (!kFeatureDefinitions[id].page) {
      continue;
    }
    for (int slot = 1; slot < kNumPageSlots; ++slot) {
      Rig r(false);
      // Synth start needs its gesture (#292).
      CustomProfile(b, id, slot, 0,
          id == FEATURE_SYNTH_START ? ACTION_HOLD : ACTION_NONE);
      ApplyCustomProfile(&r, b);
      r.Enter();
      CHECK(r.ui.options_.page() == (slot - 1) / 3);
      CHECK(r.ui.options_.preview_knob() == (slot - 1) % 3);
      CHECK(r.ui.options_.selected_ == ProfileFeature(id));
      r.Tap();
      CHECK(r.ui.options_.page() == (slot - 1) / 3);
      r.ui.options_.preview_[FEATURE_PITCH] = PITCH_SCALE_FIFTHS;
      CHECK(r.ui.options_.seed_allowed() == (id == FEATURE_PITCH));
    }
  }
  Rig empty(false);
  CustomProfile(b, 0, 0, 0, 0);
  ApplyCustomProfile(&empty, b);
  const uint32_t writes = test_writes;
  const State before = empty.settings.state();
  empty.Down(SWITCH_ASSIGN);
  empty.Tick(3100, true);
  empty.Up(SWITCH_ASSIGN);
  CHECK(test_writes == writes);
  CHECK(!memcmp(&before, &empty.settings.state(), sizeof(before)));
  CHECK(empty.ui.options_.stage() == Options::STAGE_INACTIVE);
  CHECK(empty.ui.options_.preview_knob() == -1);
}

// Features placed on page slots { id, slot }, Adjustable at value 0.
void PagesProfile(uint8_t* b, const uint8_t (*placed)[2], int count) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  b[5] = count;
  b[6] = 6 * count;
  for (int i = 0; i < count; ++i) {
    uint8_t* entry = b + 16 + 6 * i;
    entry[0] = placed[i][0];
    entry[1] = PROFILE_ADJUSTABLE;
    entry[2] = entry[4] = 0;
    entry[3] = placed[i][1];
    // Synth start needs its gesture (#292).
    entry[5] = entry[0] == FEATURE_SYNTH_START ? ACTION_HOLD : ACTION_NONE;
  }
  const size_t size = 16 + b[6];
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

// The previewed knob is lit steady; once the preview ends, LEDs outside
// the page's mask stay dark and those inside it pulse.
void CheckPageLeds(Rig* r, int page) {
  const uint8_t masks[7] = { 1, 2, 4, 3, 5, 6, 7 };
  const uint8_t mask = masks[page];
  r->Tick(100, true);  // The entry flash has ended.
  const int preview = r->ui.options_.preview_knob();
  CHECK(preview < 0 || r->ui.leds_.intensity_[LED_ASSIGN_1 + preview] == 255);
  for (int t = 0; t < 3 * 2048 && r->ui.options_.preview_knob() >= 0; ++t) {
    r->Tick(1, true);
  }
  CHECK(r->ui.options_.preview_knob() == -1);
  int peak[3] = { 0, 0, 0 };
  for (int t = 0; t < 1500; ++t) {
    r->Tick(1, true);
    for (int i = 0; i < 3; ++i) {
      const int level = r->ui.leds_.intensity_[LED_ASSIGN_1 + i];
      CHECK(level == 0 || (mask & (1 << i)));
      peak[i] = std::max(peak[i], level);
    }
  }
  for (int i = 0; i < 3; ++i) {
    CHECK((peak[i] > 0) == bool(mask & (1 << i)));
  }
}

void TestOptionalPages() {
  test_checker.Start("TestOptionalPages");
  uint8_t b[kProfileSize];
  // Sparse: pages 1, 5 and 7 keep their numbers, LEDs and knob columns.
  const uint8_t sparse[][2] = {
    { FEATURE_PITCH, 1 }, { FEATURE_DELAY, 14 }, { FEATURE_LIMITER, 21 }
  };
  Rig r(false);
  PagesProfile(b, sparse, 3);
  ApplyCustomProfile(&r, b);
  r.Enter();
  const int order[] = { 0, 4, 6, 0, 4 };
  const int knob[] = { 0, 1, 2, 0, 1 };
  const ProfileFeature setting[] = {
    FEATURE_PITCH, FEATURE_DELAY, FEATURE_LIMITER, FEATURE_PITCH, FEATURE_DELAY
  };
  for (int i = 0; i < 5; ++i) {
    if (i) {
      r.Tap();
    }
    CHECK(r.ui.options_.page() == order[i]);
    CHECK(r.ui.options_.preview_knob() == knob[i]);
    CHECK(r.ui.options_.selected_ == setting[i]);
    CheckPageLeds(&r, order[i]);
  }
  // Close on page 5: its two LEDs flash; reopening returns to page 5.
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000, true);
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] == 255);
  r.Up(SWITCH_ASSIGN);
  CHECK(!r.ui.options_.active());
  r.Tick(30, true);
  r.Enter();
  CHECK(r.ui.options_.page() == 4);
  // Seven pages edit and save like the first four.
  r.Tap();
  CHECK(r.ui.options_.page() == 6);
  r.reverb = 0.9f;
  r.Tick(1100, true);
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  Settings loaded;
  CHECK(loaded.Init(false, b));
  CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);

  // A single page 6 opens at startup and stays there.
  const uint8_t single[][2] = { { FEATURE_SEED_LATCH, 17 } };
  Rig one(false);
  PagesProfile(b, single, 1);
  ApplyCustomProfile(&one, b);
  CHECK(one.ui.options_.page() == 5);
  one.Enter();
  CHECK(one.ui.options_.page() == 5);
  CHECK(one.ui.options_.preview_knob() == 1);
  CheckPageLeds(&one, 5);
  one.Tap();
  CHECK(one.ui.options_.page() == 5);
  CHECK(one.ui.options_.preview_knob() == 1);

  // Every page-capable feature across all seven pages.
  const uint8_t full[][2] = {
    { FEATURE_PITCH, 1 }, { FEATURE_REVERB, 4 }, { FEATURE_DELAY, 8 },
    { FEATURE_PROCESSOR, 10 }, { FEATURE_LIMITER, 12 },
    { FEATURE_FROZEN, 13 }, { FEATURE_SYNTH, 15 },
    { FEATURE_ATTACKS, 17 }, { FEATURE_INTERVALS, 18 },
    { FEATURE_BRIGHTNESS, 19 }, { FEATURE_SEED_LATCH, 20 },
    { FEATURE_TIME_SCAN, 21 }, { FEATURE_SYNTH_START, 11 }
  };
  const ProfileFeature first[] = {
    FEATURE_PITCH, FEATURE_REVERB, FEATURE_DELAY, FEATURE_PROCESSOR,
    FEATURE_FROZEN, FEATURE_ATTACKS, FEATURE_BRIGHTNESS
  };
  Rig all(false);
  PagesProfile(b, full, 13);
  ApplyCustomProfile(&all, b);
  all.Enter();
  for (int page = 0; page <= kNumOptionPages; ++page) {
    CHECK(all.ui.options_.page() == page % kNumOptionPages);
    CHECK(all.ui.options_.selected_ == first[page % kNumOptionPages]);
    all.Tap();
  }
  // TIME scan on page 7, REVERB, saves to its own value.
  Rig scan(false);
  ApplyCustomProfile(&scan, b);
  scan.EnterPage(6);
  CHECK(scan.ui.options_.page() == 6);
  CheckPageLeds(&scan, 6);
  scan.reverb = 0.9f;
  scan.Tick(1100, true);
  CHECK(scan.settings.FeatureValue(FEATURE_TIME_SCAN) == 1);
  CHECK(loaded.Init(false, b));
  CHECK(loaded.FeatureValue(FEATURE_TIME_SCAN) == 1);

  // Reconfiguring keeps a remembered page 7 only while it has controls;
  // otherwise the next populated page, wrapping, takes over.
  scan.Down(SWITCH_ASSIGN);
  scan.Tick(3000, true);
  scan.Up(SWITCH_ASSIGN);
  scan.Tick(30, true);
  CHECK(!scan.ui.options_.active() && scan.ui.options_.page() == 6);
  const uint8_t second[][2] = { { FEATURE_DELAY, 5 }, { FEATURE_REVERB, 14 } };
  PagesProfile(b, second, 2);
  CHECK(scan.settings.Init(false, b));
  scan.ui.LoadOptions();
  CHECK(scan.ui.options_.page() == 1);
  scan.Tick(30, true);
  scan.Enter();
  CHECK(scan.ui.options_.page() == 1);
  scan.Tap();
  CHECK(scan.ui.options_.page() == 4);
  PagesProfile(b, sparse, 3);
  CHECK(scan.settings.Init(false, b));
  scan.ui.LoadOptions();
  CHECK(scan.ui.options_.page() == 4);

  // A slot past page 7 locks the profile: factory pages, no saving.
  const uint8_t beyond[][2] = { { FEATURE_PITCH, kNumPageSlots } };
  PagesProfile(b, beyond, 1);
  Rig locked(false);
  locked.settings.Init(false, b);
  CHECK(locked.settings.profile_locked());
  locked.ui.Init(&locked.settings, &locked.gain, &locked.calibration,
      &locked.backup);
  locked.ui.LoadOptions();
  locked.Tick(40, true);
  locked.Enter();
  for (int page = 0; page < 6; ++page) {
    CHECK(locked.ui.options_.page() == page % 5);
    locked.Tap();
  }

  // The idle timeout closes page 7 like the first four; its reserved knobs
  // do not keep it open, and reopening returns to it.
  PagesProfile(b, sparse, 3);
  AppendIdleTimeout(b, PROFILE_FIXED, 1);
  Rig idle(false);
  ApplyCustomProfile(&idle, b);
  idle.Enter();
  idle.Tick(4000, true);
  idle.Tap();
  idle.Tick(4000, true);
  idle.Tap();
  CHECK(idle.ui.options_.page() == 6);
  for (int i = 0; i < 4900; ++i) {
    idle.feedback = (i / 100) & 1 ? 0.9f : 0.1f;
    idle.Tick(1, true);
  }
  int ticks = 0;
  while (idle.ui.options_.active() && ticks < 1000) {
    idle.Tick(1, true);
    ++ticks;
  }
  CHECK(ticks > 0 && ticks <= 120);
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    CHECK(idle.ui.leds_.intensity_[LED_ASSIGN_1 + i] == 255);
  }
  idle.Tick(100, true);
  idle.Enter();
  CHECK(idle.ui.options_.page() == 6);
}

void TestProfileActions() {
  test_checker.Start("TestProfileActions");
  uint8_t b[kProfileSize];
  const int features[] = {
    FEATURE_MONOPHONIC, FEATURE_TRIM, FEATURE_SYNTH_START
  };
  for (int i = 0; i < 3; ++i) {
    for (int slot = 1; slot <= 3; ++slot) {
      for (int patched = 0; patched < 2; ++patched) {
        Rig r(false);
        CustomProfile(b, features[i], 0, 0, slot);
        // Only Synth start takes the [A] hold (#291).
        if (slot == ACTION_HOLD && features[i] != FEATURE_SYNTH_START) {
          r.settings.Init(false, b);
          CHECK(r.settings.profile_locked());
          continue;
        }
        ApplyCustomProfile(&r, b);
        r.settings.set_self_play_on_request(true);
        r.ui.set_no_input(!patched);
        r.settings.mutable_state()->gain = 123;
        r.Down(SWITCH_MEDIA);
        if (slot == 3) {
          r.Tick(1510, true);
          r.Up(SWITCH_MEDIA);
        } else {
          r.Down(slot == 1 ? SWITCH_SEED : SWITCH_FREEZE);
        }
        // Only the synth needs empty inputs; a handled hold keeps the gain.
        const bool handled = features[i] != FEATURE_SYNTH_START || !patched;
        if (features[i] == FEATURE_MONOPHONIC) {
          CHECK(r.settings.state().monophonic_mode);
        } else if (features[i] == FEATURE_TRIM) {
          CHECK(r.ui.gestures_.trim_flag_);
        } else {
          CHECK(r.ui.self_play_requested() == !patched);
        }
        const bool reset = slot == 3 && !handled;
        CHECK(r.gain.restarts == (reset ? 1 : 0));
        CHECK(r.settings.state().gain == (reset ? 0 : 123));
      }
    }
  }
  Rig off(false);
  CustomProfile(b, 0, 0, 0, 0);
  ApplyCustomProfile(&off, b);
  off.Down(SWITCH_MEDIA);
  off.Down(SWITCH_SEED);
  CHECK(!off.settings.state().monophonic_mode);
  off.Up(SWITCH_SEED);
  off.Down(SWITCH_FREEZE);
  CHECK(!off.ui.gestures_.trim_flag_);
}

// The factory profile with the Plaits bank starting elsewhere.
void PlaitsBankProfile(uint8_t* b, int value) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  uint8_t* entry = b + 16;
  while (entry[0] != FEATURE_PLAITS_BANK) {
    entry += 6;
  }
  CHECK(entry[1] == PROFILE_ADJUSTABLE);
  entry[2] = value;
  const size_t size = 16 + 6 * b[5];
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

// Plays the Plaits (or wavetable) synth with FREEZE latched.
void PlaySynth(Rig* r, bool synth_mode, bool plaits) {
  r->settings.set_synth(plaits ? SYNTH_PLAITS : SYNTH_BEADS);
  r->ui.set_no_input(synth_mode);
  r->ui.set_synth_mode(synth_mode);
  r->ui.set_playing_synth(plaits ? SYNTH_PLAITS : SYNTH_BEADS);
  r->Down(SWITCH_FREEZE);
  r->Up(SWITCH_FREEZE);
  CHECK(r->ui.freeze_button_state_);
  CHECK(r->ui.synth() == plaits);
}

// [A]+FREEZE steps the Plaits bank in Plaits synth play (#190), keeping
// FREEZE; granular, spectral and the wavetable synth keep buffer trim.
void TestPlaitsBankGesture() {
  test_checker.Start("TestPlaitsBankGesture");
  CHECK(kFeatureDefinitions[FEATURE_PLAITS_BANK].field == 14);
  CHECK(kFeatureDefinitions[FEATURE_PLAITS_BANK].values == kNumPlaitsBanks);
  {
    Rig r(false);
    CHECK(r.settings.profile().adjustable(FEATURE_PLAITS_BANK));
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 0);
    PlaySynth(&r, true, true);
    const uint8_t media = r.settings.state().media;
    // Red, yellow, then green, red and yellow again; each step is saved.
    const uint8_t banks[] = { 1, 2, 0, 1, 2 };
    for (int i = 0; i < 5; ++i) {
      const uint32_t writes = test_writes;
      r.Down(SWITCH_MEDIA);
      r.Down(SWITCH_FREEZE);
      CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == banks[i]);
      CHECK(test_writes == writes + kRecordWords);
      CHECK(!r.ui.gestures_.trim_flag_);
      CHECK(r.ui.freeze_button_state_);
      const Leds::Color color = r.ui.gestures_.plaits_bank_cues_[banks[i]].color;
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == color.r);
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == color.g);
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == color.b);
      r.Up(SWITCH_FREEZE);
      r.Up(SWITCH_MEDIA);
      CHECK(r.settings.state().media == media);
      CHECK(r.ui.freeze_button_state_);
      Settings loaded;
      CHECK(loaded.Init(false));
      CHECK(loaded.FeatureValue(FEATURE_PLAITS_BANK) == banks[i]);
      CHECK(StoredByte(loaded, FEATURE_PLAITS_BANK) == banks[i]);
    }
    // Each bank has its own colour; the cue lasts half a second.
    const GestureController::BankCue* cues = r.ui.gestures_.plaits_bank_cues_;
    for (int i = 0; i < kNumPlaitsBanks; ++i) {
      CHECK(cues[i].color.r || cues[i].color.g || cues[i].color.b);
      for (int j = 0; j < i; ++j) {
        CHECK(cues[i].color.r != cues[j].color.r ||
            cues[i].color.g != cues[j].color.g ||
            cues[i].color.b != cues[j].color.b ||
            cues[i].pulses != cues[j].pulses);
      }
    }
    r.Tick(400, true);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == 255);
    r.Tick(200, true);
    const Leds::Color quality = r.ui.media_palette_[media];
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == quality.g);
    // FREEZE alone still toggles sustain.
    r.Down(SWITCH_FREEZE);
    r.Up(SWITCH_FREEZE);
    CHECK(!r.ui.freeze_button_state_);
    // An options page commit keeps the gesture's bank.
    r.EnterLimiter();
    r.reverb = 0.9f;
    r.Tick(1200, true);
    r.Down(SWITCH_ASSIGN);
    r.Tick(3100, true);
    r.Up(SWITCH_ASSIGN);
    CHECK(r.settings.output_limiter_mode() != OUTPUT_LIMITER_STOCK);
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 2);
  }
  // A profile chooses the bank on a blank sector; a saved bank wins.
  {
    uint8_t b[kProfileSize];
    PlaitsBankProfile(b, 2);
    Rig r(false);
    ApplyCustomProfile(&r, b);
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 0);
    // A blank sector reports a new module, as the factory test expects.
    memset(reinterpret_cast<void*>(0x081c0000), 255, 0x40000);
    CHECK(!r.settings.Init(false, b));
    r.ui.Init(&r.settings, &r.gain, &r.calibration, &r.backup);
    r.ui.LoadOptions();
    r.Tick(40, true);
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 2);
    PlaySynth(&r, true, true);
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 0);
  }
  // Granular and the wavetable synth: buffer trim.
  for (int c = 0; c < 2; ++c) {
    Rig r(false);
    PlaySynth(&r, c == 1, c == 0);
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.ui.gestures_.trim_flag_);
    CHECK(!r.ui.freeze_button_state_);
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 0);
    r.Up(SWITCH_FREEZE);
    r.Up(SWITCH_MEDIA);
  }
  // Synth start placed on [A]+FREEZE keeps it, to stop the synth.
  {
    uint8_t b[kProfileSize];
    CustomProfile(b, FEATURE_SYNTH_START, 0, 0, ACTION_FREEZE);
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.settings.set_self_play_on_request(true);
    PlaySynth(&r, true, true);
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.ui.self_play_requested());
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 0);
  }
  // Monophonic grains on [A]+FREEZE give way in Plaits play only.
  for (int plaits = 0; plaits < 2; ++plaits) {
    uint8_t b[kProfileSize];
    CustomProfile(b, FEATURE_MONOPHONIC, 0, 0, ACTION_FREEZE);
    Rig r(false);
    ApplyCustomProfile(&r, b);
    CHECK(r.settings.profile().adjustable(FEATURE_PLAITS_BANK));
    PlaySynth(&r, true, plaits);
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(bool(r.settings.state().monophonic_mode) == !plaits);
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == plaits);
  }
}

// Rings' [A]+FREEZE cycles 1/2/4 voices with colours, preserving FREEZE.
// Plaits keeps bank stepping; a profile's Synth start keeps [A]+FREEZE.
void TestRingsPolyphonyGesture() {
  test_checker.Start("TestRingsPolyphonyGesture");
  {
    Rig r(false);
    r.ui.set_playable_synths(SynthMask(true, true, true));
    CHECK(r.settings.profile().adjustable(FEATURE_RINGS_POLYPHONY));
    PlaySynth(&r, true, false);
    r.settings.set_synth(SYNTH_RINGS);
    r.ui.set_playing_synth(SYNTH_RINGS);
    r.Tick(1, true);
    CHECK(r.ui.synth() == SYNTH_RINGS);
    const uint8_t media = r.settings.state().media;
    const uint8_t steps[] = { 1, 2, 0, 1, 2 };
    const uint8_t voices[] = { 1, 2, 4 };
    for (int i = 0; i < 5; ++i) {
      const uint32_t writes = test_writes;
      r.Down(SWITCH_MEDIA);
      r.Down(SWITCH_FREEZE);
      CHECK(r.settings.FeatureValue(FEATURE_RINGS_POLYPHONY) == steps[i]);
      CHECK(r.settings.rings_polyphony() == voices[steps[i]]);
      CHECK(test_writes == writes + kRecordWords);
      CHECK(!r.ui.gestures_.trim_flag_ && r.ui.freeze_button_state_);
      // Rings: red for four voices or more, green for two or fewer.
      const int poly = voices[steps[i]];
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == (poly >= 2 ? 255 : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == (poly <= 2 ? 255 : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);
      r.Up(SWITCH_FREEZE);
      r.Up(SWITCH_MEDIA);
      CHECK(r.settings.state().media == media);
      CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 0);
      Settings loaded;
      CHECK(loaded.Init(false));
      CHECK(loaded.FeatureValue(FEATURE_RINGS_POLYPHONY) == steps[i]);
    }
    r.Tick(600, true);
    const Leds::Color quality = r.ui.media_palette_[media];
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == quality.g);
    // Rings not playing (a fallback plays): buffer trim, no step.
    r.ui.set_playing_synth(SYNTH_BEADS);
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.ui.gestures_.trim_flag_);
    CHECK(r.settings.FeatureValue(FEATURE_RINGS_POLYPHONY) == 2);
  }
  // Plaits playing: the bank steps, the polyphony stays.
  {
    Rig r(false);
    r.ui.set_playable_synths(SynthMask(true, true, true));
    PlaySynth(&r, true, true);
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == 1);
    CHECK(r.settings.FeatureValue(FEATURE_RINGS_POLYPHONY) == 0);
  }
  // Synth start on [A]+FREEZE keeps it, to stop the synth.
  {
    uint8_t b[kProfileSize];
    CustomProfile(b, FEATURE_SYNTH_START, 0, 0, ACTION_FREEZE);
    Rig r(false);
    ApplyCustomProfile(&r, b);
    CHECK(r.settings.profile().adjustable(FEATURE_RINGS_POLYPHONY));
    r.settings.set_self_play_on_request(true);
    PlaySynth(&r, true, false);
    r.ui.set_playing_synth(SYNTH_RINGS);
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.ui.self_play_requested());
    CHECK(r.settings.FeatureValue(FEATURE_RINGS_POLYPHONY) == 0);
  }
}

// A Synth action steps through the playable synths only.
void TestSynthActionSteps() {
  test_checker.Start("TestSynthActionSteps");
  struct Case {
    uint8_t playable;
    uint8_t steps[3];
  } cases[] = {
    { SynthMask(true, true, true), { 1, 2, 0 } },
    { SynthMask(true, false, true), { 2, 0, 2 } },
    { SynthMask(false, true, true), { 1, 2, 1 } },
    { SynthMask(true, true, false), { 1, 0, 1 } },
    { SynthMask(false, false, true), { 2, 2, 2 } },
  };
  uint8_t b[kProfileSize];
  CustomProfile(b, FEATURE_SYNTH, 0, 0, ACTION_SEED);
  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
    Rig r(false);
    ApplyCustomProfile(&r, b);
    r.ui.set_playable_synths(cases[c].playable);
    for (int i = 0; i < 3; ++i) {
      const uint8_t value = cases[c].steps[i];
      r.Down(SWITCH_MEDIA);
      r.Down(SWITCH_SEED);
      CHECK(r.settings.synth() == value && r.ui.synth() == value);
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == kOptionColors[value].r);
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == kOptionColors[value].g);
      r.Up(SWITCH_SEED);
      r.Up(SWITCH_MEDIA);
      Settings loaded;
      CHECK(loaded.Init(false, b) && loaded.synth() == value);
    }
  }
}

void TestProfileAmounts() {
  test_checker.Start("TestProfileAmounts");
  uint8_t b[kProfileSize];
  for (int id = FEATURE_WIDTH; id <= FEATURE_DIFFUSION; ++id) {
    for (int slot = 1; slot <= 2; ++slot) {
      Rig r(false);
      CustomProfile(b, id, 0, slot, 0);
      ApplyCustomProfile(&r, b);
      r.Down(SWITCH_MEDIA);
      if (slot == 1) {
        r.pot = 1.0f;
      } else {
        r.reverb = 1.0f;
      }
      r.Tick(40, true);
      if (id == FEATURE_WIDTH) {
        CHECK(r.settings.stereo_width() == 255);
        CHECK(r.settings.diffusion() == 0.0f);
      } else {
        CHECK(r.settings.FeatureValue(FEATURE_DIFFUSION) == 255);
        CHECK(r.settings.stereo_width() == 0);
      }
      // The knob without an amount keeps its stock role during the hold.
      if (slot == 1) {
        r.reverb = 1.0f;
      } else {
        r.pot = 1.0f;
      }
      r.Tick(40, true);
      CHECK(r.ui.mix_parameter(slot == 1 ? Ui::MIX_PARAMETER_REVERB
          : Ui::MIX_PARAMETER_DRY_WET) == 1.0f);
    }
  }
  // With no amounts, DRY/WET and REVERB stay live and a tap changes quality.
  Rig none(false);
  CustomProfile(b, 0, 0, 0, 0);
  ApplyCustomProfile(&none, b);
  const uint8_t media = none.settings.state().media;
  none.Down(SWITCH_MEDIA);
  none.pot = 1.0f;
  none.reverb = 0.0f;
  none.Tick(40, true);
  CHECK(none.ui.mix_parameter(Ui::MIX_PARAMETER_DRY_WET) == 1.0f);
  CHECK(none.ui.mix_parameter(Ui::MIX_PARAMETER_REVERB) == 0.0f);
  none.Up(SWITCH_MEDIA);
  CHECK(none.settings.state().media == (media + 3) % 4);
}

// A profile of six-byte entries, without scales.
void EntriesProfile(uint8_t* b, const uint8_t (*entries)[6], int count) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  b[5] = 0;
  b[6] = b[7] = 0;
  b[12] = b[13] = 0;
  for (int i = 0; i < count; ++i) {
    memcpy(b + 16 + 6 * i, entries[i], 6);
  }
  b[5] = count;
  b[6] = 6 * count;
  const size_t size = 16 + 6 * count;
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

bool SawToggleValue(Rig* r, ProfileFeature feature) {
  switch (feature) {
    case FEATURE_REVERB:
      return r->ui.clouds_style();
    case FEATURE_PROCESSOR:
      return r->ui.spectral();
    case FEATURE_FROZEN:
      return r->ui.evolving_feedback();
    case FEATURE_SYNTH:
      return r->ui.synth();
    default:
      return r->settings.FeatureValue(feature);
  }
}

// [A]+SEED and [A]+FREEZE flip a two-value setting, save it and show the
// colour its options page gives the new value for half a second.
void TestProfileToggles() {
  test_checker.Start("TestProfileToggles");
  const ProfileFeature toggles[] = {
    FEATURE_REVERB, FEATURE_PROCESSOR, FEATURE_FROZEN, FEATURE_SYNTH,
    FEATURE_TIME_SCAN
  };
  uint8_t b[kProfileSize];
  for (int t = 0; t < 5; ++t) {
    const ProfileFeature feature = toggles[t];
    CHECK(kFeatureDefinitions[feature].action);
    CHECK(kFeatureDefinitions[feature].values ==
        (feature == FEATURE_SYNTH ? 3 : 2));
    CHECK(kFeatureDefinitions[feature].field != kNoField);
    for (int slot = ACTION_SEED; slot <= ACTION_FREEZE; ++slot) {
      const Switch button = slot == ACTION_SEED ? SWITCH_SEED : SWITCH_FREEZE;
      Rig r(false);
      CustomProfile(b, feature, 0, 0, slot);
      // The Synth toggle goes on [A]+SEED only.
      if (feature == FEATURE_SYNTH && slot == ACTION_FREEZE) {
        r.settings.Init(false, b);
        CHECK(r.settings.profile_locked());
        continue;
      }
      ApplyCustomProfile(&r, b);
      CHECK(r.settings.profile().Action(slot) == feature);
      const uint8_t media = r.settings.state().media;
      for (int flip = 0; flip < 2; ++flip) {
        const uint8_t value = flip == 0 ? 1 : 0;
        const uint32_t writes = test_writes;
        r.Down(SWITCH_MEDIA);
        r.Down(button);
        CHECK(r.settings.FeatureValue(feature) == value);
        CHECK(SawToggleValue(&r, feature) == bool(value));
        CHECK(test_writes == writes + kRecordWords);
        const OptionValue& cue = FindOptionValue(feature, value);
        CHECK(cue.setting == feature && cue.value == value);
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == cue.color.r);
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == cue.color.g);
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == cue.color.b);
        r.Up(button);
        r.Up(SWITCH_MEDIA);
        CHECK(test_writes == writes + kRecordWords);
        CHECK(r.settings.state().media == media);
        CHECK(!r.ui.freeze_button_state_ && !r.ui.gestures_.trim_flag_);
        CHECK(!r.settings.state().monophonic_mode);
        Settings loaded;
        CHECK(loaded.Init(false, b));
        CHECK(loaded.FeatureValue(feature) == value);
        r.Tick(600, true);
        const Leds::Color quality = r.ui.media_palette_[media];
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == quality.r);
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == quality.g);
      }
    }
  }
}

// In Plaits synth play [A]+FREEZE still steps the bank, so a toggle there
// waits for the synth to stop; [A]+SEED toggles at once.
void TestTogglesInPlaitsPlay() {
  test_checker.Start("TestTogglesInPlaitsPlay");
  uint8_t b[kProfileSize];
  const ProfileFeature toggles[] = { FEATURE_REVERB, FEATURE_SYNTH };
  for (int t = 0; t < 2; ++t) {
    for (int slot = ACTION_SEED; slot <= ACTION_FREEZE; ++slot) {
      if (toggles[t] == FEATURE_SYNTH && slot == ACTION_FREEZE) {
        continue;
      }
      for (int synth = 0; synth < 2; ++synth) {
        Rig r(false);
        CustomProfile(b, toggles[t], 0, 0, slot);
        ApplyCustomProfile(&r, b);
        PlaySynth(&r, synth, true);
        const uint8_t before = r.settings.FeatureValue(toggles[t]);
        r.Down(SWITCH_MEDIA);
        r.Down(slot == ACTION_SEED ? SWITCH_SEED : SWITCH_FREEZE);
        const bool stepped = synth && slot == ACTION_FREEZE;
        CHECK((r.settings.FeatureValue(toggles[t]) == before) == stepped);
        CHECK(r.settings.FeatureValue(FEATURE_PLAITS_BANK) == stepped);
        CHECK(r.ui.freeze_button_state_);
      }
    }
  }
}

// Gain, width and diffusion on each arrangement of FEEDBACK, DRY/WET and
// REVERB; a profile without a gain entry keeps the gain on FEEDBACK.
void TestFeedbackAmountSlot() {
  test_checker.Start("TestFeedbackAmountSlot");
  uint8_t b[kProfileSize];
  const uint8_t orders[][3] = {
    { 3, 1, 2 }, { 3, 2, 1 }, { 1, 3, 2 }, { 1, 2, 3 }, { 2, 3, 1 },
    { 2, 1, 3 }
  };
  const Ui::MixParameter knobs[] = {
    Ui::MIX_PARAMETER_FEEDBACK, Ui::MIX_PARAMETER_DRY_WET,
    Ui::MIX_PARAMETER_REVERB
  };
  for (int o = 0; o < 6; ++o) {
    // FEEDBACK is amount slot 3; DRY/WET and REVERB keep 1 and 2.
    int knob[3];
    for (int k = 0; k < 3; ++k) {
      knob[k] = orders[o][k] == AMOUNT_FEEDBACK ? 0 : orders[o][k];
    }
    const uint8_t entries[][6] = {
      { FEATURE_GAIN_KNOB, PROFILE_ADJUSTABLE, 0, 0, orders[o][0], 0 },
      { FEATURE_WIDTH, PROFILE_ADJUSTABLE, 0, 0, orders[o][1], 0 },
      { FEATURE_DIFFUSION, PROFILE_ADJUSTABLE, 0, 0, orders[o][2], 0 },
    };
    // The stock arrangement is also the factory one, without the gain entry.
    for (int implicit = 0; implicit < (o == 0 ? 2 : 1); ++implicit) {
      Rig r(false);
      EntriesProfile(b, implicit ? entries + 1 : entries, implicit ? 2 : 3);
      ApplyCustomProfile(&r, b);
      CHECK(r.ui.gestures_.amount_feature_[knob[0]] == FEATURE_GAIN_KNOB);
      CHECK(r.ui.gestures_.amount_feature_[knob[1]] == FEATURE_WIDTH);
      CHECK(r.ui.gestures_.amount_feature_[knob[2]] == FEATURE_DIFFUSION);
      r.settings.mutable_state()->gain = 77;
      const uint8_t media = r.settings.state().media;
      float* pots[] = { &r.feedback, &r.pot, &r.reverb };
      const float targets[] = { 0.75f, 1.0f, 1.0f };
      for (int k = 0; k < 3; ++k) {
        const uint32_t writes = test_writes;
        r.Down(SWITCH_MEDIA);
        // Two seconds in, each edit keeps the hold from restarting the gain.
        r.Tick(2000, true);
        *pots[knob[k]] = targets[k];
        r.Tick(20, true);
        CHECK(uint32_t(stmlib::system_clock.milliseconds() -
            r.ui.press_time_[SWITCH_MEDIA]) <= 1);
        if (k == 0) {
          CHECK(r.settings.state().gain == 192);
          CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == 255);
          CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == 121);
        } else if (k == 1) {
          CHECK(r.settings.stereo_width() == 255);
        } else {
          CHECK(r.settings.FeatureValue(FEATURE_DIFFUSION) == 255);
        }
        CHECK(r.ui.mix_parameter(knobs[knob[k]]) == 0.5f);
        CHECK(test_writes == writes);
        r.Up(SWITCH_MEDIA);
        // The gain's save also records the new profile's identity.
        CHECK(test_writes == writes + kRecordWords + 1);
        CHECK(r.gain.restarts == 0);
        CHECK(r.settings.state().media == media);
      }
      Settings loaded;
      CHECK(loaded.Init(false, b));
      CHECK(loaded.state().gain == 192);
      CHECK(loaded.stereo_width() == 255);
      CHECK(loaded.FeatureValue(FEATURE_DIFFUSION) == 255);
      // A hold alone still restarts automatic gain.
      r.Down(SWITCH_MEDIA);
      r.Tick(1600, true);
      r.Up(SWITCH_MEDIA);
      CHECK(r.gain.restarts == 1);
      CHECK(r.settings.state().gain == 0);
    }
  }
  // Gain fixed: FEEDBACK can hold width; the hold still restarts the gain.
  const uint8_t fixed[][6] = {
    { FEATURE_GAIN_KNOB, PROFILE_FIXED, 0, 0, 0, 0 },
    { FEATURE_WIDTH, PROFILE_ADJUSTABLE, 0, 0, AMOUNT_FEEDBACK, 0 },
  };
  Rig r(false);
  EntriesProfile(b, fixed, 2);
  ApplyCustomProfile(&r, b);
  CHECK(r.ui.gestures_.amount_feature_[0] == FEATURE_WIDTH);
  CHECK(r.ui.gestures_.amount_feature_[1] == FEATURE_NONE);
  CHECK(r.ui.gestures_.amount_feature_[2] == FEATURE_NONE);
  r.settings.mutable_state()->gain = 77;
  r.Down(SWITCH_MEDIA);
  r.feedback = 1.0f;
  r.pot = 1.0f;
  r.Tick(40, true);
  CHECK(r.settings.stereo_width() == 255);
  CHECK(r.settings.state().gain == 77);
  CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_DRY_WET) == 1.0f);
  r.Up(SWITCH_MEDIA);
  r.Down(SWITCH_MEDIA);
  r.Tick(1600, true);
  r.Up(SWITCH_MEDIA);
  CHECK(r.gain.restarts == 1 && r.settings.state().gain == 0);
}

void SetFixed(Rig* r, ProfileFeature feature, uint8_t value) {
  uint8_t b[kProfileSize];
  CustomProfile(b, feature, 0, 0, 0);
  b[17] = PROFILE_FIXED;
  b[18] = value;
  const uint32_t crc = Profile::Crc32(b, 22);
  for (int i = 0; i < 4; ++i) {
    b[22 + i] = crc >> (8 * i);
  }
  ApplyCustomProfile(r, b);
}

void TestDefaultLedIdentity() {
  test_checker.Start("TestDefaultLedIdentity");
  Rig r(false);
  CHECK(r.settings.FeatureValue(FEATURE_BRIGHTNESS) == 0);
  // Release 86014e3: factory-test frames are the unmodified triangle on
  // every LED; storage warnings are unmodified 0/255 on every LED.
  for (int tick = 0; tick < 2048; ++tick) {
    stmlib::system_clock.Tick();
    const uint32_t now = stmlib::system_clock.milliseconds();
    int triangle = (now & 2047) >> 2;
    triangle = triangle >= 256 ? 511 - triangle : triangle;
    r.ui.mode_ = Ui::MODE_FACTORY_TEST;
    r.ui.UpdateLeds();
    for (int i = 0; i < LED_LAST; ++i) {
      CHECK(r.ui.leds_.intensity_[i] == triangle);
    }
    r.ui.mode_ = Ui::MODE_STORAGE_WARNING;
    r.ui.UpdateLeds();
    for (int i = 0; i < LED_LAST; ++i) {
      CHECK(r.ui.leds_.intensity_[i] == ((now & 64) ? 255 : 0));
    }
  }
  // Release writes the frame verbatim. Exercise every value in every LED,
  // including RGB white and mixed colours, through the default level.
  for (int value = 0; value < 256; ++value) {
    for (int mixed = 0; mixed < 2; ++mixed) {
      uint8_t release[LED_LAST];
      for (int i = 0; i < LED_LAST; ++i) {
        release[i] = mixed ? uint8_t(value + i * 37) : value;
        r.ui.leds_.intensity_[i] = release[i];
      }
      r.ui.leds_.Scale(r.settings.FeatureValue(FEATURE_BRIGHTNESS));
      for (int i = 0; i < LED_LAST; ++i) {
        CHECK(r.ui.leds_.intensity_[i] == release[i]);
      }
    }
  }
  // Every quality colour reaches Write() as stock.
  r.ui.mode_ = Ui::MODE_NORMAL;
  r.ui.set_overwrite_reserve(1.0f);
  for (int media = 0; media < 4; ++media) {
    r.settings.mutable_state()->media = media;
    r.Tick(1, true);
    const Leds::Color color = r.ui.media_palette_[media];
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == color.r);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == color.g);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == color.b);
  }
}

void TestBetaOptionsLeds() {
  test_checker.Start("TestBetaOptionsLeds");
  Rig version(false);
  SetFixed(&version, FEATURE_VERSION, 1);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_G] == 128);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_R] == 0);
  version.Tick(100, true);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  version.Tick(100, true);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  version.Tick(150, true);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  version.Tick(150, true);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  version.Tick(150, true);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  version.Tick(150, true);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  version.Tick(300, true);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_R] != 0);

  Rig delay(false);
  SetFixed(&delay, FEATURE_DELAY_CUE, 1);
  for (int edge = 0; edge < 2; ++edge) {
    delay.ui.set_delay_active(edge == 0);
    delay.Tick(1, true);
    CHECK(delay.ui.leds_.intensity_[LED_MEDIA_R] == 255);
    CHECK(delay.ui.leds_.intensity_[LED_MEDIA_G] == 0);
    delay.Tick(150, true);
    CHECK(delay.ui.leds_.intensity_[LED_MEDIA_G] != 0);
  }

  Rig notch(false);
  SetFixed(&notch, FEATURE_PITCH_NOTCH, 1);
  for (int active = 0; active < 2; ++active) {
    notch.ui.set_modulation_indication(active, 0.0f);
    notch.Tick(1, true);
    CHECK((notch.ui.leds_.intensity_[LED_MEDIA_R] == 0) == bool(active));
  }

  Rig live(false);
  live.settings.set_stereo_width(255);
  live.settings.set_diffusion(0.5f);
  live.Tick(512, true);
  for (int destination = 0; destination < 6; ++destination) {
    live.settings.mutable_state()->assignable_cv_destination = destination < 4
        ? destination : 0;
    live.settings.set_width_assigned(destination == 4);
    live.settings.set_diffusion_assigned(destination == 5);
    CHECK(live.settings.width_assigned() == (destination == 4));
    CHECK(live.settings.diffusion_assigned() == (destination == 5));
    for (int i = 0; i < 3; ++i) {
      live.settings.mutable_state()->assignable_cv_amount[i] = i ? 255 : 0;
    }
    for (int polarity = -1; polarity <= 1; ++polarity) {
      live.settings.SetFeatureValue(FEATURE_LIVE_ASSIGN, 0);
      live.ui.UpdateLeds();
      uint8_t stock[3];
      for (int i = 0; i < 3; ++i) {
        stock[i] = live.ui.leds_.intensity_[LED_ASSIGN_1 + i];
      }
      if (destination == 4) {
        CHECK(stock[0] > 0 && stock[0] == stock[1] && stock[1] == stock[2]);
      }
      if (destination == 5) {
        CHECK(stock[0] == 0 && stock[1] == 0 && stock[2] > 0);
      }
      live.settings.SetFeatureValue(FEATURE_LIVE_ASSIGN, 1);
      live.ui.set_modulation_indication(false, float(polarity));
      live.ui.UpdateLeds();
      for (int i = 0; i < 3; ++i) {
        CHECK(live.ui.leds_.intensity_[LED_ASSIGN_1 + i] ==
            uint16_t(stock[i]) * (polarity ? 255 : 32) / 255);
      }
    }
  }

  Rig freeze(false);
  SetFixed(&freeze, FEATURE_SAVE_FREEZE, 1);
  freeze.ui.set_freeze_led_state(true);
  freeze.backup.busy_ = true;
  freeze.Tick(1, true);
  CHECK(freeze.ui.leds_.intensity_[LED_FREEZE] == 255);

  const int percentages[] = { 100, 66, 33 };
  for (int level = 0; level < 3; ++level) {
    Rig full(false);
    SetFixed(&full, FEATURE_BRIGHTNESS, level);
    full.settings.mutable_state()->media = 3;
    full.ui.set_freeze_led_state(true);
    full.Tick(1, true);
    CHECK(full.ui.leds_.intensity_[LED_FREEZE] ==
          255 * percentages[level] / 100);
    // Bright digital white {192, 192, 208}, as judged on the module.
    const uint8_t white[3][3] = {
      { 192, 192, 208 }, { 102, 130, 141 }, { 36, 71, 76 }
    };
    for (int i = LED_MEDIA_R; i <= LED_MEDIA_B; ++i) {
      CHECK(full.ui.leds_.intensity_[i] == white[level][i - LED_MEDIA_R]);
    }
    // Every quality colour; unlit channels stay dark.
    for (int media = 0; media < 4; ++media) {
      full.settings.mutable_state()->media = media;
      full.Tick(1, true);
      const Leds::Color color = full.ui.media_palette_[media];
      const uint8_t c[3] = { color.r, color.g, color.b };
      for (int i = 0; i < 3; ++i) {
        CHECK(full.ui.leds_.intensity_[LED_MEDIA_R + i] ==
            DimmedMedia(i, c[i], percentages[level]));
      }
    }
    full.settings.mutable_state()->media = 3;
    full.ui.mode_ = Ui::MODE_STORAGE_WARNING;
    while (!(stmlib::system_clock.milliseconds() & 64)) {
      stmlib::system_clock.Tick();
    }
    full.ui.UpdateLeds();
    CHECK(full.ui.leds_.intensity_[LED_FREEZE] ==
          255 * percentages[level] / 100);
    const uint8_t warning_red[3] = { 255, 136, 48 };
    CHECK(full.ui.leds_.intensity_[LED_MEDIA_R] == warning_red[level]);
  }
  // Red over every intensity at both levels, as the float rule truncates.
  for (int level = 1; level < 3; ++level) {
    for (int value = 0; value < 256; ++value) {
      Leds leds;
      leds.Clear();
      leds.intensity_[LED_MEDIA_R] = value;
      leds.Scale(level);
      CHECK(leds.intensity_[LED_MEDIA_R] ==
          DimmedMedia(0, value, percentages[level]));
    }
  }
  Rig gain(false);
  gain.settings.SetFeatureValue(FEATURE_ATTENUATION, 1);
  gain.Down(SWITCH_MEDIA);
  gain.feedback = 0.1f;
  gain.Tick(20, true);
  const int gains[] = { 119, 120, 136, 137 };
  for (int i = 0; i < 4; ++i) {
    gain.feedback = float(gains[i]) / 256.0f;
    gain.Tick(20, true);
    CHECK((gain.ui.leds_.intensity_[LED_MEDIA_R] == 0 &&
        gain.ui.leds_.intensity_[LED_MEDIA_G] == 255) == (i == 1 || i == 2));
  }

  Rig preview(false);
  uint8_t b[kProfileSize];
  CustomProfile(b, FEATURE_BRIGHTNESS, 2, 0, 0);
  ApplyCustomProfile(&preview, b);
  preview.Enter();
  const int preview_values[] = { 2, 1, 0 };
  for (int i = 0; i < 3; ++i) {
    const int level = preview_values[i];
    preview.pot = (float(level) + 0.5f) / 3.0f;
    preview.Tick(40, true);
    CHECK(preview.settings.FeatureValue(FEATURE_BRIGHTNESS) ==
          (i ? preview_values[i - 1] : 0));
    CHECK(preview.ui.options_.Value(FEATURE_BRIGHTNESS) == level);
    CHECK(preview.ui.leds_.intensity_[LED_ASSIGN_1] ==
          255 * percentages[level] / 100);
    preview.Tick(1002, true);
    CHECK(preview.settings.FeatureValue(FEATURE_BRIGHTNESS) == level);
  }

}

void TestBetaOptionPages() {
  test_checker.Start("TestBetaOptionPages");
  const ProfileFeature features[] = {
    FEATURE_BRIGHTNESS, FEATURE_SEED_LATCH, FEATURE_TIME_SCAN
  };
  for (int f = 0; f < 3; ++f) {
    for (int slot = 1; slot < kNumPageSlots; ++slot) {
      Rig r(false);
      uint8_t b[kProfileSize];
      CustomProfile(b, features[f], slot, 0, 0);
      ApplyCustomProfile(&r, b);
      r.Enter();
      const int knob = (slot - 1) % 3;
      if (knob == 0) {
        r.feedback = 1.0f;
      }
      if (knob == 1) {
        r.pot = 1.0f;
      }
      if (knob == 2) {
        r.reverb = 1.0f;
      }
      r.Tick(1002, true);
      const uint8_t expected = f == 1 ? 3 : f == 0 ? 2 : 1;
      CHECK(r.settings.FeatureValue(features[f]) == expected);
      Settings loaded;
      CHECK(loaded.Init(false, b));
      CHECK(loaded.FeatureValue(features[f]) == expected);
      // Removing the feature leaves its dormant nibble intact.
      uint8_t off[kProfileSize];
      CustomProfile(off, 0, 0, 0, 0);
      Settings disabled;
      CHECK(disabled.Init(false, off));
      CHECK(disabled.FeatureValue(features[f]) == 0);
      disabled.SaveState();
      CHECK(loaded.Init(false, b));
      CHECK(loaded.FeatureValue(features[f]) == expected);
      const int values = kFeatureDefinitions[features[f]].values;
      for (int value = 0; value < values; ++value) {
        const float pot = (float(value) + 0.5f) / float(values);
        if (knob == 0) {
          r.feedback = pot;
        }
        if (knob == 1) {
          r.pot = pot;
        }
        if (knob == 2) {
          r.reverb = pot;
        }
        r.Tick(40, true);
        if (f == 0) {
          CHECK(r.ui.options_.Value(FEATURE_BRIGHTNESS) == value);
          const int percent[] = { 100, 66, 33 };
          CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
              DimmedMedia(0, value ? 255 : 0, percent[value]));
          CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] ==
              DimmedMedia(1, value < 2 ? 255 : 0, percent[value]));
        }
        r.Tick(1002, true);
        CHECK(r.settings.FeatureValue(features[f]) == value);
        CHECK(loaded.Init(false, b));
        CHECK(loaded.FeatureValue(features[f]) == value);
      }
    }
  }
}

// Factory layout with the page preview and first page choices.
void EntryProfile(uint8_t* b, int preview, int first_page) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  AppendEntry(b, FEATURE_PAGE_PREVIEW, PROFILE_FIXED, preview);
  AppendEntry(b, FEATURE_FIRST_PAGE, PROFILE_FIXED, first_page);
}

void Close(Rig* r) {
  r->Down(SWITCH_ASSIGN);
  r->Tick(3000, true);
  r->Up(SWITCH_ASSIGN);
  r->Tick(100, true);
  CHECK(r->ui.options_.stage() == Options::STAGE_INACTIVE);
}

void TestOptionsEntryChoices() {
  test_checker.Start("TestOptionsEntryChoices");
  const ProfileFeature features[] = { FEATURE_PAGE_PREVIEW, FEATURE_FIRST_PAGE };
  uint8_t b[kProfileSize];
  CHECK(FEATURE_PAGE_PREVIEW == 37 && FEATURE_FIRST_PAGE == 38);
  for (int i = 0; i < 2; ++i) {
    const FeatureDefinition& d = kFeatureDefinitions[features[i]];
    CHECK(d.values == 2 && d.field == kNoField);
    CHECK(!d.page && !d.amount && !d.action && !d.stateless);
    // Power-on only: nothing on the panel can change them.
    for (int state = PROFILE_OFF; state <= PROFILE_ADJUSTABLE; ++state) {
      for (int value = 0; value < 3; ++value) {
        IdleProfile(b, PROFILE_OFF, 0);
        AppendEntry(b, features[i], state, value);
        Profile profile;
        CHECK(profile.Init(b) == (state != PROFILE_ADJUSTABLE && value < 2));
      }
    }
  }
  for (int state = PROFILE_OFF; state <= PROFILE_FIXED; ++state) {
    for (int value = 0; value < 2; ++value) {
      Rig r(false);
      for (size_t i = 0; i < kProfileSize; ++i) {
        b[i] = kProfileBlock[i];
      }
      AppendEntry(b, FEATURE_PAGE_PREVIEW, state, value);
      AppendEntry(b, FEATURE_FIRST_PAGE, state, 1 - value);
      ApplyCustomProfile(&r, b);
      const bool fixed = state == PROFILE_FIXED;
      CHECK(r.ui.options_.first_preview_knob_ ==
          (fixed && value ? MIX_PARAMETER_LAST : 0));
      CHECK(r.ui.options_.first_page_ == (fixed && !value));
    }
  }

  // Absent and Fixed 0 keep the walk and the last page.
  for (int kind = 0; kind < 2; ++kind) {
    Rig r(false);
    if (kind) {
      EntryProfile(b, 0, 0);
      ApplyCustomProfile(&r, b);
    }
    r.EnterPage(2);
    CHECK(r.ui.options_.preview_knob() == 0);
    r.Tick(2048, true);
    CHECK(r.ui.options_.preview_knob() == 1);
    Close(&r);
    r.Enter();
    CHECK(r.ui.options_.page() == 2);
    CHECK(r.ui.options_flash_page_ == 2);
    CHECK(r.ui.options_.preview_knob() == 0);
  }

  // First setting only: the state the walk ends in, at once, on every page.
  {
    Rig r(false);
    EntryProfile(b, 1, 0);
    ApplyCustomProfile(&r, b);
    r.Enter();
    // The factory layout has five pages.
    for (int page = 0; page < 6; ++page) {
      if (page) {
        r.Tap();
      }
      CHECK(r.ui.options_.page() == page % 5);
      CHECK(r.ui.options_.preview_knob() == -1);
      CHECK(r.ui.options_.indication().setting ==
          kOptionPages[page % 5].knobs[0]);
      CHECK(stmlib::system_clock.milliseconds() -
          r.ui.options_.indication_time() <= 8);
      CheckPageLeds(&r, page % 5);
    }
    // Knobs still take over and save.
    const uint32_t writes = test_writes;
    const AttackMode mode = r.settings.attack_mode();
    r.reverb = 0.9f;
    r.Tick(1, true);
    CHECK(r.ui.options_.indication().setting == FEATURE_ATTACKS);
    r.Tick(1100, true);
    CHECK(test_writes == writes + kRecordWords);
    CHECK(r.settings.attack_mode() != mode);
    Close(&r);
    r.Enter();
    CHECK(r.ui.options_.page() == 0);
    CHECK(r.ui.options_.preview_knob() == -1);
  }
  {
    // Page two's FEEDBACK LED is dark at once, not lit for its stored value.
    Rig walk(false);
    Rig quiet(false);
    EntryProfile(b, 1, 0);
    ApplyCustomProfile(&quiet, b);
    walk.EnterPage(1);
    walk.Tick(150, true);
    CHECK(walk.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
    quiet.EnterPage(1);
    quiet.Tick(150, true);
    CHECK(quiet.ui.leds_.intensity_[LED_ASSIGN_1] == 0);
  }

  // First page: reopens on page 1 from any page, already while held.
  {
    Rig r(false);
    EntryProfile(b, 0, 1);
    ApplyCustomProfile(&r, b);
    r.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
    r.EnterPage(3);
    CHECK(r.ui.options_.page() == 3);
    Close(&r);
    r.Down(SWITCH_ASSIGN);
    r.Tick(2999, true);
    CHECK(r.ui.options_.stage() == Options::STAGE_WAIT_RELEASE);
    CHECK(r.ui.options_.page() == 0);
    CHECK(r.ui.options_flash_page_ == 0);
    CHECK(r.ui.options_.indication().setting == FEATURE_PITCH);
    r.Up(SWITCH_ASSIGN);
    CHECK(r.ui.options_.page() == 0);
    CHECK(r.ui.options_.preview_knob() == 0);
    CHECK(r.ui.options_.seed_allowed());
    r.Tap();
    CHECK(r.ui.options_.page() == 1);
  }

  // Both on, after the idle timeout; without First page it keeps page 2.
  for (int first = 0; first < 2; ++first) {
    Rig r(false);
    EntryProfile(b, 1, first);
    AppendIdleTimeout(b, PROFILE_FIXED, 1);
    ApplyCustomProfile(&r, b);
    r.EnterPage(1);
    r.Tick(kOptionsIdleTimeouts[1] + 100, true);
    CHECK(r.ui.options_.stage() == Options::STAGE_INACTIVE);
    r.Enter();
    CHECK(r.ui.options_.page() == (first ? 0 : 1));
    CHECK(r.ui.options_.preview_knob() == -1);
  }

  // Custom layout with page 1 empty: the first page used, its first used knob.
  {
    const uint8_t sparse[][2] = {
      { FEATURE_DELAY, 14 }, { FEATURE_LIMITER, 21 }
    };
    Rig r(false);
    PagesProfile(b, sparse, 2);
    AppendEntry(b, FEATURE_PAGE_PREVIEW, PROFILE_FIXED, 1);
    AppendEntry(b, FEATURE_FIRST_PAGE, PROFILE_FIXED, 1);
    ApplyCustomProfile(&r, b);
    r.Enter();
    CHECK(r.ui.options_.page() == 4);
    CHECK(r.ui.options_.preview_knob() == -1);
    CHECK(r.ui.options_.indication().setting == FEATURE_DELAY);
    CheckPageLeds(&r, 4);
    r.Tap();
    CHECK(r.ui.options_.page() == 6);
    CHECK(r.ui.options_.indication().setting == FEATURE_LIMITER);
    Close(&r);
    r.Enter();
    CHECK(r.ui.options_.page() == 4);
  }

  // The same edit writes the same settings with the choices on or off; only
  // the profile identity names the other profile.
  OptionsRecord saved[2];
  for (int on = 0; on < 2; ++on) {
    Rig r(false);
    if (on) {
      EntryProfile(b, 1, 1);
      ApplyCustomProfile(&r, b);
    }
    const uint32_t writes = test_writes;
    const AttackMode mode = r.settings.attack_mode();
    r.Enter();
    r.reverb = 0.9f;
    r.Tick(1100, true);
    Close(&r);
    CHECK(test_writes == writes + 2);  // One record, no state.
    CHECK(r.settings.attack_mode() != mode);
    saved[on] = r.settings.stored_record_;
    saved[on].profile_identity = 0;
  }
  CHECK(!memcmp(&saved[0], &saved[1], sizeof(OptionsRecord)));
}

// Characterization of the gestures before Drizzlet's resolution moves out of
// Ui: each step's outcome, as one line of the Ui and settings state.
std::string GestureState(Rig* r) {
  const State& s = r->settings.state();
  char line[128];
  snprintf(line, sizeof(line),
      "q%d g%d r%d o%d a%d m%d t%d f%d l%d s%d d%d b%d y%d p%d n%d k%d c%d",
      s.media, s.gain, r->gain.restarts, s.gate_output_mode,
      s.disable_autosave, r->ui.mode_, r->ui.gestures_.trim_flag_,
      r->ui.freeze_button_state_, r->ui.seed_button_locked_,
      r->ui.seed_button_state_, s.assignable_cv_destination,
      r->settings.FeatureValue(FEATURE_PLAITS_BANK),
      r->ui.gestures_.self_play_requested_, r->ui.options_.stage(),
      r->ui.options_.page(), s.monophonic_mode,
      r->calibration.starts);
  return line;
}

// Steps: D or U and a switch (F, S, M, A; 8 ms each), K (FEEDBACK turned,
// 50 ms) or T and milliseconds. Returns the state after each step.
std::string RunGesture(Rig* r, const char* steps) {
  std::string out;
  const char* names = "FSMA";
  while (*steps) {
    const char op = *steps++;
    if (op == 'K') {
      // FEEDBACK turned to the other side of noon.
      r->feedback = r->feedback > 0.5f ? 0.2f : 0.8f;
      r->Tick(50, true);
    } else if (op == 'T') {
      r->Tick(atoi(steps), true);
      while (*steps >= '0' && *steps <= '9') {
        ++steps;
      }
    } else {
      const Switch s = Switch(strchr(names, *steps++) - names);
      if (op == 'D') {
        r->Down(s);
      } else {
        r->Up(s);
      }
    }
    while (*steps == ' ') {
      ++steps;
    }
    out += GestureState(r);
    out += '\n';
  }
  return out;
}

bool SameGesture(const char* name, const std::string& got,
    const char* expected) {
  if (got != expected) {
    printf("Gesture %s:\n%s  expected:\n%s", name, got.c_str(), expected);
    return false;
  }
  return true;
}

// The first hold, in ms from the registered press to the release, that a
// release takes as a hold: after the tap boundary, the outcome changes.
uint32_t HoldBoundary(Switch s, uint32_t from, uint32_t to,
    std::string (*outcome)(Rig*)) {
  std::string tap;
  for (uint32_t held = from; held <= to; ++held) {
    Rig r(false);
    test_down[s] = true;
    while (!r.ui.switches_.pressed(s)) {
      r.Tick(1, true);
    }
    const uint32_t start = r.ui.press_time_[s];
    while (stmlib::system_clock.milliseconds() - start < held) {
      r.Tick(1, true);
    }
    r.Up(s);
    r.Tick(20, true);
    const std::string result = outcome(&r);
    if (held == from) {
      tap = result;
    } else if (result != tap) {
      printf("Hold boundary of switch %d: %u ms\n", s, held);
      return held;
    }
  }
  return 0;
}

std::string MediaOutcome(Rig* r) {
  return GestureState(r);
}

std::string OptionsOutcome(Rig* r) {
  return r->ui.options_.stage() == Options::STAGE_INACTIVE ? "closed" : "open";
}

void TestGestureCharacterization() {
  test_checker.Start("TestGestureCharacterization");
  // The [A] hold acts from a 1492 ms press: the release registers 8 ms
  // later, at kSelfPlayHoldTime. ASSIGN opens options at kOptionsHoldTime.
  CHECK(HoldBoundary(SWITCH_MEDIA, 1450, 1550, &MediaOutcome) == 1492);
  CHECK(HoldBoundary(SWITCH_ASSIGN, 2950, 3050, &OptionsOutcome) == 3000);

  struct Script {
    const char* name;
    int synth;  // 0 granular, 1 wavetable synth, 2 Plaits.
    const char* steps;
    const char* expected;
  };
  const Script scripts[] = {
    { "tap", 0, "DM UM",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q2 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "hold", 0, "DM T3000 UM",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g0 r1 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "hold and knob", 0, "DM T200 K T200 UM",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g205 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g205 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g205 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "[A]+FREEZE", 0, "DM DF UF UM T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "[A]+SEED", 0, "DM DS US UM T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k1 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k1 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k1 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k1 c0\n" },
    { "[A]+FREEZE slow", 0, "DM T2000 DF UF T100 UM T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "ASSIGN tap", 0, "DA UA",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d1 b0 y0 p0 n0 k0 c0\n" },
    { "ASSIGN+SEED", 0, "DA DS US UA T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o1 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o1 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o1 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o1 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "ASSIGN+FREEZE", 0, "DA DF UF UA T1100",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a1 m5 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a1 m5 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a1 m5 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a1 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "ASSIGN+MEDIA", 0, "DA DM UM UA T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m1 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c1\n"
        "q3 g100 r0 o0 a0 m1 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c1\n"
        "q3 g100 r0 o0 a0 m1 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c1\n"
        "q3 g100 r0 o0 a0 m1 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c1\n" },
    { "SEED+FREEZE", 0, "DS DF UF US T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l0 s1 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "SEED hold", 0, "DS T4000 US",
        "q3 g100 r0 o0 a0 m0 t0 f0 l0 s1 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "ASSIGN almost to options", 0, "DA T2984 UA T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d1 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d1 b0 y0 p0 n0 k0 c0\n" },
    { "options", 0, "DA T3000 UA T100 DA T3000 UA T200",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p1 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p2 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p2 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p2 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "options with FREEZE held", 0, "DA T3000 DF UA T100 UF T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p1 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "options with MEDIA held", 0, "DA T3000 DM UA T100 UM T20",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p1 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "options with a knob", 0, "DA T3000 UA K T100 DA T3000 UA T200",
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p1 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p2 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p2 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p2 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p2 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p3 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "synth [A]+FREEZE", 1, "DM DF UF UM T20",
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t1 f0 l1 s0 d0 b0 y0 p0 n0 k0 c0\n" },
    { "Plaits [A]+FREEZE", 2, "DM DF UF UM T20 DM DF UF UM T600",
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b2 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b2 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b2 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b2 y0 p0 n0 k0 c0\n" },
    { "Plaits slow [A]+FREEZE", 2, "DM T2000 DF UF UM T20",
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b0 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n"
        "q3 g100 r0 o0 a0 m0 t0 f1 l1 s0 d0 b1 y0 p0 n0 k0 c0\n" },
  };
  for (size_t i = 0; i < sizeof(scripts) / sizeof(scripts[0]); ++i) {
    Rig r(false);
    r.settings.mutable_state()->gain = 100;
    if (scripts[i].synth) {
      PlaySynth(&r, true, scripts[i].synth == 2);
    }
    const std::string got = RunGesture(&r, scripts[i].steps);
    CHECK(SameGesture(scripts[i].name, got, scripts[i].expected));
  }
}

void TestSynthFaultCue() {
  test_checker.Start("TestSynthFaultCue");
  Rig r(false);
  r.settings.values_[FEATURE_BRIGHTNESS] = 0;
  const State state = r.settings.state();
  const uint32_t writes = test_writes;
  for (int synth = 0; synth < 2; ++synth) {
    r.ui.set_synth_mode(synth);
    for (int i = 0; i < 750; ++i) {
      r.ui.set_synth_fault(false);
      CHECK(!r.ui.synth_fault_);
      r.ui.UpdateLeds();
      uint8_t normal[3];
      for (int j = 0; j < 3; ++j) {
        normal[j] = r.ui.leds_.intensity_[LED_ASSIGN_1 + j];
      }
      r.ui.set_synth_fault(true);
      r.ui.UpdateLeds();
      const bool on = stmlib::system_clock.milliseconds() % 250 < 125;
      for (int j = 0; j < 3; ++j) {
        CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + j] ==
            (synth ? (on ? 255 : 0) : normal[j]));
      }
      r.ui.set_synth_fault(false);
      CHECK(!r.ui.synth_fault_);
      r.ui.UpdateLeds();
      for (int j = 0; j < 3; ++j) {
        CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + j] == normal[j]);
      }
      stmlib::system_clock.Tick();
    }
  }
  CHECK(!memcmp(&state, &r.settings.state(), sizeof(state)));
  CHECK(test_writes == writes);
}

void TestGainCeilingPage() {
  test_checker.Start("TestGainCeilingPage");
  for (int slot = 1; slot < kNumPageSlots; ++slot) {
    Rig r(false);
    uint8_t b[kProfileSize];
    CustomProfile(b, FEATURE_GAIN_CEILING, slot, 0, 0);
    ApplyCustomProfile(&r, b);
    r.Enter();
    const int knob = (slot - 1) % 3;
    for (int value = 0; value < 4; ++value) {
      const float pot = (float(value) + 0.5f) / 4.0f;
      if (knob == 0) {
        r.feedback = pot;
      } else if (knob == 1) {
        r.pot = pot;
      } else {
        r.reverb = pot;
      }
      r.Tick(40, true);
      CHECK(r.ui.options_.Value(FEATURE_GAIN_CEILING) == value);
      CHECK(r.ui.gain_ceiling() == value);
      r.Tick(1002, true);
      CHECK(r.settings.FeatureValue(FEATURE_GAIN_CEILING) == value);
      Settings loaded;
      CHECK(loaded.Init(false, b));
      CHECK(loaded.FeatureValue(FEATURE_GAIN_CEILING) == value);
    }
  }
}

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  TestGainCeilingPage();
  TestCommitPublication();
  TestPendingSeed();
  TestPendingExitButtons();
  TestBrightnessHoldsAfterClose();
  TestOptionsTablesAndLifetime();
  TestOptionsKnobsAndSettling();
  TestOptionsSeed();
  TestPageTwoKnobRoutingAndSavedMeaning();
  TestDefaultPageFourControls();
  TestOptionsPreview();
  TestOptionsPalette();
  TestOptionsConfirmations();
  TestOptionsZonesAndSaveBatch();
  TestWidthAssignCycle();
  TestWidthGestureAndMeter();
  TestDiffusionGestureAndMeter();
  TestDiffusionStorage();
  TestDiffusionAssignCycle();
  TestWidthToDiffusionTarget();
  TestPlaybackStorage();
  TestCloudsStorage();
  TestFrozenFeedbackStorage();
  TestAttackStorage();
  TestSynthEngineStorage();
  TestSynthPlayableSets();
  TestPitchStorage();
  TestSelfPlayStorage();
  TestEngineStorage();
  TestEngineOption();
  TestSynthStartOption();
  TestIntervalsStorage();
  TestIntervalsOption();
  TestSelfPlayHold();
  TestTrimGesture();
  TestMonophonicGesture();
  TestAmountSavedByAction();
  TestAudioPatchDetection();
  TestFrozenStateChangeGuard();
  TestGateOutputChordPersistence();
  TestOverwriteReserveDisplay();
  TestCorruptCalibrationLocks();
  TestSettingsRollover();
  TestStorageStaleErrorFlag();
  TestStorageFailedErase();
  TestStorageNonBlankSlot();
  TestStorageFailedProgram();
  TestStorageMatchesStmlib();
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
  TestProfilePages();
  TestOptionalPages();
  TestCustomScalePersistenceAndLeds();
  TestDefaultLedIdentity();
  TestSynthFaultCue();
  TestBetaOptionsLeds();
  TestBetaOptionPages();
  TestProfileActions();
  TestPlaitsBankGesture();
  TestRingsPolyphonyGesture();
  TestSynthActionSteps();
  TestProfileAmounts();
  TestProfileToggles();
  TestTogglesInPlaitsPlay();
  TestFeedbackAmountSlot();
  TestOptionsIdleTimeout();
  TestOptionsEntryChoices();
  TestGestureCharacterization();
  return test_checker.failures() ? 1 : 0;
}
