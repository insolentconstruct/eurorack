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

// A steady sine on the fundamental or the second mode, at low and high
// PITCH: the limiter holds its ceiling at every rate once settled; its
// attack lets a sudden high mode overshoot for the first few milliseconds.
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

// Every start finds the limiter at rest: the first strike is as loud as
// one after a long silence. Power-on, a quality changing the rate,
// leaving spectral, a placement change.
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
  Init(Processor::MEDIA_CLEAN_TAPE, false, RESONATOR_AFTER_GRAINS);
  Silence();
  processor.Configure(Processor::MEDIA_DIGITAL, false);
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
  return test_checker.failures() ? 1 : 0;
}
