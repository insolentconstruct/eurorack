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
// Plaits synth: skipped AUX, models, shared 6-op banks, voice allocation,
// model detents and RAM bounds.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/test/test_checker.h"

#define private public
#include "beads/dsp/plaits_synth.h"
#include "plaits/dsp/speech/lpc_speech_synth_controller.h"
#include "plaits/dsp/speech/lpc_speech_synth_words.h"
#undef private
#include "stmlib/utils/buffer_allocator.h"

#include "beads/test/plaits_aux.h"
#include "beads/test/plaits_fm_banks.h"

using namespace beads;

// GCC 12 to 14 on amd64 and arm64, -ffp-contract=off. Notes from a regular
// SEED pulse train, then the drone with DENSITY at noon.
const uint32_t kNoteHashes[kNumPlaitsModels] = {
  0x2a2d2972, 0x877170b5, 0xc08556f5, 0x8a883e64,
  0x7707b2a5, 0x707f8c2a, 0xbbaf2a21, 0x2c51fa9c,
  0x64f3cb53, 0x2f58cfb9, 0x394b366a, 0xa9f0b3f3,
  0xc2099a1b, 0x9736e943, 0x0e7b8cbe, 0x2109bc86,
  0x3a9917f1, 0xbc92403f, 0x9500f71a, 0x71f98e78,
  0x1bc184c0, 0xe38220f5, 0x773a4f5a, 0xd80154f8
};
const uint32_t kDroneHashes[kNumPlaitsModels] = {
  0x308767d4, 0xd33bb1f2, 0x73fd3dc2, 0xb39ab94e,
  0x697b7678, 0x3393ec36, 0x81955cfa, 0x04b4df54,
  0xd28d0da8, 0xf25eb15a, 0x7d7d6f57, 0x086d104d,
  0x21b599bc, 0xcdf47484, 0xda4689e1, 0xa0f5c1bf,
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

// Every model, resolution and corner of the controls: the voices' quantizer
// converts in range (the float-cast-overflow sanitizer traps otherwise) and
// the output stays finite.
void TestQuantizationBounds() {
  test_checker.Start("TestQuantizationBounds");
  CHECK(reinterpret_cast<uintptr_t>(Ram()) % kPlaitsRamAlignment == 0);
  for (int model = 0; model < kNumPlaitsModels; ++model) {
    for (int r = 0; r < 4; ++r) {
      for (int corner = 0; corner < 8; ++corner) {
        for (int high = 0; high < 2; ++high) {
          Reset();
          Controls c;
          c.mix.feedback = ModelCentre(model);
          c.density.base = 0.2f;
          c.p.pitch.base = high ? 48.0f : -48.0f;
          c.p.time.base = (corner & 1) ? 1.0f : 0.0f;
          c.p.size.base = (corner & 2) ? 1.0f : 0.0f;
          c.p.shape.base = (corner & 4) ? 1.0f : 0.0f;
          bool finite = true;
          for (int block = 0; block < 250; ++block) {
            bool freeze[kBlockSize];
            std::fill(&freeze[0], &freeze[kBlockSize], false);
            FloatFrame out[kBlockSize];
            synth.Render(c.seed, NULL, freeze, Synth::Resolution(r),
                c.density, c.p, c.mix, out, kBlockSize);
            for (size_t i = 0; i < kBlockSize; ++i) {
              finite = finite && std::isfinite(out[i].l) &&
                  std::isfinite(out[i].r);
            }
          }
          CHECK(finite);
        }
      }
    }
  }
}

// The 6-op banks follow the voices and Speech's banks, unpacked once and
// shared by all voices.
void TestFmBanksShared() {
  test_checker.Start("TestFmBanksShared");
  Reset();
  const plaits::fm::Patch* banks = reinterpret_cast<const plaits::fm::Patch*>(
      Ram() + kNumPlaitsVoices * kPlaitsVoiceRamSize + kPlaitsSpeechRamSize);
  for (int v = 0; v < kNumPlaitsVoices; ++v) {
    CHECK(synth.voice_[v].fm_banks_ == banks);
  }
  CHECK(FmBanksUnpacked(banks));
}

// Every 6-op patch and bank on all six voices, and another model every fourth
// note; pinned from the tree where each voice unpacked its own bank.
const uint32_t kFmBankSwitchHash = 0x22fe3f1d;

void TestFmBankSwitches() {
  test_checker.Start("TestFmBankSwitches");
  Reset();
  Controls c;
  c.mix.reverb_amount = 0.0f;
  uint32_t hash = 2166136261U;
  // 96 6-op notes, 32 per bank, and 32 others.
  for (int i = 0; i < 128; ++i) {
    const int fm_note = i - i / 4;
    const int model = i % 4 == 3 ? 9 : 2 + fm_note % kNumPlaitsFmBanks;
    const int patch = (fm_note / kNumPlaitsFmBanks * 13) % kPlaitsFmBankSize;
    c.mix.feedback = ModelCentre(model);
    c.p.time.base = float(patch) / (kPlaitsFmBankSize * 1.02f);
    c.p.pitch.base = float(i % 12);
    for (int block = 0; block < 40; ++block) {
      c.seed = block == 0 ? SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF;
      FloatFrame out[kBlockSize];
      Block(c, out);
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(out);
      for (size_t j = 0; j < sizeof(out); ++j) {
        hash = (hash ^ bytes[j]) * 16777619U;
      }
    }
    CHECK(synth.voice_[synth.active_voice_].model() == model);
  }
  CHECK(GuardIntact());
  printf("FM bank switches %08x\n", hash);
  CHECK(hash == kFmBankSwitchHash);
}

// Beads never plays AUX: with PLAITS_NO_AUX these engines skip its work.
void TestAuxSkipped() {
  test_checker.Start("TestAuxSkipped");
  for (int e = 0; e < kNumAuxSkippingEngines; ++e) {
#ifdef PLAITS_NO_AUX
    CHECK(AuxUntouched(e));
#else
    CHECK(!AuxUntouched(e));
#endif  // PLAITS_NO_AUX
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

// FEEDBACK picks one of eight models per bank (#190): green 8-15 at power-on,
// red 16-23, yellow 0-7; ends stay in the bank and a step keeps the detent.
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

// Speech word banks as Mutable Instruments' synchronous Load decodes them:
// frames, words, FNV-1a of the frames and word boundaries; then the blocks
// the sliced load takes.
const int kWordBankFrames[LPC_SPEECH_SYNTH_NUM_WORD_BANKS] = {
  229, 229, 524, 604, 926
};
const int kWordBankWords[LPC_SPEECH_SYNTH_NUM_WORD_BANKS] = {
  7, 11, 26, 26, 22
};
const uint32_t kWordBankHashes[LPC_SPEECH_SYNTH_NUM_WORD_BANKS] = {
  0x4b953df5, 0xe4c9acc0, 0x762895c2, 0x77b80f2c, 0x1a990b17
};
const int kWordBankLoadBlocks[LPC_SPEECH_SYNTH_NUM_WORD_BANKS] = {
  8, 8, 17, 19, 29
};
// The word that follows a bank change from bank (b + 1) % 5 to bank b, as
// Mutable Instruments' controller plays it with the change and its trigger
// kWordBankLoadBlocks[b] - 1 blocks later (GCC 12, -ffp-contract=off).
const uint32_t kWordBankChangeHashes[LPC_SPEECH_SYNTH_NUM_WORD_BANKS] = {
  0x11ad03a2, 0xbf169bf8, 0x3a7753d9, 0x3b593c26, 0xf5f71ca8
};

static uint8_t speech_ram[16384];
static plaits::LPCSpeechSynthWordBank word_bank;
static plaits::LPCSpeechSynthController speech;

inline uint32_t Fnv(const void* data, size_t size, uint32_t hash) {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  while (size--) {
    hash = (hash ^ *bytes++) * 16777619U;
  }
  return hash;
}

void ResetSpeech() {
  memset(speech_ram, 0, sizeof(speech_ram));
  stmlib::Random::Seed(0x12345678);
  stmlib::BufferAllocator allocator(speech_ram, sizeof(speech_ram));
  word_bank.Init(plaits::word_banks_, LPC_SPEECH_SYNTH_NUM_WORD_BANKS,
      &allocator);
  speech.Init(&word_bank);
}

// Loads a bank as the controller does, one call per block; returns the calls.
int LoadWordBank(int bank) {
  int calls = 0;
  do {
    const int before = word_bank.loading_bank_ == bank
        ? word_bank.num_frames() : 0;
    const bool was_loading = word_bank.loaded_bank_ != bank;
    const bool loaded = word_bank.Load(bank);
    ++calls;
    CHECK(word_bank.num_frames() - before <=
        plaits::kLPCSpeechSynthFramesPerLoad);
    // True once, from the call that completes the bank.
    CHECK(loaded == (was_loading && !word_bank.loading()));
  } while (word_bank.loading() && calls < 100);
  return calls;
}

bool WordBankIs(int bank) {
  const uint32_t hash = Fnv(word_bank.frames_,
      word_bank.num_frames_ * sizeof(plaits::LPCSpeechSynth::Frame),
      2166136261U);
  return !word_bank.loading() && word_bank.loaded_bank_ == bank &&
      word_bank.num_frames_ == kWordBankFrames[bank] &&
      word_bank.num_words_ == kWordBankWords[bank] &&
      Fnv(word_bank.word_boundaries_, (word_bank.num_words_ + 1) * sizeof(int),
          hash) == kWordBankHashes[bank];
}

void TestWordBankLoads() {
  test_checker.Start("TestWordBankLoads");
  for (int b = 0; b < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++b) {
    ResetSpeech();
    CHECK(LoadWordBank(b) == kWordBankLoadBlocks[b]);
    CHECK(WordBankIs(b));
    CHECK(!word_bank.Load(b));
    CHECK(WordBankIs(b));
  }
  // A bank change during a load restarts it, even back to the bank that
  // was loaded before.
  for (int a = 0; a < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++a) {
    for (int b = 0; b < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++b) {
      for (int calls = 1; calls < 4; ++calls) {
        ResetSpeech();
        LoadWordBank(a);
        for (int i = 0; i < calls; ++i) {
          word_bank.Load(b);
        }
        LoadWordBank(a);
        CHECK(WordBankIs(a));
        for (int i = 0; i < calls; ++i) {
          word_bank.Load(b);
        }
        LoadWordBank(b);
        CHECK(WordBankIs(b));
      }
    }
  }
}

// The bit stream's table reverses every byte.
void TestBitReversal() {
  test_checker.Start("TestBitReversal");
  for (int b = 0; b < 256; ++b) {
    int reversed = 0;
    for (int bit = 0; bit < 8; ++bit) {
      reversed |= ((b >> bit) & 1) << (7 - bit);
    }
    CHECK(plaits::BitStream::reverse_lut_[b] == reversed);
  }
}

static uint8_t shared_speech_ram[16384];
static plaits::LPCSpeechSynthWordBank shared_word_bank;

// Loads both word banks one call each, the first decoding as Mutable
// Instruments' Load does, the second copying the synth's decoded banks.
bool LoadSharedStep(int bank) {
  const plaits::LPCSpeechSynthWordBank* decoded =
      plaits::LPCSpeechSynthWordBank::decoded_banks_;
  plaits::LPCSpeechSynthWordBank::set_decoded_banks(NULL);
  const bool loaded = word_bank.Load(bank);
  plaits::LPCSpeechSynthWordBank::set_decoded_banks(decoded);
  const bool shared_loaded = shared_word_bank.Load(bank);
  return loaded == shared_loaded &&
      word_bank.num_frames_ == shared_word_bank.num_frames_ &&
      word_bank.num_words_ == shared_word_bank.num_words_ &&
      word_bank.loaded_bank_ == shared_word_bank.loaded_bank_ &&
      word_bank.loading_bank_ == shared_word_bank.loading_bank_ &&
      !memcmp(speech_ram, shared_speech_ram, sizeof(speech_ram));
}

// Per call, the shared decode leaves each voice's RAM as decoding does.
void TestSharedWordBanks() {
  test_checker.Start("TestSharedWordBanks");
  Reset();
  CHECK(plaits::LPCSpeechSynthWordBank::decoded_banks_ != NULL);
  CHECK(GuardIntact());
  for (int a = 0; a < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++a) {
    for (int b = 0; b < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++b) {
      for (int calls = 1; calls <= kWordBankLoadBlocks[a]; ++calls) {
        for (int reset = 0; reset < 2; ++reset) {
          ResetSpeech();
          memset(shared_speech_ram, 0, sizeof(shared_speech_ram));
          stmlib::BufferAllocator allocator(
              shared_speech_ram, sizeof(shared_speech_ram));
          shared_word_bank.Init(plaits::word_banks_,
              LPC_SPEECH_SYNTH_NUM_WORD_BANKS, &allocator);
          bool same = true;
          for (int i = 0; i < calls; ++i) {
            same = same && LoadSharedStep(a);
          }
          if (reset) {
            word_bank.Reset();
            shared_word_bank.Reset();
          }
          for (int i = 0; i < 100 && word_bank.loaded_bank_ != b; ++i) {
            same = same && LoadSharedStep(b);
          }
          for (int i = 0; i < 100 && word_bank.loaded_bank_ != a; ++i) {
            same = same && LoadSharedStep(a);
          }
          CHECK(same);
          CHECK(WordBankIs(a));
        }
      }
    }
  }
  CHECK(GuardIntact());
}

// A word that ended on bank (b + 1) % 5, then a bank change to b with a
// trigger: the word plays when the bank is loaded, sample for sample as
// Mutable Instruments' controller plays it with the change made then.
void TestWordBankChangeDelaysWord() {
  test_checker.Start("TestWordBankChangeDelaysWord");
  for (int b = 0; b < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++b) {
    const int a = (b + 1) % LPC_SPEECH_SYNTH_NUM_WORD_BANKS;
    ResetSpeech();
    LoadWordBank(a);
    float excitation[kBlockSize];
    float out[kBlockSize];
    const float f0 = 100.0f / 48000.0f;
    for (int i = 0; i < 12000; ++i) {
      speech.Render(false, i == 0, a, f0, 0.3f, 0.5f, 0.0f, 0.5f, 0.8f,
          excitation, out, kBlockSize);
    }
    uint32_t hash = 2166136261U;
    for (int i = 0; i < 8000; ++i) {
      speech.Render(false, i == 0, b, f0, 0.3f, 0.5f, 0.55f, 0.5f, 0.8f,
          excitation, out, kBlockSize);
      hash = Fnv(out, sizeof(out), hash);
      hash = Fnv(excitation, sizeof(excitation), hash);
    }
    CHECK(hash == kWordBankChangeHashes[b]);
  }
  // A waiting word is dropped by a Reset, the phonemes, or both.
  for (int drop = 0; drop < 3; ++drop) {
    const bool phonemes = drop != 0;
    ResetSpeech();
    LoadWordBank(0);
    float excitation[kBlockSize];
    float out[kBlockSize];
    const float f0 = 100.0f / 48000.0f;
    speech.Render(false, true, 4, f0, 0.3f, 0.5f, 0.55f, 0.5f, 0.8f,
        excitation, out, kBlockSize);
    CHECK(word_bank.loading());
    if (drop != 1) {
      word_bank.Reset();
    }
    for (int i = 0; i < 40; ++i) {
      speech.Render(false, false, phonemes ? -1 : 4, f0, 0.3f, 0.5f, 0.55f,
          0.5f, 0.8f, excitation, out, kBlockSize);
    }
    CHECK(speech.playback_frame_ == -1);
    CHECK(speech.remaining_frame_samples_ == 0);
  }
}

// HARMONICS from the last word bank down to the phonemes (#280), mid-word
// and after the word: the word's frame numbers never index the phonemes.
void TestPhonemesAfterWord() {
  test_checker.Start("TestPhonemesAfterWord");
  const int last = LPC_SPEECH_SYNTH_NUM_WORD_BANKS - 1;
  for (int ended = 0; ended < 2; ++ended) {
    ResetSpeech();
    LoadWordBank(last);
    float excitation[kBlockSize];
    float out[kBlockSize];
    const float f0 = 100.0f / 48000.0f;
    const int word_blocks = ended ? 4000 : 100;
    for (int i = 0; i < word_blocks; ++i) {
      speech.Render(false, i == 0, last, f0, 0.3f, 0.5f, 0.99f, 0.5f, 0.8f,
          excitation, out, kBlockSize);
    }
    CHECK(speech.playback_frame_ >= plaits::kLPCSpeechSynthNumPhonemes);
    for (int i = 0; i < 4000; ++i) {
      speech.Render(false, false, -1, f0, 0.3f, 0.5f, 0.99f, 0.5f, 0.8f,
          excitation, out, kBlockSize);
    }
    CHECK(speech.playback_frame_ == -1);
  }
  // A dip to the phonemes that ends within a frame leaves the word playing.
  ResetSpeech();
  LoadWordBank(last);
  float excitation[kBlockSize];
  float out[kBlockSize];
  const float f0 = 100.0f / 48000.0f;
  for (int i = 0; i < 100; ++i) {
    speech.Render(false, i == 0, last, f0, 0.3f, 0.5f, 0.99f, 0.5f, 0.8f,
        excitation, out, kBlockSize);
  }
  while (speech.remaining_frame_samples_ < 2 * kBlockSize) {
    speech.Render(false, false, last, f0, 0.3f, 0.5f, 0.99f, 0.5f, 0.8f,
        excitation, out, kBlockSize);
  }
  const int frame = speech.playback_frame_;
  speech.Render(false, false, -1, f0, 0.3f, 0.5f, 0.99f, 0.5f, 0.8f,
      excitation, out, kBlockSize);
  CHECK(speech.playback_frame_ == frame);
  for (int i = 0; i < 100; ++i) {
    speech.Render(false, false, last, f0, 0.3f, 0.5f, 0.99f, 0.5f, 0.8f,
        excitation, out, kBlockSize);
  }
  CHECK(speech.playback_frame_ > frame);
}

int main() {
  TestAuxSkipped();
  TestBitReversal();
  TestFmBanksShared();
  TestFmBankSwitches();
  TestWordBankLoads();
  TestWordBankChangeDelaysWord();
  TestPhonemesAfterWord();
  TestSharedWordBanks();
  TestVoiceAllocation();
  TestBankDetents();
  TestFreezeSustains();
  TestEveryModelRenders();
  TestQuantizationBounds();
  return test_checker.failures() ? 1 : 0;
}
