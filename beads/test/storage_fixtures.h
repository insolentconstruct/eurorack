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
// Bank 2 fixtures shared by the storage and record tests: sector images, a
// module in use, and the calibration safety check run after every erase or
// program.

#ifndef BEADS_TEST_STORAGE_FIXTURES_H_
#define BEADS_TEST_STORAGE_FIXTURES_H_

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

#include "beads/test/test_checker.h"

#define private public
#include "beads/settings.h"
#undef private

bool test_down[4];
uint32_t test_writes;

using namespace beads;

const uint32_t kSector = 0x081e0000;
const uint32_t kMirror = 0x081c0000;
const uint32_t kSectorSize = 0x20000;
const uint32_t kCalibrationChunk = (8 + sizeof(PersistentData) + 31) & ~31u;
const uint32_t kStateChunk = (8 + sizeof(State) + 31) & ~31u;
const uint32_t kRecordChunk = (8 + sizeof(OptionsRecord) + 31) & ~31u;
const int kStateChunks = (kSectorSize - kCalibrationChunk) / kStateChunk;
const int kRecordSlots = (kSectorSize - kCalibrationChunk) / kRecordChunk;

typedef stmlib::ChunkStorage<15, PersistentData, State> StockStorage;

uint32_t StateChunk(int index) {
  return kSector + kCalibrationChunk + kStateChunk * (index - 1);
}

uint32_t RecordChunk(int index) {
  return kMirror + kCalibrationChunk + kRecordChunk * (index - 1);
}

// Plain copies of bank 2, made with the damaged pages readable.
void Peek(void* destination, uint32_t address, size_t size) {
  TestFlashExpose(true);
  memcpy(destination, reinterpret_cast<const void*>(address), size);
  TestFlashExpose(false);
}

void Poke(uint32_t address, const void* source, size_t size) {
  TestFlashExpose(true);
  memcpy(reinterpret_cast<void*>(address), source, size);
  TestFlashExpose(false);
}

void Blank(uint32_t address, size_t size) {
  TestFlashExpose(true);
  memset(reinterpret_cast<void*>(address), 255, size);
  TestFlashExpose(false);
}

// The record checksum: a byte sum, as the firmware's on the Cortex-M7.
uint16_t Checksum(const void* data, size_t size) {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  uint16_t sum = 0;
  while (size--) {
    sum += *bytes++;
  }
  return sum ^ 0xffff;
}

// A chunk written straight into flash, header and payload.
void PokeChunk(uint32_t address, uint32_t tag, const void* data,
    uint16_t size) {
  uint8_t chunk[256];
  memset(chunk, 255, sizeof(chunk));
  const uint16_t checksum = Checksum(data, size);
  memcpy(chunk, &tag, 4);
  memcpy(chunk + 4, &size, 2);
  memcpy(chunk + 6, &checksum, 2);
  memcpy(chunk + 8, data, size);
  Poke(address, chunk, (8 + size + 31) & ~31u);
}

struct Kept {
  bool armed;
  PersistentData before;
  PersistentData after;
  int lost;
};

Kept kept;

bool HoldsCalibration(uint32_t base, const PersistentData& data) {
  uint8_t chunk[kCalibrationChunk];
  if (TestFlashDamaged(base, base + kCalibrationChunk)) {
    return false;
  }
  Peek(chunk, base, sizeof(chunk));
  uint32_t tag;
  uint16_t size;
  uint16_t checksum;
  memcpy(&tag, chunk, 4);
  memcpy(&size, chunk + 4, 2);
  memcpy(&checksum, chunk + 6, 2);
  return tag == PersistentData::tag && size == sizeof(PersistentData) &&
      checksum == Checksum(chunk + 8, sizeof(PersistentData)) &&
      !memcmp(chunk + 8, data.adc_calibration, sizeof(data.adc_calibration));
}

// Calibration safety: after every erase or program, one of the two sectors
// holds a valid copy of the calibration from before the operation, or of the
// one it saves.
void CheckKept() {
  if (kept.armed &&
      !HoldsCalibration(kSector, kept.before) &&
      !HoldsCalibration(kMirror, kept.before) &&
      !HoldsCalibration(kSector, kept.after) &&
      !HoldsCalibration(kMirror, kept.after)) {
    ++kept.lost;
  }
}

void Keep(const PersistentData& before, const PersistentData& after) {
  kept.armed = true;
  kept.before = before;
  kept.after = after;
  TestFlash().after_operation = &CheckKept;
}

// True when no step since Keep lost both calibrations.
bool Release() {
  kept.armed = false;
  TestFlash().after_operation = NULL;
  const bool safe = kept.lost == 0;
  kept.lost = 0;
  return safe;
}

void Wipe() {
  Release();
  TestFlashRepairAll();
  memset(reinterpret_cast<void*>(0x08100000), 255, 0x100000);
  TestFlashBudget() = -1;
  TestFlashFailures() = 0;
  TestFlashStuckErases() = 0;
  TestFlashErrors() = 0;
}

struct Snapshot {
  uint8_t sector[kSectorSize];
  uint8_t mirror[kSectorSize];
  uint32_t writes;
  uint32_t erases;
};

void Take(Snapshot* s) {
  Peek(s->sector, kSector, kSectorSize);
  Peek(s->mirror, kMirror, kSectorSize);
  s->writes = test_writes;
  s->erases = TestFlashErases();
}

// Nothing erased or programmed since the snapshot.
bool Unchanged(const Snapshot& s) {
  static uint8_t now[kSectorSize];
  bool same = test_writes == s.writes && TestFlashErases() == s.erases;
  Peek(now, kSector, kSectorSize);
  same = same && !memcmp(now, s.sector, kSectorSize);
  Peek(now, kMirror, kSectorSize);
  return same && !memcmp(now, s.mirror, kSectorSize);
}

static Snapshot snapshot;

// As Settings::Init sets them.
PersistentData Factory() {
  PersistentData data;
  memset(&data, 0, sizeof(data));
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    const bool cv = i == CV_ADC_CHANNEL_ASSIGN || i == CV_ADC_CHANNEL_DENSITY;
    const bool pitch = i == CV_ADC_CHANNEL_PITCH;
    data.adc_calibration[i].scale = pitch ? -120.0f : (cv ? -2.0f : -3.2f);
    data.adc_calibration[i].offset = pitch ? 60.0f : (cv ? 1.0f : 1.6f);
  }
  return data;
}

// A calibration of the module's own, within 25% of the factory values.
PersistentData Calibration(float detune) {
  PersistentData data = Factory();
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    data.adc_calibration[i].scale *= 1.0f + detune + 0.01f * i;
    data.adc_calibration[i].offset *= 1.0f - detune;
  }
  return data;
}

// What a reboot brings back: the stock state, and the options in use.
struct Saved {
  State state;
  uint8_t values[FEATURE_LAST];
  uint8_t cv_target;
};

Saved Of(const Settings& s) {
  Saved saved;
  saved.state = s.state();
  memcpy(saved.values, s.values_, sizeof(saved.values));
  saved.cv_target = s.cv_target_;
  return saved;
}

Saved Defaults() {
  Settings s;
  Wipe();
  s.Init(false);
  return Of(s);
}

bool SameCalibration(const Settings& s, const PersistentData& data) {
  return !memcmp(s.persistent_data().adc_calibration, data.adc_calibration,
      sizeof(data.adc_calibration));
}

bool SameState(const Settings& s, const Saved& saved) {
  return !memcmp(&s.state(), &saved.state, sizeof(State));
}

bool SameOptions(const Settings& s, const Saved& saved) {
  return !memcmp(s.values_, saved.values, sizeof(saved.values)) &&
      s.cv_target_ == saved.cv_target;
}

// A module in use: its calibration, then a few saves, each with the Fast
// limiter. The state terminator is chunk saves + 2; the records are in
// slots 1 and 2.
Saved Setup(const PersistentData& calibration, int saves) {
  Wipe();
  Settings s;
  s.Init(false);
  memcpy(s.mutable_persistent_data()->adc_calibration,
      calibration.adc_calibration, sizeof(calibration.adc_calibration));
  s.SavePersistentData();
  for (int i = 0; i < saves; ++i) {
    s.mutable_state()->media = i & 3;
    s.mutable_state()->gain = uint8_t(i + 1);
    s.set_output_limiter_mode(OUTPUT_LIMITER_FAST);
    s.SaveState();
  }
  return Of(s);
}

// The sector as stock firmware loads it: the same calibration and State,
// byte for byte.
bool StockLoads(const PersistentData& calibration, const State& state) {
  PersistentData data;
  State loaded;
  memset(&data, 0, sizeof(data));
  memset(&loaded, 0, sizeof(loaded));
  StockStorage stock;
  if (!stock.Init(&data, &loaded)) {
    return false;
  }
  return !memcmp(data.adc_calibration, calibration.adc_calibration,
          sizeof(calibration.adc_calibration)) && \
      !memcmp(&loaded, &state, sizeof(State));
}

bool MirrorHolds(const PersistentData& calibration) {
  return HoldsCalibration(kMirror, calibration);
}

// The record in a slot, if it reads valid.
bool RecordAt(int index, OptionsRecord* record) {
  uint8_t chunk[kRecordChunk];
  Peek(chunk, RecordChunk(index), sizeof(chunk));
  uint32_t tag;
  uint16_t size;
  uint16_t checksum;
  memcpy(&tag, chunk, 4);
  memcpy(&size, chunk + 4, 2);
  memcpy(&checksum, chunk + 6, 2);
  memcpy(record, chunk + 8, sizeof(OptionsRecord));
  return tag == OptionsRecord::tag && size == sizeof(OptionsRecord) &&
      checksum == Checksum(chunk + 8, sizeof(OptionsRecord));
}

void OnUnmaskedRead(int) {
  printf("ERROR: %s: unmasked read of a damaged flash word\n",
      test_checker.test());
  fflush(stdout);
  _exit(1);
}

// Maps bank 2 and stops at any unmasked read of a damaged word.
bool MapBank2() {
  setbuf(stdout, NULL);
  void* bank_2 = reinterpret_cast<void*>(0x08100000);
  if (mmap(bank_2, 0x100000, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != bank_2) {
    return false;
  }
  TestFlashGuard(&OnUnmaskedRead);
  return true;
}

void ProfileTemplate(uint8_t* b) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
}

void ProfileChecksum(uint8_t* b) {
  const size_t size = 16 + uint16_t(b[6]) + (uint16_t(b[7]) << 8);
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

// The factory profile with one more entry, before its scale records.
void ProfileWithEntry(uint8_t* b, uint8_t feature, uint8_t state,
    uint8_t value) {
  ProfileTemplate(b);
  const size_t bytes = b[6] | b[7] << 8;
  uint8_t* entry = b + 16 + 6 * b[5];
  memmove(entry + 6, entry, bytes - 6 * b[5]);
  entry[0] = feature;
  entry[1] = state;
  entry[2] = value;
  entry[3] = entry[4] = entry[5] = 0;
  ++b[5];
  b[6] = (bytes + 6) & 255;
  b[7] = (bytes + 6) >> 8;
  ProfileChecksum(b);
}

#endif  // BEADS_TEST_STORAGE_FIXTURES_H_
