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
// Measured PWM-to-light curves for the quality LED.

#include "beads/drivers/led_curve.h"

#include "beads/drivers/leds.h"

namespace beads {

const LedCurvePoint led_curve_r[] = {
  { 10, 0 }, { 16, 5 }, { 32, 19 }, { 128, 112 }, { 240, 222 },
  { 250, 236 }, { 255, 248 }
};
const LedCurvePoint led_curve_g[] = {
  { 40, 0 }, { 48, 3 }, { 64, 10 }, { 80, 19 }, { 112, 43 },
  { 128, 57 }, { 160, 87 }, { 192, 119 }, { 208, 138 }, { 224, 165 },
  { 240, 209 }, { 255, 257 }
};
const LedCurvePoint led_curve_b[] = {
  { 32, 0 }, { 48, 7 }, { 64, 21 }, { 96, 57 }, { 144, 119 },
  { 192, 181 }, { 208, 203 },
  { 224, 228 }, { 240, 265 }, { 255, 333 }
};

uint8_t DimLed(
    const LedCurvePoint* curve, uint8_t pwm, float level) {
  if (level >= 1.0f) {
    return pwm;
  }
  if (pwm <= curve->pwm || level <= 0.0f) {
    return 0;
  }

  const LedCurvePoint* upper = curve + 1;
  while (upper->pwm < pwm) {
    ++upper;
  }
  const LedCurvePoint& lower = upper[-1];
  const float light = float(lower.light) +
      float(pwm - lower.pwm) * float(upper->light - lower.light) /
      float(upper->pwm - lower.pwm);
  const float target = light * level;

  upper = curve + 1;
  while (float(upper->light) < target) {
    ++upper;
  }
  const LedCurvePoint& inverse_lower = upper[-1];
  return uint8_t(float(inverse_lower.pwm) +
      (target - float(inverse_lower.light)) *
      float(upper->pwm - inverse_lower.pwm) /
      float(upper->light - inverse_lower.light) + 0.5f);
}

void Leds::Scale(uint8_t level) {
  if (!level) {
    return;
  }
  const uint8_t percent = level == 1 ? 66 : level == 3 ? 15 : 33;
  const Color color = {
    intensity_[LED_MEDIA_R], intensity_[LED_MEDIA_G], intensity_[LED_MEDIA_B]
  };
  set_media(color, float(percent) / 100.0f);
  for (int i = LED_FREEZE; i < LED_LAST; ++i) {
    intensity_[i] = uint16_t(intensity_[i]) * percent / 100;
  }
}

void Leds::set_media(Color color, float a) {
  if (a != 1.0f) {
    color.r = DimLed(led_curve_r, color.r, a);
    color.g = DimLed(led_curve_g, color.g, a);
    color.b = DimLed(led_curve_b, color.b, a);
  }
  set_media(color);
}

}  // namespace beads
