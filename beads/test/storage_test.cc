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
// Boot guard tests: the masked reader, damaged settings, the calibration
// mirror, the write lock and the repair made by holding SEED at power-up.

#include <cmath>
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
const int kStateChunks = (kSectorSize - kCalibrationChunk) / kStateChunk;

uint32_t StateChunk(int index) {
  return kSector + kCalibrationChunk + kStateChunk * (index - 1);
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

void Wipe() {
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

State Defaults() {
  Settings s;
  Wipe();
  s.Init(false);
  return s.state();
}

bool SameCalibration(const Settings& s, const PersistentData& data) {
  return !memcmp(s.persistent_data().adc_calibration, data.adc_calibration,
      sizeof(data.adc_calibration));
}

bool SameState(const Settings& s, const State& state) {
  return !memcmp(&s.state(), &state, sizeof(State));
}

// A module in use: its calibration, then a few saves. Returns the last
// state; the terminator is chunk saves + 2.
State Setup(const PersistentData& calibration, int saves) {
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
  return s.state();
}

// The sector as stock firmware loads it.
bool StockLoads(const PersistentData& calibration, const State& state) {
  PersistentData data;
  State loaded;
  memset(&data, 0, sizeof(data));
  memset(&loaded, 0, sizeof(loaded));
  stmlib::ChunkStorage<15, PersistentData, State> stock;
  return stock.Init(&data, &loaded) && \
      !memcmp(data.adc_calibration, calibration.adc_calibration,
          sizeof(calibration.adc_calibration)) && \
      !memcmp(&loaded, &state, sizeof(State));
}

bool MirrorHolds(const PersistentData& calibration) {
  uint8_t chunk[kCalibrationChunk];
  Peek(chunk, kMirror, sizeof(chunk));
  const PersistentData* data = reinterpret_cast<const PersistentData*>(
      chunk + 8);
  return !memcmp(data->adc_calibration, calibration.adc_calibration,
      sizeof(calibration.adc_calibration));
}

void TestMaskedReader() {
  test_checker.Start("TestMaskedReader");
  Wipe();
  const uint32_t base = 0x08100000 + 0x1000;
  uint8_t pattern[256];
  for (int i = 0; i < 256; ++i) {
    pattern[i] = uint8_t(i * 7 + 3);
  }
  Poke(base, pattern, sizeof(pattern));
  uint32_t words[64];
  const int masked = TestFlash().masked_reads;
  CHECK(Flash::Read(base, words, sizeof(words)));
  CHECK(!memcmp(words, pattern, sizeof(words)));
  CHECK(TestFlash().masked_reads == masked + 1);
  // The previous masks come back, whatever they were.
  CHECK(TestFlash().faultmask == 0);
  CHECK(TestFlash().scb.CCR == 0);
  TestFlash().scb.CCR = 0x200;
  CHECK(Flash::Read(base, words, 32));
  CHECK(TestFlash().scb.CCR == 0x200);
  TestFlash().scb.CCR = 0x200 | SCB_CCR_BFHFNMIGN_Msk;
  TestFlash().faultmask = 1;
  CHECK(Flash::Read(base, words, 32));
  CHECK(TestFlash().faultmask == 1);
  CHECK(TestFlash().scb.CCR == (0x200 | SCB_CCR_BFHFNMIGN_Msk));
  TestFlash().faultmask = 0;
  TestFlash().scb.CCR = 0;
  // A flag left by an earlier read doesn't fail a clean one.
  TestFlashErrors() = FLASH_SR_DBECCERR;
  CHECK(Flash::Read(base, words, 32));

  TestFlashDamage(base + 64);
  TestFlash().scb.CFSR.value = 1;  // An unrelated status bit stays.
  CHECK(!Flash::Read(base, words, 128));
  CHECK(!TestFlash().stale);  // The undefined words aren't left cached.
  CHECK(TestFlash().scb.CFSR.value == 1);  // Nor the bus fault status,
  CHECK(TestFlash().stale_fault_status == 0);  // cleared before unmasking.
  TestFlash().scb.CFSR.value = 0;
  CHECK(TestFlashErrors() == 0);
  CHECK(TestFlash().faultmask == 0 && TestFlash().scb.CCR == 0);
  // Neighbouring words read clean, across a word boundary too.
  CHECK(Flash::Read(base + 32, words, 32));
  CHECK(!memcmp(words, pattern + 32, 32));
  CHECK(Flash::Read(base + 96, words, 32));
  CHECK(!Flash::Read(base + 60, words, 8));
  CHECK(!Flash::Read(base + 92, words, 8));
  // Several damaged words in one read; reads after a failed one.
  TestFlashDamage(base + 160);
  CHECK(!Flash::Read(base, words, 256));
  CHECK(Flash::Read(base, words, 64));
  CHECK(!memcmp(words, pattern, 64));
  TestFlashDamage(0x08103000);
  CHECK(!Flash::IsBlank(0x08102000, 0x2000));
  CHECK(!Flash::IsReadable(0x08100000, 0x2000));
  CHECK(Flash::IsReadable(0x08100000, 0x1000));
  CHECK(Flash::IsBlank(0x08100000, 0x1000));
  CHECK(TestFlash().violations == 0);
}

// Both copies of the calibration lost, as only damage can leave them.
void LoseCalibration(int damage) {
  if (damage == 0) {
    TestFlashDamage(kSector + 64);
    TestFlashDamage(kMirror + 32);
  } else {
    uint8_t byte = 0x12;
    Poke(kSector + 40, &byte, 1);
    Poke(kMirror + 40, &byte, 1);
  }
}

void TestCalibrationFromMirror() {
  test_checker.Start("TestCalibrationFromMirror");
  const State defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  // An uncorrectable error at every word of the calibration, or a clean one
  // failing its checks: restored from the mirror byte for byte, with the
  // states that loaded, and no lock.
  for (int damage = 0; damage < 7; ++damage) {
    const State saved = Setup(calibration, 5);
    uint8_t mirror[kCalibrationChunk];
    Peek(mirror, kMirror, sizeof(mirror));
    if (damage < 4) {
      TestFlashDamage(kSector + 32 * damage);
    } else {
      uint8_t header[8];
      Peek(header, kSector, sizeof(header));
      header[damage == 4 ? 0 : (damage == 5 ? 4 : 6)] ^= 1;
      Poke(kSector, header, sizeof(header));
    }
    Settings s;
    CHECK(s.Init(false));  // No factory test.
    CHECK(!s.write_locked());
    CHECK(s.repair() == STORAGE_REPAIR_NONE);
    CHECK(SameState(s, saved));
    CHECK(SameCalibration(s, calibration));
    CHECK(TestFlash().num_damaged == 0);
    uint8_t stored[kCalibrationChunk];
    Peek(stored, kSector, sizeof(stored));
    CHECK(!memcmp(stored, mirror, sizeof(stored)));
    CHECK(StockLoads(calibration, saved));
  }
  // SEED held as well: the state resets, as stock.
  {
    Setup(calibration, 5);
    TestFlashDamage(kSector);
    Settings s;
    CHECK(s.Init(true));
    CHECK(s.repair() == STORAGE_REPAIR_NONE);
    CHECK(SameState(s, defaults));
    CHECK(SameCalibration(s, calibration));
  }
  // An intact calibration out of range, and a mirror that isn't: the
  // mirror's.
  {
    PersistentData wild = calibration;
    wild.adc_calibration[CV_ADC_CHANNEL_PITCH].scale *= 1.5f;
    Setup(wild, 1);
    uint8_t chunk[kCalibrationChunk];
    Peek(chunk, kSector, sizeof(chunk));
    const State saved = Setup(calibration, 5);
    Poke(kSector, chunk, sizeof(chunk));
    Settings s;
    CHECK(s.Init(false) && !s.write_locked());
    CHECK(SameCalibration(s, calibration));
    CHECK(SameState(s, saved));
    CHECK(StockLoads(calibration, saved));
    // Out of range in both copies: loaded as stock loads it.
    Setup(wild, 5);
    Take(&snapshot);
    Settings both;
    CHECK(both.Init(false) && !both.write_locked());
    CHECK(SameCalibration(both, wild));
    CHECK(Unchanged(snapshot));
  }
}

void TestCalibrationLost() {
  test_checker.Start("TestCalibrationLost");
  const State defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  // The calibration unreadable or invalid, and the mirror unreadable,
  // invalid, out of range or blank: the guarded state.
  for (int mirror = 0; mirror < 4; ++mirror) {
    for (int damage = 0; damage < 2; ++damage) {
      PersistentData wild = calibration;
      wild.adc_calibration[CV_ADC_CHANNEL_SHAPE].scale *= 1.5f;
      Setup(mirror == 2 ? wild : calibration, 5);
      if (damage == 0) {
        TestFlashDamage(kSector + 96);
      } else {
        uint8_t byte = 0x12;
        Poke(kSector + 40, &byte, 1);
      }
      if (mirror == 0) {
        TestFlashDamage(kMirror + 64);
      } else if (mirror == 1) {
        uint8_t byte = 0x12;
        Poke(kMirror + 20, &byte, 1);
      } else if (mirror == 3) {
        static uint8_t blank[kSectorSize];
        memset(blank, 255, sizeof(blank));
        Poke(kMirror, blank, kSectorSize);
      }
      Take(&snapshot);
      Settings s;
      CHECK(s.Init(false));  // No factory test.
      CHECK(s.write_locked());
      CHECK(s.repair() == STORAGE_REPAIR_NONE);
      CHECK(SameState(s, defaults));
      CHECK(SameCalibration(s, Factory()));
      CHECK(Unchanged(snapshot));
    }
  }
}

void TestStateTerminator() {
  test_checker.Start("TestStateTerminator");
  const State defaults = Defaults();
  const PersistentData calibration = Calibration(-0.1f);
  // An unreadable record ends the states, as a torn one does: what an
  // interrupted append leaves. After none, one or many valid records.
  const int saves[] = { 0, 0, 10, 10 };
  const int damaged[] = { 1, 2, 12, 6 };
  for (int i = 0; i < 4; ++i) {
    Setup(calibration, saves[i]);
    State last;
    Peek(&last, StateChunk(damaged[i] - 1) + 8, sizeof(last));
    TestFlashDamage(StateChunk(damaged[i]));
    Settings s;
    CHECK(s.Init(false));
    CHECK(!s.write_locked());
    CHECK(SameCalibration(s, calibration));
    CHECK(SameState(s, damaged[i] == 1 ? defaults : last));
    // The next save finds the chunk isn't blank and rewrites the sector.
    const uint32_t erases = TestFlashErases();
    s.mutable_state()->gain = 99;
    s.SaveState();
    CHECK(TestFlashErases() == erases + (damaged[i] == 1 ? 0 : 1));
    CHECK(TestFlash().num_damaged == 0);
    CHECK(s.chunk_storage_.next_state_chunk_index_ ==
        (damaged[i] == 1 ? 3 : 2));
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameState(loaded, s.state()));
    CHECK(StockLoads(calibration, s.state()));
  }
  // Past the terminator nothing is read at boot.
  const int unread[] = { 13, 14, 40, kStateChunks };
  for (size_t i = 0; i < sizeof(unread) / sizeof(unread[0]); ++i) {
    const State saved = Setup(calibration, 10);
    TestFlashDamage(StateChunk(unread[i]));
    Take(&snapshot);
    Settings s;
    CHECK(s.Init(false));
    CHECK(!s.write_locked());
    CHECK(SameState(s, saved));
    CHECK(SameCalibration(s, calibration));
    CHECK(Unchanged(snapshot));
    // Saves append up to the damaged word, then start the sector over.
    for (int j = 0; j < unread[i] - 11; ++j) {
      s.mutable_state()->gain = uint8_t(100 + j);
      s.SaveState();
    }
    CHECK(TestFlash().num_damaged == 0);
    CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameState(loaded, s.state()));
  }
}

void TestInterruptedAppend() {
  test_checker.Start("TestInterruptedAppend");
  const PersistentData calibration = Calibration(0.05f);
  // Power lost before the program: the previous state loads.
  {
    const State saved = Setup(calibration, 4);
    Settings s;
    CHECK(s.Init(false));
    s.mutable_state()->gain = 77;
    TestFlashBudget() = 0;
    s.SaveState();
    TestFlashBudget() = -1;
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameState(loaded, saved));
  }
  // A torn record, clean but failing its checksum, after valid ones: loads
  // as stock does, and the next save starts the sector over.
  const State saved = Setup(calibration, 4);
  uint8_t torn[32];
  Peek(torn, StateChunk(5), sizeof(torn));
  torn[12] = 0x00;
  Poke(StateChunk(6), torn, sizeof(torn));
  Settings s;
  CHECK(s.Init(false));
  CHECK(!s.write_locked());
  CHECK(SameState(s, saved));
  const uint32_t erases = TestFlashErases();
  s.mutable_state()->gain = 78;
  s.SaveState();
  CHECK(TestFlashErases() == erases + 1);
  CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
  Settings loaded;
  CHECK(loaded.Init(false) && SameState(loaded, s.state()));
  CHECK(StockLoads(calibration, s.state()));
}

void TestInterruptedRewrite() {
  test_checker.Start("TestInterruptedRewrite");
  const State defaults = Defaults();
  const PersistentData old_calibration = Calibration(0.1f);
  const PersistentData new_calibration = Calibration(-0.1f);
  // A calibration save: mirror erase, four mirror words, sector erase, four
  // calibration words, the state.
  for (int cut = 0; cut <= 11; ++cut) {
    const State saved = Setup(old_calibration, 6);
    Settings s;
    CHECK(s.Init(false));
    memcpy(s.mutable_persistent_data()->adc_calibration,
        new_calibration.adc_calibration,
        sizeof(new_calibration.adc_calibration));
    TestFlashBudget() = cut;
    s.SavePersistentData();
    TestFlashBudget() = -1;

    Settings loaded;
    CHECK(loaded.Init(false));  // Never the factory test.
    CHECK(!loaded.write_locked());
    if (cut <= 5) {
      // The settings sector wasn't touched.
      CHECK(SameState(loaded, saved));
      CHECK(SameCalibration(loaded, old_calibration));
    } else if (cut <= 10) {
      // Erased, the calibration partly or wholly written: from the mirror.
      CHECK(SameState(loaded, defaults));
      CHECK(SameCalibration(loaded, new_calibration));
    } else {
      CHECK(SameState(loaded, saved));
      CHECK(SameCalibration(loaded, new_calibration));
    }
    // Whatever came back is stable.
    Settings again;
    CHECK(again.Init(false) && !again.write_locked());
    CHECK(SameState(again, loaded.state()));
    CHECK(SameCalibration(again, loaded.persistent_data()));
  }
}

void TestCalibrationWithoutState() {
  test_checker.Start("TestCalibrationWithoutState");
  const State defaults = Defaults();
  const PersistentData calibration = Calibration(0.08f);
  Setup(calibration, 3);
  static uint8_t blank[kSectorSize];
  memset(blank, 255, sizeof(blank));
  Poke(kSector + kCalibrationChunk, blank, kSectorSize - kCalibrationChunk);
  Settings s;
  CHECK(s.Init(false));  // Stock would run the factory test.
  CHECK(!s.write_locked());
  CHECK(SameState(s, defaults));
  CHECK(SameCalibration(s, calibration));
  CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
  CHECK(StockLoads(calibration, defaults));
}

void TestBlankSector() {
  test_checker.Start("TestBlankSector");
  const State defaults = Defaults();
  const PersistentData calibration = Calibration(0.12f);
  static uint8_t blank[kSectorSize];
  memset(blank, 255, sizeof(blank));
  // A new module: the factory test, and the factory values mirrored.
  {
    Wipe();
    Settings s;
    CHECK(!s.Init(false));
    CHECK(!s.write_locked());
    CHECK(MirrorHolds(Factory()));
    CHECK(StockLoads(Factory(), defaults));
    Wipe();
    Settings seed;
    CHECK(seed.Init(true));  // SEED held: no factory test, as stock.
  }
  // Erased, then the power went: the mirror's calibration comes back.
  {
    Setup(calibration, 3);
    Poke(kSector, blank, kSectorSize);
    Settings s;
    CHECK(s.Init(false));
    CHECK(!s.write_locked());
    CHECK(SameState(s, defaults));
    CHECK(SameCalibration(s, calibration));
    CHECK(StockLoads(calibration, defaults));
  }
  // A mirror that isn't blank but doesn't hold a calibration: damaged.
  for (int damage = 0; damage < 3; ++damage) {
    Setup(calibration, 3);
    Poke(kSector, blank, kSectorSize);
    if (damage == 0) {
      uint8_t byte = 0x5a;
      Poke(kMirror + 20, &byte, 1);
    } else if (damage == 1) {
      TestFlashDamage(kMirror + 32);
    } else {
      Poke(kMirror, blank, kSectorSize);
      TestFlashDamage(kMirror + 0x10000);
    }
    Take(&snapshot);
    Settings s;
    CHECK(s.Init(false));
    CHECK(s.write_locked());
    CHECK(SameCalibration(s, Factory()));
    CHECK(Unchanged(snapshot));
  }
  // With a readable sector, a damaged mirror is ignored, then rewritten.
  for (int damage = 0; damage < 2; ++damage) {
    const State saved = Setup(calibration, 3);
    if (damage) {
      TestFlashDamage(kMirror + 64);
    } else {
      uint8_t byte = 0x5a;
      Poke(kMirror + 100, &byte, 1);
    }
    Take(&snapshot);
    Settings s;
    CHECK(s.Init(false));
    CHECK(!s.write_locked());
    CHECK(SameState(s, saved));
    CHECK(Unchanged(snapshot));
    const uint32_t erases = TestFlashErases();
    s.SavePersistentData();
    CHECK(TestFlashErases() == erases + 2);
    CHECK(TestFlash().num_damaged == 0);
    CHECK(MirrorHolds(calibration));
  }
}

void TestWriteLock() {
  test_checker.Start("TestWriteLock");
  const PersistentData calibration = Calibration(0.1f);
  Setup(calibration, 5);
  LoseCalibration(0);
  Take(&snapshot);
  Settings s;
  CHECK(s.Init(false) && s.write_locked());
  // Saves, option changes and calibration apply in RAM only.
  for (int i = 0; i < kStateChunks + 5; ++i) {
    s.mutable_state()->media = i & 3;
    s.SaveState();
  }
  s.set_clouds_style(true);
  s.set_pitch_scale(PITCH_SCALE_FIFTHS);
  s.SaveState();
  s.mutable_persistent_data()->adc_calibration[0].offset = 0.8f;
  s.SavePersistentData();
  CHECK(s.clouds_style());
  CHECK(s.persistent_data().adc_calibration[0].offset == 0.8f);
  // Every way in to the flash.
  CHECK(!s.chunk_storage_.MirrorPersistentData());
  s.chunk_storage_.Recover(&s.persistent_data_, &s.state_);
  CHECK(!s.chunk_storage_.EraseSector(15, kSector));
  CHECK(!s.chunk_storage_.EraseSector(14, kMirror));
  uint8_t word[32];
  memset(word, 0, sizeof(word));
  CHECK(!s.chunk_storage_.ProgramWord(StateChunk(20), word));
  CHECK(s.write_locked());
  CHECK(Unchanged(snapshot));
  CHECK(TestFlash().num_damaged == 2);
}

void TestRepair() {
  test_checker.Start("TestRepair");
  const State defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  // Both copies lost: the factory values, mirrored first.
  for (int damage = 0; damage < 2; ++damage) {
    Setup(calibration, 5);
    LoseCalibration(damage);
    Settings s;
    CHECK(s.Init(true));
    CHECK(s.repair() == STORAGE_REPAIR_CALIBRATION_RESET);
    CHECK(!s.write_locked());
    CHECK(TestFlash().num_damaged == 0);
    CHECK(SameCalibration(s, Factory()));
    CHECK(SameState(s, defaults));
    CHECK(MirrorHolds(Factory()));
    CHECK(StockLoads(Factory(), defaults));
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(loaded.repair() == STORAGE_REPAIR_NONE);
    // Saves work again.
    loaded.mutable_state()->media = 1;
    loaded.SaveState();
    CHECK(loaded.chunk_storage_.next_state_chunk_index_ == 3);
    CHECK(TestFlash().wide_sectors == 0);
  }
  // A healthy sector: as stock, the calibration stays and the state resets.
  {
    Setup(calibration, 5);
    Settings s;
    CHECK(s.Init(true));
    CHECK(s.repair() == STORAGE_REPAIR_NONE);
    CHECK(SameCalibration(s, calibration));
    CHECK(SameState(s, defaults));
    CHECK(StockLoads(calibration, defaults));
  }
}

void TestRepairFailures() {
  test_checker.Start("TestRepairFailures");
  const PersistentData calibration = Calibration(0.1f);
  // Mirror erase refused: the settings sector isn't erased.
  {
    Setup(calibration, 5);
    LoseCalibration(0);
    Take(&snapshot);
    TestFlashFailures() = 1000;
    Settings s;
    CHECK(s.Init(true));
    TestFlashFailures() = 0;
    CHECK(s.repair() == STORAGE_REPAIR_FAILED);
    CHECK(s.write_locked());
    CHECK(SameCalibration(s, Factory()));
    snapshot.erases = TestFlashErases();
    CHECK(Unchanged(snapshot));
    s.SaveState();
    CHECK(Unchanged(snapshot));
  }
  // Erases that report success but leave the sector as it was.
  {
    Setup(calibration, 5);
    LoseCalibration(1);
    TestFlashStuckErases() = 1000;
    const uint32_t overwrites = TestFlashOverwrites();
    Settings s;
    CHECK(s.Init(true));
    TestFlashStuckErases() = 0;
    CHECK(s.repair() == STORAGE_REPAIR_FAILED);
    CHECK(s.write_locked());
    CHECK(TestFlashOverwrites() == overwrites);
    Settings again;
    CHECK(again.Init(false) && again.write_locked());
  }
  // Programs refused after the mirror erase: still guarded, and the next
  // try succeeds.
  {
    Setup(calibration, 5);
    LoseCalibration(0);
    TestFlashBudget() = 1;
    Settings s;
    CHECK(s.Init(true));
    TestFlashBudget() = -1;
    CHECK(s.repair() == STORAGE_REPAIR_FAILED);
    CHECK(s.write_locked());
    Settings guarded;
    CHECK(guarded.Init(false) && guarded.write_locked());
    Settings again;
    CHECK(again.Init(true));
    CHECK(again.repair() == STORAGE_REPAIR_CALIBRATION_RESET);
  }
}

void TestRepairPowerLoss() {
  test_checker.Start("TestRepairPowerLoss");
  const State defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  // Mirror erase, four mirror words, sector erase, four calibration words,
  // the state. Once the mirror holds the factory values, every boot
  // restores them (with the states, while the sector is unerased); before
  // that the module stays guarded.
  for (int cut = 0; cut <= 11; ++cut) {
    const State saved = Setup(calibration, 5);
    LoseCalibration(0);
    TestFlashBudget() = cut;
    Settings s;
    CHECK(s.Init(true));
    TestFlashBudget() = -1;
    CHECK(s.repair() == (cut == 11
        ? STORAGE_REPAIR_CALIBRATION_RESET
        : STORAGE_REPAIR_FAILED));
    Settings loaded;
    CHECK(loaded.Init(false));  // Never the factory test.
    CHECK(loaded.write_locked() == (cut < 5));
    CHECK(SameCalibration(loaded, Factory()));
    CHECK(SameState(loaded, cut == 5 ? saved : defaults));
    // Another try always succeeds.
    Settings again;
    CHECK(again.Init(true));
    CHECK(!again.write_locked());
    CHECK(StockLoads(Factory(), defaults));
  }
}

void TestMirrorWrites() {
  test_checker.Start("TestMirrorWrites");
  const PersistentData calibration = Calibration(0.1f);
  // A module updated from a firmware without the mirror loads as before.
  Setup(calibration, 5);
  static uint8_t blank[kSectorSize];
  memset(blank, 255, sizeof(blank));
  Poke(kMirror, blank, kSectorSize);
  Take(&snapshot);
  Settings s;
  CHECK(s.Init(false));
  CHECK(!s.write_locked());
  CHECK(Unchanged(snapshot));
  // Its next rewrite writes the mirror once.
  uint32_t erases = TestFlashErases();
  while (s.chunk_storage_.next_state_chunk_index_ <= kStateChunks) {
    s.SaveState();
  }
  CHECK(TestFlashErases() == erases);
  s.SaveState();
  CHECK(TestFlashErases() == erases + 2);
  CHECK(MirrorHolds(calibration));
  CHECK(TestFlash().wide_sectors == 0);
  static uint8_t mirror[kSectorSize];
  Peek(mirror, kMirror, kSectorSize);
  erases = TestFlashErases();
  while (s.chunk_storage_.next_state_chunk_index_ <= kStateChunks) {
    s.SaveState();
  }
  s.SaveState();
  CHECK(TestFlashErases() == erases + 1);
  static uint8_t now[kSectorSize];
  Peek(now, kMirror, kSectorSize);
  CHECK(!memcmp(now, mirror, kSectorSize));
  // A failed mirror write leaves the settings sector alone; the save is
  // held in RAM, retried at the next save, and nothing locks.
  const PersistentData recalibrated = Calibration(-0.05f);
  memcpy(s.mutable_persistent_data()->adc_calibration,
      recalibrated.adc_calibration, sizeof(recalibrated.adc_calibration));
  Take(&snapshot);
  TestFlashFailures() = 1000;
  s.SavePersistentData();
  TestFlashFailures() = 0;
  snapshot.erases = TestFlashErases();
  CHECK(Unchanged(snapshot));
  CHECK(!s.write_locked());
  s.SaveState();
  CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
  CHECK(MirrorHolds(recalibrated));
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(SameCalibration(loaded, recalibrated));
  // A clean calibration that differs from an older mirror wins, and is
  // mirrored before the erase: a cut right after it restores it.
  const PersistentData older = Calibration(-0.1f);
  Setup(older, 1);
  uint8_t chunk[kCalibrationChunk];
  Peek(chunk, kMirror, sizeof(chunk));
  Setup(calibration, 5);
  Poke(kMirror, chunk, sizeof(chunk));
  Settings current;
  CHECK(current.Init(false) && !current.write_locked());
  CHECK(SameCalibration(current, calibration));
  TestFlashBudget() = 6;
  current.SavePersistentData();
  TestFlashBudget() = -1;
  CHECK(MirrorHolds(calibration));
  Settings restored;
  CHECK(restored.Init(false) && !restored.write_locked());
  CHECK(SameCalibration(restored, calibration));
}

void OnUnmaskedRead(int) {
  printf("ERROR: %s: unmasked read of a damaged flash word\n",
      test_checker.test());
  fflush(stdout);
  _exit(1);
}

int main() {
  setbuf(stdout, NULL);
  void* bank_2 = reinterpret_cast<void*>(0x08100000);
  if (!CHECK(mmap(bank_2, 0x100000, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == bank_2)) {
    return 1;
  }
  TestFlashGuard(&OnUnmaskedRead);
  TestMaskedReader();
  TestCalibrationFromMirror();
  TestCalibrationLost();
  TestStateTerminator();
  TestInterruptedAppend();
  TestInterruptedRewrite();
  TestCalibrationWithoutState();
  TestBlankSector();
  TestWriteLock();
  TestRepair();
  TestRepairFailures();
  TestRepairPowerLoss();
  TestMirrorWrites();
  test_checker.Start("main");
  CHECK(TestFlash().violations == 0);
  CHECK(TestFlash().wide_sectors == 0);
  return test_checker.failures() ? 1 : 0;
}
