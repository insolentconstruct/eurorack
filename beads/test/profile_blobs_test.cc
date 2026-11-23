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
// Version 3 profiles in development builds: the bank table, the region
// directory and their refusals.

#include <cstdio>
#include <cstring>

#include "beads/profile.h"
#include "beads/test/profile_v3.h"
#include "beads/test/test_checker.h"

using namespace beads;

const uint8_t kBanks[4 * 8] = {
  18, 43, 36, 0, 1, 2, 3, 4,
  5, 6, 7, 8, 9, 10, 11, 12,
  13, 14, 15, 16, 17, 19, 20, 21,
  22, 23, 28, 97, 98, 99, 43, 18
};

const PlaitsDirectoryEntry kDirectory[2] = {
  { kPlaitsSectionWave, 0, 100396, 0x3de3982b },
  { kPlaitsSectionPlaits, 100416, 8400, 0x90824a8b }
};

bool StockBanks(const Profile& p) {
  const uint8_t first[3] = { 8, 16, 0 };
  bool ok = p.num_plaits_banks() == 3 && !p.directory() &&
      !p.num_sections();
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 8; ++j) {
      ok = ok && p.plaits_bank(i)[j] == first[i] + j;
    }
  }
  return ok;
}

void TestBankTable() {
  test_checker.Start("TestBankTable");
  uint8_t b[kProfileSize];
  Profile p;
  ProfileV3(b, kBanks, 4, kDirectory, 2, 3);
  CHECK(p.Init(b));
  CHECK(p.num_plaits_banks() == 4);
  CHECK(!memcmp(p.plaits_bank(0), kBanks, 8));
  CHECK(!memcmp(p.plaits_bank(3), kBanks + 24, 8));
  CHECK(p.entry(FEATURE_PLAITS_BANK).value == 3);
  CHECK(p.Values(FEATURE_PLAITS_BANK) == 4);
  CHECK(p.Values(FEATURE_DELAY) == kFeatureDefinitions[FEATURE_DELAY].values);
  CHECK(p.num_sections() == 2 && p.directory());
  CHECK(p.directory()[1].kind == kPlaitsSectionPlaits);
  CHECK(p.directory()[1].offset == 100416 && p.directory()[1].size == 8400);
  CHECK(p.directory()[1].crc == 0x90824a8b);

  // No banks: the stock three; a start bank past them is refused.
  ProfileV3(b, kBanks, 0, kDirectory, 0, 2);
  CHECK(p.Init(b) && StockBanks(p));
  ProfileV3(b, kBanks, 0, kDirectory, 0, 3);
  CHECK(!p.Init(b) && StockBanks(p));
  ProfileV3(b, kBanks, 4, kDirectory, 0, 4);
  CHECK(!p.Init(b) && StockBanks(p));
  // Fifteen banks take start banks up to 14.
  uint8_t many[15 * 8];
  for (int i = 0; i < 15 * 8; ++i) {
    many[i] = i % 24;
  }
  ProfileV3(b, many, 15, kDirectory, 0, 14);
  CHECK(p.Init(b) && p.num_plaits_banks() == 15);
  CHECK(p.plaits_bank(14)[7] == (14 * 8 + 7) % 24);
  // A profile without feature 31 starts on the first bank.
  ProfileV3(b, kBanks, 4, kDirectory, 0, 255);
  CHECK(p.Init(b) && p.entry(FEATURE_PLAITS_BANK).value == 0);
  CHECK(p.num_plaits_banks() == 4);
}

void TestBankRefusals() {
  test_checker.Start("TestBankRefusals");
  uint8_t b[kProfileSize];
  Profile p;
  // IDs the firmware does not know: never assigned, or past the table.
  const uint8_t unknown[] = { 24, 25, 26, 27, 100, 200, 255 };
  for (size_t i = 0; i < sizeof(unknown); ++i) {
    uint8_t banks[4 * 8];
    memcpy(banks, kBanks, sizeof(banks));
    banks[13] = unknown[i];
    ProfileV3(b, banks, 4, kDirectory, 2, 0);
    CHECK(!p.Init(b) && StockBanks(p));
  }
  for (int id = 0; id < 256; ++id) {
    CHECK(Profile::KnownModel(id) == (id < 24 || (id >= 28 && id <= 99)));
  }
  // Counts past the limits, then counts that disagree with the size.
  uint8_t many[16 * 8];
  memset(many, 0, sizeof(many));
  ProfileV3(b, many, 16, kDirectory, 0, 0);
  CHECK(!p.Init(b));
  PlaitsDirectoryEntry five[5];
  memset(five, 0, sizeof(five));
  ProfileV3(b, kBanks, 1, five, 5, 0);
  CHECK(!p.Init(b));
  ProfileV3(b, kBanks, 4, kDirectory, 2, 0);
  b[14] = 3;
  ProfileV3Checksum(b);
  CHECK(!p.Init(b));
  ProfileV3(b, kBanks, 4, kDirectory, 2, 0);
  b[15] = 1;
  ProfileV3Checksum(b);
  CHECK(!p.Init(b));
  // A version 2 profile may not carry counts.
  ProfileV3(b, kBanks, 0, kDirectory, 0, 0);
  b[4] = 2;
  ProfileV3Checksum(b);
  CHECK(p.Init(b) && StockBanks(p));
  b[14] = 1;
  ProfileV3Checksum(b);
  CHECK(!p.Init(b));
  // Any bank or directory byte is covered by the CRC.
  ProfileV3(b, kBanks, 4, kDirectory, 2, 0);
  b[16 + 6 * b[5] + 5] ^= 1;
  CHECK(!p.Init(b));
}

// The factory block is version 2: stock banks, no directory.
void TestFactoryProfile() {
  test_checker.Start("TestFactoryProfile");
  uint8_t b[kProfileSize];
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  Profile p;
  CHECK(p.Init(b) && StockBanks(p));
  CHECK(p.Values(FEATURE_PLAITS_BANK) == 3);
  p.Defaults();
  CHECK(StockBanks(p));
}

int main() {
  TestBankTable();
  TestBankRefusals();
  TestFactoryProfile();
  puts("Profile version 3 checks: PASS");
  return test_checker.failures() ? 1 : 0;
}
