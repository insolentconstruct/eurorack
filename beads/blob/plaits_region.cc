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
  num_libraries_ = 0;
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
        num_libraries_ = 0;
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
  // A default section's digest covers it, so its CRC is not recomputed.
  const uint32_t crc = Read32(data + size - 4);
  identity_.sections[section].crc = crc;
  if (expected_digest) {
    uint8_t digest[kPlaitsBlobDigestSize];
    PlaitsBlob::Sha256(data, size, digest);
    if (memcmp(digest, expected_digest, kPlaitsBlobDigestSize)) {
      return PLAITS_BLOB_ERROR_DIGEST;
    }
  } else if (Profile::Crc32(data, size - 4) != crc) {
    return PLAITS_BLOB_ERROR_CRC;
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

  // Libraries by ID, then models by ID; aligned, apart and inside, before
  // any blob is read.
  uint32_t next = sizeof(PlaitsSectionHeader) +
      count * sizeof(PlaitsSectionEntry);
  int previous_id = -1;
  size_t num_libraries = 0;
  for (uint32_t i = 0; i < count; ++i) {
    PlaitsSectionEntry e;
    memcpy(&e, table + i * sizeof(e), sizeof(e));
    if (e.kind == kPlaitsBlobKindLibrary) {
      if (num_libraries != i || num_libraries == kMaxPlaitsLibraries ||
          !e.id) {
        return PLAITS_BLOB_ERROR_TABLE;
      }
      library_ids_[num_libraries++] = e.id;
    } else if (e.kind != kPlaitsBlobKindModel) {
      return PLAITS_BLOB_ERROR_TABLE;
    } else if (i == num_libraries) {
      previous_id = -1;
    }
    if (e.reserved || e.id <= previous_id ||
        e.offset % kPlaitsBlobAlignment || e.offset < next ||
        e.offset > crc_offset || e.size > crc_offset - e.offset) {
      return PLAITS_BLOB_ERROR_TABLE;
    }
    previous_id = e.id;
    next = e.offset + e.size;
  }
  identity_.num_blobs = count;
  blobs_ = data;

  // Each blob on its own, then what it needs. One refused blob refuses the
  // section: it is one configuration.
  uint8_t library_digests[kMaxPlaitsLibraries][kPlaitsBlobDigestSize];
  for (uint32_t i = 0; i < count; ++i) {
    PlaitsSectionEntry e;
    memcpy(&e, table + i * sizeof(e), sizeof(e));
    const uint8_t* blob = data + e.offset;
    PlaitsBlobStatus status = PlaitsBlob::Check(
        blob, blob, blob + e.size, services, digests, num_digests,
        i < num_libraries ? library_digests[i] : NULL);
    if (status == PLAITS_BLOB_OK) {
      PlaitsBlobInfo info;
      PlaitsBlob::Info(blob, &info);
      if (info.size != e.size || Read32(blob + e.size - 4) != e.crc) {
        status = PLAITS_BLOB_ERROR_TABLE;
      } else if (info.kind != e.kind) {
        status = PLAITS_BLOB_ERROR_KIND;
      } else if (info.first_id != e.id) {
        status = PLAITS_BLOB_ERROR_MODEL;
      }
    }
    identity_.blob_status[i] = status;
  }

  // Each library takes its record and its RAM from what the synth gives.
  size_t ram = PlaitsLibraryRamAlign(
      num_libraries * sizeof(PlaitsBlobLibrary));
  PlaitsBlobStatus result = PLAITS_BLOB_OK;
  for (uint32_t i = 0; i < count; ++i) {
    const bool library = i < num_libraries;
    const uint8_t* blob = entry(i);
    PlaitsBlobStatus status = PlaitsBlobStatus(identity_.blob_status[i]);
    PlaitsBlobInfo info;
    memset(&info, 0, sizeof(info));
    if (status == PLAITS_BLOB_OK) {
      PlaitsBlob::Info(blob, &info);
      // A library needs only libraries before it: no cycle.
      status = CheckDependencies(
          blob, info, library ? i : num_libraries, library_digests);
    }
    if (status == PLAITS_BLOB_OK && library) {
      ram += PlaitsLibraryRamAlign(info.state_size);
      if (ram > library_ram_ || info.arena_size > library_scratch_) {
        status = PLAITS_BLOB_ERROR_RAM;
      }
    }
    for (uint32_t j = 0; status == PLAITS_BLOB_OK && !library &&
        j < info.num_ids; ++j) {
      uint8_t& slot = models_[info.first_id + j];
      if (slot) {
        status = PLAITS_BLOB_ERROR_TABLE;
      } else {
        slot = i + 1;
      }
    }
    identity_.blob_status[i] = status;
    if (status != PLAITS_BLOB_OK && result == PLAITS_BLOB_OK) {
      result = status;
    }
  }
  if (result != PLAITS_BLOB_OK) {
    memset(models_, 0, sizeof(models_));
  } else {
    num_libraries_ = num_libraries;
  }
  return result;
}

// Every library the blob lists is among the first num_libraries entries,
// was accepted, and is the build it was made against.
PlaitsBlobStatus PlaitsRegion::CheckDependencies(
    const uint8_t* blob,
    const PlaitsBlobInfo& info,
    size_t num_libraries,
    const uint8_t (*library_digests)[kPlaitsBlobDigestSize]) const {
  for (uint32_t i = 0; i < info.num_dependencies; ++i) {
    PlaitsBlobDependency d;
    memcpy(&d, blob + info.dependencies_offset + i * sizeof(d), sizeof(d));
    bool found = false;
    for (size_t j = 0; j < num_libraries && !found; ++j) {
      found = library_ids_[j] == d.id &&
          identity_.blob_status[j] == PLAITS_BLOB_OK &&
          !memcmp(library_digests[j], d.digest, kPlaitsBlobDigestSize);
    }
    if (!found) {
      return PLAITS_BLOB_ERROR_DEPENDENCY;
    }
  }
  return PLAITS_BLOB_OK;
}

const uint8_t* PlaitsRegion::entry(size_t index) const {
  PlaitsSectionEntry e;
  memcpy(&e, blobs_ + sizeof(PlaitsSectionHeader) + index * sizeof(e),
      sizeof(e));
  return blobs_ + e.offset;
}

const uint8_t* PlaitsRegion::blob(uint8_t model) const {
  const uint8_t index = models_[model];
  return index ? entry(index - 1) : NULL;
}

const uint8_t* PlaitsRegion::library(size_t index) const {
  return index < num_libraries_ ? entry(index) : NULL;
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
