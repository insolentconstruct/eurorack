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
// Wavetable interpolation and synth signal path tests.

#include <algorithm>
#include <cmath>
#include <cstring>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "beads/dsp/frame.h"
#include "beads/resources.h"
#include "beads/test/test_checker.h"

#define private public
#define protected public
#include "beads/dsp/synth.h"
#include "beads/dsp/wavetable_grain.h"
#include "beads/dsp/wavetable_voice.h"
#undef protected
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
      const float phase = (float(n) + 0.37f) / 4096.0f;
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
  ModulatedParameter density;
  density.base = 0.5f;
  density.cv = 0.0f;
  Parameters parameters = { };
  Processor::MixParameters mix;
  mix.feedback = 0.0f;
  mix.dry_wet = 0.0f;
  mix.reverb_amount = 0.0f;
  mix.output_gate = false;
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


struct SynthHarness {
  uint16_t reverb_buffer[32768];
  Synth synth;
  Parameters parameters;
  Processor::MixParameters mix;
  Synth::Resolution resolution;

  void Init() {
    memset(reverb_buffer, 0, sizeof(reverb_buffer));
    memset(&parameters, 0, sizeof(parameters));
    memset(&mix, 0, sizeof(mix));
    parameters.size.base = 1.0f;
    parameters.shape.base = 0.5f;
    resolution = Synth::RESOLUTION_16_BIT;
    synth.Init(reverb_buffer);
    stmlib::Random::Seed(12345);
  }

  FloatFrame Tick(bool gate, bool frozen) {
    stmlib::GateFlags seed = gate ? stmlib::GATE_FLAG_HIGH :
        stmlib::GATE_FLAG_LOW;
    ModulatedParameter density;
    density.base = 0.5f;
    density.cv = 0.0f;
    FloatFrame out;
    synth.Render(SEED_BUTTON_STATE_OFF, &seed, &frozen, resolution,
        density, parameters, mix, &out, 1);
    return out;
  }
};

void TestSynthMixEndpoints() {
  test_checker.Start("TestSynthMixEndpoints");
  static SynthHarness h;
  h.Init();
  double energy = 0.0;
  for (int i = 0; i < 4096; ++i) {
    FloatFrame out = h.Tick(false, false);
    energy += out.l * out.l;
    CHECK(out.l == out.r);
  }
  CHECK(energy > 1.0);
  CHECK(h.synth.dry_ == 0.5f && h.synth.wet_ == 0.0f);
  h.mix.dry_wet = 1.0f;
  for (int i = 0; i < 256; ++i) {
    FloatFrame out = h.Tick(false, false);
    CHECK(out.l == 0.0f && out.r == 0.0f);
  }
  CHECK(h.synth.dry_ == 0.0f && h.synth.wet_ == 1.0f);
  h.Tick(true, false);
  energy = 0.0;
  for (int i = 0; i < 4096; ++i) {
    FloatFrame out = h.Tick(false, false);
    energy += out.l * out.l + out.r * out.r;
  }
  CHECK(energy > 1.0);
}

void TestSynthFreeze() {
  test_checker.Start("TestSynthFreeze");
  static SynthHarness h;
  h.Init();
  h.Tick(true, true);
  CHECK(h.synth.wavetable_granular_player_.CountActiveGrains() == 0);
  h.Tick(false, false);
  h.Tick(true, false);
  WavetableGrain& grain = h.synth.wavetable_granular_player_.grain_[0];
  CHECK(!grain.done());
  for (int i = 0; i < 256; ++i) {
    h.Tick(false, false);
  }
  const int remaining = grain.remaining_samples();
  const float envelope = grain.envelope_sample_;
  const float phase = grain.phase_;
  for (int i = 0; i < 256; ++i) {
    h.Tick(i & 1, true);
  }
  CHECK(h.synth.wavetable_granular_player_.CountActiveGrains() == 1);
  CHECK(grain.remaining_samples() == remaining);
  CHECK(grain.envelope_sample_ == envelope);
  CHECK(grain.phase_ != phase);
  h.Tick(false, false);
  CHECK(grain.remaining_samples() == remaining - 1);
}

void TestSynthGrainPairs() {
  test_checker.Start("TestSynthGrainPairs");
  static SynthHarness h;
  static WavetableGrain reference[32];
  h.Init();
  h.parameters.size.base = 0.5f;
  WavetableGranularPlayer* player = &h.synth.wavetable_granular_player_;
  TriggerGenerator::Output no_trigger;
  memset(&no_trigger, 0, sizeof(no_trigger));
  int playing_pairs = 0;
  int other_pairs = 0;
  int stolen = 0;
  for (int i = 0; i < 3 * 2600; ++i) {
    const int t = i % 2600;
    const bool freeze = i >= 2600 && i < 5200 && t >= 400 && t < 1400;
    if (t < 72) {
      const int active = player->CountActiveGrains();
      h.Tick(i & 1, false);
      stolen += active == 32 && (i & 1);
    }
    // Pairs against one grain at a time, in index order: same mix and state.
    memcpy(static_cast<void*>(reference), player->grain_, sizeof(reference));
    for (int g = 0; g < 32; g += 2) {
      const bool both = !reference[g].done() && !reference[g + 1].done();
      playing_pairs += both;
      other_pairs += !both;
    }
    FloatFrame paired = { 0.0f, 0.0f };
    player->Render(0, 0, no_trigger, freeze, h.parameters, &paired);
    FloatFrame single = { 0.0f, 0.0f };
    for (int g = 0; g < 32; ++g) {
      reference[g].Render(&single, freeze);
    }
    CHECK(paired.l == single.l && paired.r == single.r);
    CHECK(!memcmp(static_cast<void*>(reference), player->grain_,
        sizeof(reference)));
  }
  CHECK(playing_pairs > 50000 && other_pairs > 30000 && stolen > 0);
}

void TestSynthResolution() {
  test_checker.Start("TestSynthResolution");
  const int dry_bits[] = { 4, 16, 7, 16 };
  const int wet_bits[] = { 4, 6, 7, 16 };
  static SynthHarness h;
  for (int quality = 0; quality < 4; ++quality) {
    h.Init();
    h.resolution = Synth::Resolution(quality);
    h.Tick(true, false);
    CHECK(h.synth.wavetable_granular_player_.grain_[0].scale_up_ ==
        float(1 << wet_bits[quality]));
    bool off_coarser_grid = false;
    for (int i = 0; i < 4096; ++i) {
      const FloatFrame out = h.Tick(false, false);
      // Dry gain is 1/2; the reverb mix and both limiters are unity here.
      const float scaled = out.l * float(2 << dry_bits[quality]);
      CHECK(fabsf(scaled - roundf(scaled)) < 0.001f);
      if (fabsf(scaled * 0.5f - roundf(scaled * 0.5f)) > 0.1f) {
        off_coarser_grid = true;
      }
    }
    CHECK(off_coarser_grid);
    if (wet_bits[quality] == 16) {
      continue;
    }
    // One wet grain: its output over envelope and pan gain sits on the
    // grain's sample grid.
    h.Init();
    h.resolution = Synth::Resolution(quality);
    h.mix.dry_wet = 1.0f;
    h.Tick(false, false);
    h.Tick(true, false);
    const WavetableGrain& grain =
        h.synth.wavetable_granular_player_.grain_[0];
    int checked = 0;
    for (int i = 0; i < 4096; ++i) {
      const FloatFrame out = h.Tick(false, false);
      const float gain = grain.envelope_sample_ * grain.gain_l_;
      if (fabsf(gain) > 0.01f) {
        const float level = out.l / gain * float(1 << wet_bits[quality]);
        CHECK(fabsf(level - roundf(level)) < 0.01f);
        ++checked;
      }
    }
    CHECK(checked > 1000);
  }
}

void TestSynthPitchCvAndSpread() {
  test_checker.Start("TestSynthPitchCvAndSpread");
  static SynthHarness h;
  const float amounts[] = { -1.0f, 0.0f, 1.0f };
  for (int a = 0; a < 3; ++a) {
    float frequency[2][32];
    for (int octave = 0; octave < 2; ++octave) {
      h.Init();
      h.parameters.pitch.has_cv = true;
      h.parameters.pitch.cv = float(octave * 12);
      h.parameters.pitch.attenuverter = amounts[a];
      for (int i = 0; i < 32; ++i) {
        h.Tick(false, false);
        WavetableGrain* grain =
            h.synth.wavetable_granular_player_.new_grain();
        h.Tick(true, false);
        frequency[octave][i] = grain->frequency_;
        const float expected = kWavetableRootNote / 48000.0f *
            float(1 << octave);
        // Lookup interpolation and single precision rounding.
        CHECK(fabsf(h.synth.wavetable_voice_.frequency_ - expected) <
            expected * 0.0001f);
      }
    }
    float low = frequency[0][0];
    float high = low;
    for (int i = 0; i < 32; ++i) {
      CHECK(fabsf(frequency[1][i] / frequency[0][i] - 2.0f) < 0.0001f);
      low = std::min(low, frequency[0][i]);
      high = std::max(high, frequency[0][i]);
    }
    if (a == 1) {
      CHECK(high == low);
    } else {
      CHECK(high / low > 4.0f);
    }
  }
}

void TestSynthAmplitudeSweep() {
  test_checker.Start("TestSynthAmplitudeSweep");
  static SynthHarness h;
  for (int octave = 0; octave <= 6; ++octave) {
    h.Init();
    h.parameters.pitch.base = float(octave * 12);
    double energy = 0.0;
    for (int i = 0; i < 16384; ++i) {
      const FloatFrame out = h.Tick(false, false);
      if (i >= 8192) {
        energy += out.l * out.l;
      }
    }
    const double rms = sqrt(energy / 8192.0);
    // A unit sine at dry gain 1/2 has RMS sqrt(1/8). Allow 6% for
    // differentiation droop up to C9 and the finite observation window.
    CHECK(fabs(rms / sqrt(0.125) - 1.0) < 0.06);
  }
}

void TestQuantizedSynthGrains() {
  test_checker.Start("TestQuantizedSynthGrains");
  static SynthHarness h;
  for (int quality = 0; quality < 4; ++quality) {
    h.Init();
    h.resolution = Synth::Resolution(quality);
    h.parameters.pitch.base = 4.4f;
    h.synth.set_pitch_scale(PITCH_SCALE_FIFTHS);
    h.Tick(true, false);
    WavetableGrain& grain = h.synth.wavetable_granular_player_.grain_[0];
    const float frequency = grain.frequency_;
    CHECK(fabsf(frequency - stmlib::SemitonesToRatio(7.0f) *
        kWavetableRootNote / 48000.0f) < 0.000001f);
    h.synth.set_pitch_scale(PITCH_SCALE_CHROMATIC);
    for (int i = 0; i < 20; ++i) {
      h.Tick(false, false);
      CHECK(grain.frequency_ == frequency);
    }
    float dry[128];
    h.Init();
    h.resolution = Synth::Resolution(quality);
    h.parameters.pitch.base = 4.4f;
    for (int i = 0; i < 128; ++i) {
      dry[i] = h.Tick(false, false).l;
    }
    h.Init();
    h.resolution = Synth::Resolution(quality);
    h.parameters.pitch.base = 4.4f;
    h.synth.set_pitch_scale(PITCH_SCALE_PELOG);
    for (int i = 0; i < 128; ++i) {
      CHECK(h.Tick(false, false).l == dry[i]);
    }
  }
}

// Random intervals reaches the synth's grains.
void TestRandomIntervalSynthGrains() {
  test_checker.Start("TestRandomIntervalSynthGrains");
  static SynthHarness h;
  int off_grid[2] = { 0, 0 };
  for (int intervals = 0; intervals < 2; ++intervals) {
    for (int i = 0; i < 30; ++i) {
      h.Init();
      h.parameters.pitch.base = 0.0f;
      h.parameters.pitch.cv = 0.0f;
      h.parameters.pitch.has_cv = false;
      h.parameters.pitch.attenuverter = -1.0f;
      h.synth.set_random_intervals(intervals);
      h.Tick(true, false);
      const WavetableGrain& grain =
          h.synth.wavetable_granular_player_.grain_[0];
      const float offset = 12.0f * log2f(
          grain.frequency_ * 48000.0f / kWavetableRootNote);
      off_grid[intervals] +=
          fabsf(offset - QuantizePitch(offset, PITCH_SCALE_FIFTHS)) > 0.001f;
    }
  }
  CHECK(off_grid[0] > 10 && off_grid[1] == 0);
}

// Energy of the next samples, split by channel.
static void RenderEnergy(SynthHarness* h, int samples, double* l, double* r) {
  *l = *r = 0.0;
  for (int i = 0; i < samples; ++i) {
    const FloatFrame out = h->Tick(false, false);
    *l += out.l * out.l;
    *r += out.r * out.r;
  }
}

static double Balance(double l, double r) {
  return (l - r) / (l + r);
}

// Left-right balance of one new grain, rendered alone through the synth.
static double GrainBalance(SynthHarness* h) {
  h->synth.wavetable_granular_player_.Reset();
  h->Tick(true, false);
  double l, r;
  RenderEnergy(h, 400, &l, &r);
  CHECK(l + r > 1.0e-6);
  return Balance(l, r);
}

// Same setting as the firmware callback: the saved byte and the ASSIGN CV.
static void SetWidth(SynthHarness* h, StereoWidth width, float cv) {
  h->synth.set_stereo_width(ModulatedStereoWidth(width, cv), width != 0);
}

// Mean and largest imbalance over 48 grains from the same random stream.
static void WidthSpread(SynthHarness* h, StereoWidth width, float cv,
    double* mean, double* largest) {
  stmlib::Random::Seed(777);
  SetWidth(h, width, cv);
  *mean = *largest = 0.0;
  for (int i = 0; i < 48; ++i) {
    const double balance = fabs(GrainBalance(h));
    *mean += balance / 48.0;
    *largest = std::max(*largest, balance);
  }
}

// Balance reached by the extreme pan of a given displacement.
static double ExtremeBalance(float amount) {
  const float pan = 0.5f + 0.5f * amount;
  const double l = double(1.0f - pan) * (1.0f + pan);
  const double r = double(pan) * (2.0f - pan);
  return fabs(Balance(l * l, r * r));
}

void TestSynthStereoWidth() {
  test_checker.Start("TestSynthStereoWidth");
  static SynthHarness h;
  h.Init();
  h.mix.dry_wet = 1.0f;
  h.Tick(false, false);
  double mean, largest;
  double stock_mean, full_mean, small_mean, small_largest;
  // Stock keeps its own pan law (the gate's irregularity spreads it a little),
  // and the CV cannot enable the width while the saved byte is zero.
  double stock_largest;
  WidthSpread(&h, 0, 0.0f, &stock_mean, &stock_largest);
  CHECK(stock_mean > 0.01 && stock_largest > 0.1);
  WidthSpread(&h, 0, 5.0f, &mean, &largest);
  CHECK(mean == stock_mean && largest == stock_largest);
  // Encoded 1 is zero displacement, 255 the full span, 64 about a quarter.
  WidthSpread(&h, 1, 0.0f, &mean, &largest);
  CHECK(largest < 1.0e-4);
  WidthSpread(&h, 64, 0.0f, &small_mean, &small_largest);
  const float small_amount = float(64 - 1) / float(kStereoWidthMax - 1);
  CHECK(small_largest <= ExtremeBalance(small_amount) + 1.0e-3);
  CHECK(small_largest > 0.5 * ExtremeBalance(small_amount));
  WidthSpread(&h, 255, 0.0f, &full_mean, &largest);
  CHECK(largest <= ExtremeBalance(1.0f) + 1.0e-3);
  CHECK(largest > 0.9 * ExtremeBalance(1.0f));
  CHECK(small_mean > 0.02 && full_mean > 2.0 * small_mean);
  CHECK(full_mean > 2.0 * stock_mean);
  // The CV adds to the byte's displacement and clamps to 0..1 at both ends.
  double clamped_mean, clamped_largest;
  WidthSpread(&h, 1, 5.0f, &clamped_mean, &clamped_largest);
  CHECK(clamped_mean == full_mean && clamped_largest == largest);
  WidthSpread(&h, 255, -5.0f, &mean, &clamped_largest);
  CHECK(clamped_largest < 1.0e-4);
  WidthSpread(&h, 1, 0.25f, &mean, &clamped_largest);
  CHECK(clamped_largest > 0.5 * ExtremeBalance(0.25f) &&
      clamped_largest <= ExtremeBalance(0.25f) + 1.0e-3);

  // A grain keeps the width it started with when the width changes.
  stmlib::Random::Seed(777);
  SetWidth(&h, 255, 0.0f);
  double first = 0.0;
  for (int i = 0; i < 32 && fabs(first) < 0.2; ++i) {
    first = GrainBalance(&h);
  }
  CHECK(fabs(first) >= 0.2);
  SetWidth(&h, 1, 0.0f);
  double l, r;
  RenderEnergy(&h, 400, &l, &r);
  CHECK(l + r > 1.0e-6);
  CHECK(fabs(Balance(l, r) - first) < 0.01);
  // A centred grain stays centred after the width opens; later grains spread.
  h.synth.wavetable_granular_player_.Reset();
  h.Tick(true, false);
  SetWidth(&h, 255, 0.0f);
  RenderEnergy(&h, 400, &l, &r);
  CHECK(l + r > 1.0e-6 && fabs(Balance(l, r)) < 1.0e-4);
  double widest = 0.0;
  for (int i = 0; i < 16; ++i) {
    widest = std::max(widest, fabs(GrainBalance(&h)));
  }
  CHECK(widest > 0.2);
}

int main() {
  TestSynthMixEndpoints();
  TestSynthFreeze();
  TestSynthGrainPairs();
  TestQuantizedSynthGrains();
  TestRandomIntervalSynthGrains();
  TestSynthStereoWidth();
  TestSynthResolution();
  TestSynthPitchCvAndSpread();
  TestSynthAmplitudeSweep();
  TestFmBankRatios();
  TestFinalBankRendering();
  TestGuardRow();
  TestSynthBankSelection();
  return test_checker.failures() ? 1 : 0;
}
