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
// Emulator harness: the region reader, the blob adapter and the same engines
// compiled in, called by render_compare.py under Unicorn's Cortex-M7.

#include <new>

#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/phase_weave_engine.h"
#include "beads/blob/plaits_blob.h"
#include "beads/blob/plaits_blob_engine.h"
#include "beads/test/blob_gate/test_sine.h"

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

const size_t kNumCompiled = 2;
const size_t kArenaSize = 1024;

// Engines are placed by harness_setup: no static constructors run here.
static uint8_t phase_weave_storage[sizeof(PhaseWeaveEngine)]
    __attribute__((aligned(8)));
static uint8_t sine_storage[sizeof(TestSineEngine)]
    __attribute__((aligned(8)));
static uint8_t blob_storage[kMaxPlaitsBlobs][sizeof(PlaitsBlobEngine)]
    __attribute__((aligned(8)));
static Engine* engines[kNumCompiled + kMaxPlaitsBlobs];
static uint8_t arena[kArenaSize];

extern "C" {

uint8_t harness_digests[kMaxPlaitsBlobs][kPlaitsBlobDigestSize];
PlaitsBlobIdentity harness_identity;
float harness_out[kMaxBlockSize];
float harness_aux[kMaxBlockSize];
EngineParameters harness_parameters;

uint32_t harness_setup(
    const uint8_t* begin,
    const uint8_t* end,
    uint32_t release,
    uint32_t expected_crc) {
  stmlib::BufferAllocator allocator(arena, kArenaSize);
  engines[0] = new(phase_weave_storage) PhaseWeaveEngine;
  engines[1] = new(sine_storage) TestSineEngine;
  engines[0]->Init(&allocator);
  engines[1]->Init(&allocator);
  const PlaitsBlobStatus status = PlaitsBlob::ReadRegion(
      begin, end, release, expected_crc, &plaits_blob_services,
      harness_digests, kMaxPlaitsBlobs, &harness_identity);
  for (uint32_t i = 0; status == PLAITS_BLOB_OK &&
      i < harness_identity.num_blobs; ++i) {
    PlaitsBlobEngine* e = new(blob_storage[i]) PlaitsBlobEngine;
    e->Bind(reinterpret_cast<const uint8_t*>(
        harness_identity.blobs[i].address), &plaits_blob_services);
    allocator.Init(arena, kArenaSize);
    e->Init(&allocator);
    engines[kNumCompiled + i] = e;
  }
  return status;
}

void harness_reset(uint32_t engine) {
  engines[engine]->Reset();
}

void harness_render(uint32_t engine, uint32_t size) {
  bool already_enveloped = false;
  engines[engine]->Render(
      harness_parameters, harness_out, harness_aux, size, &already_enveloped);
}

}  // extern "C"
