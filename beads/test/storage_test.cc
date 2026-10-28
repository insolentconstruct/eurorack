// Copyright 2026 Insolent Construct.
//
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
// Settings storage tests: every flash operation is checked, and records are
// only programmed onto erased flash.

#include <sys/mman.h>

#include <cstdio>
#include <cstring>

#include "stmlib/system/storage.h"

#include "beads/settings.h"
#include "beads/test/test_checker.h"

uint32_t test_errors = 0;
int test_failures = 0;
int test_stuck_erases = 0;
unsigned test_erases = 0;
unsigned test_refusals = 0;
unsigned test_overwrites = 0;

using namespace beads;

uint8_t* const kSector = reinterpret_cast<uint8_t*>(0x081e0000);
const size_t kSectorSize = 0x20000;
const size_t kCalibrationChunk = (8 + sizeof(PersistentData) + 31) & ~31;
const size_t kStateChunk = (8 + sizeof(State) + 31) & ~31;
// State records that fit after the calibration record.
const int kStateChunks = (kSectorSize - kCalibrationChunk) / kStateChunk;

typedef ChunkStorage<15, PersistentData, State> Storage;

struct Records {
  PersistentData calibration;
  State state;
};

void Fill(Records* r, int seed) {
  memset(r, 0, sizeof(*r));
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    r->calibration.adc_calibration[i].scale = -2.5f - 0.125f * i;
    r->calibration.adc_calibration[i].offset = 0.75f + 0.0625f * i;
  }
  r->state.media = seed & 3;
  r->state.gain = uint8_t(seed);
}

// The records the firmware loads at the next start.
bool Loads(const Records& expected) {
  Records loaded;
  memset(&loaded, 0, sizeof(loaded));
  Storage storage;
  return storage.Init(&loaded.calibration, &loaded.state) && \
      !memcmp(&loaded.calibration, &expected.calibration,
          sizeof(PersistentData)) && \
      !memcmp(&loaded.state, &expected.state, sizeof(State));
}

int StateRecords() {
  int count = 0;
  while (count < kStateChunks) {
    const uint8_t* chunk = kSector + kCalibrationChunk + kStateChunk * count;
    if (chunk[0] == 0xff && chunk[1] == 0xff && chunk[2] == 0xff && \
        chunk[3] == 0xff) {
      break;
    }
    ++count;
  }
  return count;
}

// A blank sector, then saves until the next one rewrites it.
void FillSector(Storage* storage, Records* r) {
  memset(kSector, 255, kSectorSize);
  Fill(r, 0);
  storage->Init(&r->calibration, &r->state);
  while (StateRecords() < kStateChunks) {
    storage->SaveState();
  }
}

void TestStaleErrorFlag() {
  test_checker.Start("TestStaleErrorFlag");
  // A corrected ECC error left by an earlier read: stmlib's rewrite skips the
  // erase and programs the records over the old ones.
  {
    Storage storage;
    Records r;
    FillSector(&storage, &r);
    stmlib::ChunkStorage<15, PersistentData, State> stock;
    stock.Init(&r.calibration, &r.state);
    const unsigned overwrites = test_overwrites;
    test_errors = FLASH_SR_SNECCERR;
    stock.SavePersistentData();
    CHECK(test_overwrites > overwrites);
    test_errors = 0;
  }
  // The Beads storage clears the flag first: one erase, no overwrite.
  Storage storage;
  Records r;
  FillSector(&storage, &r);
  r.state.media = 1;
  const unsigned overwrites = test_overwrites;
  const unsigned erases = test_erases;
  const unsigned refusals = test_refusals;
  test_errors = FLASH_SR_SNECCERR;
  storage.SaveState();
  CHECK(test_errors == 0);
  CHECK(test_refusals == refusals);
  CHECK(test_overwrites == overwrites);
  CHECK(test_erases == erases + 1);
  CHECK(StateRecords() == 1);
  CHECK(Loads(r));
  // The same on an ordinary save, which appends a record.
  r.state.media = 2;
  test_errors = FLASH_SR_SNECCERR;
  storage.SaveState();
  CHECK(test_refusals == refusals);
  CHECK(test_overwrites == overwrites);
  CHECK(StateRecords() == 2);
  CHECK(Loads(r));
}

void TestFailedErase() {
  test_checker.Start("TestFailedErase");
  for (int stuck = 0; stuck < 2; ++stuck) {
    Storage storage;
    Records r;
    FillSector(&storage, &r);
    const Records old = r;
    r.state.media = 1;
    const unsigned overwrites = test_overwrites;
    // Every attempt refused, or reported done with the sector unchanged.
    if (stuck) {
      test_stuck_erases = 100;
    } else {
      test_failures = 100;
    }
    storage.SaveState();
    test_stuck_erases = 0;
    test_failures = 0;
    // Nothing programmed over the old records, which still load.
    CHECK(test_overwrites == overwrites);
    CHECK(Loads(old));
    // The settings stay in RAM and the next save rewrites the sector.
    storage.SaveState();
    CHECK(test_overwrites == overwrites);
    CHECK(StateRecords() == 1);
    CHECK(Loads(r));
  }
}

void TestNonBlankSlot() {
  test_checker.Start("TestNonBlankSlot");
  // A record is never programmed over a free chunk that isn't blank.
  memset(kSector, 255, kSectorSize);
  Storage storage;
  Records r;
  Fill(&r, 0);
  storage.Init(&r.calibration, &r.state);
  storage.SaveState();
  kSector[kCalibrationChunk + kStateChunk * StateRecords() + 5] = 0;
  r.state.media = 1;
  const unsigned overwrites = test_overwrites;
  storage.SaveState();
  CHECK(test_overwrites == overwrites);
  CHECK(StateRecords() == 1);
  CHECK(Loads(r));
}

void TestFailedProgram() {
  test_checker.Start("TestFailedProgram");
  for (int failures = 1; failures <= 100; failures += 99) {
    memset(kSector, 255, kSectorSize);
    Storage storage;
    Records r;
    Fill(&r, 0);
    storage.Init(&r.calibration, &r.state);
    r.state.media = 1;
    storage.SaveState();
    const Records old = r;
    const int records = StateRecords();
    r.state.media = 2;
    const unsigned overwrites = test_overwrites;
    test_failures = failures;
    storage.SaveState();
    test_failures = 0;
    CHECK(test_overwrites == overwrites);
    if (failures == 1) {
      // A refused program is retried in place.
      CHECK(StateRecords() == records + 1);
      CHECK(Loads(r));
    } else {
      // Refused every time: the previous record still loads.
      CHECK(Loads(old));
      storage.SaveState();
      CHECK(test_overwrites == overwrites);
      CHECK(StateRecords() == 1);
      CHECK(Loads(r));
    }
  }
}

void TestMatchesStmlib() {
  test_checker.Start("TestMatchesStmlib");
  // Normal saves leave the sector byte for byte as stmlib's storage does.
  static uint8_t ours[kSectorSize];
  for (int pass = 0; pass < 2; ++pass) {
    memset(kSector, 255, kSectorSize);
    Records r;
    Fill(&r, 0);
    if (pass == 0) {
      Storage storage;
      storage.Init(&r.calibration, &r.state);
      for (int i = 0; i < kStateChunks + 10; ++i) {
        r.state.media = i & 3;
        r.state.gain = uint8_t(i);
        storage.SaveState();
      }
      storage.SavePersistentData();
      memcpy(ours, kSector, kSectorSize);
    } else {
      stmlib::ChunkStorage<15, PersistentData, State> storage;
      storage.Init(&r.calibration, &r.state);
      for (int i = 0; i < kStateChunks + 10; ++i) {
        r.state.media = i & 3;
        r.state.gain = uint8_t(i);
        storage.SaveState();
      }
      storage.SavePersistentData();
      CHECK(!memcmp(ours, kSector, kSectorSize));
    }
  }
}

int main() {
  if (!CHECK(mmap(kSector, kSectorSize, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == kSector)) {
    return 1;
  }
  TestStaleErrorFlag();
  TestFailedErase();
  TestNonBlankSlot();
  TestFailedProgram();
  TestMatchesStmlib();
  return test_checker.failures() ? 1 : 0;
}
