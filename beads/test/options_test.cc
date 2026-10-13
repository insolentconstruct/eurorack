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
unsigned test_writes = 0;
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
  Rig r;
  unsigned writes = test_writes;
  r.Down(SWITCH_ASSIGN); r.Tick(2992);
  assert(!r.ui.options_.active());
  r.Tick(8); r.Up(SWITCH_ASSIGN);
  assert(r.ui.options_.stage() == Options::EDITING);
  // Deliberate movement below qualification refreshes inactivity only.
  r.Tick(4000); r.pot = .54f; r.Tick(1500);
  assert(r.ui.options_.stage() == Options::EDITING);
  assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.pot = .9f; r.Tick(10);
  assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  r.settings.SaveState(); ++writes; // Preview must not leak into unrelated saves.
  assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.Down(SWITCH_ASSIGN); r.Tick(3000, false);
  assert(r.ui.options_.commit_requested());
  assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.ui.DoEvents(); r.Up(SWITCH_ASSIGN);
  assert(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  assert(test_writes == writes + 1);
  r.Tick(30); r.Enter(); r.pot = .1f; r.Tick(10); r.Tick(5000);
  assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  assert(test_writes == writes + 1); // Timeout cancels.
  r.Tick(30); r.Enter(); r.Tap();
  assert(r.ui.options_.page() == 1);
  r.pot = .9f; r.Tick(10);
  assert(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  r.Down(SWITCH_MEDIA);
  assert(r.calibration.starts == 0); // Active page consumes stock chords.
  Rig startup(true); startup.Tick(3500);
  assert(!startup.ui.options_.active());
  Rig chord; chord.Down(SWITCH_ASSIGN); chord.Down(SWITCH_MEDIA);
  assert(chord.calibration.starts == 1);
  puts("gesture/preview/commit: PASS");
}

void TestQueueRace() {
  Rig r;
  r.Enter();
  // Exit drain in SysTick precisely between main's availability check and pull.
  r.ui.options_.stage_ = Options::DRAIN;
  r.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 0);
  assert(r.ui.queue_.available() == 1);
  r.Tick(1, false);
  assert(!r.ui.options_.active());
  r.ui.queue_.PullEvent();
  printf("remaining events after interrupted pull: %zu\n", r.ui.queue_.available());
  fflush(stdout);
  assert(r.ui.queue_.available() == 0); // Old ISR Flush makes this 15.
  uint8_t media = r.settings.state().media;
  r.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 1);
  r.ui.DoEvents(); // Pending main-context flush also discards stale releases.
  assert(r.ui.queue_.available() == 0);
  assert(r.settings.state().media == media);
  puts("queue interrupt interleaving: PASS");
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
    x->l *= gain; x->r *= gain;
  }
};
void TestDsp() {
  for (float sr : {24000.0f, 32000.0f, 48000.0f}) {
    Limiter stock, fast, off;
    float decay = 1.0f / (sr * 6.0f);
    stock.Init(decay, .9f); fast.Init(decay, .9f); off.Init(decay, .9f);
    StockLimiter reference(decay);
    for (int i = 0; i < 4096; ++i) {
      FloatFrame x = {sinf(i * .17f) * 4.0f, cosf(i * .13f) * 2.0f};
      FloatFrame y = x, z = x;
      stock.ProcessOutput(&y, 1, OUTPUT_LIMITER_STOCK, sr);
      reference.Process(&z);
      assert(!memcmp(&y, &z, sizeof(y)));
      off.ProcessOutput(&x, 1, OUTPUT_LIMITER_OFF, sr);
      assert(x.l == sinf(i * .17f) * 4.0f && x.r == cosf(i * .13f) * 2.0f);
      fast.ProcessOutput(&x, 1, OUTPUT_LIMITER_FAST, sr);
      assert(std::isfinite(x.l) && std::isfinite(x.r));
    }
    for (int i = 0; i < int(sr); ++i) {
      FloatFrame x = {.1f, -.1f}, y = x;
      stock.ProcessOutput(&x, 1, OUTPUT_LIMITER_STOCK, sr);
      fast.ProcessOutput(&y, 1, OUTPUT_LIMITER_FAST, sr);
      if (i == int(sr) - 1) assert(y.l > x.l && y.l >= .099f);
    }
  }
  puts("DSP mode dispatch: PASS");
}
int main(int argc, char**) {
  assert(argc == 2);
  assert(mmap(reinterpret_cast<void*>(0x081e0000), 0x20000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x081e0000));
  TestQueueRace(); TestStorage(); TestUi(); TestDsp();
}
