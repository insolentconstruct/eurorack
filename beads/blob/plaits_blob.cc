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

#include "beads/blob/plaits_blob.h"

#include <cstring>

#include "stmlib/dsp/units.h"

#include "beads/profile.h"
#include "plaits/resources.h"

namespace beads {

const PlaitsBlobServices plaits_blob_services
    __attribute__((section(".blob_services"))) = {
  kPlaitsBlobServicesMagic,
  kPlaitsBlobServicesVersion,
  sizeof(PlaitsBlobServices),
  kNumPlaitsBlobExports,
  {
    plaits::lut_sine,
    stmlib::lut_pitch_ratio_high,
    stmlib::lut_pitch_ratio_low
  }
};

const uint32_t kPlaitsBlobExportSizes[kNumPlaitsBlobExports] = {
  641 * sizeof(float),
  257 * sizeof(float),
  257 * sizeof(float)
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
  }
};

STATIC_ASSERT(LUT_SINE_SIZE == 641, lut_sine_frozen);
STATIC_ASSERT(sizeof(stmlib::lut_pitch_ratio_high) == 257 * sizeof(float),
    lut_pitch_ratio_high_frozen);
STATIC_ASSERT(sizeof(stmlib::lut_pitch_ratio_low) == 257 * sizeof(float),
    lut_pitch_ratio_low_frozen);

static uint32_t Read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
      uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

static bool EntryInCode(uint32_t offset, const PlaitsBlobHeader& h) {
  return (offset & 1) && offset >= h.header_size && offset < h.got_offset;
}

/* static */
PlaitsBlobStatus PlaitsBlob::CheckServices(
    const PlaitsBlobServices* services) {
  if (services->magic != kPlaitsBlobServicesMagic ||
      services->version != kPlaitsBlobServicesVersion ||
      services->size != sizeof(PlaitsBlobServices) ||
      services->num_exports != kNumPlaitsBlobExports) {
    return PLAITS_BLOB_ERROR_SERVICES;
  }
  // A release that changed an export's contents would break every blob.
  for (size_t i = 0; i < kNumPlaitsBlobExports; ++i) {
    uint8_t digest[kPlaitsBlobDigestSize];
    Sha256(
        static_cast<const uint8_t*>(services->exports[i]),
        kPlaitsBlobExportSizes[i],
        digest);
    if (memcmp(digest, kPlaitsBlobExportDigests[i], kPlaitsBlobDigestSize)) {
      return PLAITS_BLOB_ERROR_SERVICES;
    }
  }
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

  PlaitsBlobHeader h;
  memcpy(&h, blob, sizeof(h));
  if (h.magic != kPlaitsBlobMagic ||
      h.header_size != sizeof(PlaitsBlobHeader) ||
      h.size % 4 || h.size > room ||
      h.size < sizeof(PlaitsBlobHeader) + 4) {
    return PLAITS_BLOB_ERROR_HEADER;
  }
  if (h.abi != kPlaitsBlobAbi) {
    return PLAITS_BLOB_ERROR_ABI;
  }
  if (services->magic != kPlaitsBlobServicesMagic ||
      services->version != kPlaitsBlobServicesVersion ||
      services->size != sizeof(PlaitsBlobServices) ||
      services->num_exports != kNumPlaitsBlobExports ||
      h.services_version != services->version) {
    return PLAITS_BLOB_ERROR_SERVICES;
  }
  const uint32_t crc_offset = h.size - 4;
  // Entries lie between the header and the GOT, which ends before the CRC.
  if (h.got_offset % 4 || h.got_size % 4 || h.got_offset > crc_offset ||
      h.got_size > crc_offset - h.got_offset ||
      !EntryInCode(h.init_offset, h) ||
      !EntryInCode(h.reset_offset, h) ||
      !EntryInCode(h.render_offset, h)) {
    return PLAITS_BLOB_ERROR_LAYOUT;
  }
  if (h.state_size > kPlaitsBlobMaxStateSize ||
      h.arena_size > kPlaitsBlobMaxArenaSize) {
    return PLAITS_BLOB_ERROR_RAM;
  }
  if (Profile::Crc32(blob, crc_offset) != Read32(blob + crc_offset)) {
    return PLAITS_BLOB_ERROR_CRC;
  }

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

  // Every import must be one of the services' exports.
  for (uint32_t i = 0; i < h.got_size; i += 4) {
    const uint32_t value = Read32(blob + h.got_offset + i);
    bool exported = false;
    for (uint32_t j = 0; j < kNumPlaitsBlobExports && !exported; ++j) {
      exported = value == reinterpret_cast<uintptr_t>(services->exports[j]);
    }
    if (!exported) {
      return PLAITS_BLOB_ERROR_IMPORTS;
    }
  }
  return PLAITS_BLOB_OK;
}

// The table alone: bounds, the release, the selection, then the entries'
// layout. Bytes past the table's size are never read.
static PlaitsBlobStatus ReadTable(
    const uint8_t* begin,
    const uint8_t* end,
    uint32_t release,
    uint32_t expected_crc,
    const PlaitsBlobServices* services,
    PlaitsBlobIdentity* identity) {
  if (identity->services_status != PLAITS_BLOB_OK) {
    return static_cast<PlaitsBlobStatus>(identity->services_status);
  }
  const uintptr_t region_begin = reinterpret_cast<uintptr_t>(begin);
  const uintptr_t region_end = reinterpret_cast<uintptr_t>(end);
  if (region_begin % kPlaitsBlobAlignment || region_end < region_begin ||
      region_end - region_begin < sizeof(PlaitsBlobRegionHeader)) {
    return PLAITS_BLOB_ERROR_PLACEMENT;
  }
  const size_t room = region_end - region_begin;
  PlaitsBlobRegionHeader t;
  memcpy(&t, begin, sizeof(t));
  if (t.magic != kPlaitsBlobRegionMagic || !expected_crc) {
    return PLAITS_BLOB_ERROR_NO_REGION;
  }
  if (t.services_version != services->version) {
    return PLAITS_BLOB_ERROR_SERVICES;
  }
  if (t.release != release) {
    return PLAITS_BLOB_ERROR_RELEASE;
  }
  if (t.version != kPlaitsBlobRegionVersion ||
      t.num_blobs > kMaxPlaitsBlobs || t.size % 4 || t.size > room ||
      t.size < sizeof(t) + t.num_blobs * sizeof(PlaitsBlobRegionEntry) + 4) {
    return PLAITS_BLOB_ERROR_TABLE;
  }
  // An interrupted or partial update fails here, before any blob is read.
  const uint32_t crc = Profile::Crc32(begin, t.size - 4);
  if (crc != Read32(begin + t.size - 4)) {
    return PLAITS_BLOB_ERROR_CRC;
  }
  identity->region_crc = crc;
  if (crc != expected_crc) {
    return PLAITS_BLOB_ERROR_SELECTION;
  }

  // Ascending, aligned, non-overlapping, inside the region, one per model.
  const uint32_t crc_offset = t.size - 4;
  uint32_t next = sizeof(t) + t.num_blobs * sizeof(PlaitsBlobRegionEntry);
  for (uint32_t i = 0; i < t.num_blobs; ++i) {
    PlaitsBlobRegionEntry e;
    memcpy(&e, begin + sizeof(t) + i * sizeof(e), sizeof(e));
    if (e.offset % kPlaitsBlobAlignment || e.offset < next ||
        e.offset > crc_offset || e.size > crc_offset - e.offset) {
      return PLAITS_BLOB_ERROR_TABLE;
    }
    next = e.offset + e.size;
    for (uint32_t j = 0; j < i; ++j) {
      PlaitsBlobRegionEntry other;
      memcpy(&other, begin + sizeof(t) + j * sizeof(other), sizeof(other));
      if (other.model == e.model) {
        return PLAITS_BLOB_ERROR_TABLE;
      }
    }
  }
  identity->num_blobs = t.num_blobs;
  return PLAITS_BLOB_OK;
}

/* static */
PlaitsBlobStatus PlaitsBlob::ReadRegion(
    const uint8_t* begin,
    const uint8_t* end,
    uint32_t release,
    uint32_t expected_crc,
    const PlaitsBlobServices* services,
    const uint8_t (*digests)[kPlaitsBlobDigestSize],
    size_t num_digests,
    PlaitsBlobIdentity* identity) {
  memset(identity, 0, sizeof(*identity));
  identity->release = release;
  identity->services_version = services->version;
  identity->services_status = CheckServices(services);
  identity->region_status = ReadTable(
      begin, end, release, expected_crc, services, identity);
  if (identity->region_status != PLAITS_BLOB_OK) {
    return static_cast<PlaitsBlobStatus>(identity->region_status);
  }

  // Any refused blob refuses the region: it is one configuration.
  for (uint32_t i = 0; i < identity->num_blobs; ++i) {
    PlaitsBlobRegionEntry e;
    memcpy(&e, begin + sizeof(PlaitsBlobRegionHeader) + i * sizeof(e),
        sizeof(e));
    const uint8_t* blob = begin + e.offset;
    PlaitsBlobStatus status = Check(
        blob, blob, blob + e.size, services, digests, num_digests,
        identity->blobs[i].digest);
    if (status == PLAITS_BLOB_OK) {
      PlaitsBlobHeader h;
      memcpy(&h, blob, sizeof(h));
      if (h.size != e.size || Read32(blob + e.size - 4) != e.crc) {
        status = PLAITS_BLOB_ERROR_TABLE;
      } else if (h.model != e.model) {
        status = PLAITS_BLOB_ERROR_MODEL;
      }
    }
    identity->blobs[i].model = e.model;
    identity->blobs[i].address = reinterpret_cast<uintptr_t>(blob);
    identity->blobs[i].size = e.size;
    identity->blobs[i].status = status;
    if (status != PLAITS_BLOB_OK &&
        identity->region_status == PLAITS_BLOB_OK) {
      identity->region_status = status;
    }
  }
  return static_cast<PlaitsBlobStatus>(identity->region_status);
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
