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
// Profile parser boundaries and placement kinds.

#include <cstring>
#include <cstdio>

#include "beads/profile.h"
#include "beads/test/test_checker.h"

using namespace beads;

void Template(uint8_t* b) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
}

void Checksum(uint8_t* b) {
  const size_t size = 16 + b[5] * 6;
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

void TestProfileParser() {
  test_checker.Start("TestProfileParser");
  uint8_t b[kProfileSize];
  Template(b);
  Profile p;
  CHECK(p.Init(b));
  Profile defaults;
  defaults.Defaults();
  for (int i = 1; i < FEATURE_LAST; ++i) {
    CHECK(!memcmp(&p.entry(ProfileFeature(i)),
        &defaults.entry(ProfileFeature(i)), sizeof(ProfileEntry)));
  }
  memset(b, 255, sizeof(b));
  CHECK(!p.Init(b));
  CHECK(p.Page(1) == FEATURE_PITCH);
  CHECK(Profile::Crc32(reinterpret_cast<const uint8_t*>("123456789"), 9)
      == 0xcbf43926);
  for (size_t i = 0; i < 104; ++i) {
    Template(b);
    b[i] ^= 0x80;
    CHECK(!p.Init(b));
    CHECK(p.Page(1) == FEATURE_PITCH);
    CHECK(p.Action(2) == FEATURE_TRIM);
  }
  const int header_offsets[] = { 0, 4, 5, 6, 7, 8, 12, 13, 14, 15 };
  for (size_t i = 0;
       i < sizeof(header_offsets) / sizeof(header_offsets[0]);
       ++i) {
    Template(b);
    b[header_offsets[i]] ^= 1;
    if (header_offsets[i] != 5 && header_offsets[i] != 6 &&
        header_offsets[i] != 7) {
      Checksum(b);
    }
    CHECK(!p.Init(b));
  }
  const uint8_t bad[][2] = {
    { 16, 0 }, { 16, FEATURE_LAST }, { 22, FEATURE_PITCH },
    { 17, 3 }, { 18, 10 }, { 19, 13 }, { 20, 3 }, { 21, 4 },
    { 17, PROFILE_OFF }, { 17, PROFILE_FIXED },
    { 20, 1 }, { 21, 1 }, { 25, 1 },
    { 79, 1 }, { 86, 1 }, { 93, 2 }, { 95, 0 }, { 96, 0 }
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
    Template(b);
    b[bad[i][0]] = bad[i][1];
    Checksum(b);
    CHECK(!p.Init(b));
  }
  Template(b);
  b[89] = PROFILE_FIXED;
  b[90] = 1;
  Checksum(b);
  CHECK(!p.Init(b));
  for (int kind = 0; kind < 3; ++kind) {
    Template(b);
    b[5] = 1;
    b[6] = 6;
    b[16] = kind == 0 ? FEATURE_PITCH :
        kind == 1 ? FEATURE_WIDTH : FEATURE_MONOPHONIC;
    b[19] = b[20] = b[21] = 0;
    b[19 + kind] = kind == 0 ? 13 : kind == 1 ? 3 : 4;
    Checksum(b);
    CHECK(!p.Init(b));
  }
  Template(b);
  b[5] = 1;
  b[6] = 6;
  b[16] = FEATURE_SYNTH_START;
  b[17] = PROFILE_FIXED;
  b[18] = 1;
  b[19] = b[20] = b[21] = 0;
  Checksum(b);
  CHECK(!p.Init(b));
  for (int kind = 0; kind < 3; ++kind) {
    Template(b);
    b[5] = 1;
    b[6] = 6;
    b[16] = kind == 0 ? FEATURE_PITCH :
        kind == 1 ? FEATURE_WIDTH : FEATURE_MONOPHONIC;
    b[17] = PROFILE_OFF;
    b[19] = b[20] = b[21] = 0;
    b[19 + kind] = 1;
    Checksum(b);
    CHECK(!p.Init(b));
  }
  Template(b);
  b[5] = 1;
  b[6] = 6;
  b[16] = b[19] = 0;
  Checksum(b);
  CHECK(!p.Init(b));
  Template(b);
  b[5] = 1;
  b[6] = 6;
  b[17] = 3;
  b[19] = 0;
  Checksum(b);
  CHECK(!p.Init(b));
  Template(b);
  b[5] = 82;
  b[6] = 236;
  b[7] = 1;
  Checksum(b);
  CHECK(!p.Init(b));
  Template(b);
  b[5] = 83;
  b[6] = 242;
  b[7] = 1;
  CHECK(!p.Init(b));
  Template(b);
  b[5] = b[6] = b[7] = 0;
  Checksum(b);
  CHECK(p.Init(b));
  CHECK(p.Page(1) == FEATURE_NONE);
}

void TestProfileKinds() {
  test_checker.Start("TestProfileKinds");
  uint8_t b[kProfileSize];
  for (int id = 1; id < FEATURE_LAST; ++id) {
    const FeatureDefinition& d = kFeatureDefinitions[id];
    for (int kind = 0; kind < 3; ++kind) {
      const int max_slot = kind == 0 ? 12 : kind == 1 ? 2 : 3;
      for (int slot = 1; slot <= max_slot; ++slot) {
        Template(b);
        b[5] = 1;
        b[6] = 6;
        b[16] = id;
        b[17] = PROFILE_ADJUSTABLE;
        b[18] = 0;
        b[19] = b[20] = b[21] = 0;
        b[19 + kind] = slot;
        Checksum(b);
        Profile p;
        CHECK(p.Init(b) ==
              (kind == 0 ? d.page : kind == 1 ? d.amount : d.action));
      }
    }
  }
}

void TestBetaProfileDefinitions() {
  test_checker.Start("TestBetaProfileDefinitions");
  uint8_t b[kProfileSize];
  CHECK(kFeatureDefinitions[FEATURE_BRIGHTNESS].values == 3);
  const uint8_t fields[] = { 255, 255, 255, 10, 255, 255, 11, 255, 12 };
  for (int id = FEATURE_VERSION; id < FEATURE_LAST; ++id) {
    CHECK(kFeatureDefinitions[id].field == fields[id - FEATURE_VERSION]);
    for (int state = PROFILE_OFF; state <= PROFILE_ADJUSTABLE; ++state) {
      for (int value = 0; value <= kFeatureDefinitions[id].values; ++value) {
        Template(b);
        b[5] = 1;
        b[6] = 6;
        b[16] = id;
        b[17] = state;
        b[18] = value;
        b[19] = b[20] = b[21] = 0;
        Checksum(b);
        Profile p;
        const bool valid = value < kFeatureDefinitions[id].values &&
            (state != PROFILE_ADJUSTABLE || kFeatureDefinitions[id].page);
        CHECK(p.Init(b) == valid);
      }
    }
  }
}

int main() {
  TestProfileParser();
  TestProfileKinds();
  TestBetaProfileDefinitions();
  puts("Profile validation: PASS");
  return test_checker.failures() ? 1 : 0;
}
