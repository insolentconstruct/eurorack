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
// One-off recovery firmware for a module that faults at startup on corrupted
// settings or frozen-buffer flash. It erases the frozen buffer, rewrites the
// settings (keeping the calibration when it reads back clean and plausible),
// then shows the result until the module is updated again.
//
// LEDs:
// - Working (up to about 30 s): blue mode LED, the three ASSIGN LEDs chase.
// - Done, calibration kept: green mode LED blinking once per second.
// - Done, calibration reset to defaults: as above, with the three ASSIGN LEDs
//   lit; recalibrate after installing the firmware.
// - Failed: red mode LED and the ASSIGN LEDs blinking four times per second.

#include <stm32h7xx_hal.h>

#include "beads/drivers/leds.h"
#include "beads/drivers/system.h"
#include "beads/drivers/watchdog.h"
#include "beads/recovery/settings_recovery.h"

using namespace beads;

Leds leds;
SettingsRecovery settings_recovery;

volatile bool ready = false;
volatile RecoveryResult result = RECOVERY_RUNNING;
uint32_t ticks;

void ShowResult() {
  const bool blink = (ticks % 1000) < 500;
  const bool fast_blink = (ticks % 250) < 125;
  Leds::Color color;
  color.r = 0;
  color.g = 0;
  color.b = 0;
  leds.Clear();
  switch (result) {
    case RECOVERY_RUNNING:
      color.b = 255;
      leds.set(Led(LED_ASSIGN_1 + (ticks / 200) % 3), kMaxLedIntensity);
      break;

    case RECOVERY_CALIBRATION_RESET:
      leds.set(LED_ASSIGN_1, kMaxLedIntensity);
      leds.set(LED_ASSIGN_2, kMaxLedIntensity);
      leds.set(LED_ASSIGN_3, kMaxLedIntensity);
      // Fall through.
    case RECOVERY_CALIBRATION_KEPT:
      color.g = blink ? 255 : 0;
      break;

    case RECOVERY_FAILED:
      color.r = fast_blink ? 255 : 0;
      leds.set(LED_ASSIGN_1, fast_blink ? kMaxLedIntensity : 0);
      leds.set(LED_ASSIGN_2, fast_blink ? kMaxLedIntensity : 0);
      leds.set(LED_ASSIGN_3, fast_blink ? kMaxLedIntensity : 0);
      break;
  }
  leds.set_media(color);
  leds.Write();
}

extern "C" {

void SysTick_Handler() {
  HAL_IncTick();
  // Only runs if something else started it.
  Watchdog::Reload();
  if (!ready) {
    return;
  }
  ++ticks;
  ShowResult();
}

}

int main(void) {
  System sys;
  sys.Init(true);
  // Flash is read back right after it is written: bypass the cache.
  SCB_DisableDCache();
  leds.Init();
  ready = true;

  result = settings_recovery.Run();
  while (1) { }
}
