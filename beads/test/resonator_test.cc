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
// Modal resonator tests: the bank alone, its tuning, level and placements.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif  // defined(__SSE__)

#define private public
#include "beads/dsp/processor.h"
#include "stmlib/dsp/cosine_oscillator.h"
#include "stmlib/dsp/filter.h"
#undef private
#include "beads/test/test_checker.h"

using namespace beads;
using namespace stmlib;

namespace rings {
extern const float lut_stiffness[];
extern const float lut_4_decades[];
}  // namespace rings

static Processor processor;
static uint8_t banks[3][262144];
static uint16_t reverb_buffer[32768];
static Parameters parameters;
static ModulatedParameter density;
static Processor::MixParameters mix;
static FloatFrame input[12];
static FloatFrame output[12];
static bool freeze[12];
static uint32_t noise_state;

const Processor::Media kMedia[4] = {
  Processor::MEDIA_DIGITAL,
  Processor::MEDIA_VINTAGE_DIGITAL,
  Processor::MEDIA_CLEAN_TAPE,
  Processor::MEDIA_DIRTY_TAPE
};

float Noise() {
  noise_state = noise_state * 1664525 + 1013904223;
  return float(int32_t(noise_state)) / 2147483648.0f;
}

void Init(Processor::Media media, bool mono, int placement) {
  memset(static_cast<void*>(&processor), 0, sizeof(processor));
  uint8_t* buffers[3] = { banks[0], banks[1], banks[2] };
  memset(banks, 0, sizeof(banks));
  processor.Init(buffers, reverb_buffer);
  processor.set_resonator(placement);
  processor.Configure(media, mono);
  memset(&parameters, 0, sizeof(parameters));
  memset(&density, 0, sizeof(density));
  memset(&mix, 0, sizeof(mix));
  memset(input, 0, sizeof(input));
  memset(freeze, 0, sizeof(freeze));
  parameters.time.base = 0.02f;
  parameters.size.base = 0.5f;
  parameters.shape.base = 0.5f;
  density.base = 0.9f;
  mix.reverb_amount = 0.3f;
  noise_state = 1;
  stmlib::Random::Seed(1);
}

size_t BlockSize() {
  return size_t(processor.sample_rate()) / 4000;
}

void Process() {
  processor.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze, density,
      parameters, mix, input, output, BlockSize());
}

void FillSine(int block, float level) {
  const size_t size = BlockSize();
  const float rate = processor.sample_rate();
  for (size_t i = 0; i < size; ++i) {
    const float t = float(block * size + i) / rate;
    input[i].l = level * sinf(2.0f * float(M_PI) * 220.0f * t);
    input[i].r = level * sinf(2.0f * float(M_PI) * 331.0f * t);
  }
}

uint64_t Hash(uint64_t hash, const FloatFrame* frames, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    uint32_t bits[2];
    memcpy(&bits[0], &frames[i].l, 4);
    memcpy(&bits[1], &frames[i].r, 4);
    hash = (hash ^ bits[0]) * 1099511628211ull;
    hash = (hash ^ bits[1]) * 1099511628211ull;
  }
  return hash;
}

// Output hash and a hash of the recording after a sine played for 0.5 s.
void RunSine(Processor::Media media, int placement, float dry_wet,
    uint64_t* out_hash, uint64_t* recording_hash) {
  Init(media, false, placement);
  mix.dry_wet = dry_wet;
  uint64_t hash = 14695981039346656037ull;
  for (int block = 0; block < 2000; ++block) {
    FillSine(block, 0.3f);
    Process();
    hash = Hash(hash, output, BlockSize());
  }
  *out_hash = hash;
  uint64_t recording = 14695981039346656037ull;
  const uint8_t* bytes = &banks[0][0];
  for (size_t i = 0; i < sizeof(banks); ++i) {
    recording = (recording ^ bytes[i]) * 1099511628211ull;
  }
  *recording_hash = recording;
}

float Goertzel(const float* x, size_t size, float frequency) {
  const float w = 2.0f * float(M_PI) * frequency;
  const float coefficient = 2.0f * cosf(w);
  float s1 = 0.0f;
  float s2 = 0.0f;
  for (size_t i = 0; i < size; ++i) {
    const float s = x[i] + coefficient * s1 - s2;
    s2 = s1;
    s1 = s;
  }
  return s1 * s1 + s2 * s2 - coefficient * s1 * s2;
}

// The defaults read table entries, the constants of the fixed resonator.
void TestResonatorDefaults() {
  test_checker.Start("TestResonatorDefaults");
  const float stiffness = stmlib::Interpolate(
      rings::lut_stiffness, kResonatorStructure, 256.0f);
  const float decades = stmlib::Interpolate(
      rings::lut_4_decades, kResonatorDamping, 256.0f);
  const float zero = 0.0f;
  const float hundred = 100.0f;
  CHECK(!memcmp(&stiffness, &zero, sizeof(float)));
  CHECK(!memcmp(&decades, &hundred, sizeof(float)));
  // String and Medium are the defaults.
  CHECK(kResonatorMaterials[1].structure == kResonatorStructure);
  CHECK(kResonatorMaterials[1].brightness == kResonatorBrightness);
  CHECK(kResonatorRingTimes[1] == kResonatorDamping);
  // Default controls, as the UI sets them, change nothing.
  for (int placement = 1; placement < 3; ++placement) {
    uint64_t hashes[2][2];
    for (int pass = 0; pass < 2; ++pass) {
      Init(Processor::MEDIA_DIGITAL, false, placement);
      if (pass) {
        ResonatorControls c;
        c.structure = kResonatorStructure;
        c.brightness = kResonatorBrightness;
        c.damping = kResonatorDamping;
        c.position = kResonatorPosition;
        c.amount = 1.0f;
        c.feedback = 0.0f;
        c.hold_feedback = false;
        c.modes = NULL;
        processor.set_resonator_controls(c);
      }
      mix.dry_wet = 0.8f;
      uint64_t hash = 14695981039346656037ull;
      for (int block = 0; block < 2000; ++block) {
        FillSine(block, 0.3f);
        processor.set_attack_trigger(block % 300 == 0);
        Process();
        hash = Hash(hash, output, BlockSize());
      }
      hashes[pass][0] = hash;
      hashes[pass][1] = Hash(14695981039346656037ull,
          reinterpret_cast<const FloatFrame*>(&banks[0][0]),
          sizeof(banks) / sizeof(FloatFrame));
    }
    CHECK(hashes[0][0] == hashes[1][0]);
    CHECK(hashes[0][1] == hashes[1][1]);
  }
}

void TestResonatorBank() {
  test_checker.Start("TestResonatorBank");
  static ModalResonator resonator;
  const size_t kLength = 48000;
  static float mono[kLength];
  static float sum[kLength];
  resonator.Init(48000.0f);
  resonator.set_frequency(220.0f / 48000.0f);
  bool finite = true;
  for (size_t block = 0; block < kLength / 12; ++block) {
    float in[12] = { 0.0f };
    float l[12];
    float r[12];
    if (!block) {
      in[0] = 1.0f;
    }
    resonator.Process(in, l, r, 12);
    for (size_t i = 0; i < 12; ++i) {
      sum[block * 12 + i] = l[i] + r[i];
      finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
    }
  }
  CHECK(finite);
  // The fundamental rings, not the fifth above it.
  CHECK(Goertzel(sum, 4800, 220.0f / 48000.0f) >
      100.0f * Goertzel(sum, 4800, 330.0f / 48000.0f));
  CHECK(Goertzel(sum, 4800, 220.0f / 48000.0f) >
      100.0f * Goertzel(sum, 4800, 165.0f / 48000.0f));
  float first = 0.0f;
  float last = 0.0f;
  for (size_t i = 0; i < 4800; ++i) {
    first += sum[i] * sum[i];
    last += sum[kLength - 4800 + i] * sum[kLength - 4800 + i];
  }
  CHECK(first > 1.0e-4f);
  CHECK(last < first * 0.01f);
  CHECK(last > 0.0f);

  // aux == out sums both halves.
  resonator.Init(48000.0f);
  resonator.set_frequency(220.0f / 48000.0f);
  for (size_t block = 0; block < 400; ++block) {
    float in[12] = { 0.0f };
    if (!block) {
      in[0] = 1.0f;
    }
    resonator.Process(in, &mono[block * 12], &mono[block * 12], 12);
  }
  float error = 0.0f;
  float peak = 0.0f;
  for (size_t i = 0; i < 4800; ++i) {
    error = std::max(error, fabsf(mono[i] - sum[i]));
    peak = std::max(peak, fabsf(sum[i]));
  }
  CHECK(peak > 0.01f);
  CHECK(error < peak * 1.0e-5f);

  // Modes at or above 0.49 of the sample rate are silent.
  resonator.Init(48000.0f);
  resonator.set_frequency(0.5f);
  float silent = 0.0f;
  for (size_t block = 0; block < 100; ++block) {
    float in[12] = { 1.0f };
    float l[12];
    float r[12];
    resonator.Process(in, l, r, 12);
    for (size_t i = 0; i < 12; ++i) {
      silent += fabsf(l[i]) + fabsf(r[i]);
    }
  }
  CHECK(silent == 0.0f);
}

float TunedFrequency(const RandomizedParameter& pitch) {
  FloatFrame frames[12];
  memset(frames, 0, sizeof(frames));
  processor.ProcessResonator(pitch, false, frames, BlockSize());
  return processor.resonator_.frequency_ * processor.sample_rate();
}

void TestResonatorPitch() {
  test_checker.Start("TestResonatorPitch");
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
  RandomizedParameter pitch;
  memset(&pitch, 0, sizeof(pitch));
  CHECK(fabsf(TunedFrequency(pitch) - 130.813f) < 0.1f);
  pitch.base = 12.0f;
  CHECK(fabsf(TunedFrequency(pitch) - 261.626f) < 0.2f);
  // CV with the attenurandomizer clockwise transposes it.
  pitch.base = 0.0f;
  pitch.has_cv = true;
  pitch.attenuverter = 0.5f;
  pitch.cv = 4.0f;
  CHECK(fabsf(TunedFrequency(pitch) - 146.832f) < 0.1f);
  // Counter-clockwise the CV is randomness, which it ignores.
  pitch.attenuverter = -0.5f;
  CHECK(fabsf(TunedFrequency(pitch) - 130.813f) < 0.1f);
  // Recent notes: the newest note sent to PITCH.
  for (uint32_t ms = 0; ms < 40; ++ms) {
    RememberNote(RANDOM_INTERVALS_RECENT_NOTES, true, 3.0f, 1.0f, ms);
  }
  for (uint32_t ms = 40; ms < 80; ++ms) {
    RememberNote(RANDOM_INTERVALS_RECENT_NOTES, true, 7.0f, 1.0f, ms);
  }
  CHECK(fabsf(TunedFrequency(pitch) - 195.998f) < 0.1f);
  pitch.attenuverter = 0.0f;
  CHECK(fabsf(TunedFrequency(pitch) - 130.813f) < 0.1f);
  RememberNote(RANDOM_INTERVALS_OFF, false, 0.0f, 0.0f, 100);
  pitch.attenuverter = -0.5f;
  CHECK(fabsf(TunedFrequency(pitch) - 130.813f) < 0.1f);
  pitch.has_cv = false;

  // Pitch scale snaps it, as it snaps the grains.
  pitch.base = 0.4f;
  CHECK(fabsf(TunedFrequency(pitch) - 130.813f) > 2.0f);
  processor.set_pitch_scale(PITCH_SCALE_CHROMATIC);
  CHECK(fabsf(TunedFrequency(pitch) - 130.813f) < 0.1f);
  pitch.base = 3.4f;
  processor.set_pitch_scale(PITCH_SCALE_MAJOR);
  CHECK(fabsf(TunedFrequency(pitch) - 164.814f) < 0.1f);
  processor.set_pitch_scale(PITCH_SCALE_OFF);

  // Out of range notes are held at four octaves.
  pitch.base = 90.0f;
  CHECK(fabsf(TunedFrequency(pitch) - 2093.0f) < 2.0f);
  pitch.base = -90.0f;
  CHECK(fabsf(TunedFrequency(pitch) - 8.176f) < 0.01f);

  // Every quality tunes at its own rate.
  for (int m = 0; m < 4; ++m) {
    Init(kMedia[m], false, RESONATOR_AFTER_GRAINS);
    pitch.base = 0.0f;
    CHECK(fabsf(TunedFrequency(pitch) - 130.813f) < 0.1f);
  }
}

// Loud noise and strikes on high, sharp modes stay below 0 dBFS.
void TestResonatorLevel() {
  test_checker.Start("TestResonatorLevel");
  for (int m = 0; m < 4; ++m) {
    for (int note = 0; note < 3; ++note) {
      Init(kMedia[m], false, RESONATOR_AFTER_GRAINS);
      RandomizedParameter pitch;
      memset(&pitch, 0, sizeof(pitch));
      pitch.base = float(note) * 12.0f;
      const size_t size = BlockSize();
      float peak = 0.0f;
      float energy = 0.0f;
      bool finite = true;
      const int blocks = 4000;
      for (int block = 0; block < blocks; ++block) {
        FloatFrame frames[12];
        FloatFrame before[12];
        for (size_t i = 0; i < size; ++i) {
          frames[i].l = 0.9f * Noise();
          frames[i].r = 0.9f * Noise();
          before[i] = frames[i];
        }
        processor.ProcessResonator(pitch, block % 200 == 0, frames, size);
        for (size_t i = 0; i < size; ++i) {
          const float l = frames[i].l - before[i].l;
          const float r = frames[i].r - before[i].r;
          finite = finite && std::isfinite(l) && std::isfinite(r);
          if (block >= 200) {
            peak = std::max(peak, std::max(fabsf(l), fabsf(r)));
          }
          energy += l * l + r * r;
        }
      }
      const float rms = sqrtf(energy / float(2 * blocks * size));
      CHECK(finite);
      CHECK(peak < 0.8f);
      CHECK(rms > 0.02f);
    }
  }
}

float ResonatorEnergy() {
  float energy = 0.0f;
  for (int b = 0; b < kNumResonatorModes / 8; ++b) {
    for (int i = 0; i < plaits::kModeBatchSize; ++i) {
      energy += fabsf(processor.resonator_.odd_[b].state_1_[i]);
      energy += fabsf(processor.resonator_.even_[b].state_1_[i]);
    }
  }
  return energy;
}

// A steady sine on a mode, low and high: the limiter holds its ceiling once
// settled; a sudden high mode overshoots it for a few milliseconds.
void TestResonatorTonalLevel() {
  test_checker.Start("TestResonatorTonalLevel");
  const float stiffness = stmlib::Interpolate(
      rings::lut_stiffness, 0.25f, 256.0f);
  for (int m = 0; m < 4; ++m) {
    for (int note = 0; note < 2; ++note) {
      for (int mode = 0; mode < 2; ++mode) {
        Init(kMedia[m], false, RESONATOR_AFTER_GRAINS);
        RandomizedParameter pitch;
        memset(&pitch, 0, sizeof(pitch));
        pitch.base = note ? 48.0f : 0.0f;
        const size_t size = BlockSize();
        FloatFrame frames[12];
        memset(frames, 0, sizeof(frames));
        processor.ProcessResonator(pitch, false, frames, size);
        const float f0 = processor.resonator_.frequency_;
        const float f = mode ? 2.0f * f0 * (1.0f + stiffness) : f0;
        float onset = 0.0f;
        float peak = 0.0f;
        bool finite = true;
        for (int block = 0; block < 4000; ++block) {
          for (size_t i = 0; i < size; ++i) {
            const float t = float(block * size + i);
            frames[i].l = frames[i].r = 0.9f * sinf(2.0f * float(M_PI) * f * t);
          }
          FloatFrame before[12];
          std::copy(frames, frames + size, before);
          processor.ProcessResonator(pitch, false, frames, size);
          for (size_t i = 0; i < size; ++i) {
            const float l = frames[i].l - before[i].l;
            const float r = frames[i].r - before[i].r;
            finite = finite && std::isfinite(l) && std::isfinite(r);
            const float level = std::max(fabsf(l), fabsf(r));
            if (block < 400) {
              onset = std::max(onset, level);
            } else {
              peak = std::max(peak, level);
            }
          }
        }
        CHECK(finite);
        CHECK(onset < 1.0f);
        CHECK(peak < 0.65f);
        CHECK(peak > 0.1f);
      }
    }
  }
}

// Before the buffer, silent input and no hits: the resonator adds exactly
// nothing, so each option plays as it does without the resonator.
void TestResonatorOptionsMatch() {
  test_checker.Start("TestResonatorOptionsMatch");
  static float result[2][8000 * 24];
  static uint8_t recording[3][262144];
  for (int delay = 0; delay < 2; ++delay) {
    for (int c = 0; c < 8; ++c) {
      for (int pass = 0; pass < 2; ++pass) {
        Init(Processor::MEDIA_DIGITAL, false, RESONATOR_OFF);
        mix.dry_wet = 0.7f;
        mix.feedback = 0.8f;
        mix.reverb_amount = 0.5f;
        parameters.size.base = delay ? 1.0f : 0.5f;
        parameters.time.attenuverter = 0.5f;
        for (int block = 0; block < 8000; ++block) {
          FillSine(block, 0.3f);
          Process();
        }
        processor.set_time_scan(c == 1 || c == 7);
        processor.set_gentle_mix(c == 2 || c == 7);
        processor.set_reverb_on_grains(c == 3 || c == 7);
        processor.set_unity_feedback(c == 4 || c == 7);
        processor.set_delay_crossfade(c == 5 || c == 7);
        processor.set_evolving_feedback(c == 6 || c == 7);
        processor.set_diffusion(c == 7 ? 0.5f : 0.0f);
        std::fill(freeze, freeze + 12, true);
        if (pass) {
          processor.set_resonator(RESONATOR_BEFORE_BUFFER);
        }
        memset(input, 0, sizeof(input));
        for (int block = 0; block < 8000; ++block) {
          density.base = block & 1024 ? 0.3f : 0.7f;
          // Frozen, then recording silence: the feedback loop too.
          if (block == 1000) {
            std::fill(freeze, freeze + 12, false);
          }
          Process();
          for (size_t i = 0; i < 12; ++i) {
            result[pass][(block * 12 + i) * 2] = output[i].l;
            result[pass][(block * 12 + i) * 2 + 1] = output[i].r;
          }
        }
        CHECK(processor.delay_active() == bool(delay));
        // The recording, feedback included, matches too.
        if (pass) {
          CHECK(!memcmp(recording, banks, sizeof(banks)));
        } else {
          memcpy(recording, banks, sizeof(banks));
        }
      }
      bool same = true;
      float level = 0.0f;
      for (int i = 0; i < 8000 * 24; ++i) {
        same = same && result[0][i] == result[1][i];
        level += fabsf(result[0][i]);
      }
      CHECK(same);
      CHECK(level > 0.0f);
    }
  }
}

// Spectral leaves no old tail for when the grains return.
void TestResonatorSpectralReset() {
  test_checker.Start("TestResonatorSpectralReset");
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
  density.base = 0.5f;
  processor.set_attack_trigger(true);
  Process();
  processor.set_attack_trigger(false);
  for (int block = 0; block < 100; ++block) {
    Process();
  }
  CHECK(ResonatorEnergy() > 0.0f);
  processor.set_spectral(true);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  for (int block = 0; block < 50; ++block) {
    processor.Buffer();
    Process();
  }
  processor.set_spectral(false);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  Process();
  CHECK(ResonatorEnergy() == 0.0f);
}

// After the grains: the recording and the dry stay as they were.
void TestResonatorAfterGrains() {
  test_checker.Start("TestResonatorAfterGrains");
  for (int m = 0; m < 4; ++m) {
    uint64_t off[2];
    uint64_t on[2];
    RunSine(kMedia[m], RESONATOR_OFF, 0.0f, &off[0], &off[1]);
    RunSine(kMedia[m], RESONATOR_AFTER_GRAINS, 0.0f, &on[0], &on[1]);
    CHECK(processor.options_ == Processor::OPTION_FLAG_RESONATOR_GRAINS);
    CHECK(on[0] == off[0]);
    CHECK(on[1] == off[1]);
    RunSine(kMedia[m], RESONATOR_OFF, 1.0f, &off[0], &off[1]);
    RunSine(kMedia[m], RESONATOR_AFTER_GRAINS, 1.0f, &on[0], &on[1]);
    CHECK(on[0] != off[0]);
    CHECK(on[1] == off[1]);
  }
}

// Before the buffer: the resonance is recorded; the dry stays dry.
void TestResonatorBeforeBuffer() {
  test_checker.Start("TestResonatorBeforeBuffer");
  for (int m = 0; m < 4; ++m) {
    uint64_t off[2];
    uint64_t on[2];
    RunSine(kMedia[m], RESONATOR_OFF, 0.0f, &off[0], &off[1]);
    RunSine(kMedia[m], RESONATOR_BEFORE_BUFFER, 0.0f, &on[0], &on[1]);
    CHECK(processor.options_ == Processor::OPTION_FLAG_RESONATOR_INPUT);
    CHECK(on[0] == off[0]);
    CHECK(on[1] != off[1]);
    RunSine(kMedia[m], RESONATOR_BEFORE_BUFFER, 1.0f, &on[0], &on[1]);
    RunSine(kMedia[m], RESONATOR_OFF, 1.0f, &off[0], &off[1]);
    CHECK(on[0] != off[0]);
  }
}

// Turning it off restores the stock block, bit for bit.
void TestResonatorOff() {
  test_checker.Start("TestResonatorOff");
  uint64_t hashes[2];
  for (int pass = 0; pass < 2; ++pass) {
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_OFF);
    if (pass) {
      processor.set_resonator(RESONATOR_BEFORE_BUFFER);
      processor.set_resonator(RESONATOR_AFTER_GRAINS);
      processor.set_resonator(RESONATOR_OFF);
    }
    CHECK(processor.options_ == 0);
    mix.dry_wet = 0.7f;
    uint64_t hash = 14695981039346656037ull;
    for (int block = 0; block < 1000; ++block) {
      FillSine(block, 0.3f);
      Process();
      hash = Hash(hash, output, 12);
    }
    hashes[pass] = hash;
  }
  CHECK(hashes[0] == hashes[1]);
  // Spectral has no resonator.
  for (int placement = 0; placement < 3; ++placement) {
    Init(Processor::MEDIA_DIGITAL, false, placement);
    processor.set_spectral(true);
    processor.Configure(Processor::MEDIA_DIGITAL, false);
    mix.dry_wet = 1.0f;
    uint64_t hash = 14695981039346656037ull;
    for (int block = 0; block < 1000; ++block) {
      FillSine(block, 0.3f);
      processor.Buffer();
      Process();
      hash = Hash(hash, output, 12);
    }
    hashes[placement ? 1 : 0] = hash;
    if (placement) {
      CHECK(hashes[1] == hashes[0]);
    }
  }
}

// Input hits strike it, in both placements; silence leaves it silent.
void TestResonatorStrike() {
  test_checker.Start("TestResonatorStrike");
  for (int placement = 1; placement < 3; ++placement) {
    for (int source = 0; source < 3; ++source) {
      Init(Processor::MEDIA_DIGITAL, false, placement);
      mix.dry_wet = 1.0f;
      mix.reverb_amount = 0.0f;
      // Grains play the struck recording before the buffer, not after.
      density.base = placement == RESONATOR_BEFORE_BUFFER ? 0.9f : 0.5f;
      float before = 0.0f;
      float after = 0.0f;
      for (int block = 0; block < 2000; ++block) {
        if (block == 1000) {
          if (source == 1) {
            processor.set_attack_trigger(true);
          } else if (source == 2) {
            processor.SetAttackOnset(true);
          }
        }
        Process();
        processor.set_attack_trigger(false);
        for (size_t i = 0; i < 12; ++i) {
          const float level = fabsf(output[i].l) + fabsf(output[i].r);
          if (block < 1000) {
            before += level;
          } else {
            after += level;
          }
        }
      }
      CHECK(before == 0.0f);
      if (source) {
        CHECK(after > 0.01f);
      } else {
        CHECK(after == 0.0f);
      }
    }
  }
}

bool Same(const float* a, const float* b, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

// The resonance alone for 1 s after one strike, at 48 kHz: its peak, and
// the second half-second's energy over the first's.
void StrikeResponse(const ResonatorControls& c, float note,
    float* peak, float* decay) {
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
  processor.set_resonator_controls(c);
  RandomizedParameter pitch;
  memset(&pitch, 0, sizeof(pitch));
  pitch.base = note;
  *peak = 0.0f;
  float energy[2] = { 0.0f, 0.0f };
  // Silence first, until the limiter has let go, as in play.
  for (int block = 0; block < 32000; ++block) {
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonator(pitch, false, frames, 12);
  }
  for (int block = 0; block < 4000; ++block) {
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonator(pitch, !block, frames, 12);
    for (size_t i = 0; i < 12; ++i) {
      *peak = std::max(*peak, std::max(fabsf(frames[i].l),
          fabsf(frames[i].r)));
      energy[block / 2000] += frames[i].l * frames[i].l +
          frames[i].r * frames[i].r;
    }
  }
  *decay = energy[1] / energy[0];
}

ResonatorControls Controls(int material, int ring) {
  ResonatorControls c;
  c.structure = kResonatorMaterials[material].structure;
  c.brightness = kResonatorMaterials[material].brightness;
  c.damping = kResonatorRingTimes[ring];
  c.position = kResonatorPosition;
  c.amount = 1.0f;
  c.feedback = 0.0f;
  c.hold_feedback = false;
  c.modes = kResonatorMaterials[material].modes;
  return c;
}

// Each Ring time rings longer than the last; the Materials sound apart, at
// levels close to String's; Amount scales the resonance exactly.
void TestResonatorControls() {
  test_checker.Start("TestResonatorControls");
  for (int material = 0; material < kNumResonatorMaterials; ++material) {
    float previous = 0.0f;
    for (int ring = 0; ring < kNumResonatorRingTimes; ++ring) {
      float peak;
      float decay;
      StrikeResponse(Controls(material, ring), 0.0f, &peak, &decay);
      CHECK(decay > previous * 1.2f);
      CHECK(peak > 0.03f && peak < 0.5f);
      previous = decay;
    }
  }
  uint64_t hashes[kNumResonatorMaterials];
  float levels[kNumResonatorMaterials];
  for (int material = 0; material < kNumResonatorMaterials; ++material) {
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
    processor.set_resonator_controls(Controls(material, 1));
    RandomizedParameter pitch;
    memset(&pitch, 0, sizeof(pitch));
    uint64_t hash = 14695981039346656037ull;
    float energy = 0.0f;
    for (int block = 0; block < 4000; ++block) {
      FloatFrame frames[12];
      for (size_t i = 0; i < 12; ++i) {
        frames[i].l = 0.3f * Noise();
        frames[i].r = 0.3f * Noise();
      }
      FloatFrame before[12];
      std::copy(frames, frames + 12, before);
      processor.ProcessResonator(pitch, false, frames, 12);
      for (size_t i = 0; i < 12; ++i) {
        const float l = frames[i].l - before[i].l;
        const float r = frames[i].r - before[i].r;
        energy += l * l + r * r;
      }
      hash = Hash(hash, frames, 12);
    }
    hashes[material] = hash;
    levels[material] = sqrtf(energy / (4000.0f * 24.0f));
    for (int other = 0; other < material; ++other) {
      CHECK(hashes[other] != hash);
    }
  }
  // On noise at C3, each Material is within 4 dB of String.
  for (int material = 0; material < kNumResonatorMaterials; ++material) {
    CHECK(levels[material] > levels[1] * 0.63f);
    CHECK(levels[material] < levels[1] * 1.58f);
  }
  // Amount: the limited resonance times the amount, bit for bit.
  float resonance[3][120];
  const float amounts[3] = { 0.0f, 1.0f, 2.0f };
  for (int a = 0; a < 3; ++a) {
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
    ResonatorControls c = Controls(1, 1);
    c.amount = amounts[a];
    processor.set_resonator_controls(c);
    RandomizedParameter pitch;
    memset(&pitch, 0, sizeof(pitch));
    for (int block = 0; block < 10; ++block) {
      FloatFrame frames[12];
      memset(frames, 0, sizeof(frames));
      processor.ProcessResonator(pitch, !block, frames, 12);
      for (size_t i = 0; i < 12; ++i) {
        resonance[a][block * 12 + i] = frames[i].l;
      }
    }
  }
  bool exact = true;
  float level = 0.0f;
  for (int i = 0; i < 120; ++i) {
    exact = exact && resonance[0][i] == 0.0f &&
        resonance[2][i] == 2.0f * resonance[1][i];
    level += fabsf(resonance[1][i]);
  }
  CHECK(exact);
  CHECK(level > 0.0f);
  // The controls outlive a rate change, which restarts the resonator.
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
  processor.set_resonator_controls(Controls(3, 0));
  Process();
  processor.Configure(Processor::MEDIA_DIRTY_TAPE, false);
  Process();
  CHECK(processor.resonator_.structure_ == kResonatorMaterials[3].structure);
  CHECK(processor.resonator_.damping_ == kResonatorRingTimes[0]);
}

// From silence, a strike (SEED or a hit) is near -8 dBFS from C2 to C5.
void TestResonatorStrikeLevel() {
  test_checker.Start("TestResonatorStrikeLevel");
  for (int note = -12; note <= 24; note += 12) {
    float peak;
    float decay;
    StrikeResponse(Controls(1, 1), float(note), &peak, &decay);
    CHECK(peak > 0.3f && peak < 0.5f);
  }
}

// The edge policy: one gate unlatched, the jack's edges latched.
void TestResonatorSeedEdges() {
  test_checker.Start("TestResonatorSeedEdges");
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
  GateFlags low[12];
  GateFlags rising[12];
  GateFlags high[12];
  std::fill(low, low + 12, GATE_FLAG_LOW);
  std::fill(high, high + 12, GATE_FLAG_HIGH);
  std::fill(rising, rising + 5, GATE_FLAG_LOW);
  std::fill(rising + 5, rising + 12, GATE_FLAG_HIGH);
  rising[5] = GateFlags(GATE_FLAG_RISING | GATE_FLAG_HIGH);
  // A held button starts nothing when it turns on.
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_ON, NULL, 12));
  // Button: each press, not the hold.
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, NULL, 12));
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_ON, NULL, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_ON, NULL, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, NULL, 12));
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_ON, NULL, 12));
  // Jack: each rising edge; merged with a held button, none.
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, low, 12));
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_OFF, rising, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, high, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, low, 12));
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_ON, low, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_ON, rising, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, high, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_ON, high, 12));
  // Latched: the jack's edges only; unlatching is no press.
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_LOCKED, NULL, 12));
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_LOCKED, rising, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_LOCKED, high, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_ON, NULL, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, NULL, 12));
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_ON, NULL, 12));
  // Sync and Add strike at the jack's edges; Ignore has no jack.
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_SYNC, rising, 12));
  CHECK(processor.SeedStrike(SEED_BUTTON_STATE_ADD, rising, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_ADD, high, 12));
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_IGNORE, NULL, 12));
  // A held button and a jack held high, when it is turned on: no strike.
  processor.set_resonator(RESONATOR_BEFORE_BUFFER);
  CHECK(!processor.SeedStrike(SEED_BUTTON_STATE_OFF, high, 12));
}

// Through the block: delay mode keeps SEED for its clock; a hit strikes as
// SEED does, once per block; frozen, a strike before the buffer is not heard.
void TestResonatorSeedBlock() {
  test_checker.Start("TestResonatorSeedBlock");
  for (int delay = 0; delay < 2; ++delay) {
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
    density.base = 0.5f;
    parameters.size.base = delay ? 1.0f : 0.5f;
    for (int block = 0; block < 8000; ++block) {
      Process();
    }
    CHECK(processor.delay_active() == bool(delay));
    CHECK(ResonatorEnergy() == 0.0f);
    processor.Process(SEED_BUTTON_STATE_OFF, NULL, freeze, density,
        parameters, mix, input, output, 12);
    processor.Process(SEED_BUTTON_STATE_ON, NULL, freeze, density,
        parameters, mix, input, output, 12);
    CHECK((ResonatorEnergy() > 0.0f) == !delay);
  }
  // SEED alone, SEED with a hit, a hit alone: the same strike.
  float outputs[3][24000];
  for (int hit = 0; hit < 3; ++hit) {
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
    density.base = 0.5f;
    mix.dry_wet = 1.0f;
    for (int block = 0; block < 1000; ++block) {
      const SeedButtonState state = block == 500 && hit != 2
          ? SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF;
      processor.set_attack_trigger(hit && block == 500);
      processor.Process(state, NULL, freeze, density, parameters, mix, input,
          output, 12);
      for (size_t i = 0; i < 12; ++i) {
        outputs[hit][(block * 12 + i) * 2] = output[i].l;
        outputs[hit][(block * 12 + i) * 2 + 1] = output[i].r;
      }
    }
  }
  CHECK(Same(outputs[0], outputs[1], 24000));
  CHECK(Same(outputs[0], outputs[2], 24000));
  float level = 0.0f;
  for (int i = 0; i < 24000; ++i) {
    level = std::max(level, fabsf(outputs[0][i]));
  }
  CHECK(level > 0.01f);
  // Frozen, SEED pressed: the same output with the resonator before the
  // buffer as without it, though it rings.
  for (int on = 0; on < 2; ++on) {
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_OFF);
    mix.dry_wet = 1.0f;
    density.base = 0.9f;
    for (int block = 0; block < 1000; ++block) {
      FillSine(block, 0.3f);
      Process();
    }
    std::fill(freeze, freeze + 12, true);
    memset(input, 0, sizeof(input));
    processor.set_resonator(on ? RESONATOR_BEFORE_BUFFER : RESONATOR_OFF);
    for (int block = 0; block < 1000; ++block) {
      const SeedButtonState state = block == 500
          ? SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF;
      processor.Process(state, NULL, freeze, density, parameters, mix, input,
          output, 12);
      for (size_t i = 0; i < 12; ++i) {
        outputs[on][(block * 12 + i) * 2] = output[i].l;
        outputs[on][(block * 12 + i) * 2 + 1] = output[i].r;
      }
    }
    CHECK(!on || ResonatorEnergy() > 0.0f);
  }
  CHECK(Same(outputs[0], outputs[1], 24000));
}

// Every quality, mono and stereo, in delay mode too: finite, and no louder
// than the stock block by more than the resonance it adds.
void TestResonatorQualities() {
  test_checker.Start("TestResonatorQualities");
  for (int m = 0; m < 4; ++m) {
    for (int mono = 0; mono < 2; ++mono) {
      for (int delay = 0; delay < 2; ++delay) {
        float peaks[3];
        for (int placement = 0; placement < 3; ++placement) {
          Init(kMedia[m], mono, placement);
          mix.dry_wet = 1.0f;
          mix.feedback = 0.5f;
          parameters.size.base = delay ? 1.0f : 0.5f;
          bool finite = true;
          float peak = 0.0f;
          for (int block = 0; block < 8000; ++block) {
            FillSine(block, 0.9f);
            processor.set_attack_trigger(block % 100 == 0);
            Process();
            for (size_t i = 0; i < BlockSize(); ++i) {
              finite = finite && std::isfinite(output[i].l) &&
                  std::isfinite(output[i].r);
              peak = std::max(peak, std::max(fabsf(output[i].l),
                  fabsf(output[i].r)));
            }
          }
          CHECK(finite);
          CHECK(processor.delay_active() == bool(delay));
          peaks[placement] = peak;
        }
        CHECK(peaks[0] > 0.1f);
        CHECK(peaks[1] < peaks[0] + 1.0f);
        // Before the buffer, the delay replays the resonance with feedback.
        CHECK(peaks[2] < peaks[0] + (delay ? 1.5f : 1.0f));
      }
    }
  }
}

// Both states of every mode, squared.
float StateEnergy() {
  float energy = 0.0f;
  for (int b = 0; b < kNumResonatorModes / 8; ++b) {
    for (int i = 0; i < plaits::kModeBatchSize; ++i) {
      const ModalResonator& r = processor.resonator_;
      energy += r.odd_[b].state_1_[i] * r.odd_[b].state_1_[i] +
          r.odd_[b].state_2_[i] * r.odd_[b].state_2_[i] +
          r.even_[b].state_1_[i] * r.even_[b].state_1_[i] +
          r.even_[b].state_2_[i] * r.even_[b].state_2_[i];
    }
  }
  return energy;
}

// Drizzlet's objects: a strike alone bends the pitch, by the table's amount
// and back; loud input never does. Pairs sound left and right. At every
// rate, pitch extreme and the CV's longest ring, the bank stays finite and
// dies away.
void TestResonatorMaterials() {
  test_checker.Start("TestResonatorMaterials");
  RandomizedParameter pitch;
  memset(&pitch, 0, sizeof(pitch));
  for (int material = 0; material < kNumResonatorMaterials; ++material) {
    const ResonatorModes* modes = kResonatorMaterials[material].modes;
    CHECK((material >= 4) == (modes != NULL));
    if (!modes) {
      continue;
    }
    for (int i = 0; i < kNumResonatorModes; ++i) {
      CHECK(modes->ratio[i] >= 0.99f);
      CHECK(modes->decay[i] > 0.0f && modes->decay[i] <= 1.0f);
      CHECK(modes->gain[i] >= 0.0f);
    }
    CHECK(fabsf(modes->glide) <= 0.1f);
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
    processor.set_resonator_controls(Controls(material, 1));
    float most = 0.0f;
    for (int block = 0; block < 4000; ++block) {
      FloatFrame frames[12];
      for (size_t i = 0; i < 12; ++i) {
        frames[i].l = 0.9f * Noise();
        frames[i].r = 0.9f * Noise();
      }
      processor.ProcessResonator(pitch, false, frames, 12);
      most = std::max(most, fabsf(processor.resonator_.glide_));
    }
    CHECK(most == 0.0f);
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonator(pitch, true, frames, 12);
    CHECK(processor.resonator_.glide_ != 0.0f || modes->glide == 0.0f);
    CHECK(fabsf(processor.resonator_.glide_) <= fabsf(modes->glide));
    float energy[3] = { 0.0f, 0.0f, 0.0f };
    for (int block = 0; block < 4000; ++block) {
      memset(frames, 0, sizeof(frames));
      processor.ProcessResonator(pitch, false, frames, 12);
      for (size_t i = 0; i < 12; ++i) {
        energy[0] += frames[i].l * frames[i].l;
        energy[1] += frames[i].r * frames[i].r;
        energy[2] += (frames[i].l - frames[i].r) * (frames[i].l - frames[i].r);
      }
    }
    // Equal pairs give a centred image; split pairs differ left and right.
    bool split = false;
    for (int i = 0; i < kNumResonatorModes; i += 2) {
      split = split || modes->ratio[i] != modes->ratio[i + 1];
    }
    CHECK((energy[2] > 0.0f) == split);
    CHECK(fabsf(processor.resonator_.glide_) < 0.1f * fabsf(modes->glide) ||
        modes->glide == 0.0f);
    CHECK(energy[0] > 0.0f && energy[1] > 0.0f);
    const Processor::Media media[3] = {
      Processor::MEDIA_DIGITAL,
      Processor::MEDIA_VINTAGE_DIGITAL,
      Processor::MEDIA_DIRTY_TAPE
    };
    for (int m = 0; m < 3; ++m) {
      for (int note = -48; note <= 48; note += 96) {
        Init(media[m], false, RESONATOR_AFTER_GRAINS);
        ResonatorControls c = Controls(material, 3);
        c.damping = 0.9995f;
        processor.set_resonator_controls(c);
        pitch.base = float(note);
        bool finite = true;
        float peak = 0.0f;
        for (int block = 0; block < 8000; ++block) {
          FloatFrame f[12];
          for (size_t i = 0; i < BlockSize(); ++i) {
            f[i].l = block < 4000 ? 0.9f * Noise() : 0.0f;
            f[i].r = block < 4000 ? 0.9f * Noise() : 0.0f;
          }
          processor.ProcessResonator(pitch, block < 4000, f, BlockSize());
          for (size_t i = 0; i < BlockSize(); ++i) {
            finite = finite && std::isfinite(f[i].l) && std::isfinite(f[i].r);
            peak = std::max(peak, std::max(fabsf(f[i].l), fabsf(f[i].r)));
          }
          CHECK(fabsf(processor.resonator_.glide_) <= fabsf(modes->glide));
        }
        CHECK(finite);
        CHECK(peak < 3.0f);
        const float late = StateEnergy();
        for (int block = 0; block < 4000; ++block) {
          FloatFrame f[12];
          memset(f, 0, sizeof(f));
          processor.ProcessResonator(pitch, false, f, BlockSize());
        }
        CHECK(StateEnergy() < late);
      }
    }
    pitch.base = 0.0f;
  }
  // A bend never outlives its object, through another or through Rings'.
  for (int via = 1; via < 5; via += 3) {
    Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
    processor.set_resonator_controls(Controls(5, 1));
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonator(pitch, true, frames, 12);
    CHECK(processor.resonator_.glide_ != 0.0f);
    processor.set_resonator_controls(Controls(via, 1));
    processor.ProcessResonator(pitch, false, frames, 12);
    CHECK(processor.resonator_.glide_ == 0.0f);
    processor.set_resonator_controls(Controls(5, 1));
    processor.ProcessResonator(pitch, false, frames, 12);
    CHECK(processor.resonator_.glide_ == 0.0f);
  }
  // Nor does a ring in the batches an object leaves silent.
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_AFTER_GRAINS);
  processor.set_resonator_controls(Controls(5, 1));
  for (int block = 0; block < 100; ++block) {
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonator(pitch, !block, frames, 12);
  }
  const float rung = StateEnergy();
  processor.set_resonator_controls(Controls(4, 1));
  FloatFrame frames[12];
  memset(frames, 0, sizeof(frames));
  processor.ProcessResonator(pitch, false, frames, 12);
  const ModalResonator& r = processor.resonator_;
  float left = 0.0f;
  for (int b = 1; b < kNumResonatorModes / 8; ++b) {
    for (int i = 0; i < plaits::kModeBatchSize; ++i) {
      left += fabsf(r.odd_[b].state_1_[i]) + fabsf(r.odd_[b].state_2_[i]) +
          fabsf(r.even_[b].state_1_[i]) + fabsf(r.even_[b].state_2_[i]);
    }
  }
  CHECK(rung > 0.0f && left == 0.0f);
}

// The resonance's peak in the 0.5 s after a strike at C3.
float StrikePeak() {
  RandomizedParameter pitch;
  memset(&pitch, 0, sizeof(pitch));
  float peak = 0.0f;
  for (int block = 0; block < 2000; ++block) {
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonator(pitch, !block, frames, BlockSize());
    for (size_t i = 0; i < BlockSize(); ++i) {
      peak = std::max(peak, std::max(fabsf(frames[i].l), fabsf(frames[i].r)));
    }
  }
  return peak;
}

// Ten seconds of silence: the tail and the limiter let go.
void Silence() {
  RandomizedParameter pitch;
  memset(&pitch, 0, sizeof(pitch));
  for (int block = 0; block < 40000; ++block) {
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonator(pitch, false, frames, BlockSize());
  }
}

bool WithinOneDecibel(float a, float b) {
  return a > b * 0.891f && a < b * 1.122f;
}

// After power-on, a rate change, spectral or a placement change, the first
// strike is as loud as one after a long silence.
void TestResonatorLimiterAtRest() {
  test_checker.Start("TestResonatorLimiterAtRest");
  for (int m = 0; m < 4; ++m) {
    Init(kMedia[m], false, RESONATOR_AFTER_GRAINS);
    const float first = StrikePeak();
    Silence();
    const float rested = StrikePeak();
    CHECK(rested > 0.1f);
    CHECK(WithinOneDecibel(first, rested));
  }
  Init(Processor::MEDIA_DIRTY_TAPE, false, RESONATOR_AFTER_GRAINS);
  Silence();
  const float rate = processor.sample_rate();
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  CHECK(processor.sample_rate() != rate);
  const float first = StrikePeak();
  Silence();
  const float rested = StrikePeak();
  CHECK(WithinOneDecibel(first, rested));
  Silence();
  processor.set_spectral(true);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  for (int block = 0; block < 10; ++block) {
    processor.Buffer();
    Process();
  }
  processor.set_spectral(false);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  CHECK(WithinOneDecibel(StrikePeak(), rested));
  Silence();
  processor.set_resonator(RESONATOR_BEFORE_BUFFER);
  CHECK(WithinOneDecibel(StrikePeak(), rested));
}

// An object with every mode silent.
ResonatorModes kSilentModes;

// The bank's modes, rebuilt from Process's own rules for the bound test:
// frequency, Q and level, per side; levels are 0 past Nyquist.
struct Mode {
  float f;
  float q;
  float a;
};

void BankModes(const ResonatorControls& c, float frequency,
    Mode modes[2][kNumResonatorModes / 2]) {
  const float q = 500.0f * Interpolate(rings::lut_4_decades, c.damping, 256.0f);
  CosineOscillator amplitudes;
  amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(c.position);
  if (c.modes) {
    for (int i = 0; i < kNumResonatorModes / 2; ++i) {
      const float pickup = amplitudes.Next() * 0.125f;
      for (int side = 0; side < 2; ++side) {
        const int e = 2 * i + side;
        Mode& m = modes[side][i];
        m.f = frequency * c.modes->ratio[e];
        m.a = m.f >= 0.49f ? 0.0f : pickup * c.modes->gain[e];
        m.f = std::min(m.f, 0.49f);
        m.q = 1.0f + m.f * q * c.modes->decay[e];
      }
    }
    return;
  }
  float stiffness = Interpolate(rings::lut_stiffness, c.structure, 256.0f);
  float stretch = 1.0f;
  float mode_q = q;
  float attenuation = 1.0f - c.structure;
  attenuation *= attenuation;
  attenuation *= attenuation;
  attenuation *= attenuation;
  const float b = c.brightness * (1.0f - 0.2f * attenuation);
  float q_loss = b * (2.0f - b) * 0.85f + 0.15f;
  const float rate = c.structure * (2.0f - c.structure) * 0.1f;
  for (int i = 0; i < kNumResonatorModes; ++i) {
    Mode& m = modes[i & 1][i >> 1];
    m.f = frequency * float(i + 1) * stretch;
    m.a = amplitudes.Next() * 0.125f;
    if (m.f >= 0.49f) {
      m.f = 0.49f;
      m.a = 0.0f;
    }
    m.q = 1.0f + m.f * mode_q;
    stretch += stiffness;
    stiffness *= stiffness < 0.0f ? 0.93f : 0.98f;
    q_loss += rate * (1.0f - q_loss);
    mode_q *= q_loss;
  }
}

// |H| of one side at w (radians per sample): TPT band-passes, as Plaits'
// ResonatorSvf, each u / (u^2 + u / Q + 1) with u = j tan(w / 2) / g.
double SideGain(const Mode* modes, double w) {
  double re = 0.0;
  double im = 0.0;
  const double t = tan(0.5 * w);
  for (int i = 0; i < kNumResonatorModes / 2; ++i) {
    if (modes[i].a == 0.0f) {
      continue;
    }
    const double g = OnePole::tan<FREQUENCY_FAST>(modes[i].f);
    const double u = t / g;
    // u j / (1 - u^2 + j u / Q)
    const double dr = 1.0 - u * u;
    const double di = u / modes[i].q;
    const double d = dr * dr + di * di;
    re += modes[i].a * (u * di) / d;
    im += modes[i].a * (u * dr) / d;
  }
  return sqrt(re * re + im * im);
}

// The largest |H| of either side, around every mode's peak and between.
double BankGain(Mode modes[2][kNumResonatorModes / 2]) {
  double sup = 0.0;
  for (int side = 0; side < 2; ++side) {
    for (int i = 0; i < kNumResonatorModes / 2; ++i) {
      const Mode& m = modes[side][i];
      if (m.a == 0.0f) {
        continue;
      }
      // The peak is at tan(w / 2) = g; look within four bandwidths of it.
      const double g = OnePole::tan<FREQUENCY_FAST>(m.f);
      for (int k = -64; k <= 64; ++k) {
        const double t = g * (1.0 + double(k) / (16.0 * m.q));
        if (t > 0.0) {
          sup = std::max(sup, SideGain(modes[side], 2.0 * atan(t)));
        }
      }
    }
    for (int k = 1; k < 2000; ++k) {
      const double w = M_PI * pow(1.0e-4, 1.0 - double(k) / 2000.0);
      sup = std::max(sup, SideGain(modes[side], w));
    }
  }
  return sup;
}

ResonatorBound BoundOf(const ResonatorControls& c) {
  ResonatorBound b;
  ModalResonator::Bound(c.structure, c.brightness, c.position, c.modes, &b);
  return b;
}

// The bank as the loop sees it, at a note and rate.
void SetUpBank(ModalResonator* bank, const ResonatorControls& c, float rate,
    int note) {
  bank->Init(rate);
  bank->set_structure(c.structure);
  bank->set_brightness(c.brightness);
  bank->set_damping(c.damping);
  bank->set_position(c.position);
  bank->set_modes(c.modes);
  bank->set_frequency(SemitonesToRatio(float(note) - 69.0f) * 440.0f / rate);
}

// The loop's bound is never under the bank's gain at any frequency: every
// Material, struck, Ring time and damping's ends, C0 to C8, three rates.
void TestResonatorLoopBound() {
  test_checker.Start("TestResonatorLoopBound");
  static ModalResonator bank;
  const float rates[3] = { 48000.0f, 32000.0f, 24000.0f };
  const float dampings[6] = {
    0.0f, kResonatorRingTimes[0], kResonatorRingTimes[1],
    kResonatorRingTimes[2], kResonatorRingTimes[3], 0.9995f
  };
  for (int r = 0; r < 3; ++r) {
    for (int m = 0; m < kNumResonatorMaterials; ++m) {
      for (int d = 0; d < 6; ++d) {
        for (int note = 12; note <= 108; note += 24) {
          ResonatorControls c = Controls(m, 1);
          c.damping = dampings[d];
          SetUpBank(&bank, c, rates[r], note);
          // Drum and Gong just struck: at the start of their glide.
          bank.Strike();
          bank.LoopGain(BoundOf(c), 12);
          Mode modes[2][kNumResonatorModes / 2];
          BankModes(c, bank.frequency_ * (1.0f + bank.glide_), modes);
          const double sup = BankGain(modes);
          CHECK(sup <= bank.peak_gain_);
          // Close to it where the modes stand apart.
          if (note == 60 && d >= 2 && d <= 4) {
            CHECK(sup >= bank.peak_gain_ * 0.5);
          }
        }
      }
    }
  }
}

// The bound's parts for every Material, and the cases it must not miss.
void TestResonatorBound() {
  test_checker.Start("TestResonatorBound");
  for (int m = 0; m < kNumResonatorMaterials; ++m) {
    const ResonatorControls c = Controls(m, 1);
    const ResonatorBound b = BoundOf(c);
    CHECK(b.spill > 0.1f && b.spill < 20.0f);
    CHECK(b.peak > 0.0f && b.peak_slope > 0.0f);
    CHECK(b.sum >= b.peak && b.sum_slope >= b.peak_slope);
    CHECK(b.lowest_ratio > 0.9f && b.lowest_ratio <= 1.0f);
    CHECK(b.largest_decay == 1.0f);
    CHECK(b.structure == c.structure && b.brightness == c.brightness);
    CHECK(b.position == c.position && b.modes == c.modes);
  }
  // Marimba's silent entries sit on its fundamental and are skipped.
  CHECK(BoundOf(Controls(4, 1)).spill < 1.0f);
  // Two modes an octave apart on one side: each adds sqrt(2) times its level
  // off the other's peak.
  static ResonatorModes octave;
  memset(&octave, 0, sizeof(octave));
  octave.ratio[0] = 1.0f;
  octave.ratio[2] = 2.0f;
  octave.gain[0] = octave.gain[2] = 1.0f;
  CosineOscillator pickup;
  pickup.Init<COSINE_OSCILLATOR_APPROXIMATE>(kResonatorPosition);
  const float first = fabsf(pickup.Next() * 0.125f);
  const float second = fabsf(pickup.Next() * 0.125f);
  ResonatorControls pair = Controls(4, 1);
  pair.modes = &octave;
  const float expected = std::max(first, second) * sqrtf(2.0f);
  CHECK(fabsf(BoundOf(pair).spill - expected) < 1.0e-6f);
  // Two modes of a side on one pitch: no spill bound.
  static ResonatorModes twins;
  twins = *kResonatorMaterials[4].modes;
  twins.gain[8] = 1.0f;
  ResonatorControls c = Controls(4, 1);
  c.modes = &twins;
  CHECK(BoundOf(c).spill >= kResonatorNoSpill);
}

// SysTick publishes the bound to the slot the audio is not reading; the
// loop only runs on the object the bound names.
void TestResonatorBoundPublished() {
  test_checker.Start("TestResonatorBoundPublished");
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_FEEDBACK);
  ResonatorControls c = Controls(1, 1);
  processor.set_resonator_controls(c);
  const uint8_t first = processor.resonator_bound_;
  const ResonatorBound& string = processor.resonator_bounds_[first];
  CHECK(string.modes == NULL && string.structure == c.structure);
  // Damping alone leaves it.
  c.damping = 0.8f;
  processor.set_resonator_controls(c);
  CHECK(processor.resonator_bound_ == first);
  processor.set_resonator_controls(Controls(4, 1));
  CHECK(processor.resonator_bound_ != first);
  CHECK(processor.resonator_bounds_[processor.resonator_bound_].modes ==
      kResonatorMaterials[4].modes);
  CHECK(processor.resonator_bounds_[first].modes == NULL);
  // Brightness and position also name the object.
  c = Controls(1, 1);
  processor.set_resonator_controls(c);
  const uint8_t slot = processor.resonator_bound_;
  c.brightness = 0.9f;
  processor.set_resonator_controls(c);
  CHECK(processor.resonator_bound_ != slot);
  c.position = 0.6f;
  processor.set_resonator_controls(c);
  CHECK(processor.resonator_bound_ == slot);
  CHECK(processor.resonator_bounds_[slot].position == 0.6f);
  // Controls the audio read before SysTick moved the bound on: no loop.
  static ModalResonator bank;
  SetUpBank(&bank, Controls(1, 1), 48000.0f, 48);
  const ResonatorBound other = BoundOf(Controls(4, 1));
  float in[12];
  for (size_t i = 0; i < 12; ++i) {
    in[i] = 0.3f * Noise();
  }
  CHECK(bank.LoopGain(other, 12) == 0.0f);
  CHECK(bank.LoopGain(BoundOf(Controls(1, 1)), 12) > 0.0f);
  (void) in;
}

// The loop's copy is the block's resonance over Amount and the bound; the
// bound falls no faster than the slowest mode decays.
void TestResonatorLoopRender() {
  test_checker.Start("TestResonatorLoopRender");
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_FEEDBACK);
  ResonatorControls c = Controls(1, 1);
  c.amount = 0.5f;
  processor.set_resonator_controls(c);
  RandomizedParameter pitch;
  memset(&pitch, 0, sizeof(pitch));
  // The grains' feedback filters at Bright digital, replayed.
  Svf high_pass[2];
  for (int channel = 0; channel < 2; ++channel) {
    high_pass[channel].Init();
    high_pass[channel].set_f_q<FREQUENCY_FAST>(40.0f / 48000.0f, 0.5f);
  }
  FloatFrame low_pass;
  low_pass = 0.0f;
  float previous = 0.0f;
  float before = 0.0f;
  for (int block = 0; block < 400; ++block) {
    // Damping drops halfway: the bound may only fall slowly.
    if (block == 200) {
      c.damping = 0.0f;
      processor.set_resonator_controls(c);
    }
    FloatFrame frames[12];
    for (size_t i = 0; i < 12; ++i) {
      frames[i].l = 0.2f * Noise();
      frames[i].r = 0.2f * Noise();
    }
    FloatFrame grains[12];
    memcpy(grains, frames, sizeof(frames));
    processor.ProcessResonatorLoop(pitch, !block, frames, 12);
    const float peak = processor.resonator_.peak_gain_;
    CHECK(peak > 0.0f);
    const float gain = 1.0f / (0.5f * peak);
    for (size_t i = 0; i < 12; ++i) {
      const FloatFrame resonance = (frames[i] - grains[i]) * gain;
      ONE_POLE(low_pass.l, high_pass[0].Process<FILTER_MODE_HIGH_PASS>(
          resonance.l), 0.999f);
      ONE_POLE(low_pass.r, high_pass[1].Process<FILTER_MODE_HIGH_PASS>(
          resonance.r), 0.999f);
      const FloatFrame loop = processor.resonator_loop_frames_[i];
      CHECK(fabsf(loop.l - low_pass.l) <=
          1.0e-5f * fabsf(low_pass.l) + 1.0e-9f);
      CHECK(fabsf(loop.r - low_pass.r) <=
          1.0e-5f * fabsf(low_pass.r) + 1.0e-9f);
    }
    // The modes left ringing decay about 4% a block at the new damping.
    if (block == 200) {
      before = previous;
      CHECK(peak > previous * 0.95f);
    }
    previous = peak;
  }
  // Ten seconds on, it has fallen to the new bound.
  for (int block = 0; block < 40000; ++block) {
    FloatFrame frames[12];
    memset(frames, 0, sizeof(frames));
    processor.ProcessResonatorLoop(pitch, false, frames, 12);
  }
  CHECK(processor.resonator_.peak_gain_ < before * 0.25f);
  // A silent bank sends nothing, nor do the filters, cleared at the rate.
  c.modes = &kSilentModes;
  processor.set_resonator_controls(c);
  FloatFrame frames[12];
  memset(frames, 0, sizeof(frames));
  processor.resonator_sample_rate_ = 0.0f;
  processor.ProcessResonatorLoop(pitch, false, frames, 12);
  CHECK(processor.resonator_.peak_gain_ == 0.0f);
  for (size_t i = 0; i < 12; ++i) {
    CHECK(processor.resonator_loop_frames_[i].l == 0.0f);
    CHECK(processor.resonator_loop_frames_[i].r == 0.0f);
  }
}

// |H| of a TPT high-pass at cutoff f (cycles per sample), and of a one-pole
// low-pass y += c (x - y), at w radians per sample.
double HighPassGain(float f, double w) {
  const double u = tan(0.5 * w) / OnePole::tan<FREQUENCY_FAST>(f);
  return u * u / sqrt((1.0 - u * u) * (1.0 - u * u) + 4.0 * u * u);
}

double LowPassGain(double c, double w) {
  const double p = 1.0 - c;
  return c / sqrt(1.0 - 2.0 * p * cos(w) + p * p);
}

// Through its filters the loop passes no more than the grains' own feedback
// does, at any frequency the recording keeps, in every quality.
void TestResonatorLoopFilters() {
  test_checker.Start("TestResonatorLoopFilters");
  const float low_pass[4] = { 0.999f, 0.9f, 0.9f, 0.75f };
  for (int m = 0; m < 4; ++m) {
    Init(kMedia[m], false, RESONATOR_FEEDBACK);
    const double sr = processor.sample_rate();
    const double ratio = processor.ratio();
    const double grains = sr / ratio;
    const double pole = 1.0 - low_pass[m];
    const double loop = 1.0 - (ratio == 2.0 ? sqrt(pole) : pole);
    for (int cutoff = 10; cutoff <= 40; cutoff += 30) {
      for (int k = 0; k <= 400; ++k) {
        const double f = 1.0 * pow(0.49 * grains, double(k) / 400.0);
        const double w = 2.0 * M_PI * f;
        const double dry = HighPassGain(cutoff / grains, w / grains) *
            LowPassGain(low_pass[m], w / grains);
        const double wet = HighPassGain(cutoff / sr, w / sr) *
            LowPassGain(loop, w / sr);
        CHECK(wet <= dry * 1.002);
      }
    }
  }
}

// The loop's copy goes through the grains' feedback filters at the
// resonator's rate, in every quality and at both cutoffs: replayed here.
// Only Sunny tape's grains run at half the rate; Scorched runs at 24 kHz.
void TestResonatorLoopChain() {
  test_checker.Start("TestResonatorLoopChain");
  const float low_pass[4] = { 0.999f, 0.9f, 1.0f - sqrtf(0.1f), 0.75f };
  RandomizedParameter pitch;
  memset(&pitch, 0, sizeof(pitch));
  for (int m = 0; m < 4; ++m) {
    for (int unity = 0; unity < 2; ++unity) {
      Init(kMedia[m], false, RESONATOR_FEEDBACK);
      processor.set_resonator_controls(Controls(1, 1));
      processor.SetFeedbackCutoff(unity);
      const float sr = processor.sample_rate();
      const size_t size = BlockSize();
      Svf high_pass[2];
      for (int channel = 0; channel < 2; ++channel) {
        high_pass[channel].Init();
        high_pass[channel].set_f_q<FREQUENCY_FAST>(
            (unity ? 10.0f : 40.0f) / sr, 0.5f);
      }
      FloatFrame lp;
      lp = 0.0f;
      for (int block = 0; block < 200; ++block) {
        FloatFrame frames[12];
        for (size_t i = 0; i < size; ++i) {
          frames[i].l = 0.2f * Noise();
          frames[i].r = 0.2f * Noise();
        }
        FloatFrame grains[12];
        memcpy(grains, frames, sizeof(frames));
        processor.ProcessResonatorLoop(pitch, !block, frames, size);
        const float gain = 1.0f / processor.resonator_.peak_gain_;
        for (size_t i = 0; i < size; ++i) {
          const FloatFrame resonance = (frames[i] - grains[i]) * gain;
          ONE_POLE(lp.l, high_pass[0].Process<FILTER_MODE_HIGH_PASS>(
              resonance.l), low_pass[m]);
          ONE_POLE(lp.r, high_pass[1].Process<FILTER_MODE_HIGH_PASS>(
              resonance.r), low_pass[m]);
          const FloatFrame loop = processor.resonator_loop_frames_[i];
          CHECK(fabsf(loop.l - lp.l) <= 1.0e-5f * fabsf(lp.l) + 1.0e-9f);
          CHECK(fabsf(loop.r - lp.r) <= 1.0e-5f * fabsf(lp.r) + 1.0e-9f);
        }
      }
    }
  }
}

// The last block's resonance goes into the recording at the grains' own
// loop gain, with the tapes' soft clip gain taken out; while FEEDBACK
// moves, at the lower end for the resonance and the higher for the clip.
void TestResonatorLoopGain() {
  test_checker.Start("TestResonatorLoopGain");
  const float soft_limit[4] = { 0.0f, 1.3f, 1.3f, 1.75f };
  for (int m = 0; m < 4; ++m) {
    for (int delay = 0; delay < 2; ++delay) {
      Init(kMedia[m], false, RESONATOR_FEEDBACK);
      const float sr = processor.sample_rate();
      const size_t size = BlockSize();
      FloatFrame in[12];
      FloatFrame recorded[12];
      for (size_t i = 0; i < size; ++i) {
        in[i].l = 0.1f * float(i);
        in[i].r = -0.05f * float(i);
        processor.resonator_loop_frames_[i].l = 0.5f;
        processor.resonator_loop_frames_[i].r = -0.25f;
      }
      // Nothing for an unset object, or at another rate.
      processor.resonator_sample_rate_ = sr;
      processor.RecordResonance(0.6f, delay, in, recorded, size);
      CHECK(!memcmp(in, recorded, sizeof(FloatFrame) * size));
      processor.set_resonator_controls(Controls(1, 1));
      processor.resonator_sample_rate_ = 0.0f;
      processor.RecordResonance(0.6f, delay, in, recorded, size);
      CHECK(!memcmp(in, recorded, sizeof(FloatFrame) * size));
      processor.resonator_sample_rate_ = sr;
      // FEEDBACK rising from 0.6 to 0.8, then steady at 0.8.
      for (int pass = 0; pass < 2; ++pass) {
        const float dry = processor.RecordResonance(
            0.8f, delay, in, recorded, size);
        CHECK(dry == 0.8f * kResonatorLoopDry);
        const float lower = pass ? 0.8f : 0.6f;
        float gain = kResonatorLoopShare * (delay ? 1.5f : 3.0f) *
            lower * lower;
        if (soft_limit[m]) {
          gain /= 1.0f + (soft_limit[m] - 1.0f) * dry;
        }
        for (size_t i = 0; i < size; ++i) {
          CHECK(fabsf(recorded[i].l - (in[i].l + 0.5f * gain)) < 1.0e-6f);
          CHECK(fabsf(recorded[i].r - (in[i].r - 0.25f * gain)) < 1.0e-6f);
        }
      }
      // Falling, the clip's gain is the higher end's.
      processor.RecordResonance(0.4f, delay, in, recorded, size);
      float gain = kResonatorLoopShare * (delay ? 1.5f : 3.0f) * 0.16f;
      if (soft_limit[m]) {
        gain /= 1.0f + (soft_limit[m] - 1.0f) * 0.8f * kResonatorLoopDry;
      }
      CHECK(fabsf(recorded[0].l - (in[0].l + 0.5f * gain)) < 1.0e-6f);
    }
  }
}

// Output and recording over 0.5 s of a sine.
void RunLoop(Processor::Media media, int placement, float feedback,
    uint64_t* out_hash, uint64_t* recording_hash) {
  Init(media, false, placement);
  processor.set_resonator_controls(Controls(1, 1));
  mix.dry_wet = 1.0f;
  mix.feedback = feedback;
  uint64_t hash = 14695981039346656037ull;
  for (int block = 0; block < 2000; ++block) {
    FillSine(block, 0.3f);
    Process();
    hash = Hash(hash, output, BlockSize());
  }
  *out_hash = hash;
  uint64_t recording = 14695981039346656037ull;
  const uint8_t* bytes = &banks[0][0];
  for (size_t i = 0; i < sizeof(banks); ++i) {
    recording = (recording ^ bytes[i]) * 1099511628211ull;
  }
  *recording_hash = recording;
}

// In the feedback: After the grains at FEEDBACK 0; with FEEDBACK, the
// grains keep half their loop and the resonance is recorded.
void TestResonatorFeedback() {
  test_checker.Start("TestResonatorFeedback");
  for (int m = 0; m < 4; ++m) {
    uint64_t after[2];
    uint64_t loop[2];
    RunLoop(kMedia[m], RESONATOR_AFTER_GRAINS, 0.0f, &after[0], &after[1]);
    RunLoop(kMedia[m], RESONATOR_FEEDBACK, 0.0f, &loop[0], &loop[1]);
    CHECK(processor.options_ == (Processor::OPTION_FLAG_RESONATOR_GRAINS |
        Processor::OPTION_FLAG_RESONATOR_INPUT));
    CHECK(loop[0] == after[0]);
    CHECK(loop[1] == after[1]);
    // Without the resonance it would be After the grains at the dry half.
    RunLoop(kMedia[m], RESONATOR_AFTER_GRAINS, 0.6f * kResonatorLoopDry,
        &after[0], &after[1]);
    RunLoop(kMedia[m], RESONATOR_FEEDBACK, 0.6f, &loop[0], &loop[1]);
    CHECK(loop[1] != after[1]);
    CHECK(processor.resonator_.peak_gain_ > 0.0f);
  }
  // The dry half reaches the grains.
  Init(Processor::MEDIA_DIGITAL, false, RESONATOR_FEEDBACK);
  processor.set_resonator_controls(Controls(1, 1));
  mix.feedback = 0.6f;
  for (int block = 0; block < 10; ++block) {
    Process();
  }
  CHECK(fabsf(processor.processor_digital_.feedback_ -
      0.6f * kResonatorLoopDry) < 1.0e-6f);
}

// Where After the grains dies away, so does the loop: delay mode at
// FEEDBACK 0.9, Bright digital, Marimba Short, 1 s of noise then 15 s of
// silence; there it ran away when the resonance was added in full.
void TestResonatorFeedbackDecays() {
  test_checker.Start("TestResonatorFeedbackDecays");
  double after = 0.0;
  for (int pass = 0; pass < 2; ++pass) {
    Init(Processor::MEDIA_DIGITAL, false,
        pass ? RESONATOR_FEEDBACK : RESONATOR_AFTER_GRAINS);
    processor.set_resonator_controls(Controls(4, 0));
    parameters.time.base = 0.3f;
    parameters.size.base = 1.0f;
    mix.dry_wet = 1.0f;
    mix.reverb_amount = 0.0f;
    mix.feedback = 0.9f;
    float peak = 0.0f;
    double energy = 0.0;
    for (int block = 0; block < 16 * 4000; ++block) {
      for (size_t i = 0; i < 12; ++i) {
        const float s = block < 4000 ? 0.3f * Noise() : 0.0f;
        input[i].l = input[i].r = s;
      }
      Process();
      for (size_t i = 0; i < 12; ++i) {
        peak = std::max(peak, std::max(fabsf(output[i].l),
            fabsf(output[i].r)));
        if (block >= 15 * 4000) {
          energy += output[i].l * output[i].l + output[i].r * output[i].r;
        }
      }
    }
    CHECK(processor.delay_active());
    CHECK(peak > 0.1f && peak < 2.0f);
    if (pass) {
      // No louder than After the grains' tail, + 6 dB.
      CHECK(energy <= after * 4.0 + 1.0e-12);
    } else {
      after = energy;
    }
  }
}

int main() {
  // Match the firmware FPU setup in drivers/system.cc.
#if defined(__SSE__)
  _mm_setcsr(_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
  uint64_t fpcr;
  __asm__ volatile("mrs %0, fpcr" : "=r" (fpcr));
  fpcr |= uint64_t(1) << 24;
  __asm__ volatile("msr fpcr, %0" : : "r" (fpcr));
#endif  // defined(__SSE__)
  TestResonatorDefaults();
  TestResonatorBank();
  TestResonatorPitch();
  TestResonatorLevel();
  TestResonatorTonalLevel();
  TestResonatorAfterGrains();
  TestResonatorBeforeBuffer();
  TestResonatorOff();
  TestResonatorStrike();
  TestResonatorQualities();
  TestResonatorControls();
  TestResonatorStrikeLevel();
  TestResonatorSeedEdges();
  TestResonatorSeedBlock();
  TestResonatorOptionsMatch();
  TestResonatorSpectralReset();
  TestResonatorLimiterAtRest();
  TestResonatorMaterials();
  TestResonatorLoopBound();
  TestResonatorBound();
  TestResonatorBoundPublished();
  TestResonatorLoopRender();
  TestResonatorLoopFilters();
  TestResonatorLoopChain();
  TestResonatorLoopGain();
  TestResonatorFeedback();
  TestResonatorFeedbackDecays();
  return test_checker.failures() ? 1 : 0;
}
