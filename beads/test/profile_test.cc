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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

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

void TestDefaultPageTwoAndExplicitLayouts() {
  test_checker.Start("TestDefaultPageTwoAndExplicitLayouts");
  Profile embedded;
  CHECK(embedded.Init(kProfileBlock));
  Profile fallback;
  fallback.Defaults();
  CHECK(embedded.Page(4) == FEATURE_FROZEN);
  CHECK(embedded.Page(5) == FEATURE_DELAY);
  CHECK(embedded.Page(6) == FEATURE_REVERB);
  CHECK(fallback.Page(4) == embedded.Page(4));
  CHECK(fallback.Page(5) == embedded.Page(5));
  CHECK(fallback.Page(6) == embedded.Page(6));
  CHECK(kFeatureDefinitions[FEATURE_REVERB].field == 1);
  CHECK(kFeatureDefinitions[FEATURE_DELAY].field == 2);
  CHECK(kFeatureDefinitions[FEATURE_FROZEN].field == 5);
  CHECK(embedded.Page(10) == FEATURE_BRIGHTNESS);
  CHECK(embedded.Page(11) == FEATURE_SEED_LATCH);
  CHECK(fallback.Page(10) == FEATURE_BRIGHTNESS);
  CHECK(fallback.Page(11) == FEATURE_SEED_LATCH);
  CHECK(embedded.entry(FEATURE_BRIGHTNESS).value == 0);
  CHECK(embedded.entry(FEATURE_SEED_LATCH).value == 0);
  CHECK(kFeatureDefinitions[FEATURE_BRIGHTNESS].field == 10);
  CHECK(kFeatureDefinitions[FEATURE_SEED_LATCH].field == 11);
  uint8_t b[kProfileSize];
  Template(b);
  for (size_t i = 0; i < b[5]; ++i) {
    uint8_t* entry = b + 16 + 6 * i;
    if (entry[0] == FEATURE_REVERB) {
      entry[3] = 5;
    } else if (entry[0] == FEATURE_DELAY) {
      entry[3] = 4;
    } else if (entry[0] == FEATURE_FROZEN) {
      entry[3] = 6;
    }
  }
  Checksum(b);
  Profile explicit_layout;
  CHECK(explicit_layout.Init(b));
  CHECK(explicit_layout.Page(4) == FEATURE_DELAY);
  CHECK(explicit_layout.Page(5) == FEATURE_REVERB);
  CHECK(explicit_layout.Page(6) == FEATURE_FROZEN);
  uint8_t legacy[512];
  memcpy(legacy, b, sizeof(legacy));
  legacy[4] = 1;
  Checksum(legacy);
  CHECK(explicit_layout.Init(legacy));
  CHECK(explicit_layout.Page(4) == FEATURE_DELAY);
  CHECK(explicit_layout.Page(5) == FEATURE_REVERB);
  CHECK(explicit_layout.Page(6) == FEATURE_FROZEN);
}

void TestOptionalPageSlots() {
  test_checker.Start("TestOptionalPageSlots");
  Profile embedded;
  CHECK(embedded.Init(kProfileBlock));
  Profile fallback;
  fallback.Defaults();
  // The factory layout fills pages 1-5.
  for (int slot = 1; slot < kNumPageSlots; ++slot) {
    CHECK((embedded.Page(slot) != FEATURE_NONE) == (slot <= 15));
    CHECK(fallback.Page(slot) == embedded.Page(slot));
  }
  CHECK(embedded.Page(1) == FEATURE_PITCH);
  uint8_t b[kProfileSize];
  for (int slot = 16; slot <= kNumPageSlots; ++slot) {
    Template(b);
    uint8_t* pitch = NULL;
    uint8_t* limiter = NULL;
    for (size_t i = 0; i < b[5]; ++i) {
      uint8_t* entry = b + 16 + 6 * i;
      if (entry[0] == FEATURE_PITCH) {
        pitch = entry;
      } else if (entry[0] == FEATURE_LIMITER) {
        limiter = entry;
      }
    }
    CHECK(pitch && limiter);
    pitch[3] = slot;
    Checksum(b);
    Profile p;
    CHECK(p.Init(b) == (slot < kNumPageSlots));
    if (slot < kNumPageSlots) {
      CHECK(p.Page(slot) == FEATURE_PITCH);
      CHECK(p.Page(1) == FEATURE_NONE);
      // Two features on one high slot are refused.
      limiter[3] = slot;
      Checksum(b);
      CHECK(!p.Init(b));
    }
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
  // Format 1 is embedded; the former date ID is refused with a valid CRC.
  Template(b);
  CHECK(b[8] == 1 && !b[9] && !b[10] && !b[11]);
  b[8] = 0x01;
  b[9] = 0x11;
  b[10] = 0x26;
  b[11] = 0x20;
  Checksum(b);
  CHECK(!p.Init(b));
  CHECK(p.Page(1) == FEATURE_PITCH);
  CHECK(p.Action(2) == FEATURE_TRIM);
  Template(b);
  const uint16_t extra_bytes = uint16_t(b[6]) + (uint16_t(b[7]) << 8) + 6;
  b[6] = extra_bytes & 255;
  b[7] = extra_bytes >> 8;
  Checksum(b);
  CHECK(!p.Init(b));
  Template(b);
  b[4] = 1;
  b[12] = 1;
  Checksum(b);
  CHECK(!p.Init(b));
  const uint8_t bad[][2] = {
    { 16, 0 }, { 16, FEATURE_LAST }, { 22, FEATURE_PITCH },
    { 17, 3 }, { 18, 10 }, { 24, 2 }, { 19, 22 }, { 20, 3 }, { 21, 4 },
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
    b[19 + kind] = kind == 0 ? kNumPageSlots : kind == 1 ? 3 : 4;
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
  // Adjustable on a page needs the gesture too (#292); Automatic does not.
  b[17] = PROFILE_ADJUSTABLE;
  b[19] = 9;
  Checksum(b);
  CHECK(!p.Init(b));
  b[21] = ACTION_HOLD;
  Checksum(b);
  CHECK(p.Init(b));
  b[17] = PROFILE_FIXED;
  b[18] = b[19] = b[21] = 0;
  Checksum(b);
  CHECK(p.Init(b));
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
  // An amount slot past the last is refused before it is marked used.
  Template(b);
  b[5] = 1;
  b[6] = 6;
  b[16] = FEATURE_WIDTH;
  b[19] = b[21] = 0;
  b[20] = AMOUNT_LAST;
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
  CHECK(p.Page(10) == FEATURE_NONE);
  CHECK(p.Page(11) == FEATURE_NONE);
}

void TestProfileKinds() {
  test_checker.Start("TestProfileKinds");
  uint8_t b[kProfileSize];
  for (int id = 1; id < FEATURE_LAST; ++id) {
    const FeatureDefinition& d = kFeatureDefinitions[id];
    for (int kind = 0; kind < 3; ++kind) {
      const int max_slot = kind == 0 ? 21 : 3;
      for (int slot = 1; slot <= max_slot + 1; ++slot) {
        Template(b);
        b[5] = 1;
        b[6] = 6;
        b[16] = id;
        const bool manual_delay = id == FEATURE_MANUAL_DELAY;
        b[17] = manual_delay ? PROFILE_FIXED : PROFILE_ADJUSTABLE;
        b[18] = manual_delay ? 1 : 0;
        b[19] = b[20] = b[21] = 0;
        b[19 + kind] = slot;
        Checksum(b);
        Profile p;
        // Only Synth start takes the [A] hold, and it needs a gesture.
        const bool hold = kind == 2 && slot == ACTION_HOLD;
        const bool synth_start = id == FEATURE_SYNTH_START;
        // Without a gain entry, manual gain keeps FEEDBACK (amount slot 3).
        const bool feedback = kind == 1 && slot == AMOUNT_FEEDBACK &&
            id != FEATURE_GAIN_KNOB;
        // Plaits play keeps [A]+FREEZE, so the Synth toggle goes on SEED.
        const bool synth_freeze = kind == 2 && slot == ACTION_FREEZE &&
            id == FEATURE_SYNTH;
        const bool valid = slot <= max_slot &&
            (kind == 0 ? d.page : kind == 1 ? d.amount : d.action) &&
            (!hold || synth_start) && (!synth_start || kind == 2) &&
            !feedback && !synth_freeze;
        CHECK(p.Init(b) == valid);
        if (kind == 0 && valid) {
          CHECK(p.Page(slot) == ProfileFeature(id));
        }
      }
    }
  }
}

void TestBetaProfileDefinitions() {
  test_checker.Start("TestBetaProfileDefinitions");
  uint8_t b[kProfileSize];
  CHECK(kFeatureDefinitions[FEATURE_BRIGHTNESS].values == 3);
  const uint8_t fields[] = {
    255, 255, 255, 10, 255, 255, 11, 255, 12, 255, 255, 255,
    255, 255, 255, 255, 14, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    kNoField  // Manual delay toggle.
  };
  CHECK(sizeof(fields) == FEATURE_LAST - FEATURE_VERSION);
  // The factory profile lists the Plaits bank and Rings' polyphony; older
  // profiles imply them.
  Template(b);
  Profile factory;
  CHECK(factory.Init(b));
  CHECK(factory.adjustable(FEATURE_PLAITS_BANK));
  CHECK(!factory.entry(FEATURE_PLAITS_BANK).value);
  // Move the bank's entry last, then drop it.
  uint8_t* bank = b + 16;
  while (bank[0] != FEATURE_PLAITS_BANK) {
    bank += 6;
  }
  uint8_t last[6];
  memcpy(last, b + 16 + 6 * (b[5] - 1), 6);
  memcpy(b + 16 + 6 * (b[5] - 1), bank, 6);
  memcpy(bank, last, 6);
  CHECK(b[16 + 6 * (b[5] - 1)] == FEATURE_PLAITS_BANK);
  --b[5];
  b[6] -= 6;
  Checksum(b);
  CHECK(factory.Init(b));
  CHECK(factory.adjustable(FEATURE_PLAITS_BANK));
  CHECK(factory.entry(FEATURE_PLAITS_BANK).feature == FEATURE_PLAITS_BANK);
  // Rings' polyphony too: moved last, dropped, it is still Adjustable at 0.
  uint8_t* poly = b + 16;
  while (poly[0] != FEATURE_RINGS_POLYPHONY) {
    poly += 6;
  }
  CHECK(factory.adjustable(FEATURE_RINGS_POLYPHONY));
  CHECK(!factory.entry(FEATURE_RINGS_POLYPHONY).value);
  memcpy(last, b + 16 + 6 * (b[5] - 1), 6);
  memcpy(b + 16 + 6 * (b[5] - 1), poly, 6);
  memcpy(poly, last, 6);
  --b[5];
  b[6] -= 6;
  Checksum(b);
  CHECK(factory.Init(b));
  CHECK(factory.adjustable(FEATURE_RINGS_POLYPHONY));
  CHECK(factory.entry(FEATURE_RINGS_POLYPHONY).feature ==
      FEATURE_RINGS_POLYPHONY);
  CHECK(!factory.entry(FEATURE_RINGS_POLYPHONY).value);
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
        // Adjustable needs a page or saved gesture (bank/polyphony only use it).
        // Uncounted full-byte values accept any byte; reserved IDs accept none.
        const bool stepped = id == FEATURE_PLAITS_BANK ||
            id == FEATURE_RINGS_POLYPHONY;
        const bool valid =
            kFeatureDefinitions[id].field != kReservedField &&
            (!kFeatureDefinitions[id].values ||
            value < kFeatureDefinitions[id].values) &&
            (state != PROFILE_ADJUSTABLE || kFeatureDefinitions[id].page ||
                kFeatureDefinitions[id].field != kNoField || stepped) &&
            (!stepped || state == PROFILE_ADJUSTABLE);
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

// Accepts and parses as Beta 3 did: a digest of every result over single
// entries and amount and action pairs in the Beta 3 vocabulary.
uint32_t Beta3Digest(uint32_t* accepted) {
  const uint8_t toggles[] = {
    FEATURE_REVERB, FEATURE_PROCESSOR, FEATURE_FROZEN, FEATURE_SYNTH,
    FEATURE_TIME_SCAN
  };
  const uint8_t pages[] = { 0, 1, 9, 21, 22 };
  const uint8_t values[] = { 0, 1, 2, 3, 4, 5, 15, 16, 255 };
  const int last = FEATURE_START_QUALITY + 1;
  uint8_t b[kProfileSize];
  std::vector<uint8_t> results;
  std::vector<uint8_t> cases;
  for (int id = 0; id < last; ++id) {
    for (int state = 0; state <= 3; ++state) {
      for (size_t v = 0; v < sizeof(values); ++v) {
        for (size_t page = 0; page < sizeof(pages); ++page) {
          for (int amount = 0; amount <= 3; ++amount) {
            for (int action = 0; action <= 4; ++action) {
              bool toggle = false;
              for (size_t t = 0; t < sizeof(toggles); ++t) {
                toggle = toggle || (id == toggles[t] && action);
              }
              if (toggle) {
                continue;
              }
              const uint8_t entry[] = {
                uint8_t(id), uint8_t(state), values[v], pages[page],
                uint8_t(amount), uint8_t(action)
              };
              cases.insert(cases.end(), entry, entry + 6);
              cases.insert(cases.end(), 6, 0);
            }
          }
        }
      }
    }
  }
  const uint8_t pairs[][2] = {
    { FEATURE_WIDTH, FEATURE_DIFFUSION },
    { FEATURE_MONOPHONIC, FEATURE_TRIM },
    { FEATURE_MONOPHONIC, FEATURE_SYNTH_START },
    { FEATURE_TRIM, FEATURE_SYNTH_START }
  };
  for (size_t p = 0; p < 4; ++p) {
    const int k = p ? 5 : 4;
    for (int a = 0; a <= 3; ++a) {
      for (int c = 0; c <= 3; ++c) {
        uint8_t entries[12] = { 0 };
        entries[0] = pairs[p][0];
        entries[6] = pairs[p][1];
        entries[1] = entries[7] = PROFILE_ADJUSTABLE;
        entries[k] = a;
        entries[6 + k] = c;
        cases.insert(cases.end(), entries, entries + 12);
      }
    }
  }
  *accepted = 0;
  for (size_t i = 0; i < cases.size(); i += 12) {
    const int count = cases[i + 6] ? 2 : 1;
    for (size_t j = 0; j < kProfileSize; ++j) {
      b[j] = kProfileBlock[j];
    }
    b[5] = count;
    b[6] = 6 * count;
    b[7] = 0;
    memcpy(b + 16, &cases[i], 6 * count);
    const uint32_t crc = Profile::Crc32(b, 16 + 6 * count);
    for (int j = 0; j < 4; ++j) {
      b[16 + 6 * count + j] = crc >> (8 * j);
    }
    Profile parsed;
    const bool ok = parsed.Init(b);
    // Beta 3 refused Synth value 2; as Rings it is accepted where Plaits is.
    if (cases[i] == FEATURE_SYNTH && cases[i + 2] == 2) {
      b[18] = 1;
      const uint32_t plaits_crc = Profile::Crc32(b, 16 + 6 * count);
      for (int j = 0; j < 4; ++j) {
        b[16 + 6 * count + j] = plaits_crc >> (8 * j);
      }
      Profile plaits;
      CHECK(ok == plaits.Init(b));
      results.push_back(false);
      continue;
    }
    // Beta 3 refused Send; it has the same valid placements as Gentle.
    if (cases[i] == FEATURE_MIX_LAW && cases[i + 2] == 2) {
      b[18] = 1;
      const uint32_t gentle_crc = Profile::Crc32(b, 16 + 6 * count);
      for (int j = 0; j < 4; ++j) {
        b[16 + 6 * count + j] = gentle_crc >> (8 * j);
      }
      Profile gentle;
      CHECK(ok == gentle.Init(b));
      results.push_back(false);
      continue;
    }
    results.push_back(ok);
    if (!ok) {
      continue;
    }
    ++*accepted;
    for (int id = 1; id < last; ++id) {
      const uint8_t* e = reinterpret_cast<const uint8_t*>(
          &parsed.entry(ProfileFeature(id)));
      results.insert(results.end(), e, e + sizeof(ProfileEntry));
    }
    for (int slot = 1; slot < kNumPageSlots; ++slot) {
      results.push_back(parsed.Page(slot));
    }
    for (int slot = 1; slot <= 2; ++slot) {
      results.push_back(parsed.Amount(slot));
    }
    for (int slot = 1; slot <= 3; ++slot) {
      results.push_back(parsed.Action(slot));
    }
  }
  return Profile::Crc32(&results[0], results.size());
}

void TestBeta3Profiles() {
  test_checker.Start("TestBeta3Profiles");
  uint32_t accepted = 0;
  // Measured on the parser before the FEEDBACK slot and the toggles.
  CHECK(Beta3Digest(&accepted) == 0x1adfe3e4);
  CHECK(accepted == 481);
  Profile factory;
  CHECK(factory.Init(kProfileBlock));
  CHECK(factory.entry(FEATURE_GAIN_KNOB).state == PROFILE_OFF);
  CHECK(factory.Amount(AMOUNT_DRY_WET) == FEATURE_WIDTH);
  CHECK(factory.Amount(AMOUNT_REVERB) == FEATURE_DIFFUSION);
  CHECK(factory.Amount(AMOUNT_FEEDBACK) == FEATURE_GAIN_KNOB);
  CHECK(factory.Action(ACTION_SEED) == FEATURE_MONOPHONIC);
  CHECK(factory.Action(ACTION_FREEZE) == FEATURE_TRIM);
  CHECK(factory.Action(ACTION_HOLD) == FEATURE_SYNTH_START);
  CHECK(factory.Amount(AMOUNT_NONE) == FEATURE_NONE);
  Profile fallback;
  fallback.Defaults();
  CHECK(fallback.Amount(AMOUNT_FEEDBACK) == FEATURE_GAIN_KNOB);
}

// Lists entries after the factory header, without scales.
bool ParseEntries(const uint8_t (*entries)[6], int count, Profile* p) {
  uint8_t b[kProfileSize];
  Template(b);
  b[5] = count;
  b[6] = 6 * count;
  b[7] = 0;
  for (int i = 0; i < count; ++i) {
    memcpy(b + 16 + 6 * i, entries[i], 6);
  }
  Checksum(b);
  return p->Init(b);
}

// Rings is Synth value 2: legal fixed, as a default and with an action.
void TestRingsSynthProfiles() {
  test_checker.Start("TestRingsSynthProfiles");
  CHECK(kFeatureDefinitions[FEATURE_SYNTH].values == 3);
  const FeatureDefinition& d = kFeatureDefinitions[FEATURE_RINGS_POLYPHONY];
  CHECK(d.values == 3 && d.field == kNoField && !d.page && !d.action);
  for (int state = PROFILE_OFF; state <= PROFILE_ADJUSTABLE; ++state) {
    for (int value = 0; value <= 3; ++value) {
      for (int action = 0; action <= ACTION_FREEZE; ++action) {
        const uint8_t entry[1][6] = {
          { FEATURE_SYNTH, uint8_t(state), uint8_t(value), 0, 0,
            uint8_t(action) }
        };
        Profile p;
        const bool valid = value < 3 && action != ACTION_FREEZE &&
            (!action || state == PROFILE_ADJUSTABLE);
        CHECK(ParseEntries(entry, 1, &p) == valid);
        if (valid) {
          CHECK(p.entry(FEATURE_SYNTH).value == value);
          CHECK(p.Action(action) == (action ? FEATURE_SYNTH : FEATURE_NONE));
        }
      }
    }
  }
}

// Manual gain is on FEEDBACK unless the profile moves it to an amount slot
// or fixes it off the knobs; each slot holds one setting.
void TestGainKnobSlot() {
  test_checker.Start("TestGainKnobSlot");
  const FeatureDefinition& d = kFeatureDefinitions[FEATURE_GAIN_KNOB];
  CHECK(d.values == 1 && d.field == kNoField && d.amount);
  CHECK(!d.page && !d.action && !d.stateless);
  const uint8_t gain = FEATURE_GAIN_KNOB;
  const uint8_t width = FEATURE_WIDTH;
  const uint8_t diff = FEATURE_DIFFUSION;
  const uint8_t adj = PROFILE_ADJUSTABLE;
  const uint8_t fixed = PROFILE_FIXED;
  struct Case {
    uint8_t entries[3][6];
    int count;
    bool valid;
    uint8_t amounts[3];  // DRY/WET, REVERB, FEEDBACK.
  };
  const Case cases[] = {
    // A FEEDBACK amount needs manual gain moved or fixed.
    { { { width, adj, 0, 0, 3, 0 } }, 1, false, { 0 } },
    { { { gain, 0, 0, 0, 0, 0 }, { width, adj, 0, 0, 3, 0 } },
        2, false, { 0 } },
    { { { gain, fixed, 0, 0, 0, 0 }, { width, adj, 0, 0, 3, 0 } },
        2, true, { 0, 0, width } },
    { { { gain, adj, 0, 0, 3, 0 }, { width, adj, 0, 0, 3, 0 } },
        2, false, { 0 } },
    { { { gain, adj, 0, 0, 1, 0 }, { width, adj, 0, 0, 1, 0 } },
        2, false, { 0 } },
    { { { gain, adj, 0, 0, 1, 0 }, { width, adj, 0, 0, 3, 0 },
        { diff, adj, 0, 0, 2, 0 } }, 3, true, { gain, diff, width } },
    { { { diff, adj, 0, 0, 3, 0 }, { gain, adj, 0, 0, 3, 0 } },
        2, false, { 0 } },
    { { { diff, adj, 0, 0, 3, 0 }, { gain, adj, 0, 0, 2, 0 } },
        2, true, { 0, gain, diff } },
    // Off is stock, Fixed takes it off the knobs, Adjustable needs a slot.
    { { { gain, adj, 0, 0, 2, 0 } }, 1, true, { 0, gain, 0 } },
    { { { gain, adj, 0, 0, 3, 0 } }, 1, true, { 0, 0, gain } },
    { { { gain, fixed, 0, 0, 0, 0 } }, 1, true, { 0, 0, 0 } },
    { { { gain, 0, 0, 0, 0, 0 } }, 1, true, { 0, 0, gain } },
    { { { gain, adj, 0, 0, 0, 0 } }, 1, false, { 0 } },
    { { { gain, fixed, 1, 0, 0, 0 } }, 1, false, { 0 } },
    { { { gain, adj, 1, 0, 2, 0 } }, 1, false, { 0 } },
    { { { gain, adj, 0, 1, 2, 0 } }, 1, false, { 0 } },
    { { { gain, adj, 0, 0, 2, 1 } }, 1, false, { 0 } },
    { { { gain, fixed, 0, 0, 2, 0 } }, 1, false, { 0 } },
    { { { gain, 0, 0, 0, 2, 0 } }, 1, false, { 0 } },
    { { { gain, adj, 0, 0, 4, 0 } }, 1, false, { 0 } },
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    Profile p;
    const bool valid = ParseEntries(cases[i].entries, cases[i].count, &p);
    if (!CHECK(valid == cases[i].valid)) {
      printf("case %d\n", int(i));
    }
    if (valid) {
      for (int slot = AMOUNT_DRY_WET; slot <= AMOUNT_FEEDBACK; ++slot) {
        CHECK(p.Amount(slot) == cases[i].amounts[slot - 1]);
      }
    }
  }
}

// The embedded factory profile and the fallback hold the same entries.
void TestFactoryProfileIsDefaults() {
  test_checker.Start("TestFactoryProfileIsDefaults");
  Profile factory;
  CHECK(factory.Init(kProfileBlock));
  Profile fallback;
  fallback.Defaults();
  for (int i = FEATURE_PITCH; i < FEATURE_LAST; ++i) {
    const ProfileFeature feature = ProfileFeature(i);
    if (!CHECK(!memcmp(&factory.entry(feature), &fallback.entry(feature),
        sizeof(ProfileEntry)))) {
      printf("feature %d\n", i);
    }
  }
}

// Hygrostat's codec reads the same cases and must give the same answers.
void TestSharedCases() {
  test_checker.Start("TestSharedCases");
  FILE* file = fopen("beads/test/profile_cases.txt", "r");
  if (!CHECK(file)) {
    return;
  }
  char line[512];
  int cases[2] = { 0, 0 };
  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#') {
      continue;
    }
    for (char* c = line; *c; ++c) {
      if (*c == ',' || *c == '\n') {
        *c = ' ';
      }
    }
    const bool accept = !strncmp(line, "accept ", 7);
    CHECK(accept || !strncmp(line, "reject ", 7));
    const char* name = strtok(line + 7, " ");
    bool corrupt = false;
    uint32_t format = kProfileFormat;
    char* t = strtok(NULL, " ");
    for (; t && strcmp(t, ":"); t = strtok(NULL, " ")) {
      corrupt = corrupt || !strcmp(t, "crc");
      if (!strncmp(t, "format=", 7)) {
        format = strtoul(t + 7, NULL, 10);
      }
    }
    uint8_t b[kProfileSize];
    Template(b);
    memset(b + 16, 0xff, kProfileSize - 16);
    size_t size = 0;
    for (t = strtok(NULL, " "); t && size < 6 * 82; t = strtok(NULL, " ")) {
      b[16 + size++] = atoi(t);
    }
    CHECK(size % 6 == 0);
    b[5] = size / 6;
    b[6] = size;
    b[7] = size >> 8;
    for (int i = 0; i < 4; ++i) {
      b[8 + i] = format >> (8 * i);
    }
    Checksum(b);
    b[16 + size] ^= corrupt ? 1 : 0;
    Profile profile;
    if (!CHECK(profile.Init(b) == accept)) {
      printf("case %s\n", name);
    }
    ++cases[accept];
  }
  fclose(file);
  CHECK(cases[0] >= 20 && cases[1] >= 15);
}

int main() {
  TestOptionalPageSlots();
  TestDefaultPageTwoAndExplicitLayouts();
  TestProfileParser();
  TestCustomScaleBank();
  TestProfileKinds();
  TestBetaProfileDefinitions();
  TestBeta3Profiles();
  TestRingsSynthProfiles();
  TestGainKnobSlot();
  TestFactoryProfileIsDefaults();
  TestSharedCases();
  puts("Profile validation: PASS");
  return test_checker.failures() ? 1 : 0;
}
