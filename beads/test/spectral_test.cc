// Copyright 2026 Insolent Construct.
//
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
// Spectral engine tests: the CMSIS FFT against ShyFFT, the port against
// Clouds' own phase vocoder, and the engine inside Beads' processor.

#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef USE_ARM_FFT
#include <arm_math.h>
#endif  // USE_ARM_FFT

#include "stmlib/fft/shy_fft.h"
#include "stmlib/utils/random.h"

#define private public
#include "beads/dsp/processor.h"
#undef private
#include "beads/dsp/pvoc/phase_vocoder.h"
#include "beads/dsp/pvoc/spectral_parameters.h"
#include "beads/resources.h"
#include "beads/test/clouds_pvoc_reference.h"
#include "beads/test/test_checker.h"
#include "beads/test/test_runner.h"

using namespace beads;

// GCC 12 (amd64), -ffp-contract=off. Qualities in Processor::Media order;
// the last is Digital with a mono input.
const uint32_t kSpectralHashes[5] = {
  0xbe5dca30, 0x00bbe4d5, 0xa44bf737, 0xba55e376, 0xf82c3475
};

const size_t kFftSize = 4096;

static uint32_t noise_state;

inline float Noise() {
  noise_state = noise_state * 1664525U + 1013904223U;
  return float(int32_t(noise_state)) / 2147483648.0f;
}

// Two partials and some noise per channel: spectra worth transforming.
void TestSignal(size_t t, float sample_rate, FloatFrame* frame) {
  const double time = double(t) / sample_rate;
  frame->l = 0.3f * float(sin(2.0 * M_PI * 220.0 * time)) +
      0.1f * float(sin(2.0 * M_PI * 1375.0 * time)) + 0.02f * Noise();
  frame->r = 0.25f * float(sin(2.0 * M_PI * 330.0 * time)) +
      0.12f * float(sin(2.0 * M_PI * 2900.0 * time)) + 0.02f * Noise();
}

#ifdef USE_ARM_FFT

// ShyFFT's spectrum is all real parts then all imaginary parts, with the
// Nyquist bin in slot N/2; CMSIS interleaves them, with Nyquist in slot 1.
// The imaginary parts have opposite signs, and only CMSIS' inverse scales
// by 1/N.
void TestFftConventions() {
  test_checker.Start("TestFftConventions");
  static float frame[kFftSize];
  static float shy_in[kFftSize];
  static float shy_out[kFftSize];
  static float arm_in[kFftSize];
  static float arm_out[kFftSize];
  static stmlib::ShyFFT<float, kFftSize, stmlib::RotationPhasor> shy;
  arm_rfft_fast_instance_f32 arm;
  shy.Init();
  CHECK(arm_rfft_fast_init_f32(&arm, kFftSize) == ARM_MATH_SUCCESS);
  noise_state = 1;
  for (size_t i = 0; i < kFftSize; ++i) {
    frame[i] = Noise() * lut_sine_window_4096[i];
  }
  memcpy(shy_in, frame, sizeof(frame));
  memcpy(arm_in, frame, sizeof(frame));
  shy.Direct(shy_in, shy_out);
  arm_rfft_fast_f32(&arm, arm_in, arm_out, 0);

  const size_t half = kFftSize / 2;
  double power = 0.0;
  double error = 0.0;
  double unflipped = 0.0;
  for (size_t i = 0; i < half; ++i) {
    const double re = arm_out[2 * i];
    const double im = i ? -arm_out[2 * i + 1] : arm_out[1];
    power += shy_out[i] * shy_out[i] + shy_out[i + half] * shy_out[i + half];
    error += (re - shy_out[i]) * (re - shy_out[i]) +
        (im - shy_out[i + half]) * (im - shy_out[i + half]);
    if (i) {
      const double wrong = arm_out[2 * i + 1] - shy_out[i + half];
      unflipped += (re - shy_out[i]) * (re - shy_out[i]) + wrong * wrong;
    }
  }
  const double snr = 10.0 * log10(power / error);
  const double unflipped_snr = 10.0 * log10(power / unflipped);
  printf("Forward FFT: CMSIS vs ShyFFT %.1f dB, without the sign fix "
         "%.1f dB\n", snr, unflipped_snr);
  CHECK(snr > 100.0);
  CHECK(unflipped_snr < 10.0);

  // Inverse: CMSIS takes the interleaved layout and divides by N.
  for (size_t i = 0; i < half; ++i) {
    arm_in[2 * i] = shy_out[i];
    arm_in[2 * i + 1] = i ? -shy_out[i + half] : shy_out[half];
  }
  memcpy(shy_in, shy_out, sizeof(shy_out));
  shy.Inverse(shy_in, shy_out);
  arm_rfft_fast_f32(&arm, arm_in, arm_out, 1);
  power = error = 0.0;
  for (size_t i = 0; i < kFftSize; ++i) {
    const double expected = frame[i] * double(kFftSize);
    power += expected * expected;
    error += (shy_out[i] - expected) * (shy_out[i] - expected);
  }
  const double shy_snr = 10.0 * log10(power / error);
  power = error = 0.0;
  for (size_t i = 0; i < kFftSize; ++i) {
    const double scaled = arm_out[i] * double(kFftSize);
    power += shy_out[i] * shy_out[i];
    error += (scaled - shy_out[i]) * (scaled - shy_out[i]);
  }
  const double inverse_snr = 10.0 * log10(power / error);
  printf("Inverse FFT: CMSIS x N vs ShyFFT %.1f dB; ShyFFT round trip "
         "%.1f dB\n", inverse_snr, shy_snr);
  CHECK(inverse_snr > 100.0);
  CHECK(shy_snr > 90.0);
}

#endif  // USE_ARM_FFT

struct Scenario {
  const char* name;
  int num_channels;
  clouds_reference::Settings settings;
  int freeze_after;  // Blocks; negative: never.
  // With CMSIS, in dB. Coarse magnitude steps and the glitches' loudest-bin
  // choices turn rounding differences into whole steps.
  double min_snr;
};

const Scenario kScenarios[] = {
  { "neutral", 2,
    { 0.0f, 0.0f, false, false, 0.5f, 1.0f, 0.0f, 0.5f }, -1, 85.0 },
  { "shift and warp", 2,
    { 0.3f, 7.0f, false, false, 0.3f, 0.75f, 0.3f, 0.8f }, -1, 70.0 },
  { "down, sparse refresh", 1,
    { 0.8f, -5.0f, false, false, 0.7f, 0.3f, 0.0f, 0.2f }, -1, 55.0 },
  { "quantized, frozen", 2,
    { 0.5f, 12.0f, false, false, 0.1f, 0.9f, 0.6f, 0.5f }, 2000, 6.0 },
  { "glitch", 2,
    { 1.0f, 3.0f, false, true, 0.95f, 0.6f, 0.1f, 0.6f }, -1, 40.0 },
};

static uint8_t spectral_banks[2][kSpectralBufferSize];
static float fft_buffer[kFftSize];
static float ifft_buffer[kFftSize];
static PhaseVocoder phase_vocoder;

// Beads' port (CMSIS) and Clouds' build (ShyFFT) fed identical input and
// parameters, with the same random sequence.
double ReferenceSnr(const Scenario& scenario) {
  const size_t kBlockSize = 32;
  const size_t kNumBlocks = 4000;
  const size_t kSkip = 500;
  static float input[kNumBlocks * kBlockSize * 2];
  static float reference[kNumBlocks * kBlockSize * 2];
  noise_state = 7;
  for (size_t t = 0; t < kNumBlocks * kBlockSize; ++t) {
    FloatFrame frame;
    TestSignal(t, 32000.0f, &frame);
    input[2 * t] = frame.l;
    input[2 * t + 1] = frame.r;
  }

  clouds_reference::Settings s = scenario.settings;
  stmlib::Random::Seed(0x5eed);
  clouds_reference::Init(scenario.num_channels);
  for (size_t block = 0; block < kNumBlocks; ++block) {
    s.freeze = scenario.freeze_after >= 0 &&
        int(block) >= scenario.freeze_after;
    const size_t offset = block * kBlockSize * 2;
    clouds_reference::Process(
        s, &input[offset], &reference[offset], kBlockSize);
    clouds_reference::Buffer();
  }

  SpectralParameters p;
  p.position = s.position;
  p.pitch = s.pitch;
  p.gate = s.gate;
  p.spectral.quantization = s.quantization;
  p.spectral.refresh_rate = s.refresh_rate;
  p.spectral.phase_randomization = s.phase_randomization;
  p.spectral.warp = s.warp;
  memset(spectral_banks, 0, sizeof(spectral_banks));
  void* buffer[2] = { spectral_banks[0], spectral_banks[1] };
  size_t buffer_size[2] = { kSpectralBufferSize, kSpectralBufferSize };
  phase_vocoder.Init(
      buffer, buffer_size, fft_buffer, ifft_buffer,
      lut_sine_window_4096, kFftSize, scenario.num_channels, 16, 48000.0f);
  stmlib::Random::Seed(0x5eed);
  double power = 0.0;
  double error = 0.0;
  bool finite = true;
  for (size_t block = 0; block < kNumBlocks; ++block) {
    p.freeze = scenario.freeze_after >= 0 &&
        int(block) >= scenario.freeze_after;
    const size_t offset = block * kBlockSize * 2;
    FloatFrame out[kBlockSize];
    memset(out, 0, sizeof(out));  // Mono leaves R alone.
    phase_vocoder.Process(
        p, reinterpret_cast<const FloatFrame*>(&input[offset]), out,
        kBlockSize);
    phase_vocoder.Buffer();
    for (size_t i = 0; i < kBlockSize; ++i) {
      finite = finite && std::isfinite(out[i].l) && std::isfinite(out[i].r);
      if (block < kSkip) {
        continue;
      }
      for (int c = 0; c < scenario.num_channels; ++c) {
        const double expected = reference[offset + 2 * i + c];
        const double actual = c ? out[i].r : out[i].l;
        power += expected * expected;
        error += (actual - expected) * (actual - expected);
      }
    }
  }
  CHECK(finite);
  CHECK(power > 1.0);  // The reference is not silent.
  return error == 0.0 ? 200.0 : 10.0 * log10(power / error);
}

void TestCloudsReference() {
  test_checker.Start("TestCloudsReference");
  for (size_t i = 0; i < sizeof(kScenarios) / sizeof(kScenarios[0]); ++i) {
    const double snr = ReferenceSnr(kScenarios[i]);
    printf("Clouds reference, %s: %.1f dB\n", kScenarios[i].name, snr);
#ifdef USE_ARM_FFT
    CHECK(snr >= kScenarios[i].min_snr);
#else
    CHECK(snr == 200.0);  // Identical with the same FFT.
#endif  // USE_ARM_FFT
  }
}

static Processor processor;
static uint8_t banks[3][262144];
static uint16_t reverb[32768];

void InitProcessor(int quality, bool mono, bool spectral) {
  memset(static_cast<void*>(&processor), 0, sizeof(processor));
  memset(banks, 0xa5, sizeof(banks));
  memset(reverb, 0, sizeof(reverb));
  uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
  processor.Init(buffers, reverb);
  processor.set_spectral(spectral);
  processor.Configure(Processor::Media(quality), mono);
}

void DefaultControls(Parameters* p, ModulatedParameter* density,
                     Processor::MixParameters* mix) {
  memset(p, 0, sizeof(*p));
  p->time.base = 0.3f;
  p->time.attenuverter = 0.2f;
  p->size.base = 0.7f;
  p->shape.base = 0.4f;
  p->shape.attenuverter = -0.3f;
  p->pitch.base = 7.0f;
  p->pitch.attenuverter = 0.1f;
  density->base = 0.7f;
  density->cv = 0.0f;
  mix->feedback = 0.3f;
  mix->dry_wet = 1.0f;
  mix->reverb_amount = 0.2f;
  mix->output_gate = false;
}

// Three seconds through the whole processor, frames computed as the main
// loop would after every block.
uint32_t RenderSpectral(int quality, bool mono, double* rms,
                        float diffusion, float size) {
  InitProcessor(quality, mono, true);
  processor.set_diffusion(diffusion);
  stmlib::Random::Seed(0x12345678);
  noise_state = 3;
  Parameters p;
  ModulatedParameter density;
  Processor::MixParameters mix;
  DefaultControls(&p, &density, &mix);
  p.size.base = size;
  mix.reverb_amount = mono ? 0.0f : mix.reverb_amount;  // Keeps L = R.
  const float sample_rate = processor.sample_rate();
  const size_t block_size = size_t(sample_rate) / 4000;
  const size_t num_blocks = 3 * 4000;
  uint32_t hash = 2166136261U;
  double energy = 0.0;
  size_t count = 0;
  bool finite = true;
  for (size_t block = 0; block < num_blocks; ++block) {
    FloatFrame in[24];
    FloatFrame out[24];
    bool freeze[24] = { };
    for (size_t i = 0; i < block_size; ++i) {
      TestSignal(block * block_size + i, sample_rate, &in[i]);
    }
    processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze,
        density, p, mix, in, out, block_size);
    processor.Buffer();
    for (size_t i = 0; i < block_size; ++i) {
      finite = finite && std::isfinite(out[i].l) && std::isfinite(out[i].r);
      if (mono) {
        finite = finite && out[i].l == out[i].r;
      }
    }
    if (block >= num_blocks / 2) {
      for (size_t i = 0; i < block_size; ++i) {
        energy += out[i].l * out[i].l + out[i].r * out[i].r;
        ++count;
      }
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(out);
      for (size_t i = 0; i < block_size * sizeof(FloatFrame); ++i) {
        hash = (hash ^ bytes[i]) * 16777619U;
      }
    }
  }
  CHECK(finite);
  *rms = sqrt(energy / double(2 * count));
  return hash;
}

void TestSpectralRender() {
  test_checker.Start("TestSpectralRender");
  for (int q = 0; q < 5; ++q) {
    const bool mono = q == 4;
    const int quality = mono ? 3 : q;
    double rms = 0.0;
    const uint32_t hash = RenderSpectral(quality, mono, &rms, 0.0f, 0.7f);
    double again_rms = 0.0;
    const uint32_t again = RenderSpectral(
        quality, mono, &again_rms, 0.0f, 0.7f);
    printf("Spectral q%d%s %08x RMS %.4f\n", quality, mono ? " mono" : "",
           hash, rms);
    CHECK(rms > 0.02 && rms < 1.0);
    CHECK(hash == again);
#ifndef GOLDEN_CAPTURE
    CHECK(hash == kSpectralHashes[q]);
#endif
  }
}

// The diffuser and delay mode stay off: SIZE at its end is a warp.
void TestSpectralBypasses() {
  test_checker.Start("TestSpectralBypasses");
  double rms = 0.0;
  const uint32_t plain = RenderSpectral(3, false, &rms, 0.0f, 0.7f);
  CHECK(RenderSpectral(3, false, &rms, 1.0f, 0.7f) == plain);
  RenderSpectral(3, false, &rms, 0.0f, 1.0f);
  CHECK(processor.delay_mode_transition_ < 0.0f);
  CHECK(rms > 0.02);
}

// Random values change only when a frame starts; knob values reach the
// frame's controls through Clouds' mapping and the pitch scale.
void TestFrameControls() {
  test_checker.Start("TestFrameControls");
  InitProcessor(3, false, true);
  stmlib::Random::Seed(1);
  Parameters p;
  ModulatedParameter density;
  Processor::MixParameters mix;
  DefaultControls(&p, &density, &mix);
  p.time.attenuverter = 0.8f;
  p.pitch.base = 0.4f;
  p.pitch.attenuverter = 0.0f;
  density.base = 1.0f;
  const SpectralParameters& frame = processor.spectral_processor_.parameters_;
  int changes = 0;
  int last_change = -1000;
  float position = -1.0f;
  for (size_t block = 0; block < 400; ++block) {
    FloatFrame in[12];
    FloatFrame out[12];
    bool freeze[12] = { };
    freeze[5] = block == 300;
    for (size_t i = 0; i < 12; ++i) {
      TestSignal(block * 12 + i, 48000.0f, &in[i]);
    }
    processor.set_pitch_scale(block < 200 ? PITCH_SCALE_OFF
                                          : PITCH_SCALE_CHROMATIC);
    processor.Process(block == 100 ? SEED_BUTTON_STATE_ON
                                   : SEED_BUTTON_STATE_LOCKED,
        NULL, freeze, density, p, mix, in, out, 12);
    processor.Buffer();
    if (frame.position != position) {
      // A hop is 85.3 blocks of 12 samples.
      CHECK(int(block) - last_change >= 85);
      last_change = int(block);
      position = frame.position;
      ++changes;
    }
    CHECK(frame.gate == (block == 100));
    CHECK(frame.freeze == (block == 300));
    // The scale applies from the first frame after it changes (block 255).
    if (block < 255) {
      CHECK(fabsf(frame.pitch - 0.4f) < 1e-6f);
    } else {
      CHECK(frame.pitch == 0.0f);
    }
  }
  // 4800 samples, 1024-sample hops.
  printf("Frame draws: %d\n", changes);
  CHECK(changes == 5);
  CHECK(fabsf(frame.spectral.refresh_rate - 1.0f) < 1e-6f);
  CHECK(fabsf(frame.spectral.phase_randomization - 1.0f) < 1e-6f);
  CHECK(fabsf(frame.spectral.warp - (4.0f * 0.008f + 0.5f)) < 1e-5f);
}

// Each engine change clears the recording it takes over.
void TestEngineBuffers() {
  test_checker.Start("TestEngineBuffers");
  InitProcessor(3, false, false);
  for (int bank = 0; bank < 3; ++bank) {
    bool clear = true;
    for (size_t i = 0; i < sizeof(banks[bank]) && clear; ++i) {
      clear = banks[bank][i] == 0;
    }
    CHECK(clear || bank == 2);
  }
  memset(banks, 0x5a, sizeof(banks));
  processor.set_spectral(true);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  bool clear = true;
  for (int bank = 0; bank < 2; ++bank) {
    for (size_t i = 0; i < sizeof(banks[bank]); ++i) {
      clear = clear && banks[bank][i] == 0;
    }
  }
  CHECK(clear);
  double rms = 0.0;
  RenderSpectral(3, false, &rms, 0.0f, 0.7f);
  memset(banks, 0x5a, sizeof(banks));
  processor.set_spectral(false);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  clear = true;
  for (int bank = 0; bank < 2; ++bank) {
    for (size_t i = 0; i < 200000; ++i) {
      clear = clear && banks[bank][i] == 0;
    }
  }
  CHECK(clear);
}

// A stalled main loop (a flash erase) loses the frames whose input has been
// overwritten: the next call computes only the newest one. Attacks stay off.
void TestStalledFrames() {
  test_checker.Start("TestStalledFrames");
  InitProcessor(3, false, true);
  Parameters p;
  ModulatedParameter density;
  Processor::MixParameters mix;
  DefaultControls(&p, &density, &mix);
  // The spectral engine alone: no dry signal, reverb or feedback.
  mix.feedback = 0.0f;
  mix.dry_wet = 1.0f;
  mix.reverb_amount = 0.0f;
  CHECK(!processor.attack_recording());
  const STFT& stft = processor.spectral_processor_.phase_vocoder_.stft_[0];
  double replay = 0.0;
  double after = 0.0;
  for (size_t block = 0; block < 2000; ++block) {
    FloatFrame in[12];
    FloatFrame out[12];
    bool freeze[12] = { };
    for (size_t i = 0; i < 12; ++i) {
      TestSignal(block * 12 + i, 48000.0f, &in[i]);
    }
    processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze, density, p,
        mix, in, out, 12);
    // No frames from block 500 to 1000: about six hops.
    if (block < 500 || block >= 1000) {
      processor.Buffer();
      CHECK(stft.ready_ == stft.done_);
    }
    for (size_t i = 0; i < 12; ++i) {
      CHECK(std::isfinite(out[i].l) && std::isfinite(out[i].r));
      const double e = out[i].l * out[i].l + out[i].r * out[i].r;
      // A full ring (5120 samples) after the stall begins, nothing old
      // plays again; frames resume afterwards.
      if (block >= 940 && block < 1000) {
        replay += e;
      }
      if (block >= 1200) {
        after += e;
      }
    }
  }
  printf("Stall: replay %.3g, after %.3g\n", replay, after);
  CHECK(replay < 1e-9);
  CHECK(after > 1.0);
  // The counters wrap.
  STFT& s = processor.spectral_processor_.phase_vocoder_.stft_[0];
  s.done_ = size_t(-2);
  s.ready_ = size_t(1);
  processor.Buffer();
  CHECK(s.ready_ == s.done_);
}

int main(int argc, char** argv) {
  setbuf(stdout, NULL);
  const HostTest tests[] = {
#ifdef USE_ARM_FFT
    { "TestFftConventions", TestFftConventions },
    { "TestCloudsReference", TestCloudsReference },
    { "TestSpectralRender", TestSpectralRender },
    { "TestSpectralBypasses", TestSpectralBypasses },
    { "TestFrameControls", TestFrameControls },
    { "TestEngineBuffers", TestEngineBuffers },
    { "TestStalledFrames", TestStalledFrames },
#else
    { "TestCloudsReference", TestCloudsReference },
#endif  // USE_ARM_FFT
  };
  const int result = RunHostTests(argc, argv, tests,
      sizeof(tests) / sizeof(tests[0]));
  return result ? result : (test_checker.failures() ? 1 : 0);
}
