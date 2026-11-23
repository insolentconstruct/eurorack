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
#include "beads/blob/plaits_blob_engine.h"
#include "beads/blob/plaits_libraries.h"
#include "beads/blob/plaits_region.h"
#include "beads/blob/synth_choice.h"
#include "beads/dsp/plaits_voice.h"
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
    { 4, kPlaitsBlobKindLibrary, PLAITS_BLOB_ERROR_HEADER },
    { 4, 4, PLAITS_BLOB_ERROR_KIND },
    { 4, kPlaitsBlobKindSynth, PLAITS_BLOB_ERROR_HEADER },
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
    { 15, 128, PLAITS_BLOB_ERROR_LAYOUT },
    { 16, 1, PLAITS_BLOB_ERROR_LAYOUT },
    { 16, kMaxPlaitsBlobDependencies + 1, PLAITS_BLOB_ERROR_LAYOUT },
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

  // A changed byte: the digest covers it, without a CRC pass.
  Build(b);
  Seal(b, true);
  b[100] ^= 1;
  CHECK(Check(b) == PLAITS_BLOB_ERROR_DIGEST);

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

// A synth blob: one ID, its RAM, three entries, one import and both sizes.
const uint32_t kSynthRam = 180000;

void MakeSynth(uint8_t* b, uint8_t* digest) {
  PlaitsBlobSynthHeader h;
  memset(&h, 0, sizeof(h));
  h.magic = kPlaitsBlobMagic;
  h.header_size = sizeof(h);
  h.abi = kPlaitsBlobAbi2;
  h.services_version = kPlaitsBlobServicesVersion2;
  h.kind = kPlaitsBlobKindSynth;
  h.id = 1;
  h.num_ids = 1;
  h.size = kBlobSize;
  h.ram_size = kSynthRam;
  h.init_offset = 0x61;
  h.configure_offset = 0x79;
  h.render_offset = 0x7d;
  h.got_offset = kGotOffset;
  h.got_size = 4;
  h.controls_size = 36;
  h.block_size = 12;
  memcpy(b, &h, sizeof(h));
  for (uint32_t i = sizeof(h); i < kGotOffset; ++i) {
    b[i] = i * 3;
  }
  Write32(b + kGotOffset, kPlaitsBlobRandomStateAddress);
  Write32(b + kBlobSize - 4, Profile::Crc32(b, kBlobSize - 4));
  PlaitsBlob::Sha256(b, kBlobSize, digest);
}

void TestSynthRefusals() {
  test_checker.Start("TestSynthRefusals");
  Services();
  uint8_t* b = region + 64;
  memset(region, 0, sizeof(region));
  MakeSynth(b, digest);
  CHECK(Check(b) == PLAITS_BLOB_OK);
  PlaitsBlobInfo info;
  PlaitsBlob::Info(b, &info);
  CHECK(info.abi == 2 && info.kind == kPlaitsBlobKindSynth);
  CHECK(info.first_id == 1 && info.num_ids == 1 && info.size == kBlobSize);
  CHECK(info.state_size == kSynthRam && info.arena_size == 0);
  CHECK(info.init_offset == 0x61 && info.reset_offset == 0x79);
  CHECK(info.render_offset == 0x7d);
  CHECK(info.controls_size == 36 && info.block_size == 12);
  CHECK(!info.num_exports && !info.num_dependencies);
  // A model has no control block or block size: its gains are not read.
  Build2(b);
  Seal(b, true);
  PlaitsBlob::Info(b, &info);
  CHECK(!info.controls_size && !info.block_size);

  const Fault faults[] = {
    { 2, kPlaitsBlobAbi, PLAITS_BLOB_ERROR_HEADER },
    { 3, kPlaitsBlobServicesVersion2 + 1, PLAITS_BLOB_ERROR_SERVICES },
    { 3, 0, PLAITS_BLOB_ERROR_SERVICES },
    { 5, 0, PLAITS_BLOB_ERROR_HEADER },
    { 5, 256, PLAITS_BLOB_ERROR_HEADER },
    { 6, 2, PLAITS_BLOB_ERROR_HEADER },
    { 6, 0, PLAITS_BLOB_ERROR_HEADER },
    { 8, kPlaitsBlobMaxSynthRamSize, PLAITS_BLOB_OK },
    { 8, kPlaitsBlobMaxSynthRamSize + 1, PLAITS_BLOB_ERROR_RAM },
    { 9, 1, PLAITS_BLOB_ERROR_HEADER },
    { 10, 0x60, PLAITS_BLOB_ERROR_LAYOUT },
    { 11, kGotOffset + 1, PLAITS_BLOB_ERROR_LAYOUT },
    { 12, 0x41, PLAITS_BLOB_ERROR_LAYOUT },
    { 13, kGotOffset + 2, PLAITS_BLOB_ERROR_LAYOUT },
    { 15, kGotOffset + 4, PLAITS_BLOB_ERROR_HEADER },
    { 16, 1, PLAITS_BLOB_ERROR_HEADER },
    { 17, 1, PLAITS_BLOB_ERROR_HEADER },
    { 18, 0, PLAITS_BLOB_ERROR_HEADER },
    { 19, 0, PLAITS_BLOB_ERROR_HEADER },
    { 20, 1, PLAITS_BLOB_ERROR_HEADER },
    { 23, 1, PLAITS_BLOB_ERROR_HEADER },
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    memset(region, 0, sizeof(region));
    MakeSynth(b, digest);
    Write32(b + 4 * faults[i].field, faults[i].value);
    Seal(b, true, kBlobSize);
    const PlaitsBlobStatus status = Check(b);
    if (status != faults[i].expected) {
      printf("ERROR: TestSynthRefusals: field %d = 0x%x gave %d\n",
          faults[i].field, faults[i].value, status);
    }
    CHECK(status == faults[i].expected);
  }

  // Imports as for a model; the digest covers every byte.
  memset(region, 0, sizeof(region));
  MakeSynth(b, digest);
  Write32(b + kGotOffset, kPlaitsBlobRandomStateAddress + 4);
  Seal(b, true, kBlobSize);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_IMPORTS);
  MakeSynth(b, digest);
  b[100] ^= 1;
  Seal(b, false, kBlobSize);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_DIGEST);
}

// A region of WAVE, TERR (two grids), PLTS (two version 2 blobs, the first
// serving IDs 2 to 4) and RNGS (one synth), as blob_sections.py lays them
// out.
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
const uint32_t kRingsOffset = (kPlaitsOffset + kPlaitsSize + 31) / 32 * 32;
const uint32_t kRingsSize = 32 + kBlobSize + 4;
const uint32_t kFlashSize = kRingsOffset + kRingsSize + 2048;
const size_t kNumSections = 4;

uint8_t* flash;
uint8_t blob_digests[3][kPlaitsBlobDigestSize];
PlaitsDirectoryEntry directory[kNumSections + 1];
PlaitsDefaultSection defaults[kNumSections];
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

  uint8_t* r = flash + kRingsOffset;
  SectionHeader(r, kPlaitsSectionRings, kRingsSize, 1);
  MakeSynth(r + 32, blob_digests[2]);
  directory[3].crc = SealSection(r);

  const uint32_t kinds[kNumSections] = {
    kPlaitsSectionWave, kPlaitsSectionTerrain, kPlaitsSectionPlaits,
    kPlaitsSectionRings
  };
  const uint32_t at[kNumSections] = {
    kWaveOffset, kTerrainOffset, kPlaitsOffset, kRingsOffset
  };
  const uint32_t sizes[kNumSections] = {
    kWaveSize, kTerrainSectionSize, kPlaitsSize, kRingsSize
  };
  for (size_t i = 0; i < kNumSections; ++i) {
    directory[i].kind = defaults[i].kind = kinds[i];
    directory[i].offset = defaults[i].offset = at[i];
    directory[i].size = defaults[i].size = sizes[i];
    PlaitsBlob::Sha256(flash + at[i], sizes[i], defaults[i].digest);
  }
}

void Read(const PlaitsDirectoryEntry* d, size_t n, size_t digests = 3) {
  plaits_region.Read(
      flash, flash + kFlashSize, kRelease, d, n, defaults, kNumSections,
      &plaits_blob_services, blob_digests, digests);
}

uint32_t Status(PlaitsSection s) {
  return plaits_region.identity().sections[s].status;
}

void TestSections() {
  test_checker.Start("TestSections");
  BuildRegion();
  Read(directory, kNumSections);
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
  Read(directory, kNumSections);
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
  Read(directory, kNumSections);
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
    { PLAITS_SECTION_RINGS, 0, kPlaitsSectionMagic + 1, true,
      PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_RINGS, 1, kPlaitsSectionPlaits, true,
      PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_RINGS, 3, kRelease + 1, true, PLAITS_BLOB_ERROR_RELEASE },
    { PLAITS_SECTION_RINGS, 4, kRingsSize - 4, false,
      PLAITS_BLOB_ERROR_HEADER },
    { PLAITS_SECTION_RINGS, 5, 2, true, PLAITS_BLOB_ERROR_SELECTION },
    { PLAITS_SECTION_RINGS, 30, 0x12345678, false, PLAITS_BLOB_ERROR_CRC },
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    BuildRegion();
    uint8_t* s = flash + directory[faults[i].section].offset;
    Write32(s + 4 * faults[i].word, faults[i].value);
    if (faults[i].reseal) {
      SealSection(s);
    }
    Read(directory, kNumSections);
    bool ok = Status(faults[i].section) == uint32_t(faults[i].expected);
    for (int j = 0; j < PLAITS_SECTION_LAST; ++j) {
      ok = ok && (j == faults[i].section ||
          Status(PlaitsSection(j)) == PLAITS_BLOB_OK);
    }
    if (!ok) {
      printf("ERROR: TestSectionFaults: case %d gave %d %d %d %d\n",
          static_cast<int>(i), Status(PLAITS_SECTION_WAVE),
          Status(PLAITS_SECTION_TERRAIN), Status(PLAITS_SECTION_PLAITS),
          Status(PLAITS_SECTION_RINGS));
    }
    CHECK(ok);
  }

  // Content limits, each selected with its new CRC.
  BuildRegion();
  uint8_t* w = flash + kWaveOffset;
  w[32 + 2 * kWaveRowSize * 2 + 2] ^= 1;  // Row 2, sample 1 vs 257.
  directory[0].crc = SealSection(w);
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_ERROR_CONTENT);
  BuildRegion();
  w[32 + 192 * kWaveRowSize * 2 + 100] ^= 1;  // The guard row.
  directory[0].crc = SealSection(w);
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_ERROR_CONTENT);
  BuildRegion();
  uint8_t* t = flash + kTerrainOffset;
  Write32(t + 20, 0);
  directory[1].crc = SealSection(t);
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_ERROR_CONTENT);
  BuildRegion();
  Write32(t + 20, 17);
  directory[1].crc = SealSection(t);
  Read(directory, kNumSections);
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
  plaits_region.Read(flash, flash + kFlashSize, kRelease, directory,
      kNumSections, defaults, kNumSections, &copy, blob_digests, 3);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_SERVICES);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_ERROR_SERVICES);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  CHECK(!plaits_region.synth_blob());
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
    { 3, 0, kPlaitsSectionPlaits },
    { 2, 0, kPlaitsSectionRings },
    { 3, 1, kRingsOffset + 4 },
    { 2, 2, kRingsOffset - kPlaitsOffset + 32 },
    // The last section past the region's end.
    { 3, 2, kFlashSize },
    { 3, 2, 0xffffffe0 },
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    BuildRegion();
    uint32_t* words = reinterpret_cast<uint32_t*>(&directory[faults[i].entry]);
    words[faults[i].word] = faults[i].value;
    Read(directory, kNumSections);
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
  Read(directory, kNumSections + 1);
  CHECK(plaits_region.identity().directory_status ==
      PLAITS_BLOB_ERROR_DIRECTORY);
  plaits_region.Read(flash + 4, flash + kFlashSize, kRelease, directory,
      kNumSections, defaults, kNumSections, &plaits_blob_services,
      blob_digests, 3);
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
    { 0, 0, kPlaitsBlobKindLibrary | 2 << 8, PLAITS_BLOB_ERROR_KIND },
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
    Read(directory, kNumSections);
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
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_TABLE);
  CHECK(plaits_region.identity().num_blobs == 0);

  // Two blobs serving the same ID.
  BuildRegion();
  uint8_t* p = flash + kPlaitsOffset;
  MakeBlob2(p + kSecondBlob, 4, 1, blob_digests[1]);
  SetEntry(1, 0, kPlaitsBlobKindModel | 4 << 8);
  SetEntry(1, 3, Read32(p + kSecondBlob + kBlobSize - 4));
  directory[2].crc = SealSection(p);
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_TABLE);
  CHECK(plaits_region.identity().blob_status[0] == PLAITS_BLOB_OK);
  CHECK(plaits_region.identity().blob_status[1] == PLAITS_BLOB_ERROR_TABLE);
  CHECK(!plaits_region.blob(2));

  // The second blob is not on this firmware's list.
  BuildRegion();
  Read(directory, kNumSections, 1);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_DIGEST);
  CHECK(!plaits_region.blob(2));
}

// A version 2 blob with one ID and no imports: a model's entries, or a
// library's RAM, scratch and two exports; then its dependencies. Returns its
// size.
const uint32_t kDependentGot = 200;
const uint32_t kLibraryRam = 256;
// Header flags MakeBlob gives a model.
uint32_t model_flags = 0;
const uint32_t kLibraryScratch = 64;

uint32_t MakeBlob(
    uint8_t* b,
    uint32_t kind,
    uint32_t id,
    const PlaitsBlobDependency* dependencies,
    uint32_t num_dependencies,
    uint8_t* digest) {
  PlaitsBlobHeader2 h;
  memset(&h, 0, sizeof(h));
  h.magic = kPlaitsBlobMagic;
  h.header_size = sizeof(h);
  h.abi = kPlaitsBlobAbi2;
  h.services_version = kPlaitsBlobServicesVersion2;
  h.kind = kind;
  h.first_id = id;
  h.num_ids = 1;
  h.got_offset = kDependentGot;
  uint32_t next = kDependentGot;
  if (kind == kPlaitsBlobKindLibrary) {
    h.state_size = kLibraryRam;
    h.arena_size = kLibraryScratch;
    h.reset_offset = next;
    h.render_offset = 2;
    Write32(b + next, 0x61);
    Write32(b + next + 4, 0x80);
    next += 8;
  } else {
    h.state_size = 64;
    h.init_offset = 0x61;
    h.reset_offset = 0x79;
    h.render_offset = 0x7d;
    h.out_gain = 1.0f;
    h.flags = model_flags;
  }
  if (num_dependencies) {
    h.dependencies_offset = next;
    h.num_dependencies = num_dependencies;
    memcpy(b + next, dependencies,
        num_dependencies * sizeof(PlaitsBlobDependency));
    next += num_dependencies * sizeof(PlaitsBlobDependency);
  }
  h.size = next + 4;
  memcpy(b, &h, sizeof(h));
  for (uint32_t i = sizeof(h); i < kDependentGot; ++i) {
    b[i] = i * (id + kind);
  }
  Write32(b + h.size - 4, Profile::Crc32(b, h.size - 4));
  PlaitsBlob::Sha256(b, h.size, digest);
  return h.size;
}

PlaitsBlobDependency Needs(uint32_t id, const uint8_t* digest) {
  PlaitsBlobDependency d;
  d.id = id;
  memcpy(d.digest, digest, sizeof(d.digest));
  return d;
}

void TestLibraryRefusals() {
  test_checker.Start("TestLibraryRefusals");
  Services();
  uint8_t* b = region + 64;
  const Fault faults[] = {
    { 6, 2, PLAITS_BLOB_ERROR_HEADER },
    { 5, 0, PLAITS_BLOB_ERROR_HEADER },
    { 17, 1, PLAITS_BLOB_ERROR_HEADER },
    { 18, 1, PLAITS_BLOB_ERROR_HEADER },
    { 19, 1, PLAITS_BLOB_ERROR_HEADER },
    { 12, 0, PLAITS_BLOB_ERROR_LAYOUT },
    { 12, kMaxPlaitsLibraryExports + 1, PLAITS_BLOB_ERROR_LAYOUT },
    { 12, 3, PLAITS_BLOB_ERROR_LAYOUT },
    { 11, kDependentGot - 4, PLAITS_BLOB_ERROR_LAYOUT },
    { 11, kDependentGot + 2, PLAITS_BLOB_ERROR_LAYOUT },
    { 13, kDependentGot + 4, PLAITS_BLOB_ERROR_LAYOUT },
    { 10, 0x60, PLAITS_BLOB_ERROR_LAYOUT },
    { 10, kDependentGot + 1, PLAITS_BLOB_ERROR_LAYOUT },
    { 10, 0x61, PLAITS_BLOB_OK },
    { 8, kPlaitsBlobMaxLibraryRamSize, PLAITS_BLOB_OK },
    { 8, kPlaitsBlobMaxLibraryRamSize + 1, PLAITS_BLOB_ERROR_RAM },
    { 9, kPlaitsBlobMaxArenaSize + 1, PLAITS_BLOB_ERROR_RAM },
    { 16, 1, PLAITS_BLOB_ERROR_LAYOUT },
    { 15, kDependentGot + 8, PLAITS_BLOB_ERROR_LAYOUT },
  };
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    memset(region, 0, sizeof(region));
    const uint32_t size = MakeBlob(
        b, kPlaitsBlobKindLibrary, 3, NULL, 0, digest);
    Write32(b + 4 * faults[i].field, faults[i].value);
    Seal(b, true, size);
    const PlaitsBlobStatus status = Check(b);
    if (status != faults[i].expected) {
      printf("ERROR: library field %d = 0x%x gave %d\n",
          faults[i].field, faults[i].value, status);
    }
    CHECK(status == faults[i].expected);
  }

  // Export offsets name code or constants: not the header, not the GOT.
  const uint32_t exports[3] = { 0x20, kDependentGot, 0x60 };
  const PlaitsBlobStatus expected[3] = {
    PLAITS_BLOB_ERROR_LAYOUT, PLAITS_BLOB_ERROR_LAYOUT, PLAITS_BLOB_OK
  };
  for (int i = 0; i < 3; ++i) {
    memset(region, 0, sizeof(region));
    const uint32_t size = MakeBlob(
        b, kPlaitsBlobKindLibrary, 3, NULL, 0, digest);
    Write32(b + kDependentGot + 4, exports[i]);
    Seal(b, true, size);
    CHECK(Check(b) == expected[i]);
  }

  // A dependency list ends at the CRC and names libraries.
  memset(region, 0, sizeof(region));
  uint8_t other[kPlaitsBlobDigestSize];
  memset(other, 7, sizeof(other));
  PlaitsBlobDependency d = Needs(1, other);
  uint32_t size = MakeBlob(b, kPlaitsBlobKindModel, 13, &d, 1, digest);
  CHECK(size == kDependentGot + sizeof(d) + 4);
  CHECK(Check(b) == PLAITS_BLOB_OK);
  d.id = 0;
  MakeBlob(b, kPlaitsBlobKindModel, 13, &d, 1, digest);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_LIBRARY);
  d.id = 1;
  size = MakeBlob(b, kPlaitsBlobKindModel, 13, &d, 1, digest);
  Write32(b + 60, kDependentGot + 4);
  Seal(b, true, size);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_LAYOUT);
  // A version 1 header has no dependencies or libraries.
  PlaitsBlobHeader* h1 = Build(b);
  h1->state_size = 36;
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_OK);
}

// A PLTS section of blobs at 32-byte offsets in table order; kinds[i] is the
// entry's kind. Returns the directory entry.
const size_t kMaxTestBlobs = kMaxPlaitsLibraries + 2;
uint8_t test_digests[kMaxTestBlobs][kPlaitsBlobDigestSize];

struct TestBlob {
  uint32_t kind;
  uint32_t id;
  PlaitsBlobDependency dependencies[kMaxPlaitsBlobDependencies];
  uint32_t num_dependencies;
};

PlaitsDirectoryEntry WritePlaits(
    const TestBlob* blobs,
    size_t count,
    const uint32_t* entry_kinds) {
  uint8_t* p = flash + kPlaitsOffset;
  memset(p, 0xff, kFlashSize - kPlaitsOffset);
  uint32_t offset = 32 + 16 * count;
  for (size_t i = 0; i < count; ++i) {
    offset = (offset + 31) / 32 * 32;
    const uint32_t size = MakeBlob(
        p + offset, blobs[i].kind, blobs[i].id, blobs[i].dependencies,
        blobs[i].num_dependencies, test_digests[i]);
    const uint32_t kind = entry_kinds ? entry_kinds[i] : blobs[i].kind;
    SetEntry(i, 0, kind | blobs[i].id << 8);
    SetEntry(i, 1, offset);
    SetEntry(i, 2, size);
    SetEntry(i, 3, Read32(p + offset + size - 4));
    offset += size;
  }
  const uint32_t size = offset + 4;
  SectionHeader(p, kPlaitsSectionPlaits, size, count);
  PlaitsDirectoryEntry e;
  e.kind = kPlaitsSectionPlaits;
  e.offset = kPlaitsOffset;
  e.size = size;
  e.crc = SealSection(p);
  return e;
}

const size_t kTestLibraryRam = 2 * 1024;

uint32_t ReadPlaits(
    const TestBlob* blobs,
    size_t count,
    const uint32_t* entry_kinds = NULL,
    size_t ram = kTestLibraryRam,
    size_t scratch = kLibraryScratch) {
  PlaitsDirectoryEntry e = WritePlaits(blobs, count, entry_kinds);
  plaits_region.set_library_ram(ram, scratch);
  plaits_region.Read(
      flash, flash + kFlashSize, kRelease, &e, 1, defaults, kNumSections,
      &plaits_blob_services, test_digests, count);
  return Status(PLAITS_SECTION_PLAITS);
}

uint32_t BlobStatus(size_t i) {
  return plaits_region.identity().blob_status[i];
}

void TestDependencies() {
  test_checker.Start("TestDependencies");
  uint8_t lib1[kPlaitsBlobDigestSize];
  uint8_t lib3[kPlaitsBlobDigestSize];
  uint8_t scratch[256];
  // Each library's own digest, as its dependents record it.
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 1, NULL, 0, lib1);
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 3, NULL, 0, lib3);

  // A library, a model that needs it and one that does not.
  TestBlob ok[3] = {
    { kPlaitsBlobKindLibrary, 3, { }, 0 },
    { kPlaitsBlobKindModel, 15, { Needs(3, lib3) }, 1 },
    { kPlaitsBlobKindModel, 43, { }, 0 },
  };
  CHECK(ReadPlaits(ok, 3) == PLAITS_BLOB_OK);
  CHECK(plaits_region.num_libraries() == 1);
  CHECK(plaits_region.library_id(0) == 3);
  CHECK(plaits_region.library(0) == flash + kPlaitsOffset + 96);
  CHECK(!plaits_region.library(1));
  CHECK(plaits_region.blob(15) && plaits_region.blob(43));
  CHECK(!plaits_region.blob(3));

  // Missing: the library is not in the section.
  CHECK(ReadPlaits(ok + 1, 2) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(0) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(1) == PLAITS_BLOB_OK);
  CHECK(!plaits_region.blob(15) && !plaits_region.blob(43));
  CHECK(plaits_region.num_libraries() == 0);

  // Wrong digest: built against another build of the library.
  TestBlob wrong[2] = {
    { kPlaitsBlobKindLibrary, 3, { }, 0 },
    { kPlaitsBlobKindModel, 15, { Needs(3, lib1) }, 1 },
  };
  CHECK(ReadPlaits(wrong, 2) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(0) == PLAITS_BLOB_OK);
  CHECK(BlobStatus(1) == PLAITS_BLOB_ERROR_DEPENDENCY);
  // Another ID with the right digest is not the library either.
  wrong[1].dependencies[0] = Needs(4, lib3);
  CHECK(ReadPlaits(wrong, 2) == PLAITS_BLOB_ERROR_DEPENDENCY);

  // Wrong version: a library for newer services is refused, then its users.
  PlaitsDirectoryEntry e = WritePlaits(ok, 3, NULL);
  uint8_t* library = flash + kPlaitsOffset + 96;
  Write32(library + 12, kPlaitsBlobServicesVersion2 + 1);
  const uint32_t library_size = Read32(library + 28);
  Write32(library + library_size - 4,
      Profile::Crc32(library, library_size - 4));
  PlaitsBlob::Sha256(library, library_size, test_digests[0]);
  SetEntry(0, 3, Read32(library + library_size - 4));
  e.crc = SealSection(flash + kPlaitsOffset);
  plaits_region.Read(
      flash, flash + kFlashSize, kRelease, &e, 1, defaults, kNumSections,
      &plaits_blob_services, test_digests, 3);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_SERVICES);
  CHECK(BlobStatus(0) == PLAITS_BLOB_ERROR_SERVICES);
  CHECK(BlobStatus(1) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(2) == PLAITS_BLOB_OK);

  // A library this firmware does not list fails; so do its users.
  e = WritePlaits(ok, 3, NULL);
  plaits_region.Read(
      flash, flash + kFlashSize, kRelease, &e, 1, defaults, kNumSections,
      &plaits_blob_services, test_digests + 1, 2);
  CHECK(BlobStatus(0) == PLAITS_BLOB_ERROR_DIGEST);
  CHECK(BlobStatus(1) == PLAITS_BLOB_ERROR_DEPENDENCY);

  // A chain: library 6 needs library 1, which comes first.
  uint8_t lib6[kPlaitsBlobDigestSize];
  PlaitsBlobDependency needs_1 = Needs(1, lib1);
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 6, &needs_1, 1, lib6);
  TestBlob chain[4] = {
    { kPlaitsBlobKindLibrary, 1, { }, 0 },
    { kPlaitsBlobKindLibrary, 6, { Needs(1, lib1) }, 1 },
    { kPlaitsBlobKindModel, 13, { Needs(1, lib1) }, 1 },
    { kPlaitsBlobKindModel, 90, { Needs(6, lib6), Needs(1, lib1) }, 2 },
  };
  CHECK(ReadPlaits(chain, 4) == PLAITS_BLOB_OK);
  CHECK(plaits_region.num_libraries() == 2);

  // A cycle: library 2 needs library 5, listed after it, which needs 2.
  uint8_t lib2[kPlaitsBlobDigestSize];
  uint8_t lib5[kPlaitsBlobDigestSize];
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 2, NULL, 0, lib2);
  PlaitsBlobDependency needs_2 = Needs(2, lib2);
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 5, &needs_2, 1, lib5);
  TestBlob cycle[2] = {
    { kPlaitsBlobKindLibrary, 2, { Needs(5, lib5) }, 1 },
    { kPlaitsBlobKindLibrary, 5, { Needs(2, lib2) }, 1 },
  };
  CHECK(ReadPlaits(cycle, 2) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(0) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(1) == PLAITS_BLOB_ERROR_DEPENDENCY);
  // Nor a later library, even the right build of one.
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 5, NULL, 0, lib5);
  TestBlob forward[2] = {
    { kPlaitsBlobKindLibrary, 2, { Needs(5, lib5) }, 1 },
    { kPlaitsBlobKindLibrary, 5, { }, 0 },
  };
  CHECK(ReadPlaits(forward, 2) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(0) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(BlobStatus(1) == PLAITS_BLOB_OK);
  // A library may not need itself.
  TestBlob itself[1] = {
    { kPlaitsBlobKindLibrary, 3, { Needs(3, lib3) }, 1 },
  };
  CHECK(ReadPlaits(itself, 1) == PLAITS_BLOB_ERROR_DEPENDENCY);

  // Without its RAM: the records and each library's RAM, on 32 bytes.
  const size_t need = PlaitsLibraryRamAlign(sizeof(PlaitsBlobLibrary)) +
      kLibraryRam;
  CHECK(ReadPlaits(ok, 3, NULL, need) == PLAITS_BLOB_OK);
  CHECK(ReadPlaits(ok, 3, NULL, need - 1) == PLAITS_BLOB_ERROR_RAM);
  CHECK(plaits_region.num_libraries() == 0);
  CHECK(BlobStatus(0) == PLAITS_BLOB_ERROR_RAM);
  CHECK(BlobStatus(1) == PLAITS_BLOB_ERROR_DEPENDENCY);
  CHECK(ReadPlaits(ok, 3, NULL, 0) == PLAITS_BLOB_ERROR_RAM);
  CHECK(ReadPlaits(ok, 3, NULL, need, kLibraryScratch - 1) ==
      PLAITS_BLOB_ERROR_RAM);
  // Two libraries: two records and both RAMs.
  const size_t need_2 = PlaitsLibraryRamAlign(2 * sizeof(PlaitsBlobLibrary)) +
      2 * kLibraryRam;
  CHECK(ReadPlaits(chain, 4, NULL, need_2) == PLAITS_BLOB_OK);
  CHECK(ReadPlaits(chain, 4, NULL, need_2 - 1) == PLAITS_BLOB_ERROR_RAM);

  // Model IDs start again after the libraries.
  TestBlob low[2] = {
    { kPlaitsBlobKindLibrary, 3, { }, 0 },
    { kPlaitsBlobKindModel, 2, { }, 0 },
  };
  CHECK(ReadPlaits(low, 2) == PLAITS_BLOB_OK);

  // Libraries come first, have an ID, and number at most eight.
  TestBlob late[2] = {
    { kPlaitsBlobKindModel, 2, { }, 0 },
    { kPlaitsBlobKindLibrary, 3, { }, 0 },
  };
  CHECK(ReadPlaits(late, 2) == PLAITS_BLOB_ERROR_TABLE);
  TestBlob zero[1] = { { kPlaitsBlobKindLibrary, 0, { }, 0 } };
  CHECK(ReadPlaits(zero, 1) == PLAITS_BLOB_ERROR_TABLE);
  TestBlob many[kMaxPlaitsLibraries + 1];
  for (size_t i = 0; i <= kMaxPlaitsLibraries; ++i) {
    many[i].kind = kPlaitsBlobKindLibrary;
    many[i].id = i + 1;
    many[i].num_dependencies = 0;
  }
  CHECK(ReadPlaits(many, kMaxPlaitsLibraries, NULL, 8 * 1024) ==
      PLAITS_BLOB_OK);
  CHECK(ReadPlaits(many, kMaxPlaitsLibraries + 1, NULL, 8 * 1024) ==
      PLAITS_BLOB_ERROR_TABLE);
  // An entry's kind must be its blob's.
  const uint32_t swapped[3] = {
    kPlaitsBlobKindLibrary, kPlaitsBlobKindLibrary, kPlaitsBlobKindModel
  };
  CHECK(ReadPlaits(ok, 3, swapped) == PLAITS_BLOB_ERROR_KIND);
  CHECK(BlobStatus(1) == PLAITS_BLOB_ERROR_KIND);
  plaits_region.set_library_ram(0, 0);
}

void TestLibraries() {
  test_checker.Start("TestLibraries");
  uint8_t lib1[kPlaitsBlobDigestSize];
  uint8_t lib6[kPlaitsBlobDigestSize];
  uint8_t scratch[256];
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 1, NULL, 0, lib1);
  PlaitsBlobDependency needs_1 = Needs(1, lib1);
  MakeBlob(scratch, kPlaitsBlobKindLibrary, 6, &needs_1, 1, lib6);
  TestBlob chain[3] = {
    { kPlaitsBlobKindLibrary, 1, { }, 0 },
    { kPlaitsBlobKindLibrary, 6, { Needs(1, lib1) }, 1 },
    { kPlaitsBlobKindModel, 90, { Needs(6, lib6), Needs(1, lib1) }, 2 },
  };
  CHECK(ReadPlaits(chain, 3) == PLAITS_BLOB_OK);

  // Records first, then each library's RAM, all cleared; exports are
  // addresses in the blob.
  static uint8_t ram[kTestLibraryRam];
  memset(ram, 0xa5, sizeof(ram));
  PlaitsLibraries libraries;
  libraries.Init(
      plaits_region, &plaits_blob_services, ram, sizeof(ram), scratch,
      kLibraryScratch);
  const PlaitsBlobLibrary* one = libraries.Find(1);
  const PlaitsBlobLibrary* six = libraries.Find(6);
  CHECK(one == reinterpret_cast<PlaitsBlobLibrary*>(ram));
  CHECK(six == one + 1);
  CHECK(!libraries.Find(2) && !libraries.Find(0));
  CHECK(one->context == NULL && six->context == NULL);
  CHECK(one->exports[0] == plaits_region.library(0) + 0x61);
  CHECK(one->exports[1] == plaits_region.library(0) + 0x80);
  CHECK(six->exports[1] == plaits_region.library(1) + 0x80);
  const size_t used = PlaitsLibraryRamAlign(2 * sizeof(PlaitsBlobLibrary)) +
      2 * kLibraryRam;
  bool cleared = !one->exports[2] &&
      !six->exports[kMaxPlaitsLibraryExports - 1];
  for (size_t i = 2 * sizeof(PlaitsBlobLibrary); i < used; ++i) {
    cleared = cleared && !ram[i];
  }
  CHECK(cleared);
  CHECK(ram[used] == 0xa5);

  // A dependent gets its libraries in its listed order.
  const PlaitsBlobLibrary* resolved[kMaxPlaitsBlobDependencies];
  libraries.Resolve(plaits_region.blob(90), resolved);
  CHECK(resolved[0] == six && resolved[1] == one);

  // RAM short of what the region accounted for: nothing past it is built.
  libraries.Init(
      plaits_region, &plaits_blob_services, ram, used - 1, scratch,
      kLibraryScratch);
  CHECK(libraries.Find(1) && !libraries.Find(6));
}

// RNGS on its own: one synth blob after the header, selected and checked as
// the other sections; a fault there hides no other section, nor they it.
uint8_t* rings_section;

uint32_t ReadRings(size_t digests = 3) {
  directory[3].crc = SealSection(rings_section);
  Read(directory, kNumSections, digests);
  return Status(PLAITS_SECTION_RINGS);
}

bool OthersOk() {
  return Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK &&
      Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_OK &&
      Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK &&
      plaits_region.blob(43);
}

void TestRings() {
  test_checker.Start("TestRings");
  rings_section = flash + kRingsOffset;
  uint8_t* synth = rings_section + 32;
  BuildRegion();
  CHECK(ReadRings() == PLAITS_BLOB_OK && OthersOk());
  CHECK(plaits_region.synth_blob() == synth);
  CHECK(plaits_region.section(PLAITS_SECTION_RINGS) == synth);
  CHECK(plaits_region.identity().sections[3].crc == directory[3].crc);
  CHECK(plaits_region.identity().sections[3].size == kRingsSize);

  // The section holds exactly one blob, filling its payload.
  const uint32_t counts[2] = { 0, 2 };
  for (int i = 0; i < 2; ++i) {
    BuildRegion();
    Write32(rings_section + 20, counts[i]);
    CHECK(ReadRings() == PLAITS_BLOB_ERROR_TABLE && OthersOk());
    CHECK(!plaits_region.synth_blob());
  }
  BuildRegion();
  Write32(rings_section + 16, kRingsSize + 32);
  directory[3].size = kRingsSize + 32;
  CHECK(ReadRings() == PLAITS_BLOB_ERROR_TABLE && OthersOk());
  BuildRegion();
  Write32(rings_section + 16, kRingsSize - 32);
  directory[3].size = kRingsSize - 32;
  CHECK(ReadRings() == PLAITS_BLOB_ERROR_HEADER && OthersOk());

  // The blob: its own checks, on the allowlist, and a synth's kind.
  BuildRegion();
  synth[100] ^= 1;
  Write32(synth + kBlobSize - 4, Profile::Crc32(synth, kBlobSize - 4));
  CHECK(ReadRings() == PLAITS_BLOB_ERROR_DIGEST && OthersOk());
  BuildRegion();
  Write32(synth + 4 * 19, 0);
  Write32(synth + kBlobSize - 4, Profile::Crc32(synth, kBlobSize - 4));
  PlaitsBlob::Sha256(synth, kBlobSize, blob_digests[2]);
  CHECK(ReadRings() == PLAITS_BLOB_ERROR_HEADER && OthersOk());
  BuildRegion();
  CHECK(ReadRings(2) == PLAITS_BLOB_ERROR_DIGEST && OthersOk());
  CHECK(!plaits_region.synth_blob());
  // A model or a library in RNGS, each on the allowlist.
  BuildRegion();
  MakeBlob2(synth, 43, 1, blob_digests[2]);
  CHECK(ReadRings() == PLAITS_BLOB_ERROR_KIND && OthersOk());
  CHECK(!plaits_region.synth_blob());
  BuildRegion();
  memset(synth, 0xff, kBlobSize);
  const uint32_t library_size = MakeBlob(
      synth, kPlaitsBlobKindLibrary, 3, NULL, 0, blob_digests[2]);
  Write32(rings_section + 16, 32 + library_size + 4);
  directory[3].size = 32 + library_size + 4;
  CHECK(ReadRings() == PLAITS_BLOB_ERROR_KIND && OthersOk());
  // A synth blob in PLTS is not a table entry.
  BuildRegion();
  SetEntry(1, 0, kPlaitsBlobKindSynth | 43 << 8);
  directory[2].crc = SealSection(flash + kPlaitsOffset);
  CHECK(ReadRings() == PLAITS_BLOB_OK);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_TABLE);
  CHECK(plaits_region.synth_blob() == synth && !plaits_region.blob(43));

  // Erased, misaligned, left out, or past a stale tail.
  BuildRegion();
  memset(rings_section, 0xff, kRingsSize);
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_ERROR_NO_REGION);
  CHECK(OthersOk() && !plaits_region.synth_blob());
  BuildRegion();
  memset(rings_section + kRingsSize, 0x5a, 512);
  CHECK(ReadRings() == PLAITS_BLOB_OK && OthersOk());
  BuildRegion();
  directory[3].offset += 4;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_ERROR_DIRECTORY);
  CHECK(!plaits_region.synth_blob() && !plaits_region.blob(43));
  BuildRegion();
  Read(directory, kNumSections - 1);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_ERROR_NO_REGION);
  CHECK(OthersOk() && !plaits_region.synth_blob());
  BuildRegion();
  Read(directory + 3, 1);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_OK);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_NO_REGION);
  CHECK(plaits_region.synth_blob() == synth);

  // The default layout: each section by its own digest.
  BuildRegion();
  Read(NULL, 0);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_OK && OthersOk());
  CHECK(plaits_region.synth_blob() == synth);
  defaults[3].digest[5] ^= 1;
  Read(NULL, 0);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_ERROR_DIGEST);
  CHECK(OthersOk() && !plaits_region.synth_blob());
  BuildRegion();
  defaults[2].digest[5] ^= 1;
  Read(NULL, 0);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_DIGEST);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_OK);
  CHECK(plaits_region.synth_blob() == synth);
}

// A rebuilt blob's first 48 samples of OUT fade in on a squared ramp from 0,
// in blocks of any size; nothing after them changes.
void TestFadeIn() {
  test_checker.Start("TestFadeIn");
  const size_t n = kPlaitsBlobFadeSize + 12;
  float out[n];
  for (size_t i = 0; i < n; ++i) {
    out[i] = -2.0f;
  }
  size_t position = PlaitsBlobEngine::FadeIn(out, 5, 0);
  CHECK(position == 5);
  position = PlaitsBlobEngine::FadeIn(out + 5, 24, position);
  position = PlaitsBlobEngine::FadeIn(out + 29, n - 29, position);
  CHECK(position == kPlaitsBlobFadeSize);
  bool ramp = out[0] == 0.0f;
  for (size_t i = 0; i < n; ++i) {
    const float x = float(i) * kPlaitsBlobFadeStep;
    const float gain = i < kPlaitsBlobFadeSize ? x * x : 1.0f;
    ramp = ramp && out[i] == -2.0f * gain;
  }
  CHECK(ramp);
  CHECK(out[kPlaitsBlobFadeSize - 1] > -2.0f);
  // The table is the squared ramp, as the emulator computes it.
  bool table = true;
  for (size_t i = 0; i < kPlaitsBlobFadeSize; ++i) {
    const float x = float(i) * kPlaitsBlobFadeStep;
    table = table && kPlaitsBlobFadeGain[i] == x * x;
  }
  CHECK(table);
  // Past the window a block is left alone.
  float block[12];
  for (size_t i = 0; i < 12; ++i) {
    block[i] = 0.5f;
  }
  CHECK(PlaitsBlobEngine::FadeIn(block, 12, kPlaitsBlobFadeSize) ==
      kPlaitsBlobFadeSize);
  CHECK(block[0] == 0.5f && block[11] == 0.5f);
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
  Read(directory, kNumSections);
  CHECK(flash_calls == 8);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  flash_calls = 0;
  flash_failing_call = 4;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_ERROR_ECC);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  CHECK(plaits_region.blob(43) && !plaits_region.section(
      PLAITS_SECTION_TERRAIN));
  flash_calls = 0;
  flash_failing_call = 6;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_ECC);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_OK);
  CHECK(!plaits_region.blob(2) && !plaits_region.blob(43));
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_OK);
  CHECK(plaits_region.synth_blob() == flash + kRingsOffset + 32);
  flash_calls = 0;
  flash_failing_call = 8;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_RINGS) == PLAITS_BLOB_ERROR_ECC);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  CHECK(!plaits_region.synth_blob() && plaits_region.blob(43));
  // A flag left from before the read is cleared, not counted.
  flash_calls = 0;
  flash_failing_call = 1;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_OK);
  plaits_region.set_flash_check(NULL);
}

// The firmware's rule (beads.cc): Plaits plays when every model of every
// bank is compiled in or served by a checked blob.
void TestPlayable() {
  test_checker.Start("TestPlayable");
  BuildRegion();
  Read(directory, kNumSections);
  // This region serves 2-4 (one blob) and 43; nothing is compiled in.
  const uint8_t served[4] = { 2, 3, 4, 43 };
  const uint8_t stock[24] = {
    8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    0, 1, 2, 3, 4, 5, 6, 7
  };
  CHECK(PlaitsVoice::Playable(served, 4, plaits_region));
  for (int i = 0; i < 24; ++i) {
    CHECK(PlaitsVoice::Available(i, plaits_region) == (i >= 2 && i <= 4));
  }
  CHECK(!PlaitsVoice::Playable(stock, 24, plaits_region));
  CHECK(!PlaitsVoice::Available(44, plaits_region));
  CHECK(!PlaitsVoice::Available(24, plaits_region));
  // Without a valid PLTS no model is available.
  BuildRegion();
  flash[kPlaitsOffset + 40] ^= 1;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_ERROR_CRC);
  CHECK(!PlaitsVoice::Playable(served, 4, plaits_region));
  CHECK(!PlaitsVoice::Available(2, plaits_region));
  // Fifteen banks: one missing model anywhere locks Plaits out.
  BuildRegion();
  Read(directory, kNumSections);
  uint8_t many[15 * 8];
  for (int i = 0; i < 15 * 8; ++i) {
    many[i] = served[i % 4];
  }
  CHECK(PlaitsVoice::Playable(many, sizeof(many), plaits_region));
  many[15 * 8 - 1] = 44;
  CHECK(!PlaitsVoice::Playable(many, sizeof(many), plaits_region));
  many[15 * 8 - 1] = 0;
  CHECK(!PlaitsVoice::Playable(many, sizeof(many), plaits_region));
  many[15 * 8 - 1] = 43;
  CHECK(PlaitsVoice::Playable(many, sizeof(many), plaits_region));
}

void TestLocks() {
  test_checker.Start("TestLocks");
  CHECK(PlaitsRegion::Lock(true, true) == PLAITS_SYNTH_AS_PROFILED);
  CHECK(PlaitsRegion::Lock(false, true) == PLAITS_SYNTH_FORCE_BEADS);
  CHECK(PlaitsRegion::Lock(true, false) == PLAITS_SYNTH_FORCE_PLAITS);
  CHECK(PlaitsRegion::Lock(false, false) == PLAITS_SYNTH_SILENT);
}

// Each section fails independently before selecting a synth.
void TestFaultMatrix() {
  test_checker.Start("TestFaultMatrix");
  const uint8_t served[4] = { 2, 3, 4, 43 };
  static uint8_t saved[kFlashSize];
  for (int p = 0; p < 5; ++p) {
    for (int w = 0; w < 5; ++w) {
      for (int t = 0; t < 6; ++t) {
        BuildRegion();
        const int faults[kNumSections] = { w, t, p, 0 };
        for (size_t i = 0; i < kNumSections; ++i) {
          uint8_t* section = flash + directory[i].offset;
          if (faults[i] == 1) {
            section[32] ^= 1;
          } else if (faults[i] == 2) {
            section[0] ^= 1;
          } else if (faults[i] == 3) {
            memset(section + 32, 0xff, directory[i].size - 32);
          } else if (faults[i] == 4) {
            memset(section, 0xff, directory[i].size);
          }
        }
        memcpy(saved, flash, kFlashSize);
        flash_calls = 0;
        flash_failing_call = 4;
        plaits_region.set_flash_check(t == 5 ? &FakeFlashCheck : NULL);
        Read(directory, kNumSections);
        plaits_region.set_flash_check(NULL);
        CHECK(!memcmp(saved, flash, kFlashSize));
        CHECK(plaits_region.ok(PLAITS_SECTION_PLAITS) == (p == 0));
        CHECK(plaits_region.ok(PLAITS_SECTION_WAVE) == (w == 0));
        CHECK(plaits_region.ok(PLAITS_SECTION_TERRAIN) == (t == 0));
        CHECK(plaits_region.ok(PLAITS_SECTION_RINGS));
        const bool playable = PlaitsVoice::Playable(served, 4, plaits_region);
        CHECK(playable == (p == 0));
        CHECK((plaits_region.waves() != NULL) == (w == 0));
        CHECK((plaits_region.terrains() != NULL) == (t == 0));
        CHECK(plaits_region.num_terrains() == (t == 0 ? 2 : 0));
        const uint8_t mask = SynthMask(w == 0, playable, true);
        for (uint8_t selected = SYNTH_BEADS; selected <= SYNTH_PLAITS;
             ++selected) {
          const SynthEngine expected = !p && !w ? SynthEngine(selected)
              : !w ? SYNTH_BEADS : !p ? SYNTH_PLAITS : SYNTH_SILENT;
          CHECK(EffectiveSynth(selected, mask) == expected);
        }
      }
    }
  }
}

// A model's header flags: TERR's record only for version 2 models.
void TestModelFlags() {
  test_checker.Start("TestModelFlags");
  Services();
  uint8_t* b = region + 32;
  const uint32_t v2[4] = { 0, PLAITS_BLOB_FLAG_ENVELOPED,
      PLAITS_BLOB_FLAG_TERRAINS,
      PLAITS_BLOB_FLAG_ENVELOPED | PLAITS_BLOB_FLAG_TERRAINS };
  for (int i = 0; i < 4; ++i) {
    Build2(b)->flags = v2[i];
    Seal(b, true);
    CHECK(Check(b) == PLAITS_BLOB_OK);
  }
  const uint32_t unknown[3] = { 4, 0x80000000, PLAITS_BLOB_FLAG_TERRAINS | 8 };
  for (int i = 0; i < 3; ++i) {
    Build2(b)->flags = unknown[i];
    Seal(b, true);
    CHECK(Check(b) == PLAITS_BLOB_ERROR_HEADER);
  }
  Build(b)->flags = PLAITS_BLOB_FLAG_ENVELOPED;
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_OK);
  Build(b)->flags = PLAITS_BLOB_FLAG_TERRAINS;
  Seal(b, true);
  CHECK(Check(b) == PLAITS_BLOB_ERROR_HEADER);
}

// Sets a PLTS blob's flags and seals the blob, its entry and the section.
void FlagBlob(int entry, uint32_t offset, uint32_t flags, uint8_t* digest) {
  uint8_t* p = flash + kPlaitsOffset;
  uint8_t* b = p + offset;
  Write32(b + 68, flags);
  Write32(b + kBlobSize - 4, Profile::Crc32(b, kBlobSize - 4));
  PlaitsBlob::Sha256(b, kBlobSize, digest);
  SetEntry(entry, 3, Read32(b + kBlobSize - 4));
  directory[2].crc = SealSection(p);
}

// WAVE's rows for Beads' synth, TERR's grids for a model flagged for them.
void TestWavesAndTerrains() {
  test_checker.Start("TestWavesAndTerrains");
  BuildRegion();
  FlagBlob(1, kSecondBlob, PLAITS_BLOB_FLAG_TERRAINS, blob_digests[1]);
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  CHECK(plaits_region.waves() ==
      reinterpret_cast<const int16_t*>(flash + kWaveOffset + 32));
  CHECK(plaits_region.terrains() ==
      reinterpret_cast<const int8_t*>(flash + kTerrainOffset + 32));
  CHECK(plaits_region.num_terrains() == 2);

  static uint8_t ram[1024];
  uint8_t scratch[64];
  PlaitsLibraries libraries;
  libraries.Init(
      plaits_region, &plaits_blob_services, ram, sizeof(ram), scratch,
      sizeof(scratch));
  const PlaitsBlobLibrary* sentinel =
      reinterpret_cast<const PlaitsBlobLibrary*>(scratch);
  const PlaitsBlobLibrary* resolved[kMaxPlaitsBlobRecords];
  for (size_t i = 0; i < kMaxPlaitsBlobRecords; ++i) {
    resolved[i] = sentinel;
  }
  // No library listed: TERR's record comes first.
  libraries.Resolve(plaits_region.blob(43), resolved);
  CHECK(resolved[0] != sentinel && resolved[0] && resolved[1] == sentinel);
  if (resolved[0] == sentinel || !resolved[0]) {
    return;
  }
  const PlaitsBlobTerrains* terrains =
      static_cast<const PlaitsBlobTerrains*>(resolved[0]->context);
  CHECK(terrains->grids == plaits_region.terrains());
  CHECK(terrains->count == 2);
  CHECK(terrains->grids &&
      terrains->grids[kTerrainSize + 5] == int8_t((kTerrainSize + 5) * 3));
  // An unflagged model gets nothing more.
  resolved[0] = sentinel;
  libraries.Resolve(plaits_region.blob(2), resolved);
  CHECK(resolved[0] == sentinel);

  // A damaged TERR: no grids, the other sections and the record as before.
  flash[kTerrainOffset + 32 + 7] ^= 1;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_ERROR_CRC);
  CHECK(!plaits_region.terrains() && plaits_region.num_terrains() == 0);
  CHECK(plaits_region.waves() && plaits_region.blob(43));
  libraries.Init(
      plaits_region, &plaits_blob_services, ram, sizeof(ram), scratch,
      sizeof(scratch));
  libraries.Resolve(plaits_region.blob(43), resolved);
  terrains = static_cast<const PlaitsBlobTerrains*>(resolved[0]->context);
  CHECK(!terrains->grids && terrains->count == 0);

  // Bad content counts no grids either.
  BuildRegion();
  Write32(flash + kTerrainOffset + 20, 3);
  directory[1].crc = SealSection(flash + kTerrainOffset);
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_TERRAIN) == PLAITS_BLOB_ERROR_CONTENT);
  CHECK(plaits_region.num_terrains() == 0);

  // A damaged WAVE: no rows, so Beads' synth cannot play; TERR unaffected.
  BuildRegion();
  flash[kWaveOffset + 32 + 1000] ^= 1;
  Read(directory, kNumSections);
  CHECK(Status(PLAITS_SECTION_WAVE) == PLAITS_BLOB_ERROR_CRC);
  CHECK(!plaits_region.waves());
  CHECK(plaits_region.num_terrains() == 2);
  // A double ECC error in TERR or WAVE: that section gives nothing.
  BuildRegion();
  plaits_region.set_flash_check(&FakeFlashCheck);
  flash_calls = 0;
  flash_failing_call = 4;
  Read(directory, kNumSections);
  CHECK(!plaits_region.terrains() && plaits_region.num_terrains() == 0);
  CHECK(plaits_region.waves());
  flash_calls = 0;
  flash_failing_call = 2;
  Read(directory, kNumSections);
  CHECK(!plaits_region.waves() && plaits_region.num_terrains() == 2);
  plaits_region.set_flash_check(NULL);
  // Neither section listed.
  BuildRegion();
  Read(directory + 2, 2);
  CHECK(!plaits_region.waves() && !plaits_region.terrains());
  CHECK(plaits_region.num_terrains() == 0);
}

// The voice's bind hands a flagged model TERR's record after all four of its
// libraries, or first when it lists none.
void TestTerrainBind() {
  test_checker.Start("TestTerrainBind");
  uint8_t digests[4][kPlaitsBlobDigestSize];
  uint8_t scratch[256];
  for (int i = 0; i < 4; ++i) {
    MakeBlob(scratch, kPlaitsBlobKindLibrary, i + 1, NULL, 0, digests[i]);
  }
  TestBlob blobs[6] = {
    { kPlaitsBlobKindLibrary, 1, { }, 0 },
    { kPlaitsBlobKindLibrary, 2, { }, 0 },
    { kPlaitsBlobKindLibrary, 3, { }, 0 },
    { kPlaitsBlobKindLibrary, 4, { }, 0 },
    { kPlaitsBlobKindModel, 43, { }, 0 },
    { kPlaitsBlobKindModel, 90, { Needs(4, digests[3]), Needs(2, digests[1]),
        Needs(1, digests[0]), Needs(3, digests[2]) }, 4 },
  };
  BuildRegion();
  model_flags = PLAITS_BLOB_FLAG_TERRAINS;
  const PlaitsDirectoryEntry d[2] = { directory[1], WritePlaits(blobs, 6, NULL) };
  model_flags = 0;
  plaits_region.set_library_ram(kTestLibraryRam, kLibraryScratch);
  plaits_region.Read(
      flash, flash + kFlashSize, kRelease, d, 2, defaults, kNumSections,
      &plaits_blob_services, test_digests, 6);
  CHECK(Status(PLAITS_SECTION_PLAITS) == PLAITS_BLOB_OK);
  CHECK(plaits_region.num_terrains() == 2);
  static uint8_t ram[kTestLibraryRam];
  PlaitsLibraries libraries;
  libraries.Init(
      plaits_region, &plaits_blob_services, ram, sizeof(ram), scratch,
      kLibraryScratch);
  static PlaitsBlobEngine engine;
  engine.Bind(plaits_region.blob(90), &plaits_blob_services, 90, &libraries);
  const PlaitsBlobLibrary* const* records = engine.libraries();
  const uint32_t order[4] = { 4, 2, 1, 3 };
  for (int i = 0; i < 4; ++i) {
    CHECK(records[i] == libraries.Find(order[i]));
  }
  const PlaitsBlobTerrains* t = records[4]
      ? static_cast<const PlaitsBlobTerrains*>(records[4]->context) : NULL;
  CHECK(t && t->count == 2 && t->grids == plaits_region.terrains());
  engine.Bind(plaits_region.blob(43), &plaits_blob_services, 43, &libraries);
  CHECK(engine.libraries()[0] == records[4]);
  plaits_region.set_library_ram(0, 0);
}

int main() {
  static uint8_t storage[kFlashSize + 32];
  flash = storage + (32 - reinterpret_cast<uintptr_t>(storage) % 32) % 32;
  TestSha256();
  TestAccepts();
  TestPlacement();
  TestRefusals();
  TestSynthRefusals();
  TestServicesTable();
  TestServicesFrozen();
  TestSections();
  TestSectionFaults();
  TestDirectory();
  TestPlaitsTable();
  TestRings();
  TestLibraryRefusals();
  TestDependencies();
  TestLibraries();
  TestFadeIn();
  TestFlashErrors();
  TestPlayable();
  TestLocks();
  TestFaultMatrix();
  TestModelFlags();
  TestWavesAndTerrains();
  TestTerrainBind();
  puts("Plaits blob checks: PASS");
  return test_checker.failures() ? 1 : 0;
}
