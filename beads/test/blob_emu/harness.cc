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
// Emulator harness: the region reader, a blob voice (one slot and arena, as
// PlaitsVoice) and the same engines compiled in, sharing their own arena as
// PlaitsVoice's do; called by render_compare.py under Unicorn's Cortex-M7.

#include <new>

#include "stmlib/utils/buffer_allocator.h"
#include "stmlib/utils/random.h"

#include "beads/blob/models/phase_weave_engine.h"
#include "beads/blob/models/spectral_spiral_engine.h"
#include "beads/blob/plaits_blob.h"
#include "beads/blob/plaits_blob_engine.h"
#include "beads/blob/plaits_region.h"
#include "beads/test/blob_gate/test_sine.h"
#include "plaits/dsp/engine/particle_engine.h"

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

const size_t kNumCompiled = 4;
const size_t kArenaSize = 16384;
const size_t kCanaryWords = 16;
const size_t kMaxDigests = 8;

// Engines are placed by harness_setup: no static constructors run here.
static uint8_t phase_weave_storage[sizeof(PhaseWeaveEngine)]
    __attribute__((aligned(8)));
static uint8_t sine_storage[sizeof(TestSineEngine)]
    __attribute__((aligned(8)));
static uint8_t particle_storage[sizeof(ParticleEngine)]
    __attribute__((aligned(8)));
static uint8_t spectral_spiral_storage[sizeof(SpectralSpiralEngine)]
    __attribute__((aligned(8)));
static Engine* compiled[kNumCompiled];
static uint8_t compiled_arena[kArenaSize] __attribute__((aligned(8)));
static PlaitsRegion region;

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
uint8_t harness_digests[kMaxDigests][kPlaitsBlobDigestSize];
uint32_t harness_num_digests;
PlaitsDirectoryEntry harness_directory[PLAITS_SECTION_LAST];
uint32_t harness_num_entries;
PlaitsDefaultSection harness_defaults[PLAITS_SECTION_LAST];
uint32_t harness_num_defaults;
PlaitsRegionIdentity harness_identity;
uint32_t harness_lock;
uintptr_t harness_slot;
float harness_out[kMaxBlockSize];
float harness_aux[kMaxBlockSize];
EngineParameters harness_parameters;

static PlaitsBlobEngine* blob_engine() {
  return reinterpret_cast<PlaitsBlobEngine*>(harness_blob_voice.engine);
}

// Reads the region: from the directory, or the defaults when use_directory
// is 0. Returns the PLTS status.
uint32_t harness_setup(
    const uint8_t* begin,
    const uint8_t* end,
    uint32_t release,
    uint32_t use_directory) {
  stmlib::BufferAllocator allocator(compiled_arena, kArenaSize);
  compiled[0] = new(phase_weave_storage) PhaseWeaveEngine;
  compiled[1] = new(sine_storage) TestSineEngine;
  compiled[2] = new(particle_storage) ParticleEngine;
  compiled[3] = new(spectral_spiral_storage) SpectralSpiralEngine;
  for (size_t i = 0; i < kNumCompiled; ++i) {
    // All engines share the same RAM space, as in PlaitsVoice::Init.
    allocator.Free();
    compiled[i]->Init(&allocator);
  }
  new(harness_blob_voice.engine) PlaitsBlobEngine;
  harness_slot = reinterpret_cast<uintptr_t>(blob_engine()->state());

  region.Read(
      begin, end, release,
      use_directory ? harness_directory : NULL, harness_num_entries,
      harness_defaults, harness_num_defaults,
      &plaits_blob_services, harness_digests, harness_num_digests);
  harness_identity = region.identity();
  harness_lock = PlaitsRegion::Lock(
      region.ok(PLAITS_SECTION_PLAITS), region.ok(PLAITS_SECTION_WAVE));
  return harness_identity.sections[PLAITS_SECTION_PLAITS].status;
}

// A voice switching to a compiled-in engine: Reset only.
void harness_select_compiled(uint32_t engine) {
  compiled[engine]->Reset();
}

// A voice switching to a blob: bound, constructed in the zeroed slot and
// arena, then reset. Returns 1 when no checked blob serves the model.
uint32_t harness_select_blob(uint32_t model) {
  const uint8_t* blob = region.blob(model);
  if (!blob) {
    return 1;
  }
  stmlib::BufferAllocator allocator(harness_blob_voice.arena, kArenaSize);
  blob_engine()->Bind(blob, &plaits_blob_services, model);
  blob_engine()->Init(&allocator);
  blob_engine()->Reset();
  return 0;
}

// Each render starts from the given generator state and returns the one it
// leaves, so the blob and compiled-in renders draw the same numbers.
uint32_t harness_render_compiled(uint32_t engine, uint32_t size,
    uint32_t seed) {
  bool already_enveloped = false;
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
