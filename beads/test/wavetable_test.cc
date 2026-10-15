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
// Final wavetable interpolation row tests.

#include <cmath>
#include <cstring>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/granular_player.h"
#include "beads/resources.h"
#include "beads/test/test_checker.h"

#define private public
#include "beads/dsp/synth.h"
#include "beads/dsp/wavetable_grain.h"
#include "beads/dsp/wavetable_voice.h"
#undef private

using namespace beads;

void TestFinalBankRendering() {
  test_checker.Start("TestFinalBankRendering");
  const int stride = kTableSize + 4;
  const int rows = WAV_INTEGRATED_WAVES_SIZE / stride;
  const int bank = (rows - 1) / kNumWavesPerTable - 1;
  CHECK(rows == 193 && bank == 11);
  const float positions[] = { 0.999f, 1.0f };
  for (int i = 0; i < 2; ++i) {
    WavetableGrain grain;
    grain.Init(48000);
    grain.Start(bank, 16, positions[i], 0.00371f, 4096, 0.5f,
        0.5f, 0.5f, 0.37f);
    for (int n = 0; n < 4096; ++n) {
      float phase = (float(n) + 0.37f) / 4096.0f;
      CHECK(std::isfinite(grain.ReadSample(phase)));
      FloatFrame out = { };
      grain.Render(&out);
      CHECK(std::isfinite(out.l) && std::isfinite(out.r));
    }
    WavetableVoice voice;
    voice.Init(48000);
    Parameters p = { };
    p.time.base = positions[i];
    bool freeze = false;
    for (int n = 0; n < 8192; ++n) {
      FloatFrame out = { };
      voice.Render(bank, 16, &freeze, p, &out, 1);
      CHECK(std::isfinite(out.l) && std::isfinite(out.r));
      CHECK(fabsf(out.l) <= 1.0f && out.l == out.r);
    }
    CHECK(voice.bank_ == bank && voice.wave_lp_ > 0.998f);
    // Force the endpoint; the smoother approaches it from below.
    voice.wave_lp_ = 1.0f;
    p.time.base = 1.0f;
    FloatFrame out = { };
    voice.Render(bank, 16, &freeze, p, &out, 1);
    CHECK(std::isfinite(out.l) && std::isfinite(out.r));
  }
}

void TestGuardRow() {
  test_checker.Start("TestGuardRow");
  const int stride = kTableSize + 4;
  const int rows = WAV_INTEGRATED_WAVES_SIZE / stride;
  CHECK(!memcmp(wav_integrated_waves + (rows - 2) * stride,
      wav_integrated_waves + (rows - 1) * stride, stride * sizeof(int16_t)));
}

void TestFmBankRatios() {
  test_checker.Start("TestFmBankRatios");
  const float carrier[] = { 2.0f, 1.0f, 1.0f, 1.0f };
  const float modulator[] = { 1.0f, 0.5f, 3.0f, 8.0f };
  const float amount[] = { 4.0f, 8.0f, 3.0f, 2.0f };
  const double two_pi = 6.283185307179586;
  for (int bank = 0; bank < 4; ++bank) {
    for (int wave = 0; wave < 16; ++wave) {
      const int16_t* row = wav_integrated_waves +
          ((bank + 8) * 16 + wave) * (kTableSize + 4);
      double expected[256];
      double mean = 0.0;
      for (int n = 0; n < 256; ++n) {
        const double t = double(n) / 256.0;
        const double index = amount[bank] * wave * wave / 225.0;
        expected[n] = sin(two_pi * (t * carrier[bank] +
            sin(two_pi * t * modulator[bank]) * index));
        mean += expected[n];
      }
      mean /= 256.0;
      double dot = 0.0, energy = 0.0, reference_energy = 0.0;
      for (int n = 1; n < 256; ++n) {
        // Differentiation recovers the source wave from the integrated table.
        const double sample = row[n + 4] - row[n + 3];
        const double reference = expected[n] - mean;
        dot += sample * reference;
        energy += sample * sample;
        reference_energy += reference * reference;
      }
      CHECK(dot / sqrt(energy * reference_energy) > 0.9999);
      WavetableVoice voice;
      voice.Init(48000);
      voice.bank_ = bank + 8;
      voice.wave_lp_ = float(wave) / 15.0f;
      Parameters parameters = { };
      parameters.time.base = voice.wave_lp_;
      bool freeze = false;
      FloatFrame out = { };
      voice.Render(bank + 8, 16, &freeze, parameters, &out, 1);
      CHECK(std::isfinite(out.l) && out.l == out.r);
    }
  }
}

void TestSynthBankSelection() {
  test_checker.Start("TestSynthBankSelection");
  static uint16_t reverb_buffer[32768];
  static Synth synth;
  synth.Init(reverb_buffer);
  const size_t kSize = 8;
  stmlib::GateFlags seed[kSize] = { };
  bool freeze[kSize] = { };
  ModulatedParameter density = { 0.5f, 0.0f };
  Parameters parameters = { };
  Processor::MixParameters mix = { 0.0f, 0.0f, 0.0f, false };
  FloatFrame out[kSize];
  // FEEDBACK selects one of 12 banks. Moving up, bank b + 1 starts at
  // (b + 1.048) / 12; moving down, bank b returns at (b + 0.952) / 12.
  const int kSteps = 12000;
  int bank = 0;
  for (int i = 0; i <= kSteps; ++i) {
    mix.feedback = float(i) / kSteps;
    synth.Render(SEED_BUTTON_STATE_OFF, seed, freeze,
        Synth::RESOLUTION_16_BIT, density, parameters, mix, out, kSize);
    const int selected = synth.bank_quantizer_.quantized_value();
    if (selected != bank) {
      CHECK(selected == bank + 1);
      CHECK(fabsf(mix.feedback * 12.0f - (bank + 1.048f)) < 0.002f);
      bank = selected;
    }
  }
  CHECK(bank == 11);
  for (int i = kSteps; i >= 0; --i) {
    mix.feedback = float(i) / kSteps;
    synth.Render(SEED_BUTTON_STATE_OFF, seed, freeze,
        Synth::RESOLUTION_16_BIT, density, parameters, mix, out, kSize);
    const int selected = synth.bank_quantizer_.quantized_value();
    if (selected != bank) {
      CHECK(selected == bank - 1);
      CHECK(fabsf(mix.feedback * 12.0f - (selected + 0.952f)) < 0.002f);
      bank = selected;
    }
  }
  CHECK(bank == 0);
}

int main() {
  TestFmBankRatios();
  TestFinalBankRendering();
  TestGuardRow();
  TestSynthBankSelection();
  return test_checker.failures() ? 1 : 0;
}
