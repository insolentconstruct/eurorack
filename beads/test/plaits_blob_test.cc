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
// Plaits blob checks: every refusal path, on synthetic blobs and regions.

#include <cstdio>
#include <cstring>

#include "beads/blob/plaits_blob.h"
#include "beads/blob/plaits_region.h"
#include "beads/profile.h"
#include "plaits/resources.h"
#include "beads/test/test_checker.h"

using namespace beads;

const uint32_t kExport = 0x08021440;
const size_t kBlobSize = 256;
const uint32_t kGotOffset = 240;

uint8_t region[1024] __attribute__((aligned(32)));
uint8_t digest[kPlaitsBlobDigestSize];
PlaitsBlobServices services;

void Write32(uint8_t* p, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    p[i] = value >> (8 * i);
  }
}

uint32_t Read32(const uint8_t* p) {
  return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24;
}

// Version 1: header, code bytes, one import, the CRC; the digest list is its
// own.
PlaitsBlobHeader* Build(uint8_t* b) {
  memset(region, 0, sizeof(region));
  PlaitsBlobHeader* h = reinterpret_cast<PlaitsBlobHeader*>(b);
  h->magic = kPlaitsBlobMagic;
  h->header_size = sizeof(PlaitsBlobHeader);
  h->abi = kPlaitsBlobAbi;
  h->services_version = kPlaitsBlobServicesVersion;
  h->model = 36;
  h->size = kBlobSize;
  h->state_size = 36;
  h->arena_size = 0;
  h->init_offset = 0x41;
  h->reset_offset = 0x59;
  h->render_offset = 0x5d;
  h->got_offset = kGotOffset;
  h->got_size = 4;
  h->flags = 0;
  h->out_gain = 0.93f;
  h->aux_gain = 0.93f;
  for (uint32_t i = sizeof(PlaitsBlobHeader); i < kGotOffset; ++i) {
    b[i] = i * 7;
  }
  Write32(b + kGotOffset, kExport);
  return h;
}

// Version 2, the same layout with the larger header.
PlaitsBlobHeader2* Build2(uint8_t* b) {
  memset(region, 0, sizeof(region));
  PlaitsBlobHeader2* h = reinterpret_cast<PlaitsBlobHeader2*>(b);
  h->magic = kPlaitsBlobMagic;
  h->header_size = sizeof(PlaitsBlobHeader2);
  h->abi = kPlaitsBlobAbi2;
  h->services_version = kPlaitsBlobServicesVersion2;
  h->kind = kPlaitsBlobKindModel;
  h->first_id = 18;
  h->num_ids = 1;
  h->size = kBlobSize;
  h->state_size = 272;
  h->arena_size = 16384;
  h->init_offset = 0x61;
  h->reset_offset = 0x79;
  h->render_offset = 0x7d;
  h->got_offset = kGotOffset;
  h->got_size = 4;
  h->out_gain = -2.0f;
  h->aux_gain = 1.0f;
  for (uint32_t i = sizeof(PlaitsBlobHeader2); i < kGotOffset; ++i) {
    b[i] = i * 5;
  }
  Write32(b + kGotOffset, kPlaitsBlobRandomStateAddress);
  return h;
}

// The size word is found by the header size, which faults may change.
void Seal(uint8_t* b, bool list, uint32_t size = 0) {
  if (!size) {
    size = Read32(b + (Read32(b + 4) == sizeof(PlaitsBlobHeader) ? 20 : 28));
  }
  Write32(b + size - 4, Profile::Crc32(b, size - 4));
  if (list) {
    PlaitsBlob::Sha256(b, size, digest);
  }
}

PlaitsBlobStatus Check(uint8_t* b) {
  return PlaitsBlob::Check(
      b, region, region + sizeof(region), &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest), 1,
      NULL);
}

void Services() {
  services = plaits_blob_services;
  services.exports[0] = reinterpret_cast<const void*>(uintptr_t(kExport));
}

bool Digest(const char* text, size_t size, const char* expected) {
  uint8_t d[kPlaitsBlobDigestSize];
  PlaitsBlob::Sha256(reinterpret_cast<const uint8_t*>(text), size, d);
  char hex[2 * kPlaitsBlobDigestSize + 1];
  for (size_t i = 0; i < kPlaitsBlobDigestSize; ++i) {
    snprintf(hex + 2 * i, 3, "%02x", d[i]);
  }
  return !strcmp(hex, expected);
}

void TestSha256() {
  test_checker.Start("TestSha256");
  CHECK(Digest("", 0,
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  CHECK(Digest("abc", 3,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  const char* two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  CHECK(Digest(two, 56,
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  char a[1000];
  memset(a, 'a', sizeof(a));
  CHECK(Digest(a, 64,
      "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"));
  CHECK(Digest(a, 1000,
      "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"));
}

void TestAccepts() {
  test_checker.Start("TestAccepts");
  Services();
  uint8_t* b = region + 64;
  Build(b);
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_OK);
  CHECK(PlaitsBlob::Check(b, region, region + sizeof(region), &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      0, NULL) == PLAITS_BLOB_ERROR_DIGEST);
  PlaitsBlobInfo info;
  PlaitsBlob::Info(b, &info);
  CHECK(info.abi == 1 && info.first_id == 36 && info.num_ids == 1);
  CHECK(info.size == kBlobSize && info.render_offset == 0x5d);

  Build2(b);
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_OK);
  PlaitsBlob::Info(b, &info);
  CHECK(info.abi == 2 && info.first_id == 18 && info.state_size == 272);
  CHECK(info.arena_size == 16384 && info.init_offset == 0x61);
}

void TestPlacement() {
  test_checker.Start("TestPlacement");
  Services();
  uint8_t* b = region + 64;
  Build(b);
  Seal(b, true);
  CHECK(PlaitsBlob::Check(b + 4, region, region + sizeof(region), &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1, NULL) == PLAITS_BLOB_ERROR_PLACEMENT);
  CHECK(PlaitsBlob::Check(b, region + 96, region + sizeof(region), &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1, NULL) == PLAITS_BLOB_ERROR_PLACEMENT);
  CHECK(PlaitsBlob::Check(b, region, b + 32, &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1, NULL) == PLAITS_BLOB_ERROR_PLACEMENT);
  CHECK(PlaitsBlob::Check(b, region + sizeof(region), region, &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1, NULL) == PLAITS_BLOB_ERROR_PLACEMENT);
  // The blob ends past the region.
  CHECK(PlaitsBlob::Check(b, region, b + kBlobSize - 4, &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1, NULL) == PLAITS_BLOB_ERROR_HEADER);
  // A version 2 header with no room for its last 32 bytes.
  Build2(b);
  Seal(b, true);
  CHECK(PlaitsBlob::Check(b, region, b + 80, &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1, NULL) == PLAITS_BLOB_ERROR_HEADER);
}

struct Fault {
  int field;
  uint32_t value;
  PlaitsBlobStatus expected;
};

// Each fault, with the CRC recomputed so that the targeted check fires.
void CheckFaults(const Fault* faults, size_t count, bool version_2) {
  uint8_t* b = region + 64;
  for (size_t i = 0; i < count; ++i) {
    uint32_t* words = version_2
        ? reinterpret_cast<uint32_t*>(Build2(b))
        : reinterpret_cast<uint32_t*>(Build(b));
    words[faults[i].field] = faults[i].value;
    const uint32_t size = words[version_2 ? 7 : 5];
    if (size <= kBlobSize && size >= 8 && size % 4 == 0) {
      Seal(b, true, size);
    }
    const PlaitsBlobStatus status = Check(b);
    if (status != faults[i].expected) {
      printf("ERROR: version %d field %d = 0x%x gave %d\n",
          version_2 ? 2 : 1, faults[i].field, faults[i].value, status);
    }
    CHECK(status == faults[i].expected);
  }
}

void TestRefusals() {
  test_checker.Start("TestRefusals");
  Services();
  uint8_t* b = region + 64;
  const Fault faults[] = {
    { 0, kPlaitsBlobMagic + 1, PLAITS_BLOB_ERROR_HEADER },
    { 1, 60, PLAITS_BLOB_ERROR_HEADER },
    { 1, 96, PLAITS_BLOB_ERROR_HEADER },
    { 5, 0xfffffffc, PLAITS_BLOB_ERROR_HEADER },
    { 5, 254, PLAITS_BLOB_ERROR_HEADER },
    { 5, 64, PLAITS_BLOB_ERROR_HEADER },
    { 2, kPlaitsBlobAbi2, PLAITS_BLOB_ERROR_HEADER },
    { 2, kPlaitsBlobAbi2 + 1, PLAITS_BLOB_ERROR_ABI },
    { 2, 0, PLAITS_BLOB_ERROR_ABI },
    { 3, kPlaitsBlobServicesVersion2, PLAITS_BLOB_ERROR_SERVICES },
    { 3, 0, PLAITS_BLOB_ERROR_SERVICES },
    { 4, 256, PLAITS_BLOB_ERROR_HEADER },
    { 11, 0xfffffff0, PLAITS_BLOB_ERROR_LAYOUT },
    { 11, 242, PLAITS_BLOB_ERROR_LAYOUT },
    { 11, 32, PLAITS_BLOB_ERROR_LAYOUT },
    { 12, 0xfffffffc, PLAITS_BLOB_ERROR_LAYOUT },
    { 12, 16, PLAITS_BLOB_ERROR_LAYOUT },
    { 8, 0x40, PLAITS_BLOB_ERROR_LAYOUT },
    { 9, 0x3f, PLAITS_BLOB_ERROR_LAYOUT },
    { 10, kGotOffset + 1, PLAITS_BLOB_ERROR_LAYOUT },
    { 6, kPlaitsBlobMaxStateSize + 1, PLAITS_BLOB_ERROR_RAM },
    { 7, kPlaitsBlobMaxArenaSize + 1, PLAITS_BLOB_ERROR_RAM },
  };
  CheckFaults(faults, sizeof(faults) / sizeof(faults[0]), false);

  const Fault faults_2[] = {
    { 1, 64, PLAITS_BLOB_ERROR_HEADER },
    { 3, kPlaitsBlobServicesVersion2 + 1, PLAITS_BLOB_ERROR_SERVICES },
    { 3, 0, PLAITS_BLOB_ERROR_SERVICES },
    { 4, kPlaitsBlobKindLibrary, PLAITS_BLOB_ERROR_KIND },
    { 4, 0, PLAITS_BLOB_ERROR_KIND },
    { 5, 256, PLAITS_BLOB_ERROR_HEADER },
    { 6, 0, PLAITS_BLOB_ERROR_HEADER },
    { 6, 239, PLAITS_BLOB_ERROR_HEADER },
    { 6, 0xffffffff, PLAITS_BLOB_ERROR_HEADER },
    { 7, 92, PLAITS_BLOB_ERROR_HEADER },
    { 8, kPlaitsBlobMaxStateSize2 + 1, PLAITS_BLOB_ERROR_RAM },
    { 9, kPlaitsBlobMaxArenaSize + 1, PLAITS_BLOB_ERROR_RAM },
    { 10, 0x41, PLAITS_BLOB_ERROR_LAYOUT },
    { 13, kGotOffset + 2, PLAITS_BLOB_ERROR_LAYOUT },
    { 15, 128, PLAITS_BLOB_ERROR_LIBRARY },
    { 16, 1, PLAITS_BLOB_ERROR_LIBRARY },
    { 20, 1, PLAITS_BLOB_ERROR_HEADER },
    { 23, 1, PLAITS_BLOB_ERROR_HEADER },
  };
  CheckFaults(faults_2, sizeof(faults_2) / sizeof(faults_2[0]), true);
  // A version 2 blob may serve several IDs, up to 255.
  PlaitsBlobHeader2* h2 = Build2(b);
  h2->first_id = 2;
  h2->num_ids = 3;
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_OK);
  h2->first_id = 253;
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_OK);
  h2->first_id = 254;
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_HEADER);

  Build(b);
  Seal(b, true);
  b[100] ^= 1;
  CHECK(Check(b) == PLAITS_BLOB_ERROR_CRC);

  Build(b);
  Seal(b, true);
  b[100] ^= 1;
  Seal(b, false);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_DIGEST);

  // An import that is not an export, on a listed blob.
  Build(b);
  Write32(b + kGotOffset, kExport + 4);
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_IMPORTS);
  // A version 1 blob may not import a version 2 export.
  Build(b);
  Write32(b + kGotOffset, kPlaitsBlobRandomStateAddress);
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_IMPORTS);
  // A version 2 blob imports the generator's state, but no other RAM word.
  Build2(b);
  Write32(b + kGotOffset, kPlaitsBlobRandomStateAddress + 4);
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_IMPORTS);
  Build2(b);
  Write32(b + kGotOffset, reinterpret_cast<uintptr_t>(plaits::lut_fold));
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_OK);

  // The firmware's own table: another version, or an export moved.
  Build(b);
  Seal(b, true);
  services.version = kPlaitsBlobServicesVersion2 + 1;
  CHECK(Check(b) == PLAITS_BLOB_ERROR_SERVICES);
  Services();
  services.version = kPlaitsBlobServicesVersion;
  CHECK(Check(b) == PLAITS_BLOB_ERROR_SERVICES);
  Services();
  services.num_exports = kNumPlaitsBlobExports - 1;
  CHECK(Check(b) == PLAITS_BLOB_ERROR_SERVICES);
  Services();
  services.magic = kPlaitsBlobServicesMagic + 1;
  CHECK(Check(b) == PLAITS_BLOB_ERROR_SERVICES);
  Services();
  services.size = sizeof(PlaitsBlobServices) + 4;
  CHECK(Check(b) == PLAITS_BLOB_ERROR_SERVICES);
  Services();
  services.exports[0] = reinterpret_cast<const void*>(uintptr_t(kExport + 4));
  CHECK(Check(b) == PLAITS_BLOB_ERROR_IMPORTS);
}

// The frozen exports: header fields, then one bit of one export's contents.
void TestServicesFrozen() {
  test_checker.Start("TestServicesFrozen");
  CHECK(PlaitsBlob::CheckServices(&plaits_blob_services) == PLAITS_BLOB_OK);
  static float sine[641];
  memcpy(sine, plaits::lut_sine, sizeof(sine));
  PlaitsBlobServices copy = plaits_blob_services;
  copy.exports[PLAITS_BLOB_EXPORT_LUT_SINE] = sine;
  CHECK(PlaitsBlob::CheckServices(&copy) == PLAITS_BLOB_OK);
  reinterpret_cast<uint8_t*>(sine)[1000] ^= 1;
  CHECK(PlaitsBlob::CheckServices(&copy) == PLAITS_BLOB_ERROR_SERVICES);
  static int16_t bump[257];
  memcpy(bump, plaits::lut_ws_bump, sizeof(bump));
  copy = plaits_blob_services;
  copy.exports[PLAITS_BLOB_EXPORT_LUT_WS_BUMP] = bump;
  CHECK(PlaitsBlob::CheckServices(&copy) == PLAITS_BLOB_OK);
  bump[256] ^= 1;
  CHECK(PlaitsBlob::CheckServices(&copy) == PLAITS_BLOB_ERROR_SERVICES);
  copy = plaits_blob_services;
  copy.exports[PLAITS_BLOB_EXPORT_RANDOM_STATE] = reinterpret_cast<const void*>(
      uintptr_t(kPlaitsBlobRandomStateAddress + 4));
  CHECK(PlaitsBlob::CheckServices(&copy) == PLAITS_BLOB_ERROR_SERVICES);
  copy = plaits_blob_services;
  copy.version += 1;
  CHECK(PlaitsBlob::CheckServices(&copy) == PLAITS_BLOB_ERROR_SERVICES);
  copy = plaits_blob_services;
  copy.num_exports -= 1;
  CHECK(PlaitsBlob::CheckServices(&copy) == PLAITS_BLOB_ERROR_SERVICES);
}

void TestServicesTable() {
  test_checker.Start("TestServicesTable");
  CHECK(plaits_blob_services.magic == kPlaitsBlobServicesMagic);
  CHECK(plaits_blob_services.version == kPlaitsBlobServicesVersion2);
  CHECK(plaits_blob_services.size == sizeof(PlaitsBlobServices));
  CHECK(plaits_blob_services.num_exports == kNumPlaitsBlobExports);
  CHECK(kNumPlaitsBlobExportsV1 == 3);
}

// A region of WAVE, TERR (two grids) and PLTS (two version 2 blobs, the
// first serving IDs 2 to 4), as blob_sections.py lays them out.
const uint32_t kRelease = 20261121;
const uint32_t kWaveSize = 32 + kNumWaveRows * kWaveRowSize * 2 + 4;
const uint32_t kTerrainSectionSize = 32 + 2 * kTerrainSize + 4;
const uint32_t kWaveOffset = 0;
const uint32_t kTerrainOffset = (kWaveSize + 31) / 32 * 32;
const uint32_t kPlaitsOffset =
    (kTerrainOffset + kTerrainSectionSize + 31) / 32 * 32;
const uint32_t kFirstBlob = 64;
const uint32_t kSecondBlob = 320;
const uint32_t kPlaitsSize = kSecondBlob + kBlobSize + 4;
const uint32_t kFlashSize = kPlaitsOffset + kPlaitsSize + 512;

uint8_t* flash;
uint8_t blob_digests[2][kPlaitsBlobDigestSize];
PlaitsDirectoryEntry directory[3];
PlaitsDefaultSection defaults[3];
PlaitsRegion plaits_region;

void SectionHeader(uint8_t* s, uint32_t kind, uint32_t size, uint32_t count) {
  const uint32_t words[8] = {
    kPlaitsSectionMagic, kind, kPlaitsSectionVersion, kRelease, size, count,
    0, 0
  };
  for (int i = 0; i < 8; ++i) {
    Write32(s + 4 * i, words[i]);
  }
}

uint32_t SealSection(uint8_t* s) {
  const uint32_t size = Read32(s + 16);
  const uint32_t crc = Profile::Crc32(s, size - 4);
  Write32(s + size - 4, crc);
  return crc;
}

void MakeBlob2(uint8_t* b, uint32_t first, uint32_t count, uint8_t* digest) {
  PlaitsBlobHeader2 h;
  memset(&h, 0, sizeof(h));
  h.magic = kPlaitsBlobMagic;
  h.header_size = sizeof(h);
  h.abi = kPlaitsBlobAbi2;
  h.services_version = kPlaitsBlobServicesVersion2;
  h.kind = kPlaitsBlobKindModel;
  h.first_id = first;
  h.num_ids = count;
  h.size = kBlobSize;
  h.state_size = 64;
  h.init_offset = 0x61;
  h.reset_offset = 0x79;
  h.render_offset = 0x7d;
  h.got_offset = kGotOffset;
  memcpy(b, &h, sizeof(h));
  for (uint32_t i = sizeof(h); i < kBlobSize - 4; ++i) {
    b[i] = i * first;
  }
  Write32(b + kBlobSize - 4, Profile::Crc32(b, kBlobSize - 4));
  PlaitsBlob::Sha256(b, kBlobSize, digest);
}

// PLTS entry word w (0: kind and ID) of entry i.
void SetEntry(int i, int w, uint32_t value) {
  Write32(flash + kPlaitsOffset + 32 + 16 * i + 4 * w, value);
}

void BuildRegion() {
  memset(flash, 0xff, kFlashSize);
  uint8_t* w = flash + kWaveOffset;
  SectionHeader(w, kPlaitsSectionWave, kWaveSize, kNumWaveRows);
  for (uint32_t row = 0; row < kNumWaveRows; ++row) {
    uint8_t* r = w + 32 + row * kWaveRowSize * 2;
    for (uint32_t j = 0; j < kWaveRowSize; ++j) {
      const int16_t v = int16_t(
          (j % 256) * 64 - 8192 + (row < 192 ? row : 191));
      r[2 * j] = v;
      r[2 * j + 1] = v >> 8;
    }
  }
  directory[0].crc = SealSection(w);

  uint8_t* t = flash + kTerrainOffset;
  SectionHeader(t, kPlaitsSectionTerrain, kTerrainSectionSize, 2);
  for (uint32_t i = 0; i < 2 * kTerrainSize; ++i) {
    t[32 + i] = i * 3;
  }
  directory[1].crc = SealSection(t);

  uint8_t* p = flash + kPlaitsOffset;
  SectionHeader(p, kPlaitsSectionPlaits, kPlaitsSize, 2);
  MakeBlob2(p + kFirstBlob, 2, 3, blob_digests[0]);
  MakeBlob2(p + kSecondBlob, 43, 1, blob_digests[1]);
  const uint32_t ids[2] = { 2, 43 };
  const uint32_t offsets[2] = { kFirstBlob, kSecondBlob };
  for (int i = 0; i < 2; ++i) {
    SetEntry(i, 0, kPlaitsBlobKindModel | ids[i] << 8);
    SetEntry(i, 1, offsets[i]);
    SetEntry(i, 2, kBlobSize);
    SetEntry(i, 3, Read32(p + offsets[i] + kBlobSize - 4));
  }
  directory[2].crc = SealSection(p);

  const uint32_t kinds[3] = {
    kPlaitsSectionWave, kPlaitsSectionTerrain, kPlaitsSectionPlaits
  };
  const uint32_t at[3] = { kWaveOffset, kTerrainOffset, kPlaitsOffset };
  const uint32_t sizes[3] = { kWaveSize, kTerrainSectionSize, kPlaitsSize };
  for (int i = 0; i < 3; ++i) {
    directory[i].kind = defaults[i].kind = kinds[i];
    directory[i].offset = defaults[i].offset = at[i];
    directory[i].size = defaults[i].size = sizes[i];
    PlaitsBlob::Sha256(flash + at[i], sizes[i], defaults[i].digest);
  }
}

void Read(const PlaitsDirectoryEntry* d, size_t n, size_t digests = 2) {
  plaits_region.Read(
      flash, flash + kFlashSize, kRelease, d, n, defaults, 3,
      &plaits_blob_services, blob_digests, digests);
}

uint32_t Status(PlaitsSection s) {
  return plaits_region.identity().sections[s].status;
}

void TestSections() {
  test_checker.Start("TestSections");
  BuildRegion();
  Read(directory, 3);
  const PlaitsRegionIdentity& id = plaits_region.identity();
  CHECK(id.release == kRelease && id.services_status == PLAITS_BLOB_OK);
  CHECK(id.directory_status == PLAITS_BLOB_OK);
  for (int i = 0; i < PLAITS_SECTION_LAST; ++i) {
    CHECK(Status(PlaitsSection(i)) == PLAITS_BLOB_OK);
    CHECK(id.sections[i].crc == directory[i].crc);
  }
  CHECK(id.num_blobs == 2);
  CHECK(id.blob_status[0] == PLAITS_BLOB_OK);
  CHECK(plaits_region.blob(2) == flash + kPlaitsOffset + kFirstBlob);
  CHECK(plaits_region.blob(4) == flash + kPlaitsOffset + kFirstBlob);
  CHECK(plaits_region.blob(43) == flash + kPlaitsOffset + kSecondBlob);
  CHECK(!plaits_region.blob(5) && !plaits_region.blob(1));
  CHECK(!plaits_region.blob(0) && !plaits_region.blob(255));
  CHECK(plaits_region.section(PLAITS_SECTION_WAVE) == flash + 32);

  // A stale tail from a longer, older region is never read.
  memset(flash + kPlaitsOffset + kPlaitsSize, 0x5a, 512);
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);

  // Without a directory, the defaults by digest; the directory unused.
  BuildRegion();
  Read(NULL, 0);
  CHECK(plaits_region.identity().directory_status ==
      PLAITS_BLOB_ERROR_NO_REGION);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  flash[kPlaitsOffset + kFirstBlob + 100] ^= 1;
  MakeBlob2(flash + kPlaitsOffset + kFirstBlob, 2, 3, blob_digests[0]);
  SetEntry(0, 3, Read32(flash + kPlaitsOffset + kFirstBlob + kBlobSize - 4));
  SealSection(flash + kPlaitsOffset);
  Read(NULL, 0);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  defaults[2].digest[0] ^= 1;
  Read(NULL, 0);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_DIGEST);
  CHECK(!plaits_region.blob(43));

  // Erased flash: every section absent.
  BuildRegion();
  memset(flash, 0xff, kFlashSize);
  Read(directory, 3);
  for (int i = 0; i < PLAITS_SECTION_LAST; ++i) {
    CHECK(Status(PlaitsSection(i)) == PLAITS_BLOB_ERROR_NO_REGION);
  }
  // A section the directory leaves out is absent.
  BuildRegion();
  Read(directory + 2, 1);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_ERROR_NO_REGION);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
}

// Each section's faults, on its own: the others stay readable.
void TestSectionFaults() {
  test_checker.Start("TestSectionFaults");
  struct {
    PlaitsSection section;
    uint32_t word;
    uint32_t value;
    bool reseal;
    PlaitsBlobStatus expected;
  } faults[] = {
    { PLAITS_SECTION_WAVE, 0, kPlaitsSectionMagic + 1, true,
      PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_WAVE, 1, kPlaitsSectionPlaits, true,
      PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_WAVE, 2, kPlaitsSectionVersion + 1, true,
      PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_WAVE, 3, kRelease - 1, true, PLAITS_BLOB_ERROR_RELEASE },
    { PLAITS_SECTION_WAVE, 4, kWaveSize - 4, false, PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_WAVE, 5, kNumWaveRows - 1, true,
      PLAITS_BLOB_ERROR_SELECTION },
    { PLAITS_SECTION_WAVE, 6, 1, true, PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_WAVE, 9, 0, false, PLAITS_BLOB_ERROR_CRC },
    { PLAITS_SECTION_TERRAIN, 5, 3, true, PLAITS_BLOB_ERROR_SELECTION },
    { PLAITS_SECTION_TERRAIN, 9, 0, false, PLAITS_BLOB_ERROR_CRC },
    { PLAITS_SECTION_PLAITS, 3, kRelease + 1, true,
      PLAITS_BLOB_ERROR_RELEASE },
    { PLAITS_SECTION_PLAITS, 40, 0x12345678, false, PLAITS_BLOB_ERROR_CRC },
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    BuildRegion();
    uint8_t* s = flash + directory[faults[i].section].offset;
    Write32(s + 4 * faults[i].word, faults[i].value);
    if (faults[i].reseal) {
      SealSection(s);
    }
    Read(directory, 3);
    bool ok = Status(faults[i].section) == uint32_t(faults[i].expected);
    for (int j = 0; j < PLAITS_SECTION_LAST; ++j) {
      ok = ok && (j == faults[i].section ||
          Status(PlaitsSection(j)) == PLAITS_BLOB_OK);
    }
    if (!ok) {
      printf("ERROR: TestSectionFaults: case %d gave %d %d %d\n",
          static_cast<int>(i), Status(PLAITS_SECTION_WAVE),
          Status(PLAITS_SECTION_TERRAIN), Status(PLAITS_SECTION_PLAITS));
    }
    CHECK(ok);
  }

  // Content limits, each selected with its new CRC.
  BuildRegion();
  uint8_t* w = flash + kWaveOffset;
  w[32 + 2 * kWaveRowSize * 2 + 2] ^= 1;  // Row 2, sample 1 vs 257.
  directory[0].crc = SealSection(w);
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_ERROR_CONTENT);
  BuildRegion();
  w[32 + 192 * kWaveRowSize * 2 + 100] ^= 1;  // The guard row.
  directory[0].crc = SealSection(w);
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_ERROR_CONTENT);
  BuildRegion();
  uint8_t* t = flash + kTerrainOffset;
  Write32(t + 20, 0);
  directory[1].crc = SealSection(t);
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_ERROR_CONTENT);
  BuildRegion();
  Write32(t + 20, 17);
  directory[1].crc = SealSection(t);
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_ERROR_CONTENT);

  // Sixteen grids are the most; seventeen are refused even when complete.
  for (uint32_t grids = 16; grids <= 17; ++grids) {
    memset(flash, 0xff, kFlashSize);
    const uint32_t size = 32 + grids * kTerrainSize + 4;
    SectionHeader(flash, kPlaitsSectionTerrain, size, grids);
    memset(flash + 32, 0x11, grids * kTerrainSize);
    PlaitsDirectoryEntry terrain = {
      kPlaitsSectionTerrain, 0, size, SealSection(flash)
    };
    Read(&terrain, 1);
    CHECK(Status(PLAITS_SECTION_TERRAIN) == (grids == 16
        ? PLAITS_BLOB_OK : PLAITS_BLOB_ERROR_CONTENT));
  }

  // The services' contents changed: PLTS refused, the others read.
  BuildRegion();
  static float sine[641];
  memcpy(sine, plaits::lut_sine, sizeof(sine));
  sine[3] += 1.0f;
  PlaitsBlobServices copy = plaits_blob_services;
  copy.exports[PLAITS_BLOB_EXPORT_LUT_SINE] = sine;
  plaits_region.Read(flash, flash + kFlashSize, kRelease, directory, 3,
      defaults, 3, &copy, blob_digests, 2);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_SERVICES);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  CHECK(!plaits_region.blob(43));
}

// The directory's layout, checked before any section is read.
void TestDirectory() {
  test_checker.Start("TestDirectory");
  struct {
    int entry;
    int word;
    uint32_t value;
  } faults[] = {
    { 0, 0, 0x12345678 },
    { 1, 0, kPlaitsSectionWave },
    { 2, 0, kPlaitsSectionTerrain },
    { 1, 1, kTerrainOffset + 4 },
    { 1, 1, kTerrainOffset - 32 },
    { 2, 1, 0xffffffe0 },
    { 2, 2, 0xffffffe0 },
    { 2, 2, kFlashSize },
    { 2, 2, 32 },
    { 0, 2, kTerrainOffset + 4 },
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    BuildRegion();
    uint32_t* words = reinterpret_cast<uint32_t*>(&directory[faults[i].entry]);
    words[faults[i].word] = faults[i].value;
    Read(directory, 3);
    bool ok = plaits_region.identity().directory_status ==
        PLAITS_BLOB_ERROR_DIRECTORY;
    for (int j = 0; j < PLAITS_SECTION_LAST; ++j) {
      ok = ok && Status(PlaitsSection(j)) == PLAITS_BLOB_ERROR_DIRECTORY;
    }
    if (!ok) {
      printf("ERROR: TestDirectory: case %d\n", static_cast<int>(i));
    }
    CHECK(ok);
  }
  BuildRegion();
  Read(directory, 4);
  CHECK(plaits_region.identity().directory_status ==
      PLAITS_BLOB_ERROR_DIRECTORY);
  plaits_region.Read(flash + 4, flash + kFlashSize, kRelease, directory, 3,
      defaults, 3, &plaits_blob_services, blob_digests, 2);
  CHECK(plaits_region.identity().directory_status ==
      PLAITS_BLOB_ERROR_DIRECTORY);
  CHECK(!plaits_region.blob(2));
}

// PLTS entry faults, the section CRC refreshed and selected.
void TestPlaitsTable() {
  test_checker.Start("TestPlaitsTable");
  struct {
    int entry;
    int word;
    uint32_t value;
    PlaitsBlobStatus expected;
  } faults[] = {
    { -1, 5, kMaxPlaitsRegionBlobs + 1, PLAITS_BLOB_ERROR_TABLE },
    { -1, 5, 40, PLAITS_BLOB_ERROR_TABLE },
    { 0, 0, kPlaitsBlobKindLibrary | 2 << 8, PLAITS_BLOB_ERROR_LIBRARY },
    { 0, 0, 0 | 2 << 8, PLAITS_BLOB_ERROR_TABLE },
    { 0, 0, kPlaitsBlobKindModel | 2 << 8 | 1 << 16,
      PLAITS_BLOB_ERROR_TABLE },
    { 0, 1, kFirstBlob + 4, PLAITS_BLOB_ERROR_TABLE },
    { 0, 1, 32, PLAITS_BLOB_ERROR_TABLE },
    { 1, 1, kFirstBlob, PLAITS_BLOB_ERROR_TABLE },
    { 1, 1, 0xffffffe0, PLAITS_BLOB_ERROR_TABLE },
    { 1, 2, 0xffffffe0, PLAITS_BLOB_ERROR_TABLE },
    { 1, 2, kPlaitsSize, PLAITS_BLOB_ERROR_TABLE },
    { 1, 0, kPlaitsBlobKindModel | 44 << 8, PLAITS_BLOB_ERROR_MODEL },
    { 1, 0, kPlaitsBlobKindModel | 3 << 8, PLAITS_BLOB_ERROR_MODEL },
    { 1, 0, kPlaitsBlobKindModel | 2 << 8, PLAITS_BLOB_ERROR_TABLE },
    { 1, 0, kPlaitsBlobKindModel | 1 << 8, PLAITS_BLOB_ERROR_TABLE },
    { 1, 3, 0, PLAITS_BLOB_ERROR_TABLE },
    { 1, 2, kBlobSize - 32, PLAITS_BLOB_ERROR_HEADER },
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    BuildRegion();
    uint8_t* p = flash + kPlaitsOffset;
    if (faults[i].entry < 0) {
      Write32(p + 4 * faults[i].word, faults[i].value);
    } else {
      SetEntry(faults[i].entry, faults[i].word, faults[i].value);
    }
    directory[2].crc = SealSection(p);
    Read(directory, 3);
    const uint32_t status = Status(PLAITS_SECTION_PLAITS);
    if (status != uint32_t(faults[i].expected)) {
      printf("ERROR: TestPlaitsTable: case %d gave %d\n",
          static_cast<int>(i), status);
    }
    CHECK(status == uint32_t(faults[i].expected));
    // All or none: no blob is served.
    CHECK(!plaits_region.blob(2) && !plaits_region.blob(43));
  }

  // A misaligned entry that overlaps nothing: entry 1 moved 4 bytes up and
  // shortened, still inside the section.
  BuildRegion();
  SetEntry(1, 1, kSecondBlob + 4);
  SetEntry(1, 2, kBlobSize - 8);
  directory[2].crc = SealSection(flash + kPlaitsOffset);
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_TABLE);
  CHECK(plaits_region.identity().num_blobs == 0);

  // Two blobs serving the same ID.
  BuildRegion();
  uint8_t* p = flash + kPlaitsOffset;
  MakeBlob2(p + kSecondBlob, 4, 1, blob_digests[1]);
  SetEntry(1, 0, kPlaitsBlobKindModel | 4 << 8);
  SetEntry(1, 3, Read32(p + kSecondBlob + kBlobSize - 4));
  directory[2].crc = SealSection(p);
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_TABLE);
  CHECK(plaits_region.identity().blob_status[0] == PLAITS_BLOB_OK);
  CHECK(plaits_region.identity().blob_status[1] == PLAITS_BLOB_ERROR_TABLE);
  CHECK(!plaits_region.blob(2));

  // The second blob is not on this firmware's list.
  BuildRegion();
  Read(directory, 3, 1);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_DIGEST);
  CHECK(!plaits_region.blob(2));
}

// A double ECC error reported after one section's read faults it alone.
int flash_calls;
int flash_failing_call;

bool FakeFlashCheck() {
  return ++flash_calls != flash_failing_call;
}

void TestFlashErrors() {
  test_checker.Start("TestFlashErrors");
  BuildRegion();
  plaits_region.set_flash_check(&FakeFlashCheck);
  // Two calls per section: before its read (cleared) and after it.
  flash_calls = 0;
  flash_failing_call = 0;
  Read(directory, 3);
  CHECK(flash_calls == 6);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  flash_calls = 0;
  flash_failing_call = 4;
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_ERROR_ECC);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  CHECK(plaits_region.blob(43) && !plaits_region.section(
      PLAITS_SECTION_TERRAIN));
  flash_calls = 0;
  flash_failing_call = 6;
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_ECC);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_OK);
  CHECK(!plaits_region.blob(2) && !plaits_region.blob(43));
  // A flag left from before the read is cleared, not counted.
  flash_calls = 0;
  flash_failing_call = 1;
  Read(directory, 3);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  plaits_region.set_flash_check(NULL);
}

void TestLocks() {
  test_checker.Start("TestLocks");
  CHECK(PlaitsRegion::Lock(true, true) == PLAITS_SYNTH_AS_PROFILED);
  CHECK(PlaitsRegion::Lock(false, true) == PLAITS_SYNTH_FORCE_BEADS);
  CHECK(PlaitsRegion::Lock(true, false) == PLAITS_SYNTH_FORCE_PLAITS);
  CHECK(PlaitsRegion::Lock(false, false) == PLAITS_SYNTH_SILENT);
}

int main() {
  static uint8_t storage[kFlashSize + 32];
  flash = storage + (32 - reinterpret_cast<uintptr_t>(storage) % 32) % 32;
  TestSha256();
  TestAccepts();
  TestPlacement();
  TestRefusals();
  TestServicesTable();
  TestServicesFrozen();
  TestSections();
  TestSectionFaults();
  TestDirectory();
  TestPlaitsTable();
  TestFlashErrors();
  TestLocks();
  puts("Plaits blob checks: PASS");
  return test_checker.failures() ? 1 : 0;
}
