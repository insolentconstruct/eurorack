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

#include "beads/blob/plaits_blob.h"

#include <cstring>

#include "stmlib/dsp/units.h"

#include "beads/profile.h"
#include "plaits/resources.h"

// The exports' sizes, frozen. Outside the namespace: stmlib's
// STATIC_ASSERT names its struct by line number.
STATIC_ASSERT(LUT_SINE_SIZE == 641, lut_sine_frozen);
STATIC_ASSERT(sizeof(stmlib::lut_pitch_ratio_high) == 257 * sizeof(float),
    lut_pitch_ratio_high_frozen);
STATIC_ASSERT(sizeof(stmlib::lut_pitch_ratio_low) == 257 * sizeof(float),
    lut_pitch_ratio_low_frozen);
STATIC_ASSERT(LUT_FOLD_SIZE == 516, lut_fold_frozen);
STATIC_ASSERT(LUT_FM_FREQUENCY_QUANTIZER_SIZE == 130,
    lut_fm_frequency_quantizer_frozen);
STATIC_ASSERT(LUT_4X_DOWNSAMPLER_FIR_SIZE == 4,
    lut_4x_downsampler_fir_frozen);
STATIC_ASSERT(LUT_WS_INVERSE_TAN_SIZE == 257, lut_ws_inverse_tan_frozen);
STATIC_ASSERT(LUT_WS_INVERSE_SIN_SIZE == 257, lut_ws_inverse_sin_frozen);
STATIC_ASSERT(LUT_WS_LINEAR_SIZE == 257, lut_ws_linear_frozen);
STATIC_ASSERT(LUT_WS_BUMP_SIZE == 257, lut_ws_bump_frozen);

namespace beads {

const PlaitsBlobServices plaits_blob_services
    __attribute__((section(".blob_services"))) = {
  kPlaitsBlobServicesMagic,
  kPlaitsBlobServicesVersion2,
  sizeof(PlaitsBlobServices),
  kNumPlaitsBlobExports,
  {
    plaits::lut_sine,
    stmlib::lut_pitch_ratio_high,
    stmlib::lut_pitch_ratio_low,
    reinterpret_cast<const void*>(kPlaitsBlobRandomStateAddress),
    plaits::lut_fold,
    plaits::lut_fm_frequency_quantizer,
    plaits::lut_4x_downsampler_fir,
    plaits::lut_ws_inverse_tan,
    plaits::lut_ws_inverse_sin,
    plaits::lut_ws_linear,
    plaits::lut_ws_bump
  }
};

const uint32_t kPlaitsBlobExportSizes[kNumPlaitsBlobExports] = {
  641 * sizeof(float),
  257 * sizeof(float),
  257 * sizeof(float),
  0,
  516 * sizeof(float),
  130 * sizeof(float),
  4 * sizeof(float),
  257 * sizeof(int16_t),
  257 * sizeof(int16_t),
  257 * sizeof(int16_t),
  257 * sizeof(int16_t)
};

const uint8_t kPlaitsBlobExportDigests[kNumPlaitsBlobExports][
    kPlaitsBlobDigestSize] = {
  {
    0x71, 0xfe, 0xf7, 0xc6, 0x62, 0xa7, 0x46, 0x45,
    0x5a, 0x27, 0x6f, 0xd5, 0x72, 0x58, 0xcb, 0xb3,
    0x03, 0x4e, 0xaf, 0x4d, 0x84, 0x92, 0x2d, 0x9c,
    0x14, 0x67, 0xc4, 0xae, 0x99, 0x62, 0xb1, 0xeb
  },
  {
    0xb5, 0x37, 0x83, 0xe5, 0xf0, 0x04, 0xcf, 0xea,
    0xc6, 0x7a, 0x88, 0x17, 0x4b, 0x45, 0x6f, 0xcb,
    0x81, 0x1e, 0xf5, 0x5b, 0xab, 0x50, 0x22, 0xc8,
    0x21, 0x9a, 0x0d, 0x6a, 0xf0, 0x68, 0x3b, 0x28
  },
  {
    0xdb, 0xc5, 0xc7, 0xf5, 0x0b, 0x41, 0x85, 0x56,
    0x12, 0x98, 0xa3, 0xb2, 0x1e, 0x17, 0x08, 0x9b,
    0x7f, 0x22, 0xc8, 0x86, 0x4c, 0x91, 0x49, 0xc2,
    0x14, 0xef, 0x8f, 0xb5, 0x4e, 0x31, 0x82, 0xca
  },
  { 0 },  // stmlib::Random's state: checked by address.
  {
    0x05, 0xbd, 0x34, 0xe5, 0x47, 0xbf, 0xdf, 0x17,
    0x93, 0xb8, 0x48, 0x9b, 0x1b, 0x82, 0x3d, 0x8d,
    0xdf, 0x20, 0xbb, 0x43, 0x31, 0x2f, 0xf8, 0xd1,
    0xea, 0x25, 0xcf, 0x48, 0x26, 0x3c, 0x63, 0xde
  },
  {
    0x8a, 0x69, 0xa3, 0x96, 0x01, 0x91, 0x59, 0xd6,
    0x12, 0xa1, 0x78, 0x5e, 0x9d, 0xca, 0x5d, 0xbc,
    0x09, 0xd5, 0xec, 0xdd, 0xfd, 0xe5, 0x83, 0xfa,
    0xa8, 0x28, 0x22, 0xb2, 0xd5, 0x7f, 0x61, 0xd8
  },
  {
    0x6b, 0x20, 0xc3, 0x4c, 0xaa, 0xf5, 0x50, 0xca,
    0xa2, 0xa9, 0xaf, 0x5b, 0xe3, 0xfe, 0x6f, 0xce,
    0xf9, 0xd9, 0x92, 0x62, 0xcf, 0xbd, 0xda, 0x5e,
    0xa6, 0xd7, 0x8a, 0xc4, 0xce, 0x6e, 0x56, 0x64
  },
  {
    0xc8, 0x88, 0xd5, 0xfe, 0xcb, 0x17, 0x2e, 0x6b,
    0x6f, 0x69, 0xce, 0x07, 0x58, 0xca, 0xfd, 0x52,
    0xd6, 0x95, 0xf0, 0x1a, 0x20, 0xa3, 0x86, 0xd2,
    0x68, 0xa8, 0x6f, 0x52, 0x9e, 0x6d, 0x1f, 0xad
  },
  {
    0x69, 0x0f, 0x21, 0x1e, 0xdb, 0x48, 0xdc, 0xcb,
    0x2f, 0x63, 0xc4, 0x98, 0x4d, 0x84, 0x6f, 0x02,
    0x54, 0x80, 0x68, 0x1f, 0xef, 0xd3, 0xcb, 0x81,
    0x8d, 0x05, 0xac, 0x5e, 0xd7, 0x72, 0x06, 0xf5
  },
  {
    0x15, 0xb7, 0x80, 0x2c, 0x6f, 0xdf, 0x39, 0x04,
    0xaa, 0xca, 0x0d, 0xe2, 0x0d, 0x80, 0x98, 0x23,
    0x78, 0x45, 0xe8, 0x9f, 0xe8, 0x1b, 0xf2, 0x2b,
    0x9c, 0x34, 0x33, 0x3d, 0xf6, 0x51, 0x1f, 0xfe
  },
  {
    0xd6, 0xa4, 0xae, 0xde, 0x50, 0x46, 0xfc, 0x0b,
    0x7e, 0xd7, 0x13, 0xb7, 0xb1, 0x7a, 0xa3, 0x9d,
    0x53, 0x27, 0x74, 0x20, 0x7b, 0xc4, 0xbe, 0x8f,
    0xb9, 0x91, 0xc0, 0x23, 0x0f, 0x09, 0x04, 0xbf
  }
};

static uint32_t Read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
      uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

static bool EntryInCode(
    uint32_t offset,
    uint32_t header_size,
    uint32_t got_offset) {
  return (offset & 1) && offset >= header_size && offset < got_offset;
}

static bool ServicesTable(const PlaitsBlobServices* services) {
  return services->magic == kPlaitsBlobServicesMagic &&
      services->version == kPlaitsBlobServicesVersion2 &&
      services->size == sizeof(PlaitsBlobServices) &&
      services->num_exports == kNumPlaitsBlobExports;
}

/* static */
PlaitsBlobStatus PlaitsBlob::CheckServices(
    const PlaitsBlobServices* services) {
  if (!ServicesTable(services)) {
    return PLAITS_BLOB_ERROR_SERVICES;
  }
  // A release that changed an export's contents would break every blob.
  for (size_t i = 0; i < kNumPlaitsBlobExports; ++i) {
    const uint8_t* data = static_cast<const uint8_t*>(services->exports[i]);
    if (!kPlaitsBlobExportSizes[i]) {
      if (reinterpret_cast<uintptr_t>(data) != kPlaitsBlobRandomStateAddress) {
        return PLAITS_BLOB_ERROR_SERVICES;
      }
      continue;
    }
    uint8_t digest[kPlaitsBlobDigestSize];
    Sha256(data, kPlaitsBlobExportSizes[i], digest);
    if (memcmp(digest, kPlaitsBlobExportDigests[i], kPlaitsBlobDigestSize)) {
      return PLAITS_BLOB_ERROR_SERVICES;
    }
  }
  return PLAITS_BLOB_OK;
}

// Either header version, widened to version 2.
static PlaitsBlobStatus ReadHeader(
    const uint8_t* blob,
    size_t room,
    PlaitsBlobInfo* info,
    PlaitsBlobHeader2* h) {
  memset(h, 0, sizeof(*h));
  PlaitsBlobHeader h1;
  memcpy(&h1, blob, sizeof(h1));
  if (h1.magic != kPlaitsBlobMagic) {
    return PLAITS_BLOB_ERROR_HEADER;
  }
  if (h1.abi != kPlaitsBlobAbi && h1.abi != kPlaitsBlobAbi2) {
    return PLAITS_BLOB_ERROR_ABI;
  }
  if (h1.abi == kPlaitsBlobAbi) {
    if (h1.header_size != sizeof(PlaitsBlobHeader)) {
      return PLAITS_BLOB_ERROR_HEADER;
    }
    h->magic = h1.magic;
    h->header_size = h1.header_size;
    h->abi = h1.abi;
    h->services_version = h1.services_version;
    h->kind = kPlaitsBlobKindModel;
    h->first_id = h1.model;
    h->num_ids = 1;
    h->size = h1.size;
    h->state_size = h1.state_size;
    h->arena_size = h1.arena_size;
    h->init_offset = h1.init_offset;
    h->reset_offset = h1.reset_offset;
    h->render_offset = h1.render_offset;
    h->got_offset = h1.got_offset;
    h->got_size = h1.got_size;
    h->flags = h1.flags;
  } else {
    if (h1.header_size != sizeof(PlaitsBlobHeader2) ||
        room < sizeof(PlaitsBlobHeader2)) {
      return PLAITS_BLOB_ERROR_HEADER;
    }
    memcpy(h, blob, sizeof(*h));
  }
  const bool library = h->kind == kPlaitsBlobKindLibrary;
  info->abi = h->abi;
  info->kind = h->kind;
  info->first_id = h->first_id;
  info->num_ids = h->num_ids;
  info->size = h->size;
  info->state_size = h->state_size;
  info->arena_size = h->arena_size;
  info->init_offset = h->init_offset;
  info->reset_offset = library ? 0 : h->reset_offset;
  info->render_offset = library ? 0 : h->render_offset;
  info->exports_offset = library ? h->reset_offset : 0;
  info->num_exports = library ? h->render_offset : 0;
  info->dependencies_offset = h->dependencies_offset;
  info->num_dependencies = h->num_dependencies;
  return PLAITS_BLOB_OK;
}

/* static */
PlaitsBlobStatus PlaitsBlob::Check(
    const uint8_t* blob,
    const uint8_t* begin,
    const uint8_t* end,
    const PlaitsBlobServices* services,
    const uint8_t (*digests)[kPlaitsBlobDigestSize],
    size_t num_digests,
    uint8_t* digest) {
  const uintptr_t address = reinterpret_cast<uintptr_t>(blob);
  const uintptr_t region_begin = reinterpret_cast<uintptr_t>(begin);
  const uintptr_t region_end = reinterpret_cast<uintptr_t>(end);
  if (address % kPlaitsBlobAlignment ||
      address < region_begin || address > region_end ||
      region_end - address < sizeof(PlaitsBlobHeader)) {
    return PLAITS_BLOB_ERROR_PLACEMENT;
  }
  const size_t room = region_end - address;

  PlaitsBlobInfo info;
  PlaitsBlobHeader2 h;
  PlaitsBlobStatus status = ReadHeader(blob, room, &info, &h);
  if (status != PLAITS_BLOB_OK) {
    return status;
  }
  if (h.size % 4 || h.size > room || h.size < h.header_size + 4) {
    return PLAITS_BLOB_ERROR_HEADER;
  }
  if (!ServicesTable(services) || !h.services_version ||
      h.services_version > services->version ||
      (h.abi == kPlaitsBlobAbi &&
          h.services_version != kPlaitsBlobServicesVersion)) {
    return PLAITS_BLOB_ERROR_SERVICES;
  }
  const bool library = h.kind == kPlaitsBlobKindLibrary;
  if (h.kind != kPlaitsBlobKindModel && !library) {
    return PLAITS_BLOB_ERROR_KIND;
  }
  // A library has one nonzero ID, and no flags or gains.
  if (!h.num_ids || h.first_id >= kNumPlaitsModelIds ||
      h.num_ids > kNumPlaitsModelIds - h.first_id ||
      (library && (h.num_ids != 1 || !h.first_id || h.flags ||
          Read32(blob + 72) || Read32(blob + 76))) ||
      h.reserved[0] || h.reserved[1] || h.reserved[2] || h.reserved[3]) {
    return PLAITS_BLOB_ERROR_HEADER;
  }
  const uint32_t crc_offset = h.size - 4;
  // Code, the GOT, a library's export table, the dependency list, the CRC.
  const uint32_t got_end = h.got_offset + h.got_size;
  if (h.got_offset % 4 || h.got_size % 4 || h.got_offset > crc_offset ||
      h.got_size > crc_offset - h.got_offset) {
    return PLAITS_BLOB_ERROR_LAYOUT;
  }
  uint32_t next = got_end;
  if (library) {
    // reset_offset and render_offset hold the export table and its count.
    const uint32_t table = h.reset_offset;
    const uint32_t count = h.render_offset;
    if (!count || count > kMaxPlaitsLibraryExports || table % 4 ||
        table < got_end || table > crc_offset ||
        count > (crc_offset - table) / 4 ||
        (h.init_offset &&
            !EntryInCode(h.init_offset, h.header_size, h.got_offset))) {
      return PLAITS_BLOB_ERROR_LAYOUT;
    }
    for (uint32_t i = 0; i < count; ++i) {
      const uint32_t offset = Read32(blob + table + 4 * i);
      if (offset < h.header_size || offset >= h.got_offset) {
        return PLAITS_BLOB_ERROR_LAYOUT;
      }
    }
    next = table + 4 * count;
  } else if (!EntryInCode(h.init_offset, h.header_size, h.got_offset) ||
      !EntryInCode(h.reset_offset, h.header_size, h.got_offset) ||
      !EntryInCode(h.render_offset, h.header_size, h.got_offset)) {
    return PLAITS_BLOB_ERROR_LAYOUT;
  }
  // The dependency list ends at the CRC; IDs are libraries' (nonzero).
  if (h.num_dependencies > kMaxPlaitsBlobDependencies ||
      (!h.num_dependencies && h.dependencies_offset) ||
      (h.num_dependencies && (h.dependencies_offset < next ||
          h.dependencies_offset % 4 ||
          h.dependencies_offset > crc_offset ||
          crc_offset - h.dependencies_offset !=
              h.num_dependencies * sizeof(PlaitsBlobDependency)))) {
    return PLAITS_BLOB_ERROR_LAYOUT;
  }
  for (uint32_t i = 0; i < h.num_dependencies; ++i) {
    const uint32_t id = Read32(
        blob + h.dependencies_offset + i * sizeof(PlaitsBlobDependency));
    if (!id || id >= kNumPlaitsModelIds) {
      return PLAITS_BLOB_ERROR_LIBRARY;
    }
  }
  const size_t max_state = library ? kPlaitsBlobMaxLibraryRamSize
      : (h.abi == kPlaitsBlobAbi
          ? kPlaitsBlobMaxStateSize : kPlaitsBlobMaxStateSize2);
  if (h.state_size > max_state || h.arena_size > kPlaitsBlobMaxArenaSize) {
    return PLAITS_BLOB_ERROR_RAM;
  }
  // The digest covers every byte: no CRC pass (55 cycles/B, bitwise).
  uint8_t own_digest[kPlaitsBlobDigestSize];
  if (!digest) {
    digest = own_digest;
  }
  Sha256(blob, h.size, digest);
  bool listed = false;
  for (size_t i = 0; i < num_digests && !listed; ++i) {
    listed = !memcmp(digest, digests[i], kPlaitsBlobDigestSize);
  }
  if (!listed) {
    return PLAITS_BLOB_ERROR_DIGEST;
  }

  // Every import must be an export of the blob's services version.
  const size_t num_exports = h.services_version == kPlaitsBlobServicesVersion
      ? kNumPlaitsBlobExportsV1 : kNumPlaitsBlobExports;
  for (uint32_t i = 0; i < h.got_size; i += 4) {
    const uint32_t value = Read32(blob + h.got_offset + i);
    bool exported = false;
    for (uint32_t j = 0; j < num_exports && !exported; ++j) {
      exported = value == reinterpret_cast<uintptr_t>(services->exports[j]);
    }
    if (!exported) {
      return PLAITS_BLOB_ERROR_IMPORTS;
    }
  }
  return PLAITS_BLOB_OK;
}

/* static */
void PlaitsBlob::Info(const uint8_t* blob, PlaitsBlobInfo* info) {
  PlaitsBlobHeader2 h;
  ReadHeader(blob, sizeof(PlaitsBlobHeader2), info, &h);
}

static const uint32_t sha256_k[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
  0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
  0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
  0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t RotateRight(uint32_t x, int n) {
  return (x >> n) | (x << (32 - n));
}

static void Sha256Block(const uint8_t* block, uint32_t* state) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = uint32_t(block[4 * i]) << 24 | uint32_t(block[4 * i + 1]) << 16 |
        uint32_t(block[4 * i + 2]) << 8 | uint32_t(block[4 * i + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const uint32_t s0 = RotateRight(w[i - 15], 7) ^
        RotateRight(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = RotateRight(w[i - 2], 17) ^
        RotateRight(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t v[8];
  memcpy(v, state, sizeof(v));
  for (int i = 0; i < 64; ++i) {
    const uint32_t s1 = RotateRight(v[4], 6) ^ RotateRight(v[4], 11) ^
        RotateRight(v[4], 25);
    const uint32_t choice = (v[4] & v[5]) ^ (~v[4] & v[6]);
    const uint32_t t1 = v[7] + s1 + choice + sha256_k[i] + w[i];
    const uint32_t s0 = RotateRight(v[0], 2) ^ RotateRight(v[0], 13) ^
        RotateRight(v[0], 22);
    const uint32_t majority = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
    for (int j = 7; j > 0; --j) {
      v[j] = v[j - 1];
    }
    v[4] += t1;
    v[0] = t1 + s0 + majority;
  }
  for (int i = 0; i < 8; ++i) {
    state[i] += v[i];
  }
}

/* static */
void PlaitsBlob::Sha256(const uint8_t* data, size_t size, uint8_t* digest) {
  uint32_t state[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
  };
  const size_t full = size & ~size_t(63);
  for (size_t i = 0; i < full; i += 64) {
    Sha256Block(data + i, state);
  }

  // The tail, the 0x80 marker and the bit length fill one or two blocks.
  uint8_t tail[128];
  const size_t rest = size - full;
  const size_t tail_size = rest < 56 ? 64 : 128;
  memset(tail, 0, sizeof(tail));
  memcpy(tail, data + full, rest);
  tail[rest] = 0x80;
  const uint64_t bits = uint64_t(size) * 8;
  for (int i = 0; i < 8; ++i) {
    tail[tail_size - 1 - i] = uint8_t(bits >> (8 * i));
  }
  for (size_t i = 0; i < tail_size; i += 64) {
    Sha256Block(tail + i, state);
  }
  for (int i = 0; i < 8; ++i) {
    digest[4 * i] = uint8_t(state[i] >> 24);
    digest[4 * i + 1] = uint8_t(state[i] >> 16);
    digest[4 * i + 2] = uint8_t(state[i] >> 8);
    digest[4 * i + 3] = uint8_t(state[i]);
  }
}

}  // namespace beads
