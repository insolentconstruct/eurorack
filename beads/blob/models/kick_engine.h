// Copyright 2012 Emilie Gillet.
// Copyright 2026 Lyle Mills.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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
//
// Kick: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_KICK_ENGINE_H_
#define BEADS_BLOB_MODELS_KICK_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// Excitation delays and decays at the 48 kHz sample rate.
const int32_t kKickPulse0DelayTicks = 0;
const float kKickPulse0Decay = 3340.0f / 4096.0f;
const int32_t kKickPulse1DelayTicks = 48;  // 1.0e-3 * 48000, line 2313
const float kKickPulse1Decay = 3072.0f / 4096.0f;

// Braids truncates the strike levels to integers before sample scaling.
const float kKickPulse0Level = 275251.0f / 32768.0f;
const float kKickPulse1Level = -13763.0f / 32768.0f;

// Pulse 1 holds a constant level until its delay expires.
const float kKickPulse1Hold = 16384.0f / 32768.0f;
const int32_t kKickClickDelayTicks = 192;  // 4.0e-3 * 48000, line 2317
const float kKickClickSemitones = 17.0f;  // 17 << 7, line 2352

// Braids' SVF frequency ceiling and internal sample clip.
const float kKickSvfMaxNormalizedFrequency = 0.125f;
const float kKickClip = 32767.0f / 32768.0f;

// Braids' fixed punch coefficient and the rate-adjusted modulation scales.
const float kKickPunchStockCoefficient = 34.0f;
const float kKickSvfRateRatio = 2.0f;  // Braids' 96 kHz SVF / this port's rate
const float kKickPunchFreqScale = kKickSvfRateRatio / 8192.0f;
const float kKickPunchDampScale = 1.0f / 8.0f;
const float kKickPunchFloor = 2048.0f / 32768.0f;
const float kKickPunchThreshold = 4096.0f / 32768.0f;

// excitation >> 4, the raw exciter blended dry alongside the filtered signal
const float kKickExcitationDryMix = 1.0f / 16.0f;

class Kick {
 public:
  Kick() { }
  ~Kick() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  // Excitation pulses 0 and 1 (digital_oscillator.cc's pulse_[0]/pulse_[1]).
  float pulse0_state_;
  int32_t pulse0_counter_;
  float pulse1_state_;
  int32_t pulse1_counter_;

  // Pulse 2 supplies only the pitch-click countdown.
  int32_t click_counter_;

  // SVF low-pass, band-pass and output smoothing states.
  float svf_lp_;
  float svf_bp_;
  float lp_state_;

  // ParameterInterpolator targets for the knob-derived quantities.
  float resonance_domain_;
  float tone_coefficient_;
  float punch_scale_;
  float balance_gain0_;
  float balance_gain1_;

  DISALLOW_COPY_AND_ASSIGN(Kick);
};

class KickEngine : public Engine {
 public:
  KickEngine() { }
  ~KickEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    kick_.Init(allocator);
  }

  virtual void Reset() {
    kick_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    kick_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Kick kick_;

  DISALLOW_COPY_AND_ASSIGN(KickEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_KICK_ENGINE_H_
