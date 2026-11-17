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
// Precompiled Plaits models ("blobs"): the fixed binary interface and checks.

#ifndef BEADS_BLOB_PLAITS_BLOB_H_
#define BEADS_BLOB_PLAITS_BLOB_H_

#include "stmlib/stmlib.h"

#include "plaits/dsp/engine/engine.h"

namespace beads {

const uint32_t kPlaitsBlobMagic = 0x424d5a44;  // DZMB
const uint32_t kPlaitsBlobAbi = 1;
const uint32_t kPlaitsBlobServicesMagic = 0x56535a44;  // DZSV
const uint32_t kPlaitsBlobServicesVersion = 1;
const uint32_t kPlaitsBlobServicesAddress = 0x08021400;

// Blobs start on a cache line; state and arena come from the voice's RAM.
const size_t kPlaitsBlobAlignment = 32;
const size_t kPlaitsBlobMaxStateSize = 256;
const size_t kPlaitsBlobMaxArenaSize = 16384;
const size_t kPlaitsBlobDigestSize = 32;

enum PlaitsBlobFlags {
  PLAITS_BLOB_FLAG_ENVELOPED = 1
};

// Sixteen little-endian words, code and constants, the imports (GOT), then
// the profile's CRC-32 of all that. Entry offsets carry the Thumb bit.
struct PlaitsBlobHeader {
  uint32_t magic;
  uint32_t header_size;
  uint32_t abi;
  uint32_t services_version;
  uint32_t model;
  uint32_t size;
  uint32_t state_size;
  uint32_t arena_size;
  uint32_t init_offset;
  uint32_t reset_offset;
  uint32_t render_offset;
  uint32_t got_offset;
  uint32_t got_size;
  uint32_t flags;
  float out_gain;
  float aux_gain;
};

// Version 1 exports Plaits' sine and stmlib's pitch ratio tables, at fixed
// addresses after this header (beads/blob/services_v1.ld).
const size_t kNumPlaitsBlobExports = 3;

struct PlaitsBlobServices {
  uint32_t magic;
  uint32_t version;
  uint32_t size;
  uint32_t num_exports;
  const void* exports[kNumPlaitsBlobExports];
};

STATIC_ASSERT(sizeof(PlaitsBlobHeader) == 64, plaits_blob_header);
STATIC_ASSERT(sizeof(plaits::EngineParameters) == 24, plaits_blob_abi);

enum PlaitsBlobStatus {
  PLAITS_BLOB_OK,
  PLAITS_BLOB_ERROR_PLACEMENT,
  PLAITS_BLOB_ERROR_HEADER,
  PLAITS_BLOB_ERROR_ABI,
  PLAITS_BLOB_ERROR_SERVICES,
  PLAITS_BLOB_ERROR_LAYOUT,
  PLAITS_BLOB_ERROR_RAM,
  PLAITS_BLOB_ERROR_CRC,
  PLAITS_BLOB_ERROR_DIGEST,
  PLAITS_BLOB_ERROR_IMPORTS,
  PLAITS_BLOB_ERROR_LAST
};

typedef void (*PlaitsBlobInitFn)(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size);
typedef void (*PlaitsBlobResetFn)(
    void* state,
    const PlaitsBlobServices* services);
typedef void (*PlaitsBlobRenderFn)(
    void* state,
    const PlaitsBlobServices* services,
    const plaits::EngineParameters* parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped);

extern const PlaitsBlobServices plaits_blob_services;

class PlaitsBlob {
 public:
  // Checks a blob at blob, inside [begin, end), before any of its code runs.
  static PlaitsBlobStatus Check(
      const uint8_t* blob,
      const uint8_t* begin,
      const uint8_t* end,
      const PlaitsBlobServices* services,
      const uint8_t (*digests)[kPlaitsBlobDigestSize],
      size_t num_digests);

  static void Sha256(
      const uint8_t* data,
      size_t size,
      uint8_t* digest);
};

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_BLOB_H_
