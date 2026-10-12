// Host tests for the guarded page, real UI and real STAT chunk storage.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstddef>
#include <sys/mman.h>
#define private public
#include "beads/ui.h"
#include "beads/settings.h"
#undef private
#include "beads/calibrator.h"
#include "beads/background_buffer_backup.h"
#include "beads/gain_control.h"
#include "beads/dsp/limiter.h"

bool test_down[4] = {};
unsigned test_writes = 0, test_erases = 0;
bool test_interrupt = false;
namespace stmlib { SystemClock system_clock; }
namespace beads {
bool Leds::Init() { Clear(); return true; }
void Leds::Clear() { std::fill(intensity_, intensity_ + LED_LAST, 0); }
void Leds::Write() { }
}
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
  float pot = 0.5f;
  Rig(bool startup_assign = false) {
    std::fill(test_down, test_down + 4, false);
    test_down[SWITCH_ASSIGN] = startup_assign;
    memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
    settings.Init(false);
    ui.Init(&settings, &gain, &calibration, &backup);
    Tick(40);
  }
  void Tick(int n, bool events = true) {
    while (n--) {
      ui.ProcessPot(Ui::MIX_PARAMETER_FEEDBACK, 0.5f);
      ui.ProcessPot(Ui::MIX_PARAMETER_DRY_WET, pot);
      ui.ProcessPot(Ui::MIX_PARAMETER_REVERB, 0.5f);
      ui.Poll();
      if (events) ui.DoEvents();
    }
  }
  void Down(Switch s) { test_down[s] = true; Tick(8); }
  void Up(Switch s) { test_down[s] = false; Tick(8); }
  void Enter() {
    Down(SWITCH_ASSIGN); Tick(2999);
    assert(ui.options_.stage() == Options::WAIT_RELEASE);
    Up(SWITCH_ASSIGN);
    assert(ui.options_.stage() == Options::EDITING);
  }
  void Tap() { Down(SWITCH_ASSIGN); Up(SWITCH_ASSIGN); }
};

void TestUi() {
  {
    Rig r;
    auto assignment = r.settings.state().assignable_cv_destination;
    unsigned writes = test_writes;
    r.Down(SWITCH_ASSIGN); r.Tick(2992);
    assert(!r.ui.options_.active());
    r.Tick(8); assert(r.ui.options_.stage() == Options::WAIT_RELEASE);
    r.Up(SWITCH_ASSIGN);
    assert(r.settings.state().assignable_cv_destination == assignment);
    assert(test_writes == writes);
    r.Tick(5000);
    assert(r.ui.options_.stage() == Options::DRAIN);
    assert(test_writes == writes);
    r.Tick(10); assert(!r.ui.options_.active());
  }
  {
    Rig r(true); r.Tick(3500);
    assert(!r.ui.options_.active());
    r.Up(SWITCH_ASSIGN); r.Tick(30); r.Enter();
  }
  for (int s = 0; s < 3; ++s) {
    Rig r; r.Down(Switch(s)); r.Down(SWITCH_ASSIGN); r.Tick(3500);
    assert(!r.ui.options_.active());
  }
  {
    Rig r; r.Down(SWITCH_ASSIGN); r.Tick(2000);
    r.pot = 0.7f; r.Tick(1600);
    assert(!r.ui.options_.active());
    assert(r.ui.assignable_cv_pot_[1].editing_hidden_parameter());
  }
  {
    Rig r; r.Down(SWITCH_ASSIGN); r.Tick(2000);
    test_down[SWITCH_FREEZE] = true; r.Tick(1);
    test_down[SWITCH_FREEZE] = false; r.Tick(1200);
    assert(!r.ui.options_.active());
  }
  for (int mode = Ui::MODE_CALIBRATION_1; mode <= Ui::MODE_PANIC; ++mode) {
    Rig r; r.ui.mode_ = Ui::Mode(mode);
    r.Down(SWITCH_ASSIGN); r.Tick(3500);
    assert(!r.ui.options_.active());
  }
  {
    Rig r; r.pot = 1.0f; r.Tick(20); r.Enter();
    unsigned writes = test_writes;
    r.Tick(100); assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    r.pot = 0.92f; r.Tick(10);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    r.pot = 0.8f; r.Tick(10);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    r.settings.SaveState(); // Unrelated frozen-buffer save must not commit preview.
    assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    ++writes;
    r.pot = 0.66f; r.Tick(10);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    r.pot = 0.63f; r.Tick(10);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_FAST);
    r.pot = 0.69f; r.Tick(10);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_FAST);
    r.pot = 0.71f; r.Tick(10);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    assert(r.ui.mix_parameter(Ui::MIX_PARAMETER_DRY_WET) == 1.0f);
    r.Down(SWITCH_ASSIGN); r.Tick(3000);
    assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    r.Up(SWITCH_ASSIGN);
    assert(test_writes == writes + 1);
    r.Tick(50); assert(test_writes == writes + 1);
    assert(r.ui.mix_parameter(Ui::MIX_PARAMETER_DRY_WET) == 1.0f);
    r.pot = 0.5f; r.Tick(10);
    assert(r.ui.mix_parameter(Ui::MIX_PARAMETER_DRY_WET) < 1.0f);
  }
  for (int s = 0; s < 3; ++s) {
    Rig r; r.Enter(); r.pot = 1.0f; r.Tick(10);
    unsigned writes = test_writes;
    r.Down(Switch(s)); r.Down(SWITCH_ASSIGN); r.Tick(20);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    assert(r.calibration.starts == 0);
    assert(test_writes == writes);
    r.Up(Switch(s)); r.Up(SWITCH_ASSIGN); r.Tick(20);
    assert(!r.ui.options_.active());
    assert(r.calibration.starts == 0);
  }
  {
    Rig r; r.Enter(); unsigned writes = test_writes;
    r.Tap(); assert(r.ui.options_.page() == 1);
    r.pot = 1.0f; r.Tick(10);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    r.Tap(); assert(r.ui.options_.page() == 2);
    r.Tap(); assert(r.ui.options_.page() == 0);
    r.Tick(20); assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    r.pot = 0.5f; r.Tick(20);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_FAST);
    r.pot = 0.1f; r.Tick(20);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    r.Down(SWITCH_ASSIGN); r.Tick(3000);
    assert(test_writes == writes); // Away-and-back is not dirty.
    r.Up(SWITCH_ASSIGN); r.Tick(20);
  }
  {
    Rig r; r.Down(SWITCH_ASSIGN); r.Down(SWITCH_MEDIA);
    assert(r.calibration.starts == 1); // Stock chord still works before entry.
  }
  {
    Rig r; stmlib::system_clock.count_ = 0xfffff000;
    r.Tick(30); r.Enter(); r.pot = 0.9f; r.Tick(20);
    r.Down(SWITCH_ASSIGN); r.Tick(3000);
    assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  }
  {
    Rig r; r.Enter(); r.Down(SWITCH_ASSIGN);
    unsigned writes = test_writes;
    uint8_t assignment = r.settings.state().assignable_cv_destination;
    r.ui.Panic(); r.Tick(2000);
    assert(r.ui.options_.stage() == Options::DRAIN);
    r.Up(SWITCH_ASSIGN); r.Tick(50);
    assert(r.settings.state().assignable_cv_destination == assignment);
    assert(test_writes == writes);
  }
  {
    Rig r; r.Enter(); r.pot = .9f; r.Tick(20);
    unsigned writes = test_writes;
    test_down[SWITCH_ASSIGN] = true; r.Tick(3010, false);
    assert(r.ui.options_.stage() == Options::DRAIN);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    test_down[SWITCH_ASSIGN] = false; r.Tick(100, false);
    assert(r.ui.options_.stage() == Options::DRAIN);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    assert(r.ui.options_.commit_requested());
    r.Tick(1, false); // An interrupt between the commit check and State store.
    assert(r.ui.options_.commit_requested());
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    r.ui.DoEvents();
    assert(!r.ui.options_.commit_requested());
    assert(test_writes == writes + 1);
    r.Tick(10, false);
    assert(r.ui.options_.stage() == Options::INACTIVE);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    r.ui.DoEvents();
    assert(test_writes == writes + 1);
  }
  {
    Rig r; r.Enter(); r.pot = .9f; r.Tick(20);
    unsigned writes = test_writes;
    r.Tick(5000);
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    assert(test_writes == writes);
    r.Tick(50); assert(!r.ui.options_.active());
  }
  for (int a = 0; a < SWITCH_LAST; ++a) {
    for (int b = a + 1; b < SWITCH_LAST; ++b) {
      for (int order = 0; order < 3; ++order) {
        Rig r;
        if (order == 2) {
          test_down[a] = test_down[b] = true; r.Tick(8);
        } else {
          r.Down(Switch(order ? b : a));
          r.Down(Switch(order ? a : b));
        }
        r.Tick(3500);
        assert(!r.ui.options_.active());
        assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
      }
    }
  }
  {
    Rig r; r.Down(SWITCH_ASSIGN); r.Down(SWITCH_SEED);
    assert(r.settings.state().gate_output_mode == 1);
    assert(!r.ui.seed_button_state());
  }
  {
    Rig r; r.Down(SWITCH_ASSIGN); r.Down(SWITCH_FREEZE);
    assert(r.settings.state().disable_autosave == 1);
    assert(!r.ui.freeze_button_state());
  }
  {
    Rig r; r.Down(SWITCH_SEED); r.Down(SWITCH_FREEZE);
    assert(r.ui.seed_button_locked());
    assert(!r.ui.freeze_button_state());
  }
  {
    Rig r; r.Enter();
    r.ui.set_freeze_led_state(true);
    stmlib::system_clock.count_ = 3599; r.Tick(1);
    assert(r.ui.leds_.intensity_[LED_MEDIA_G] == 255);
    assert(r.ui.leds_.intensity_[LED_MEDIA_R] == 0);
    assert(r.ui.leds_.intensity_[LED_FREEZE] == 255);
    assert(r.ui.leds_.intensity_[LED_VU_METER] == 128);
    r.pot = .64f; stmlib::system_clock.count_ = 4799; r.Tick(1);
    assert(r.ui.leds_.intensity_[LED_MEDIA_B] == 255);
    stmlib::system_clock.count_ = 4999; r.Tick(1);
    assert(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);
    stmlib::system_clock.count_ = 5099; r.Tick(1);
    assert(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);
    r.pot = .9f; stmlib::system_clock.count_ = 5999; r.Tick(1);
    assert(r.ui.leds_.intensity_[LED_MEDIA_R] == 255);
    stmlib::system_clock.count_ = 6199; r.Tick(1);
    assert(r.ui.leds_.intensity_[LED_MEDIA_R] == 0);
    stmlib::system_clock.count_ = 6299; r.Tick(1);
    assert(r.ui.leds_.intensity_[LED_MEDIA_R] == 255);
    r.Tick(5000);
    assert(r.ui.leds_.intensity_[LED_MEDIA_R] == 192);
    assert(r.ui.leds_.intensity_[LED_MEDIA_G] == 192);
    assert(r.ui.leds_.intensity_[LED_MEDIA_B] == 208);
  }
  {
    Rig r;
    r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
    r.ui.LoadOptions();
    assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  }
  puts("UI integration: PASS");
}

void TestStorage() {
  Settings s;
  memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
  assert(!s.Init(false));
  assert(s.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings old_record; assert(old_record.Init(false));
  assert(old_record.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  for (int i = 0; i < 256; ++i) {
    s.mutable_state()->padding[0] = i;
    assert(s.output_limiter_mode() == (i == 0xa1 ? OUTPUT_LIMITER_FAST :
        i == 0xa2 ? OUTPUT_LIMITER_OFF : OUTPUT_LIMITER_STOCK));
  }
  for (int i = 0; i < 3; ++i) {
    s.set_output_limiter_mode(OutputLimiterMode(i)); s.SaveState();
    Settings loaded; assert(loaded.Init(false));
    assert(loaded.output_limiter_mode() == i);
    assert(loaded.state().media == 3);
  }
  // The exact legacy layout uses the same tag and size and reads the new record.
  State legacy_state = {};
  PersistentData legacy_calibration = {};
  stmlib::ChunkStorage<15, PersistentData, State> legacy;
  assert(legacy.Init(&legacy_calibration, &legacy_state));
  assert(legacy_state.media == 3 && legacy_state.padding[0] == 0xa2);
  memset(legacy_state.padding, 0, sizeof(legacy_state.padding)); legacy.SaveState();
  Settings downgrade; assert(downgrade.Init(false));
  assert(downgrade.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  s.Init(false); s.set_output_limiter_mode(OUTPUT_LIMITER_FAST);
  for (int i = 0; i < 4100; ++i) s.SaveState();
  Settings rollover; assert(rollover.Init(false));
  assert(rollover.output_limiter_mode() == OUTPUT_LIMITER_FAST);
  rollover.SavePersistentData();
  Settings calibration; assert(calibration.Init(false));
  assert(calibration.output_limiter_mode() == OUTPUT_LIMITER_FAST);
  test_interrupt = true; calibration.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  calibration.SaveState(); test_interrupt = false;
  Settings interrupted; assert(interrupted.Init(false));
  assert(interrupted.output_limiter_mode() == OUTPUT_LIMITER_FAST);
  Settings recovery; assert(recovery.Init(true));
  assert(recovery.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  puts("STAT real ChunkStorage: PASS");
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
    x->l *= gain; x->r *= gain;
  }
};
void TestDsp(const char* output) {
  for (float sr : {24000.0f, 32000.0f, 48000.0f}) {
    Limiter l; l.Init(1.0f / (sr * 6.0f), .9f);
    Limiter bypass; bypass.Init(1.0f / (sr * 6.0f), .9f);
    StockLimiter reference(1.0f / (sr * 6.0f));
    for (int i = 0; i < 100000; ++i) {
      FloatFrame x = {sinf(i * .17f) * (i % 101), cosf(i * .13f) * 2.0f};
      FloatFrame y = x;
      l.ProcessOutput(&x, 1, OUTPUT_LIMITER_STOCK, sr); reference.Process(&y);
      assert(!memcmp(&x, &y, sizeof(x)));
      assert(std::isfinite(x.l) && std::isfinite(x.r));
      y = x; bypass.ProcessOutput(&x, 1, OUTPUT_LIMITER_OFF, sr);
      assert(!memcmp(&x, &y, sizeof(x)));
    }
    for (int mode = 0; mode < 2; ++mode) {
      l.Init(1.0f / (sr * 6.0f), .9f);
      FloatFrame x = {};
      for (int i = 0; i < int(sr / 10); ++i) {
        x = {4.0f, -4.0f}; l.ProcessOutput(&x, 1, OutputLimiterMode(mode), sr);
      }
      assert(fabsf(x.l - .9f) < .0001f);
      char name[512];
      snprintf(name, sizeof(name), "%s/recovery-%.0f-%d.csv", output, sr, mode);
      FILE* curve = fopen(name, "w"); assert(curve);
      fprintf(curve, "seconds,gain\n");
      int recovery = -1;
      for (int i = 0; i < int(sr * 30); ++i) {
        x = {.1f, -.1f}; l.ProcessOutput(&x, 1, OutputLimiterMode(mode), sr);
        assert(std::isfinite(x.l));
        if (x.l >= .099f && recovery < 0) recovery = i;
        if (i % int(sr / 100) == 0) {
          fprintf(curve, "%.6f,%.9f\n", i / sr, x.l / .1f);
        }
      }
      fclose(curve);
      assert(recovery >= 0);
      if (mode == OUTPUT_LIMITER_FAST) assert(recovery / sr < 1.0f);
      printf("recovery sr=%.0f mode=%d t99=%.6f gain=%.6f\n", sr, mode, recovery / sr, x.l / .1f);
    }
    // Sustained overload: bound detector modulation after settling.
    for (float frequency : {20.0f, 55.0f, 220.0f, 1000.0f, 5000.0f}) {
      l.Init(1.0f / (sr * 6.0f), .9f);
      float min_gain = 1.0f, max_gain = 0.0f;
      for (int i = 0; i < int(2 * sr); ++i) {
        float signal = 1.5f * sinf(i * (2.0f * 3.14159265f * frequency / sr));
        FloatFrame y = {signal, -signal};
        l.ProcessOutput(&y, 1, OUTPUT_LIMITER_FAST, sr);
        assert(std::isfinite(y.l) && std::isfinite(y.r));
        if (i > sr && fabsf(signal) > .1f) {
          float gain = y.l / signal;
          min_gain = std::min(gain, min_gain);
          max_gain = std::max(gain, max_gain);
        }
      }
      float ripple = 20.0f * log10f(max_gain / min_gain);
      assert(ripple < .5f);
      printf("sustained sr=%.0f frequency=%.0f min_gain=%.9f max_gain=%.9f ripple_db=%.6f\n",
          sr, frequency, min_gain, max_gain, ripple);
    }
    FloatFrame x = {4.0f, -4.0f};
    l.ProcessOutput(&x, 1, OUTPUT_LIMITER_OFF, sr);
    assert(x.l == 4.0f && x.r == -4.0f);
    CONSTRAIN(x.l, -1.0f, .999999f); CONSTRAIN(x.r, -1.0f, .999999f);
    assert(x.l == .999999f && x.r == -1.0f);
  }
  puts("DSP: PASS");
}
int main(int argc, char** argv) {
  assert(argc == 2);
  assert(mmap(reinterpret_cast<void*>(0x081e0000), 0x20000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x081e0000));
  TestStorage(); TestUi(); TestDsp(argv[1]);
}
