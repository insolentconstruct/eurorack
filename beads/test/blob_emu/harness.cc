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
//
// -----------------------------------------------------------------------------
//
// Emulator harness: the region reader, the libraries built as at synth entry,
// a blob voice (one slot and arena, as PlaitsVoice) and the same engines
// compiled in, built in PlaitsVoice's order and sharing their own arena as
// PlaitsVoice's do; called by render_compare.py under Unicorn's Cortex-M7.

#include <algorithm>
#include <new>

#include "stmlib/utils/buffer_allocator.h"
#include "stmlib/utils/random.h"

#include "beads/blob/models/phase_weave_engine.h"
#include "beads/blob/models/spectral_spiral_engine.h"
#include "beads/blob/models/pulsar_engine.h"
#include "beads/blob/models/glisson_engine.h"
#include "beads/blob/models/reed_pipe_engine.h"
#include "beads/blob/models/sideband_engine.h"
#include "beads/blob/models/undertow_engine.h"
#include "beads/blob/models/phase_flock_engine.h"
#include "beads/blob/models/scanned_engine.h"
#include "beads/blob/models/question_mark_engine.h"
#include "beads/blob/models/triple_engine.h"
#include "beads/blob/models/raw_fm_engine.h"
#include "beads/blob/models/buzz_engine.h"
#include "beads/blob/models/struck_bell_engine.h"
#include "beads/blob/models/acid_engine.h"
#include "beads/blob/plaits_blob.h"
#include "beads/blob/plaits_blob_engine.h"
#include "beads/blob/plaits_libraries.h"
#include "beads/blob/plaits_region.h"
#include "beads/test/blob_gate/test_sine.h"
#include "plaits/dsp/engine/additive_engine.h"
#include "plaits/dsp/engine/bass_drum_engine.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "plaits/dsp/engine/fm_engine.h"
#include "plaits/dsp/engine/grain_engine.h"
#include "plaits/dsp/engine/hi_hat_engine.h"
#include "plaits/dsp/engine/modal_engine.h"
#include "plaits/dsp/engine/noise_engine.h"
#include "plaits/dsp/engine/particle_engine.h"
#include "plaits/dsp/engine/snare_drum_engine.h"
#include "plaits/dsp/engine/speech_engine.h"
#include "plaits/dsp/engine/string_engine.h"
#include "plaits/dsp/engine/swarm_engine.h"
#include "plaits/dsp/engine/virtual_analog_engine.h"
#include "plaits/dsp/engine/waveshaping_engine.h"
#include "plaits/dsp/engine/wavetable_engine.h"
#include "plaits/dsp/engine2/chiptune_engine.h"
#include "plaits/dsp/engine2/phase_distortion_engine.h"
#include "plaits/dsp/engine2/six_op_engine.h"
#include "plaits/dsp/engine2/string_machine_engine.h"
#include "plaits/dsp/engine2/virtual_analog_vcf_engine.h"
#include "plaits/dsp/engine2/wave_terrain_engine.h"
#include "plaits/dsp/speech/lpc_speech_synth_words.h"
#include "plaits/resources.h"

using namespace beads;
using namespace plaits;

class TestSineEngine : public Engine {
 public:
  TestSineEngine() { }
  ~TestSineEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    sine_.Init();
  }

  virtual void Reset() {
    sine_.Init();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    sine_.Render(parameters, out, aux, size);
  }

 private:
  TestSine sine_;

  DISALLOW_COPY_AND_ASSIGN(TestSineEngine);
};

// The proof's Palette models and test sine, then the stock engines in
// PlaitsVoice's registry order (the 6-op engine serves models 2-4).
const size_t kFirstStock = 16;
const size_t kNumStock = 22;
const size_t kNumCompiled = kFirstStock + kNumStock;
const size_t kSixOpSlot = kFirstStock + 2;
const size_t kChiptuneSlot = kFirstStock + 5;
const int kNumFmBanks = 3;
const int kFmBankSize = 32;
const size_t kArenaSize = 16384;
const size_t kCanaryWords = 16;
const size_t kMaxDigests = 64;
// As the development build: kPlaitsLibraryRamSize, and the first voice's RAM
// as scratch (enough for a word bank).
const size_t kLibraryRamSize = 64 * 1024;
const size_t kScratchSize = 16 * 1024;
const size_t kSpeechRamSize = 36 * 1024;

// Engines are placed by harness_setup: no static constructors run here.
static uint8_t phase_weave_storage[sizeof(PhaseWeaveEngine)]
    __attribute__((aligned(8)));
static uint8_t sine_storage[sizeof(TestSineEngine)]
    __attribute__((aligned(8)));
static uint8_t spectral_spiral_storage[sizeof(SpectralSpiralEngine)]
    __attribute__((aligned(8)));

static uint8_t pulsar_storage[sizeof(PulsarEngine)]
    __attribute__((aligned(8)));

static uint8_t glisson_storage[sizeof(GlissonEngine)]
    __attribute__((aligned(8)));

static uint8_t reed_pipe_storage[sizeof(ReedPipeEngine)]
    __attribute__((aligned(8)));

static uint8_t sideband_storage[sizeof(SidebandEngine)]
    __attribute__((aligned(8)));

static uint8_t undertow_storage[sizeof(UndertowEngine)]
    __attribute__((aligned(8)));

static uint8_t phase_flock_storage[sizeof(PhaseFlockEngine)]
    __attribute__((aligned(8)));

static uint8_t scanned_storage[sizeof(ScannedEngine)]
    __attribute__((aligned(8)));

static uint8_t question_mark_storage[sizeof(QuestionMarkEngine)]
    __attribute__((aligned(8)));

static uint8_t triple_storage[sizeof(TripleEngine)]
    __attribute__((aligned(8)));

static uint8_t raw_fm_storage[sizeof(RawFmEngine)]
    __attribute__((aligned(8)));

static uint8_t buzz_storage[sizeof(BuzzEngine)]
    __attribute__((aligned(8)));

static uint8_t struck_bell_storage[sizeof(StruckBellEngine)]
    __attribute__((aligned(8)));

static uint8_t acid_storage[sizeof(AcidEngine)]
    __attribute__((aligned(8)));

// The stock engines, as PlaitsEngines in beads/dsp/plaits_voice.cc.
struct StockEngines {
  VirtualAnalogVCFEngine virtual_analog_vcf_engine;
  PhaseDistortionEngine phase_distortion_engine;
  SixOpEngine six_op_engine;
  WaveTerrainEngine wave_terrain_engine;
  StringMachineEngine string_machine_engine;
  ChiptuneEngine chiptune_engine;
  VirtualAnalogEngine virtual_analog_engine;
  WaveshapingEngine waveshaping_engine;
  FMEngine fm_engine;
  GrainEngine grain_engine;
  AdditiveEngine additive_engine;
  WavetableEngine wavetable_engine;
  ChordEngine chord_engine;
  SpeechEngine speech_engine;
  SwarmEngine swarm_engine;
  NoiseEngine noise_engine;
  ParticleEngine particle_engine;
  StringEngine string_engine;
  ModalEngine modal_engine;
  BassDrumEngine bass_drum_engine;
  SnareDrumEngine snare_drum_engine;
  HiHatEngine hi_hat_engine;
};
static uint8_t stock_storage[sizeof(StockEngines)] __attribute__((aligned(8)));
static uint8_t stock_arena[kArenaSize] __attribute__((aligned(8)));
// PlaitsVoice::UnpackFmBanks' banks.
static fm::Patch fm_banks[kNumFmBanks * kFmBankSize];
static Engine* compiled[kNumCompiled];
static uint8_t compiled_arena[kArenaSize] __attribute__((aligned(8)));
// The compiled-in engines' decoded word banks (PlaitsVoice::DecodeSpeechBanks).
static uint8_t compiled_speech_ram[kSpeechRamSize] __attribute__((aligned(8)));
static PlaitsRegion region;
static PlaitsLibraries libraries;

extern "C" {

// Guard bands around the blob voice's slot and arena.
struct BlobVoice {
  uint32_t canary_before[kCanaryWords];
  uint8_t engine[sizeof(PlaitsBlobEngine)] __attribute__((aligned(8)));
  uint32_t canary_between[kCanaryWords];
  uint8_t arena[kArenaSize] __attribute__((aligned(8)));
  uint32_t canary_after[kCanaryWords];
};

BlobVoice harness_blob_voice;
uint8_t harness_library_ram[kLibraryRamSize] __attribute__((aligned(32)));
uint8_t harness_scratch[kScratchSize] __attribute__((aligned(8)));
uint32_t harness_library_ram_size = kLibraryRamSize;
uint8_t harness_digests[kMaxDigests][kPlaitsBlobDigestSize];
uint32_t harness_num_digests;
PlaitsDirectoryEntry harness_directory[PLAITS_SECTION_LAST];
uint32_t harness_num_entries;
PlaitsDefaultSection harness_defaults[PLAITS_SECTION_LAST];
uint32_t harness_num_defaults;
PlaitsRegionIdentity harness_identity;
uint32_t harness_lock;
// The checked synth blob of RNGS, or 0.
uintptr_t harness_synth_blob;
uintptr_t harness_slot;
// Wave terrain lookups past the bank, counted by terrain_probe.h.
uint32_t harness_terrain_overshoot;
uint32_t harness_num_terrains;
const uint8_t* harness_user_data = NULL;
float harness_out[kMaxBlockSize];
float harness_aux[kMaxBlockSize];
EngineParameters harness_parameters;

static PlaitsBlobEngine* blob_engine() {
  return reinterpret_cast<PlaitsBlobEngine*>(harness_blob_voice.engine);
}

static void DecodeSpeechBanks() {
  stmlib::BufferAllocator allocator(compiled_speech_ram, kSpeechRamSize);
  LPCSpeechSynthWordBank* banks = allocator.Allocate<LPCSpeechSynthWordBank>(
      LPC_SPEECH_SYNTH_NUM_WORD_BANKS);
  LPCSpeechSynthWordBank::set_decoded_banks(NULL);
  for (int i = 0; i < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++i) {
    stmlib::BufferAllocator scratch(harness_scratch, kScratchSize);
    banks[i].Init(word_banks_, LPC_SPEECH_SYNTH_NUM_WORD_BANKS, &scratch);
    banks[i].Decode(i, &allocator);
  }
  LPCSpeechSynthWordBank::set_decoded_banks(banks);
}

// Engines are built in zeroed storage and RAM, as at power-on.
static void BuildProof() {
  std::fill(&compiled_arena[0], &compiled_arena[kArenaSize], 0);
  std::fill(&phase_weave_storage[0], &phase_weave_storage[sizeof(
      phase_weave_storage)], 0);
  std::fill(&sine_storage[0], &sine_storage[sizeof(sine_storage)], 0);
  std::fill(&spectral_spiral_storage[0], &spectral_spiral_storage[sizeof(
      spectral_spiral_storage)], 0);
  std::fill(&pulsar_storage[0], &pulsar_storage[sizeof(pulsar_storage)], 0);
  std::fill(&glisson_storage[0], &glisson_storage[sizeof(glisson_storage)], 0);
  std::fill(&reed_pipe_storage[0],
      &reed_pipe_storage[sizeof(reed_pipe_storage)], 0);
  std::fill(&sideband_storage[0],
      &sideband_storage[sizeof(sideband_storage)], 0);
  std::fill(&undertow_storage[0],
      &undertow_storage[sizeof(undertow_storage)], 0);
  std::fill(&phase_flock_storage[0],
      &phase_flock_storage[sizeof(phase_flock_storage)], 0);
  std::fill(&scanned_storage[0],
      &scanned_storage[sizeof(scanned_storage)], 0);
  std::fill(&question_mark_storage[0],
      &question_mark_storage[sizeof(question_mark_storage)], 0);
  std::fill(&triple_storage[0],
      &triple_storage[sizeof(triple_storage)], 0);
  std::fill(&raw_fm_storage[0],
      &raw_fm_storage[sizeof(raw_fm_storage)], 0);
  std::fill(&buzz_storage[0],
      &buzz_storage[sizeof(buzz_storage)], 0);
  std::fill(&struck_bell_storage[0],
      &struck_bell_storage[sizeof(struck_bell_storage)], 0);
  std::fill(&acid_storage[0],
      &acid_storage[sizeof(acid_storage)], 0);
  stmlib::BufferAllocator allocator(compiled_arena, kArenaSize);
  compiled[0] = new(phase_weave_storage) PhaseWeaveEngine;
  compiled[1] = new(sine_storage) TestSineEngine;
  compiled[2] = new(spectral_spiral_storage) SpectralSpiralEngine;
  compiled[3] = new(pulsar_storage) PulsarEngine;
  compiled[4] = new(glisson_storage) GlissonEngine;
  compiled[5] = new(reed_pipe_storage) ReedPipeEngine;
  compiled[6] = new(sideband_storage) SidebandEngine;
  compiled[7] = new(undertow_storage) UndertowEngine;
  compiled[8] = new(phase_flock_storage) PhaseFlockEngine;
  compiled[9] = new(scanned_storage) ScannedEngine;
  compiled[10] = new(question_mark_storage) QuestionMarkEngine;
  compiled[11] = new(triple_storage) TripleEngine;
  compiled[12] = new(raw_fm_storage) RawFmEngine;
  compiled[13] = new(buzz_storage) BuzzEngine;
  compiled[14] = new(struck_bell_storage) StruckBellEngine;
  compiled[15] = new(acid_storage) AcidEngine;
  for (size_t i = 0; i < kFirstStock; ++i) {
    // All engines share the same RAM space, as in PlaitsVoice::Init.
    allocator.Free();
    compiled[i]->Init(&allocator);
  }
}

static void BuildStock() {
  std::fill(&stock_arena[0], &stock_arena[kArenaSize], 0);
  std::fill(&stock_storage[0], &stock_storage[sizeof(stock_storage)], 0);
  StockEngines* e = new(stock_storage) StockEngines;
  Engine* stock[kNumStock] = {
    &e->virtual_analog_vcf_engine, &e->phase_distortion_engine,
    &e->six_op_engine, &e->wave_terrain_engine, &e->string_machine_engine,
    &e->chiptune_engine, &e->virtual_analog_engine, &e->waveshaping_engine,
    &e->fm_engine, &e->grain_engine, &e->additive_engine,
    &e->wavetable_engine, &e->chord_engine, &e->speech_engine,
    &e->swarm_engine, &e->noise_engine, &e->particle_engine,
    &e->string_engine, &e->modal_engine, &e->bass_drum_engine,
    &e->snare_drum_engine, &e->hi_hat_engine
  };
  stmlib::BufferAllocator stock_allocator(stock_arena, kArenaSize);
  for (size_t i = 0; i < kNumStock; ++i) {
    // PlaitsVoice::Init builds the 6-op engine once per model it serves.
    const int repeat = stock[i] == &e->six_op_engine ? 3 : 1;
    for (int j = 0; j < repeat; ++j) {
      stock_allocator.Free();
      stock[i]->Init(&stock_allocator);
    }
    compiled[kFirstStock + i] = stock[i];
  }
}

// Reads the region: from the directory, or the defaults when use_directory
// is 0, then builds its libraries. Returns the PLTS status.
uint32_t harness_setup(
    const uint8_t* begin,
    const uint8_t* end,
    uint32_t release,
    uint32_t use_directory) {
  DecodeSpeechBanks();
  for (int i = 0; i < kNumFmBanks; ++i) {
    for (int j = 0; j < kFmBankSize; ++j) {
      fm_banks[i * kFmBankSize + j].Unpack(
          fm_patches_table[i] + j * fm::Patch::SYX_SIZE);
    }
  }
  BuildProof();
  BuildStock();
  new(harness_blob_voice.engine) PlaitsBlobEngine;
  harness_slot = reinterpret_cast<uintptr_t>(blob_engine()->state());

  region.set_library_ram(harness_library_ram_size, kScratchSize);
  region.Read(
      begin, end, release,
      use_directory ? harness_directory : NULL, harness_num_entries,
      harness_defaults, harness_num_defaults,
      &plaits_blob_services, harness_digests, harness_num_digests);
  harness_identity = region.identity();
  harness_num_terrains = region.num_terrains();
  harness_synth_blob = reinterpret_cast<uintptr_t>(region.synth_blob());
  harness_lock = PlaitsRegion::Lock(
      region.ok(PLAITS_SECTION_PLAITS), region.ok(PLAITS_SECTION_WAVE));
  libraries.Init(
      region, &plaits_blob_services, harness_library_ram,
      harness_library_ram_size, harness_scratch, kScratchSize);
  return harness_identity.sections[PLAITS_SECTION_PLAITS].status;
}

// A voice switching to a compiled-in engine: Reset only, after the user data
// PlaitsVoice loads, or the 6-op bank of the model.
void harness_select_compiled(uint32_t engine, uint32_t model) {
  if (engine == kSixOpSlot) {
    static_cast<SixOpEngine*>(compiled[engine])->LoadUnpackedBank(
        &fm_banks[(model - 2) * kFmBankSize]);
  } else {
    compiled[engine]->LoadUserData(
        engine == kFirstStock + 3 ? harness_user_data : NULL);
  }
  compiled[engine]->Reset();
}

// The engines built afresh, as at power-on, then one selected: the reference
// for a rebuilt blob, which equals an engine's first selection.
void harness_rebuild_compiled(uint32_t engine, uint32_t model) {
  if (engine < kFirstStock) {
    BuildProof();
  } else {
    BuildStock();
  }
  harness_select_compiled(engine, model);
}

// A voice switching to a blob: bound, constructed in the zeroed slot and
// arena, then reset. Returns 1 when no checked blob serves the model.
uint32_t harness_select_blob(uint32_t model) {
  const uint8_t* blob = region.blob(model);
  if (!blob) {
    return 1;
  }
  stmlib::BufferAllocator allocator(harness_blob_voice.arena, kArenaSize);
  blob_engine()->Bind(blob, &plaits_blob_services, model, &libraries);
  blob_engine()->Init(&allocator);
  blob_engine()->Reset();
  return 0;
}

// Each render starts from the given generator state and returns the one it
// leaves, so the blob and compiled-in renders draw the same numbers.
uint32_t harness_render_compiled(uint32_t engine, uint32_t size,
    uint32_t seed) {
  bool already_enveloped = false;
  if (engine == kChiptuneSlot) {
    // As PlaitsVoice::Render sets it.
    static_cast<ChiptuneEngine*>(compiled[engine])->set_envelope_shape(
        harness_parameters.trigger & TRIGGER_UNPATCHED
            ? ChiptuneEngine::NO_ENVELOPE
            : 0.0f);
  }
  stmlib::Random::Seed(seed);
  compiled[engine]->Render(
      harness_parameters, harness_out, harness_aux, size, &already_enveloped);
  return stmlib::Random::state();
}

uint32_t harness_render_blob(uint32_t size, uint32_t seed) {
  bool already_enveloped = false;
  stmlib::Random::Seed(seed);
  blob_engine()->Render(
      harness_parameters, harness_out, harness_aux, size, &already_enveloped);
  return stmlib::Random::state();
}

}  // extern "C"
