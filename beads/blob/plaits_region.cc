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
// The region after the app: WAVE, TERR and PLTS sections.

#include "beads/blob/plaits_region.h"

#include <cstring>

#include "beads/profile.h"

namespace beads {

static uint32_t Read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
      uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

static int SectionIndex(uint32_t kind) {
  switch (kind) {
    case kPlaitsSectionWave:
      return PLAITS_SECTION_WAVE;
    case kPlaitsSectionTerrain:
      return PLAITS_SECTION_TERRAIN;
    case kPlaitsSectionPlaits:
      return PLAITS_SECTION_PLAITS;
    default:
      return -1;
  }
}

static const uint32_t kSectionKinds[PLAITS_SECTION_LAST] = {
  kPlaitsSectionWave, kPlaitsSectionTerrain, kPlaitsSectionPlaits
};

// The smallest section: header, then the CRC.
const uint32_t kMinSectionSize = sizeof(PlaitsSectionHeader) + 4;

// Rows repeat their first four samples at the end, and the guard row repeats
// the last wave (resources/wavetables.py).
static bool WaveGuardsOk(const uint8_t* rows) {
  const size_t row_bytes = kWaveRowSize * 2;
  for (uint32_t i = 0; i < kNumWaveRows; ++i) {
    const uint8_t* row = rows + i * row_bytes;
    if (memcmp(row, row + (kWaveRowSize - 4) * 2, 8)) {
      return false;
    }
  }
  return !memcmp(
      rows + (kNumWaveRows - 1) * row_bytes,
      rows + (kNumWaveRows - 2) * row_bytes,
      row_bytes);
}

void PlaitsRegion::Read(
    const uint8_t* begin,
    const uint8_t* end,
    uint32_t release,
    const PlaitsDirectoryEntry* directory,
    size_t num_entries,
    const PlaitsDefaultSection* defaults,
    size_t num_defaults,
    const PlaitsBlobServices* services,
    const uint8_t (*digests)[kPlaitsBlobDigestSize],
    size_t num_digests) {
  memset(&identity_, 0, sizeof(identity_));
  memset(models_, 0, sizeof(models_));
  blobs_ = NULL;
  for (int i = 0; i < PLAITS_SECTION_LAST; ++i) {
    sections_[i] = NULL;
    identity_.sections[i].status = PLAITS_BLOB_ERROR_NO_REGION;
  }
  identity_.release = release;
  identity_.services_status = PlaitsBlob::CheckServices(services);
  identity_.directory_status = directory
      ? PLAITS_BLOB_OK : PLAITS_BLOB_ERROR_NO_REGION;

  const uintptr_t region_begin = reinterpret_cast<uintptr_t>(begin);
  const uintptr_t region_end = reinterpret_cast<uintptr_t>(end);
  const size_t count = directory ? num_entries : num_defaults;
  bool layout_ok = region_begin % kPlaitsBlobAlignment == 0 &&
      region_end >= region_begin && count <= PLAITS_SECTION_LAST;
  const size_t room = layout_ok ? region_end - region_begin : 0;

  // Every entry inside, aligned, in order and apart, before any is read.
  uint32_t next = 0;
  int previous = -1;
  for (size_t i = 0; i < count && layout_ok; ++i) {
    const uint32_t kind = directory ? directory[i].kind : defaults[i].kind;
    const uint32_t offset = directory
        ? directory[i].offset : defaults[i].offset;
    const uint32_t size = directory ? directory[i].size : defaults[i].size;
    const int index = SectionIndex(kind);
    layout_ok = index >= 0 && index > previous &&
        offset % kPlaitsBlobAlignment == 0 &&
        offset >= next && offset <= room && size <= room - offset &&
        size >= kMinSectionSize;
    previous = index;
    next = offset + size;
  }
  if (!layout_ok) {
    identity_.directory_status = PLAITS_BLOB_ERROR_DIRECTORY;
    for (int i = 0; i < PLAITS_SECTION_LAST; ++i) {
      identity_.sections[i].status = PLAITS_BLOB_ERROR_DIRECTORY;
    }
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    const uint32_t kind = directory ? directory[i].kind : defaults[i].kind;
    const uint32_t offset = directory
        ? directory[i].offset : defaults[i].offset;
    const uint32_t size = directory ? directory[i].size : defaults[i].size;
    const PlaitsSection section = PlaitsSection(SectionIndex(kind));
    const uint8_t* data = begin + offset;
    identity_.sections[section].address = uint32_t(
        reinterpret_cast<uintptr_t>(data));
    identity_.sections[section].size = size;
    if (flash_check_) {
      flash_check_();
    }
    PlaitsBlobStatus status = ReadSection(
        section, data, size, release,
        directory ? directory[i].crc : 0,
        directory ? NULL : defaults[i].digest,
        services, digests, num_digests);
    // A double ECC error read as garbage: this section alone is damaged.
    if (flash_check_ && !flash_check_()) {
      status = PLAITS_BLOB_ERROR_ECC;
      if (section == PLAITS_SECTION_PLAITS) {
        memset(models_, 0, sizeof(models_));
      }
    }
    identity_.sections[section].status = status;
    if (status == PLAITS_BLOB_OK) {
      sections_[section] = data;
    }
  }
}

PlaitsBlobStatus PlaitsRegion::ReadSection(
    PlaitsSection section,
    const uint8_t* data,
    uint32_t size,
    uint32_t release,
    uint32_t expected_crc,
    const uint8_t* expected_digest,
    const PlaitsBlobServices* services,
    const uint8_t (*digests)[kPlaitsBlobDigestSize],
    size_t num_digests) {
  PlaitsSectionHeader h;
  memcpy(&h, data, sizeof(h));
  // Erased flash: nothing was written here.
  if (h.magic == 0xffffffff) {
    return PLAITS_BLOB_ERROR_NO_REGION;
  }
  if (h.magic != kPlaitsSectionMagic || h.kind != kSectionKinds[section] ||
      h.version != kPlaitsSectionVersion || h.reserved[0] || h.reserved[1]) {
    return PLAITS_BLOB_ERROR_HEADER;
  }
  if (h.release != release) {
    return PLAITS_BLOB_ERROR_RELEASE;
  }
  if (h.size != size || size % 4) {
    return PLAITS_BLOB_ERROR_HEADER;
  }
  // An interrupted install fails here; bytes past size are never read.
  const uint32_t crc = Profile::Crc32(data, size - 4);
  if (crc != Read32(data + size - 4)) {
    return PLAITS_BLOB_ERROR_CRC;
  }
  identity_.sections[section].crc = crc;
  if (expected_digest) {
    uint8_t digest[kPlaitsBlobDigestSize];
    PlaitsBlob::Sha256(data, size, digest);
    if (memcmp(digest, expected_digest, kPlaitsBlobDigestSize)) {
      return PLAITS_BLOB_ERROR_DIGEST;
    }
  } else if (crc != expected_crc) {
    return PLAITS_BLOB_ERROR_SELECTION;
  }

  const uint32_t payload = size - kMinSectionSize;
  switch (section) {
    case PLAITS_SECTION_WAVE:
      if (h.count != kNumWaveRows ||
          payload != kNumWaveRows * kWaveRowSize * 2 ||
          !WaveGuardsOk(data + sizeof(h))) {
        return PLAITS_BLOB_ERROR_CONTENT;
      }
      return PLAITS_BLOB_OK;

    case PLAITS_SECTION_TERRAIN:
      if (!h.count || h.count > kMaxTerrains ||
          payload != h.count * kTerrainSize) {
        return PLAITS_BLOB_ERROR_CONTENT;
      }
      return PLAITS_BLOB_OK;

    default:
      if (identity_.services_status != PLAITS_BLOB_OK) {
        return PLAITS_BLOB_ERROR_SERVICES;
      }
      return ReadBlobs(data, size, h.count, services, digests, num_digests);
  }
}

PlaitsBlobStatus PlaitsRegion::ReadBlobs(
    const uint8_t* data,
    uint32_t size,
    uint32_t count,
    const PlaitsBlobServices* services,
    const uint8_t (*digests)[kPlaitsBlobDigestSize],
    size_t num_digests) {
  const uint32_t crc_offset = size - 4;
  if (count > kMaxPlaitsRegionBlobs ||
      count * sizeof(PlaitsSectionEntry) >
          crc_offset - sizeof(PlaitsSectionHeader)) {
    return PLAITS_BLOB_ERROR_TABLE;
  }
  const uint8_t* table = data + sizeof(PlaitsSectionHeader);

  // By ID, aligned, ascending, apart and inside, before any blob is read.
  uint32_t next = sizeof(PlaitsSectionHeader) +
      count * sizeof(PlaitsSectionEntry);
  int previous_id = -1;
  for (uint32_t i = 0; i < count; ++i) {
    PlaitsSectionEntry e;
    memcpy(&e, table + i * sizeof(e), sizeof(e));
    if (e.kind == kPlaitsBlobKindLibrary) {
      return PLAITS_BLOB_ERROR_LIBRARY;
    }
    if (e.kind != kPlaitsBlobKindModel || e.reserved ||
        e.id <= previous_id ||
        e.offset % kPlaitsBlobAlignment || e.offset < next ||
        e.offset > crc_offset || e.size > crc_offset - e.offset) {
      return PLAITS_BLOB_ERROR_TABLE;
    }
    previous_id = e.id;
    next = e.offset + e.size;
  }
  identity_.num_blobs = count;
  blobs_ = data;

  // One refused blob refuses the section: it is one configuration.
  PlaitsBlobStatus result = PLAITS_BLOB_OK;
  for (uint32_t i = 0; i < count; ++i) {
    PlaitsSectionEntry e;
    memcpy(&e, table + i * sizeof(e), sizeof(e));
    const uint8_t* blob = data + e.offset;
    PlaitsBlobStatus status = PlaitsBlob::Check(
        blob, blob, blob + e.size, services, digests, num_digests, NULL);
    if (status == PLAITS_BLOB_OK) {
      PlaitsBlobInfo info;
      PlaitsBlob::Info(blob, &info);
      if (info.size != e.size || Read32(blob + e.size - 4) != e.crc) {
        status = PLAITS_BLOB_ERROR_TABLE;
      } else if (info.first_id != e.id) {
        status = PLAITS_BLOB_ERROR_MODEL;
      }
      for (uint32_t j = 0; status == PLAITS_BLOB_OK && j < info.num_ids;
          ++j) {
        uint8_t& slot = models_[info.first_id + j];
        if (slot) {
          status = PLAITS_BLOB_ERROR_TABLE;
        } else {
          slot = i + 1;
        }
      }
    }
    identity_.blob_status[i] = status;
    if (status != PLAITS_BLOB_OK && result == PLAITS_BLOB_OK) {
      result = status;
    }
  }
  if (result != PLAITS_BLOB_OK) {
    memset(models_, 0, sizeof(models_));
  }
  return result;
}

const uint8_t* PlaitsRegion::blob(uint8_t model) const {
  const uint8_t index = models_[model];
  if (!index) {
    return NULL;
  }
  PlaitsSectionEntry e;
  memcpy(&e, blobs_ + sizeof(PlaitsSectionHeader) +
      (index - 1) * sizeof(e), sizeof(e));
  return blobs_ + e.offset;
}

/* static */
PlaitsSynthLock PlaitsRegion::Lock(bool plaits, bool wave) {
  if (plaits && wave) {
    return PLAITS_SYNTH_AS_PROFILED;
  } else if (wave) {
    return PLAITS_SYNTH_FORCE_BEADS;
  } else if (plaits) {
    return PLAITS_SYNTH_FORCE_PLAITS;
  }
  return PLAITS_SYNTH_SILENT;
}

}  // namespace beads
