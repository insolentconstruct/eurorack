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
// Precompiled Plaits models ("blobs"): the fixed binary interface and checks.

#ifndef BEADS_BLOB_PLAITS_BLOB_H_
#define BEADS_BLOB_PLAITS_BLOB_H_

#include "stmlib/stmlib.h"

#include "plaits/dsp/engine/engine.h"

namespace beads {

const uint32_t kPlaitsBlobMagic = 0x424d5a44;  // DZMB
const uint32_t kPlaitsBlobAbi = 1;
const uint32_t kPlaitsBlobAbi2 = 2;
const uint32_t kPlaitsBlobServicesMagic = 0x56535a44;  // DZSV
const uint32_t kPlaitsBlobServicesVersion = 1;
const uint32_t kPlaitsBlobServicesVersion2 = 2;
const uint32_t kPlaitsBlobServicesAddress = 0x08021400;
// stmlib::Random's state, shared by the app and every blob that draws.
const uint32_t kPlaitsBlobRandomStateAddress = 0x20000000;

// Blobs start on a cache line; state (8-byte aligned) and arena come from the
// voice's RAM. Version 2's slot holds the largest stock state, 6-op FM.
const size_t kPlaitsBlobAlignment = 32;
const size_t kPlaitsBlobStateAlignment = 8;
const size_t kPlaitsBlobMaxStateSize = 256;
const size_t kPlaitsBlobMaxStateSize2 = 4352;
const size_t kPlaitsBlobMaxArenaSize = 16384;
const size_t kPlaitsBlobDigestSize = 32;
const uint32_t kPlaitsBlobKindModel = 1;
const uint32_t kPlaitsBlobKindLibrary = 2;
const uint32_t kNumPlaitsModelIds = 256;

enum PlaitsBlobFlags {
  PLAITS_BLOB_FLAG_ENVELOPED = 1
};

// Version 1: sixteen little-endian words, code and constants, the imports
// (GOT), then a CRC-32 of all that. Entry offsets carry the Thumb bit.
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

// Version 2: 24 words. A model serves num_ids consecutive IDs from first_id;
// libraries and dependencies arrive with phase 2 and are refused for now.
struct PlaitsBlobHeader2 {
  uint32_t magic;
  uint32_t header_size;
  uint32_t abi;
  uint32_t services_version;
  uint32_t kind;
  uint32_t first_id;
  uint32_t num_ids;
  uint32_t size;
  uint32_t state_size;
  uint32_t arena_size;
  uint32_t init_offset;
  uint32_t reset_offset;
  uint32_t render_offset;
  uint32_t got_offset;
  uint32_t got_size;
  uint32_t dependencies_offset;
  uint32_t num_dependencies;
  uint32_t flags;
  float out_gain;
  float aux_gain;
  uint32_t reserved[4];
};

// Frozen: address, size, alignment and contents never change (services_v2.ld,
// CheckServices). Version 1 blobs may import only the first three.
enum PlaitsBlobExport {
  PLAITS_BLOB_EXPORT_LUT_SINE,  // plaits::lut_sine, 641 floats
  PLAITS_BLOB_EXPORT_LUT_PITCH_RATIO_HIGH,  // stmlib, 257 floats
  PLAITS_BLOB_EXPORT_LUT_PITCH_RATIO_LOW,  // stmlib, 257 floats
  PLAITS_BLOB_EXPORT_RANDOM_STATE,  // stmlib::Random::rng_state_, in RAM
  PLAITS_BLOB_EXPORT_LUT_FOLD,
  PLAITS_BLOB_EXPORT_LUT_FM_FREQUENCY_QUANTIZER,
  PLAITS_BLOB_EXPORT_LUT_4X_DOWNSAMPLER_FIR,
  PLAITS_BLOB_EXPORT_LUT_WS_INVERSE_TAN,
  PLAITS_BLOB_EXPORT_LUT_WS_INVERSE_SIN,
  PLAITS_BLOB_EXPORT_LUT_WS_LINEAR,
  PLAITS_BLOB_EXPORT_LUT_WS_BUMP,
  PLAITS_BLOB_EXPORT_LAST
};

const size_t kNumPlaitsBlobExports = PLAITS_BLOB_EXPORT_LAST;
const size_t kNumPlaitsBlobExportsV1 = PLAITS_BLOB_EXPORT_RANDOM_STATE;
// Size 0: a RAM export, checked by address, not contents.
extern const uint32_t kPlaitsBlobExportSizes[kNumPlaitsBlobExports];
extern const uint8_t kPlaitsBlobExportDigests[kNumPlaitsBlobExports][
    kPlaitsBlobDigestSize];

struct PlaitsBlobServices {
  uint32_t magic;
  uint32_t version;
  uint32_t size;
  uint32_t num_exports;
  const void* exports[kNumPlaitsBlobExports];
};

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
  PLAITS_BLOB_ERROR_NO_REGION,
  PLAITS_BLOB_ERROR_RELEASE,
  PLAITS_BLOB_ERROR_SELECTION,
  PLAITS_BLOB_ERROR_TABLE,
  PLAITS_BLOB_ERROR_MODEL,
  PLAITS_BLOB_ERROR_KIND,
  PLAITS_BLOB_ERROR_LIBRARY,
  PLAITS_BLOB_ERROR_CONTENT,
  PLAITS_BLOB_ERROR_DIRECTORY,
  PLAITS_BLOB_ERROR_ECC,
  PLAITS_BLOB_ERROR_LAST
};

// Version 1 entries. Init constructs the blob's state object in place and
// returns it; reset and render take that pointer.
typedef void* (*PlaitsBlobInitFn)(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size);
typedef void (*PlaitsBlobResetFn)(
    void* object,
    const PlaitsBlobServices* services);
typedef void (*PlaitsBlobRenderFn)(
    void* object,
    const PlaitsBlobServices* services,
    const plaits::EngineParameters* parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped);

// Version 2 entries: init also gets one context per dependency; reset gets
// the selected ID minus the first.
typedef void* (*PlaitsBlobInitFn2)(
    void* state,
    const PlaitsBlobServices* services,
    uint8_t* arena,
    size_t arena_size,
    void* const* libraries);
typedef void (*PlaitsBlobResetFn2)(
    void* object,
    const PlaitsBlobServices* services,
    uint32_t argument);

extern const PlaitsBlobServices plaits_blob_services;

// What the loader needs from either header version, once Check passed.
struct PlaitsBlobInfo {
  uint32_t abi;
  uint32_t first_id;
  uint32_t num_ids;
  uint32_t size;
  uint32_t state_size;
  uint32_t arena_size;
  uint32_t init_offset;
  uint32_t reset_offset;
  uint32_t render_offset;
};

class PlaitsBlob {
 public:
  // Checks the services table and its frozen exports, once at boot.
  static PlaitsBlobStatus CheckServices(const PlaitsBlobServices* services);

  // Checks a blob at blob, inside [begin, end), before any of its code runs;
  // digest receives its SHA-256 (or is NULL). Version 1 and 2 headers.
  static PlaitsBlobStatus Check(
      const uint8_t* blob,
      const uint8_t* begin,
      const uint8_t* end,
      const PlaitsBlobServices* services,
      const uint8_t (*digests)[kPlaitsBlobDigestSize],
      size_t num_digests,
      uint8_t* digest);

  // The fields of a blob that passed Check.
  static void Info(const uint8_t* blob, PlaitsBlobInfo* info);

  static void Sha256(
      const uint8_t* data,
      size_t size,
      uint8_t* digest);
};

STATIC_ASSERT(sizeof(PlaitsBlobHeader) == 64, plaits_blob_header);
STATIC_ASSERT(sizeof(PlaitsBlobHeader2) == 96, plaits_blob_header_2);
// On the target the table fits the 64 bytes before the first export.
STATIC_ASSERT(16 + 4 * kNumPlaitsBlobExports <= 64, plaits_blob_services);
STATIC_ASSERT(sizeof(plaits::EngineParameters) == 24, plaits_blob_abi);

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_BLOB_H_
