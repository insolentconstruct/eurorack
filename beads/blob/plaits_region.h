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
// The region after the app: WAVE, TERR and PLTS sections, each valid on its
// own; a directory in the profile or the release's default layout says where.

#ifndef BEADS_BLOB_PLAITS_REGION_H_
#define BEADS_BLOB_PLAITS_REGION_H_

#include "stmlib/stmlib.h"

#include "beads/blob/plaits_blob.h"

namespace beads {

const uint32_t kPlaitsSectionMagic = 0x43535a44;  // DZSC
const uint32_t kPlaitsSectionVersion = 1;
const uint32_t kPlaitsSectionWave = 0x45564157;  // WAVE
const uint32_t kPlaitsSectionTerrain = 0x52524554;  // TERR
const uint32_t kPlaitsSectionPlaits = 0x53544c50;  // PLTS
const uint32_t kPlaitsRegionEnd = 0x08100000;
const size_t kMaxPlaitsRegionBlobs = 128;

// Beta 4 content limits (ABI.md).
const uint32_t kNumWaveRows = 193;
const uint32_t kWaveRowSize = 260;
const uint32_t kMaxTerrains = 16;
const uint32_t kTerrainSize = 64 * 64;

enum PlaitsSection {
  PLAITS_SECTION_WAVE,
  PLAITS_SECTION_TERRAIN,
  PLAITS_SECTION_PLAITS,
  PLAITS_SECTION_LAST
};

// Then the payload, then a CRC-32 of everything before it.
struct PlaitsSectionHeader {
  uint32_t magic;
  uint32_t kind;
  uint32_t version;
  uint32_t release;
  uint32_t size;
  uint32_t count;
  uint32_t reserved[2];
};

// PLTS payload: one per blob, offsets from the section's start.
struct PlaitsSectionEntry {
  uint8_t kind;
  uint8_t id;
  uint16_t reserved;
  uint32_t offset;
  uint32_t size;
  uint32_t crc;
};

// From the profile: where a section is and the CRC it was written with.
struct PlaitsDirectoryEntry {
  uint32_t kind;
  uint32_t offset;
  uint32_t size;
  uint32_t crc;
};

// The release's own layout, used without a profile directory.
struct PlaitsDefaultSection {
  uint32_t kind;
  uint32_t offset;
  uint32_t size;
  uint8_t digest[kPlaitsBlobDigestSize];
};

STATIC_ASSERT(sizeof(PlaitsSectionHeader) == 32, plaits_section_header);
STATIC_ASSERT(sizeof(PlaitsSectionEntry) == 16, plaits_section_entry);
STATIC_ASSERT(sizeof(PlaitsDirectoryEntry) == 16, plaits_directory_entry);

// Read over SWD by symbol: with app.bin's SHA-256 and the profile CRC, it
// names the configuration that runs. Digests stay in the app's allowlist.
struct PlaitsRegionIdentity {
  uint32_t release;
  uint32_t services_status;
  uint32_t directory_status;
  struct {
    uint32_t status;
    uint32_t address;
    uint32_t size;
    uint32_t crc;
  } sections[PLAITS_SECTION_LAST];
  uint32_t num_blobs;
  uint8_t blob_status[kMaxPlaitsRegionBlobs];
};

// Reports and clears the flash's uncorrectable ECC flag: false after a
// double error since the last call (FLASH_SR1 DBECCERR on the H7).
typedef bool (*PlaitsFlashCheck)();

// Which synth may play, before any blob code runs (ABI.md, faults).
enum PlaitsSynthLock {
  PLAITS_SYNTH_AS_PROFILED,
  PLAITS_SYNTH_FORCE_BEADS,
  PLAITS_SYNTH_FORCE_PLAITS,
  PLAITS_SYNTH_SILENT
};

class PlaitsRegion {
 public:
  PlaitsRegion() : flash_check_(NULL) { }
  ~PlaitsRegion() { }

  // Checked around each section's read; NULL on the host and emulator.
  inline void set_flash_check(PlaitsFlashCheck check) {
    flash_check_ = check;
  }

  // Reads [begin, end): the directory's sections, or the defaults when
  // directory is NULL. Sections fail independently; blobs all or none.
  void Read(
      const uint8_t* begin,
      const uint8_t* end,
      uint32_t release,
      const PlaitsDirectoryEntry* directory,
      size_t num_entries,
      const PlaitsDefaultSection* defaults,
      size_t num_defaults,
      const PlaitsBlobServices* services,
      const uint8_t (*digests)[kPlaitsBlobDigestSize],
      size_t num_digests);

  // A checked blob serving model, or NULL.
  const uint8_t* blob(uint8_t model) const;

  inline bool ok(PlaitsSection section) const {
    return identity_.sections[section].status == PLAITS_BLOB_OK;
  }

  inline const uint8_t* section(PlaitsSection section) const {
    return ok(section) ? sections_[section] + sizeof(PlaitsSectionHeader)
        : NULL;
  }

  inline const PlaitsRegionIdentity& identity() const { return identity_; }

  static PlaitsSynthLock Lock(bool plaits, bool wave);

 private:
  PlaitsBlobStatus ReadSection(
      PlaitsSection section,
      const uint8_t* data,
      uint32_t size,
      uint32_t release,
      uint32_t expected_crc,
      const uint8_t* expected_digest,
      const PlaitsBlobServices* services,
      const uint8_t (*digests)[kPlaitsBlobDigestSize],
      size_t num_digests);
  PlaitsBlobStatus ReadBlobs(
      const uint8_t* data,
      uint32_t size,
      uint32_t count,
      const PlaitsBlobServices* services,
      const uint8_t (*digests)[kPlaitsBlobDigestSize],
      size_t num_digests);

  PlaitsRegionIdentity identity_;
  // Entry index + 1 per model ID; 0 when no checked blob serves it.
  uint8_t models_[kNumPlaitsModelIds];
  const uint8_t* blobs_;
  const uint8_t* sections_[PLAITS_SECTION_LAST];
  PlaitsFlashCheck flash_check_;

  DISALLOW_COPY_AND_ASSIGN(PlaitsRegion);
};

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_REGION_H_
