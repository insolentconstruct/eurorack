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
// Plaits synth: models, voice allocation, model detents and RAM bounds.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/test/test_checker.h"

#define private public
#include "beads/dsp/plaits_synth.h"
#undef private

using namespace beads;

// GCC 12 to 14 on amd64 and arm64, -ffp-contract=off. Notes from a regular
// SEED pulse train, then the drone with DENSITY at noon.
const uint32_t kNoteHashes[kNumPlaitsModels] = {
  0x2a2d2972, 0x877170b5, 0xc08556f5, 0x8a883e64,
  0x7707b2a5, 0x707f8c2a, 0xbbaf2a21, 0x2c51fa9c,
  0x64f3cb53, 0x2f58cfb9, 0x394b366a, 0xa9f0b3f3,
  0xc2099a1b, 0x9736e943, 0x0e7b8cbe, 0x885b531a,
  0x3a9917f1, 0xbc92403f, 0x9500f71a, 0x71f98e78,
  0x1bc184c0, 0xe38220f5, 0x773a4f5a, 0xd80154f8
};
const uint32_t kDroneHashes[kNumPlaitsModels] = {
  0x308767d4, 0xd33bb1f2, 0x73fd3dc2, 0xb39ab94e,
  0x697b7678, 0x3393ec36, 0x81955cfa, 0x04b4df54,
  0xd28d0da8, 0xf25eb15a, 0x7d7d6f57, 0x086d104d,
  0x21b599bc, 0xcdf47484, 0xda4689e1, 0x5b9c1a8d,
  0x1cae6339, 0x6740b0fa, 0x98eeb4e3, 0x093a4c92,
  0xf8ddbb0e, 0xa08d1e70, 0x926e7d00, 0x19ecc742
};

const size_t kBlockSize = 12;
const size_t kGuardSize = 4096;
const uint8_t kGuard = 0xa5;

static PlaitsSynth synth;
static uint16_t reverb_buffer[32768];
static double ram[(kPlaitsSynthRamSize + kGuardSize) / sizeof(double)];

inline uint8_t* Ram() { return reinterpret_cast<uint8_t*>(ram); }

struct Controls {
  Parameters p;
  ModulatedParameter density;
  Processor::MixParameters mix;
  SeedButtonState seed;
  bool freeze;

  Controls() {
    memset(&p, 0, sizeof(p));
    p.time.base = p.size.base = p.shape.base = 0.5f;
    density.base = 0.5f;
    density.cv = 0.0f;
    mix.feedback = 0.0f;
    mix.dry_wet = 0.5f;
    mix.reverb_amount = 0.3f;
    mix.output_gate = false;
    seed = SEED_BUTTON_STATE_LOCKED;
    freeze = false;
  }
};

void Reset() {
  memset(static_cast<void*>(&synth), 0, sizeof(synth));
  memset(reverb_buffer, 0, sizeof(reverb_buffer));
  memset(ram, kGuard, sizeof(ram));
  stmlib::Random::Seed(0x12345678);
  synth.Init(reverb_buffer);
  synth.InitVoices(Ram());
}

void Block(const Controls& c, FloatFrame* out) {
  bool freeze[kBlockSize];
  std::fill(&freeze[0], &freeze[kBlockSize], c.freeze);
  synth.Render(c.seed, NULL, freeze, Synth::RESOLUTION_16_BIT, c.density,
      c.p, c.mix, out, kBlockSize);
}

// Banks in Plaits' order: green, red, yellow.
const int kBankFirst[kNumPlaitsBanks] = { 8, 16, 0 };

inline int BankOf(int model) {
  return model >= 16 ? 1 : (model >= 8 ? 0 : 2);
}

// Selects the model's bank and returns FEEDBACK at the model's centre.
inline float ModelCentre(int model) {
  synth.set_bank(BankOf(model));
  return (float(model - kBankFirst[BankOf(model)]) + 0.5f) / kPlaitsBankSize;
}

bool GuardIntact() {
  const uint8_t* guard = Ram() + kPlaitsSynthRamSize;
  for (size_t i = 0; i < kGuardSize; ++i) {
    if (guard[i] != kGuard) {
      return false;
    }
  }
  return true;
}

// Two seconds of one model; returns the hash of the output.
uint32_t RenderModel(int model, bool drone, double* rms) {
  Reset();
  Controls c;
  c.mix.feedback = ModelCentre(model);
  c.density.base = drone ? 0.5f : 0.3f;
  c.p.pitch.attenuverter = 0.3f;
  uint32_t hash = 2166136261U;
  double energy = 0.0;
  bool finite = true;
  const int kBlocks = 8000;
  for (int block = 0; block < kBlocks; ++block) {
    FloatFrame out[kBlockSize];
    Block(c, out);
    for (size_t i = 0; i < kBlockSize; ++i) {
      finite = finite && std::isfinite(out[i].l) && std::isfinite(out[i].r) &&
          fabsf(out[i].l) < 4.0f && fabsf(out[i].r) < 4.0f;
      energy += out[i].l * out[i].l + out[i].r * out[i].r;
    }
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(out);
    for (size_t i = 0; i < sizeof(out); ++i) {
      hash = (hash ^ bytes[i]) * 16777619U;
    }
  }
  CHECK(finite);
  CHECK(synth.drone_ == drone);
  // Notes reach every voice; the drone plays the first.
  for (int v = 0; v < (drone ? 1 : kNumPlaitsVoices); ++v) {
    CHECK(synth.voice_[v].model() == model);
  }
  *rms = sqrt(energy / double(2 * kBlocks * kBlockSize));
  return hash;
}

void TestEveryModelRenders() {
  test_checker.Start("TestEveryModelRenders");
  for (int drone = 0; drone < 2; ++drone) {
    for (int model = 0; model < kNumPlaitsModels; ++model) {
      double rms = 0.0;
      double repeat_rms = 0.0;
      const uint32_t hash = RenderModel(model, drone, &rms);
      CHECK(hash == RenderModel(model, drone, &repeat_rms));
      CHECK(GuardIntact());
      printf("%s model %2d rms %.4f %08x\n", drone ? "Drone" : "Notes",
          model, rms, hash);
      CHECK(rms > 0.001);
      CHECK(hash == (drone ? kDroneHashes[model] : kNoteHashes[model]));
    }
  }
}

// Each trigger starts the next voice, which keeps the settings it started
// with; the seventh note reuses the first voice.
void TestVoiceAllocation() {
  test_checker.Start("TestVoiceAllocation");
  Reset();
  Controls c;
  FloatFrame out[kBlockSize];
  // DENSITY at noon: the last voice drones until SEED plays a note.
  c.seed = SEED_BUTTON_STATE_OFF;
  Block(c, out);
  CHECK(synth.drone_ && synth.active_voice_ == 0);
  for (int note = 1; note <= 8; ++note) {
    c.p.pitch.base = float(note);
    c.p.time.base = 0.1f * float(note);
    c.mix.feedback = ModelCentre(note);
    c.seed = SEED_BUTTON_STATE_ON;
    Block(c, out);
    c.seed = SEED_BUTTON_STATE_OFF;
    for (int i = 0; i < 100; ++i) {
      Block(c, out);
    }
    CHECK(!synth.drone_);
    const int voice = note % kNumPlaitsVoices;
    CHECK(synth.active_voice_ == voice);
    CHECK(synth.voice_[voice].model() == note);
    CHECK(synth.patch_[voice].model == note);
    CHECK(fabsf(synth.patch_[voice].note - (kPlaitsRootNote + note)) < 1e-4f);
    CHECK(fabsf(synth.patch_[voice].harmonics - 0.1f * note) < 1e-6f);
    // Earlier voices keep their own notes until reused.
    for (int previous = note - 1; previous > note - kNumPlaitsVoices &&
        previous >= 1; --previous) {
      const int v = previous % kNumPlaitsVoices;
      CHECK(synth.patch_[v].model == previous);
      CHECK(fabsf(synth.patch_[v].note - (kPlaitsRootNote + previous)) < 1e-4f);
    }
  }
  // Leaving noon and coming back resumes the drone on the last voice, which
  // then follows the knobs.
  c.density.base = 0.6f;
  Block(c, out);
  CHECK(!synth.drone_);
  c.density.base = 0.5f;
  c.p.pitch.base = -5.0f;
  c.mix.feedback = ModelCentre(20);
  Block(c, out);
  CHECK(synth.drone_ && synth.active_voice_ == 8 % kNumPlaitsVoices);
  CHECK(synth.patch_[synth.active_voice_].model == 20);
  CHECK(fabsf(synth.patch_[synth.active_voice_].note -
      (kPlaitsRootNote - 5.0f)) < 1e-4f);
  // A patched SEED input never drones.
  stmlib::GateFlags seed[kBlockSize];
  std::fill(&seed[0], &seed[kBlockSize], stmlib::GATE_FLAG_LOW);
  bool freeze[kBlockSize] = { };
  c.density.base = 0.6f;
  synth.Render(c.seed, seed, freeze, Synth::RESOLUTION_16_BIT, c.density,
      c.p, c.mix, out, kBlockSize);
  c.density.base = 0.5f;
  synth.Render(c.seed, seed, freeze, Synth::RESOLUTION_16_BIT, c.density,
      c.p, c.mix, out, kBlockSize);
  CHECK(!synth.drone_);
}

// FEEDBACK selects one of eight models in the current bank (#190): green
// 8-15 (the power-on bank), red 16-23, yellow 0-7. Both ends stay in the
// bank whatever came before; a bank change keeps the knob's detent.
void TestBankDetents() {
  test_checker.Start("TestBankDetents");
  Reset();
  CHECK(synth.first_model_ == 8);
  Controls c;
  FloatFrame out[kBlockSize];
  const int kSteps = 8000;
  for (int pass = 0; pass < 2; ++pass) {
    for (int bank = 0; bank < kNumPlaitsBanks; ++bank) {
      const int first = kBankFirst[bank];
      // Switch at either end of the knob from the previous bank.
      const int previous = (bank + kNumPlaitsBanks - 1) % kNumPlaitsBanks;
      for (int end = 0; end < 2; ++end) {
        synth.set_bank(previous);
        c.mix.feedback = end ? 1.0f : 0.0f;
        Block(c, out);
        synth.set_bank(bank);
        Block(c, out);
        CHECK(synth.patch_[synth.active_voice_].model ==
            first + (end ? kPlaitsBankSize - 1 : 0));
      }
      int model = first + kPlaitsBankSize - 1;
      int steps = 0;
      for (int i = kSteps; i >= 0; --i) {
        c.mix.feedback = float(i) / kSteps;
        Block(c, out);
        const int selected = first + synth.model_quantizer_.quantized_value();
        if (selected != model) {
          CHECK(selected == model - 1);
          model = selected;
          ++steps;
        }
        CHECK(synth.patch_[synth.active_voice_].model == model);
      }
      CHECK(model == first && steps == kPlaitsBankSize - 1);
      for (int i = 0; i <= kSteps; ++i) {
        c.mix.feedback = float(i) / kSteps;
        Block(c, out);
        const int selected = first + synth.model_quantizer_.quantized_value();
        if (selected != model) {
          // Eight equal detents; steps up one hysteresis late.
          CHECK(selected == model + 1);
          const float position = (selected - first + 0.048f) / 8.0f;
          CHECK(fabsf(c.mix.feedback - position) < 0.0005f);
          model = selected;
        }
      }
      CHECK(model == first + kPlaitsBankSize - 1);
    }
  }
  // Notes keep the model they started with when the bank changes.
  Reset();
  synth.set_bank(1);
  c.mix.feedback = 1.0f;
  c.seed = SEED_BUTTON_STATE_ON;
  Block(c, out);
  const int voice = synth.active_voice_;
  CHECK(synth.patch_[voice].model == 23);
  c.seed = SEED_BUTTON_STATE_OFF;
  synth.set_bank(2);
  Block(c, out);
  CHECK(synth.patch_[voice].model == 23);
  // Inside the hysteresis band, a bank change keeps the detent.
  for (int bank = 0; bank < kNumPlaitsBanks; ++bank) {
    synth.set_bank(0);
    c.mix.feedback = 0.4f;
    Block(c, out);
    c.mix.feedback = 2.98f / 8.0f;
    Block(c, out);
    CHECK(synth.model_quantizer_.quantized_value() == 3);
    synth.set_bank(bank);
    c.seed = SEED_BUTTON_STATE_ON;
    Block(c, out);
    c.seed = SEED_BUTTON_STATE_OFF;
    CHECK(synth.patch_[synth.active_voice_].model == kBankFirst[bank] + 3);
  }
  // Noon is the edge between the fourth and fifth models: each bank step
  // keeps the side the knob came from, with no jump to another model.
  for (int from_above = 0; from_above < 2; ++from_above) {
    synth.set_bank(0);
    c.mix.feedback = from_above ? 0.6f : 0.4f;
    Block(c, out);
    c.mix.feedback = 0.5f;
    for (int step = 0; step <= kNumPlaitsBanks; ++step) {
      const int bank = step % kNumPlaitsBanks;
      synth.set_bank(bank);
      for (int i = 0; i < 4; ++i) {
        c.seed = i == 0 ? SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF;
        Block(c, out);
        CHECK(synth.patch_[synth.active_voice_].model ==
            kBankFirst[bank] + 3 + from_above);
      }
    }
  }
}

// FREEZE holds a ringing note at its level instead of letting it decay.
void TestFreezeSustains() {
  test_checker.Start("TestFreezeSustains");
  double level[2];
  for (int freeze = 0; freeze < 2; ++freeze) {
    Reset();
    Controls c;
    c.mix.feedback = ModelCentre(8);  // Virtual analog, through the LPG.
    c.mix.reverb_amount = 0.0f;
    c.mix.dry_wet = 0.2f;
    FloatFrame out[kBlockSize];
    c.seed = SEED_BUTTON_STATE_ON;
    Block(c, out);
    c.seed = SEED_BUTTON_STATE_OFF;
    for (int i = 0; i < 40; ++i) {
      Block(c, out);
    }
    c.freeze = freeze;
    double energy = 0.0;
    for (int i = 0; i < 8000; ++i) {
      Block(c, out);
      if (i >= 7000) {
        for (size_t j = 0; j < kBlockSize; ++j) {
          energy += out[j].l * out[j].l;
        }
      }
    }
    level[freeze] = energy;
  }
  printf("Freeze off %.3g, on %.3g\n", level[0], level[1]);
  CHECK(level[0] < 1e-6);
  CHECK(level[1] > 1.0);

  // FREEZE held from the first block, before any note: voices that never
  // played stay silent.
  Reset();
  Controls c;
  c.mix.feedback = ModelCentre(8);
  c.mix.reverb_amount = 0.0f;
  c.seed = SEED_BUTTON_STATE_OFF;
  c.density.base = 0.3f;
  c.freeze = true;
  double energy = 0.0;
  for (int i = 0; i < 2000; ++i) {
    FloatFrame out[kBlockSize];
    Block(c, out);
    for (size_t j = 0; j < kBlockSize; ++j) {
      energy += out[j].l * out[j].l + out[j].r * out[j].r;
    }
  }
  printf("Freeze from start %.3g\n", energy);
  CHECK(energy < 1e-6);
}

int main() {
  TestVoiceAllocation();
  TestBankDetents();
  TestFreezeSustains();
  TestEveryModelRenders();
  return test_checker.failures() ? 1 : 0;
}
