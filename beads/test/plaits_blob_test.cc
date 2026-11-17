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
// Plaits blob checks: every refusal path, on synthetic blobs.

#include <cstdio>
#include <cstring>

#include "beads/blob/plaits_blob.h"
#include "beads/profile.h"
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

// Header, code bytes, one import, the CRC; the digest list is its own.
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

void Seal(uint8_t* b, bool list) {
  const uint32_t size = reinterpret_cast<PlaitsBlobHeader*>(b)->size;
  Write32(b + size - 4, Profile::Crc32(b, size - 4));
  if (list) {
    PlaitsBlob::Sha256(b, size, digest);
  }
}

PlaitsBlobStatus Check(uint8_t* b) {
  return PlaitsBlob::Check(
      b, region, region + sizeof(region), &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest), 1);
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
      0) == PLAITS_BLOB_ERROR_DIGEST);
}

void TestPlacement() {
  test_checker.Start("TestPlacement");
  Services();
  uint8_t* b = region + 64;
  Build(b);
  Seal(b, true);
  CHECK(PlaitsBlob::Check(b + 4, region, region + sizeof(region), &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1) == PLAITS_BLOB_ERROR_PLACEMENT);
  CHECK(PlaitsBlob::Check(b, region + 96, region + sizeof(region), &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1) == PLAITS_BLOB_ERROR_PLACEMENT);
  CHECK(PlaitsBlob::Check(b, region, b + 32, &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1) == PLAITS_BLOB_ERROR_PLACEMENT);
  CHECK(PlaitsBlob::Check(b, region + sizeof(region), region, &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1) == PLAITS_BLOB_ERROR_PLACEMENT);
  // The blob ends past the region.
  CHECK(PlaitsBlob::Check(b, region, b + kBlobSize - 4, &services,
      reinterpret_cast<const uint8_t (*)[kPlaitsBlobDigestSize]>(digest),
      1) == PLAITS_BLOB_ERROR_HEADER);
}

// Each fault, with the CRC recomputed so that the targeted check fires.
void TestRefusals() {
  test_checker.Start("TestRefusals");
  Services();
  uint8_t* b = region + 64;
  struct {
    int field;
    uint32_t value;
    PlaitsBlobStatus expected;
  } faults[] = {
    { 0, kPlaitsBlobMagic + 1, PLAITS_BLOB_ERROR_HEADER },
    { 1, 60, PLAITS_BLOB_ERROR_HEADER },
    { 5, 0xfffffffc, PLAITS_BLOB_ERROR_HEADER },
    { 5, 254, PLAITS_BLOB_ERROR_HEADER },
    { 5, 64, PLAITS_BLOB_ERROR_HEADER },
    { 2, kPlaitsBlobAbi + 1, PLAITS_BLOB_ERROR_ABI },
    { 3, kPlaitsBlobServicesVersion + 1, PLAITS_BLOB_ERROR_SERVICES },
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
  for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
    PlaitsBlobHeader* h = Build(b);
    uint32_t* words = reinterpret_cast<uint32_t*>(h);
    words[faults[i].field] = faults[i].value;
    if (h->size <= kBlobSize && h->size >= 8) {
      Seal(b, true);
    }
    const bool refused = Check(b) == faults[i].expected;
    if (!refused) {
      printf("ERROR: TestRefusals: field %d = 0x%x gave %d\n",
          faults[i].field, faults[i].value, Check(b));
    }
    CHECK(refused);
  }

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

  // The firmware's own table: another version, or an export moved.
  Build(b);
  Seal(b, true);
  services.version = kPlaitsBlobServicesVersion + 1;
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

void TestServicesTable() {
  test_checker.Start("TestServicesTable");
  CHECK(plaits_blob_services.magic == kPlaitsBlobServicesMagic);
  CHECK(plaits_blob_services.version == kPlaitsBlobServicesVersion);
  CHECK(plaits_blob_services.size == sizeof(PlaitsBlobServices));
  CHECK(plaits_blob_services.num_exports == kNumPlaitsBlobExports);
}

int main() {
  TestSha256();
  TestAccepts();
  TestPlacement();
  TestRefusals();
  TestServicesTable();
  puts("Plaits blob checks: PASS");
  return test_checker.failures() ? 1 : 0;
}
