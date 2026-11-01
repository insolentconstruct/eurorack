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
  const size_t size = 16 + uint16_t(b[6]) + (uint16_t(b[7]) << 8);
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
    if (header_offsets[i] == 12) {
      b[12] = 2;
    }
    if (header_offsets[i] != 5 && header_offsets[i] != 6 &&
        header_offsets[i] != 7) {
      Checksum(b);
    }
    CHECK(!p.Init(b));
  }
  Template(b);
  b[6] = 90;
  Checksum(b);
  CHECK(!p.Init(b));
  Template(b);
  b[4] = 1;
  b[12] = 1;
  Checksum(b);
  CHECK(!p.Init(b));
  const uint8_t bad[][2] = {
    { 16, 0 }, { 16, FEATURE_LAST }, { 22, FEATURE_PITCH },
    { 17, 3 }, { 18, 10 }, { 24, 2 }, { 19, 13 }, { 20, 3 }, { 21, 4 },
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
  const uint8_t fields[] = { 255, 255, 255, 10, 255, 255, 11, 255, 12, 255 };
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

void PutFloat(uint8_t* b, float value) {
  uint32_t bits;
  memcpy(&bits, &value, 4);
  for (int i = 0; i < 4; ++i) {
    b[i] = bits >> (8 * i);
  }
}

void CustomBank(uint8_t* b) {
  Template(b);
  b[5] = 1;
  b[6] = uint8_t(6 + 2 * kScaleRecordSize);
  b[7] = (6 + 2 * kScaleRecordSize) >> 8;
  b[12] = 1;
  b[13] = 2;
  b[16] = FEATURE_PITCH;
  b[17] = PROFILE_ADJUSTABLE;
  b[18] = 15;
  b[19] = 1;
  b[20] = b[21] = 0;
  for (int i = 0; i < 2; ++i) {
    uint8_t* scale = b + 22 + i * kScaleRecordSize;
    memset(scale, 0, kScaleRecordSize);
    scale[0] = i ? 3 : 15;
    scale[1] = 3;
    scale[2] = 17 + i;
    scale[3] = 83;
    scale[4] = 199;
    scale[5] = i;
    PutFloat(scale + 8, 13.0f);
    memcpy(scale + 12, i ? "Second" : "First", 6);
    PutFloat(scale + 48, 3.25f);
    PutFloat(scale + 52, 8.125f);
  }
  Checksum(b);
}

void TestCustomScaleBank() {
  test_checker.Start("TestCustomScaleBank");
  uint8_t b[kProfileSize];
  CustomBank(b);
  Profile p;
  CHECK(p.Init(b));
  CHECK(p.custom_bank() && p.scale_count() == 2);
  CHECK(p.FirstScale() == 15 && p.NextScale(PitchScale(15)) == 3);
  CHECK(p.NextScale(PITCH_SCALE_MAJOR) == 15);
  CHECK(!p.HasScale(2) && p.HasScale(0) && p.HasScale(1));
  CHECK(p.FindScale(15)->notes[2] == 8.125f);
  CHECK(p.FindScale(15)->color[1] == 83);
  b[18] = 2;
  Checksum(b);
  CHECK(!p.Init(b));
  CustomBank(b);
  CHECK(p.Init(b));
  // Swapping records changes the cycle, not the saved scale identity.
  uint8_t temporary[kScaleRecordSize];
  memcpy(temporary, b + 22, sizeof(temporary));
  memcpy(b + 22, b + 22 + kScaleRecordSize, sizeof(temporary));
  memcpy(b + 22 + kScaleRecordSize, temporary, sizeof(temporary));
  Checksum(b);
  CHECK(p.Init(b));
  CHECK(p.FirstScale() == 3 && p.entry(FEATURE_PITCH).value == 15);
  CHECK(p.FindScale(15)->notes[2] == 8.125f);
  // Every type of invalid record is rejected even with a correct checksum.
  const int bad_offsets[] = { 0, 1, 5, 6, 7, 11, 12, 43, 44, 56, 140 };
  const uint8_t bad_values[] = { 1, 25, 4, 1, 1, 255, 31, 1, 1, 1, 1 };
  for (size_t i = 0; i < sizeof(bad_offsets) / sizeof(bad_offsets[0]); ++i) {
    CustomBank(b);
    b[22 + bad_offsets[i]] = bad_values[i];
    Checksum(b);
    CHECK(!p.Init(b));
    CHECK(!p.custom_bank() && p.FirstScale() == PITCH_SCALE_FIFTHS);
  }
  CustomBank(b);
  b[22 + kScaleRecordSize] = 15;
  Checksum(b);
  CHECK(!p.Init(b));
  CustomBank(b);
  PutFloat(b + 22 + 48, 13.0f);
  Checksum(b);
  CHECK(!p.Init(b));
  CustomBank(b);
  PutFloat(b + 22 + 52, 3.25f);
  Checksum(b);
  CHECK(!p.Init(b));
  CustomBank(b);
  const uint8_t nan[] = { 0, 0, 192, 127 };
  memcpy(b + 22 + 8, nan, 4);
  Checksum(b);
  CHECK(!p.Init(b));
  const uint32_t malformed[] = {
    0x80000000, 0x00000001, 0x007fffff, 0x7f800000, 0x7fc00000
  };
  for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
    for (int field = 0; field < 2; ++field) {
      CustomBank(b);
      const size_t offset = 22 + (field ? 48 : 8);
      for (int byte = 0; byte < 4; ++byte) {
        b[offset + byte] = malformed[i] >> (8 * byte);
      }
      Checksum(b);
      CHECK(!p.Init(b));
    }
  }
  CustomBank(b);
  PutFloat(b + 22 + 8, 0.01f);
  PutFloat(b + 22 + 48, 0.002f);
  PutFloat(b + 22 + 52, 0.008f);
  Checksum(b);
  CHECK(p.Init(b));
  CustomBank(b);
  PutFloat(b + 22 + 8, 0.009f);
  PutFloat(b + 22 + 48, 0.002f);
  PutFloat(b + 22 + 52, 0.008f);
  Checksum(b);
  CHECK(!p.Init(b));
  CustomBank(b);
  b[22 + 47] = 128;  // Root -0 is numerically zero but not canonical +0.
  Checksum(b);
  CHECK(!p.Init(b));
  // Canonical UTF-8 labels work; overlongs, surrogates and controls do not.
  const uint8_t names[][5] = {
    { 0xe2, 0x82, 0xac, 0, 0 }, { 0xf0, 0x9f, 0x8e, 0xb5, 0 },
    { 0xc0, 0xaf, 0, 0, 0 }, { 0xed, 0xa0, 0x80, 0, 0 },
    { 0xc2, 0x80, 0, 0, 0 }, { 0xf4, 0x90, 0x80, 0x80, 0 }
  };
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    CustomBank(b);
    memset(b + 22 + 12, 0, kScaleNameSize);
    memcpy(b + 22 + 12, names[i], sizeof(names[i]));
    Checksum(b);
    CHECK(p.Init(b) == (i < 2));
  }
  CustomBank(b);
  b[13] = kMaxCustomScales;
  const size_t bytes = 6 + kMaxCustomScales * kScaleRecordSize;
  b[6] = bytes & 255;
  b[7] = bytes >> 8;
  for (size_t i = 0; i < kMaxCustomScales; ++i) {
    uint8_t* scale = b + 22 + i * kScaleRecordSize;
    memset(scale, 0, kScaleRecordSize);
    scale[0] = i + 2;
    scale[1] = kMaxScaleDegrees;
    memcpy(scale + 12, "Maximum", 7);
    PutFloat(scale + 8, 13.0f);
    for (size_t j = 0; j < kMaxScaleDegrees; ++j) {
      PutFloat(scale + 44 + 4 * j, float(j) * 0.5f);
    }
  }
  Checksum(b);
  CHECK(p.Init(b));
  CHECK(p.scale_count() == kMaxCustomScales);
  CHECK(p.FindScale(15)->size == kMaxScaleDegrees);
  CHECK(p.FindScale(15)->notes[23] == 11.5f);
  // A valid empty cycle is distinct from the factory bank.
  Template(b);
  b[12] = 1;
  Checksum(b);
  CHECK(p.Init(b));
  CHECK(p.custom_bank() && p.scale_count() == 0);
  CHECK(p.FirstScale() == PITCH_SCALE_OFF);
  // Legacy input may really be only 512 bytes; ASan checks no extension reads.
  uint8_t legacy[512];
  Template(b);
  memcpy(legacy, b, sizeof(legacy));
  legacy[4] = 1;
  Checksum(legacy);
  CHECK(p.Init(legacy));
  CHECK(!p.custom_bank() && p.scale_count() == 8);
}

int main() {
  TestProfileParser();
  TestCustomScaleBank();
  TestProfileKinds();
  TestBetaProfileDefinitions();
  puts("Profile validation: PASS");
  return test_checker.failures() ? 1 : 0;
}
