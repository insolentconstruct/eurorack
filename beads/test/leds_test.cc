// Copyright 2026 Insolent Construct.
// Author: Insolent Construct
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
// Host checks for quality LED dimming.

#include "beads/drivers/led_curve.h"
#include "beads/drivers/leds.h"
#include "beads/test/test_checker.h"

using namespace beads;

static uint8_t frame[LED_LAST];

void Leds::Write() {
  for (int i = 0; i < LED_LAST; ++i) {
    frame[i] = intensity_[i];
  }
}

void TestLevels() {
  test_checker.Start("levels");
  const Leds::Color full[] = {
    { 128, 0, 48 }, { 255, 144, 0 }, { 0, 255, 192 }, { 192, 192, 208 }
  };
  // Q4 at 50% is +1 red and -1 blue versus the direct-drive sweep.
  // At 15%, the table inverse is within one PWM step of the sweep.
  const Leds::Color expected[][4] = {
    { { 89, 0, 43 }, { 70, 0, 40 }, { 51, 0, 37 }, { 29, 0, 34 } },
    { { 181, 117, 0 }, { 140, 103, 0 }, { 97, 86, 0 }, { 51, 65, 0 } },
    { { 0, 226, 144 }, { 0, 200, 122 }, { 0, 158, 98 }, { 0, 106, 69 } },
    { { 131, 151, 156 }, { 103, 131, 130 }, { 72, 107, 104 }, { 39, 78, 72 } }
  };
  const uint8_t levels[] = { 1, 0, 2, 3 };
  const uint8_t percent[] = { 66, 100, 33, 15 };
  Leds leds;
  for (int q = 0; q < 4; ++q) {
    for (int dim = 0; dim < 4; ++dim) {
      const uint8_t white = 137;
      for (int i = LED_FREEZE; i < LED_LAST; ++i) {
        leds.set(Led(i), white);
      }
      leds.set_media(full[q], dim == 1 ? 0.5f : 1.0f);
      leds.Scale(levels[dim]);
      leds.Write();
      CHECK(frame[LED_MEDIA_R] == expected[q][dim].r);
      CHECK(frame[LED_MEDIA_G] == expected[q][dim].g);
      CHECK(frame[LED_MEDIA_B] == expected[q][dim].b);
      for (int i = LED_FREEZE; i < LED_LAST; ++i) {
        CHECK(frame[i] == white * percent[dim] / 100);
      }
    }
  }

  for (int pwm = 0; pwm < 256; ++pwm) {
    const uint8_t value = uint8_t(pwm);
    const Leds::Color color = { value, value, value };
    leds.set_media(color, 1.0f);
    for (int i = LED_FREEZE; i < LED_LAST; ++i) {
      leds.set(Led(i), value);
    }
    leds.Scale(0);
    leds.Write();
    for (int i = 0; i < LED_LAST; ++i) {
      CHECK(frame[i] == value);
    }
  }
}

void TestMonotonicity() {
  test_checker.Start("monotonicity");
  const LedCurvePoint* curves[] = { led_curve_r, led_curve_g, led_curve_b };
  for (int channel = 0; channel < 3; ++channel) {
    const LedCurvePoint* curve = curves[channel];
    for (int step = 0; step <= 100; ++step) {
      const float level = step / 100.0f;
      CHECK(DimLed(curve, 0, level) == 0);
      for (int pwm = 1; pwm <= 255; ++pwm) {
        const uint8_t dimmed = DimLed(curve, pwm, level);
        CHECK(dimmed <= pwm);
        CHECK(dimmed >= DimLed(curve, pwm - 1, level));
        if (step > 0) {
          CHECK(dimmed >= DimLed(curve, pwm, (step - 1) / 100.0f));
        } else {
          CHECK(dimmed == 0);
        }
      }
    }
  }
}

int main() {
  TestLevels();
  TestMonotonicity();
  return test_checker.failures() != 0;
}
