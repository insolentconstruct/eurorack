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

#include "beads/blob/synth_choice.h"
#include "beads/test/storage_fixtures.h"
#ifdef PLAITS_BLOBS
#include "beads/test/profile_v3.h"
#endif  // PLAITS_BLOBS

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

// Reads copy bytes, so the destination may be any record at any address.
void TestReadIntoRecords() {
  test_checker.Start("TestReadIntoRecords");
  Wipe();
  const uint32_t base = 0x08100000 + 0x1000;
  const PersistentData calibration = Calibration(0.1f);
  Poke(base, &calibration, sizeof(calibration));
  PersistentData data;
  CHECK(Flash::Read(base, &data, sizeof(data)));
  CHECK(!memcmp(&data, &calibration, sizeof(data)));
  uint8_t bytes[sizeof(calibration) + 1];
  bytes[0] = 0x5a;
  CHECK(Flash::Read(base, &bytes[1], sizeof(calibration)));
  CHECK(bytes[0] == 0x5a);
  CHECK(!memcmp(&bytes[1], &calibration, sizeof(calibration)));
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
  const Saved defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  // Each unreadable calibration word, or a clean but invalid calibration, is
  // restored from the mirror byte for byte, keeping loaded states and the
  // options, unlocked.
  for (int damage = 0; damage < 7; ++damage) {
    const Saved saved = Setup(calibration, 5);
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
    Keep(calibration, calibration);
    CHECK(s.Init(false));  // No factory test.
    CHECK(Release());
    CHECK(!s.write_locked());
    CHECK(s.repair() == STORAGE_REPAIR_NONE);
    CHECK(SameState(s, saved));
    CHECK(SameOptions(s, saved));
    CHECK(SameCalibration(s, calibration));
    CHECK(TestFlash().num_damaged == 0);
    uint8_t stored[kCalibrationChunk];
    Peek(stored, kSector, sizeof(stored));
    CHECK(!memcmp(stored, mirror, sizeof(stored)));
    CHECK(StockLoads(calibration, saved.state));
  }
  // The factory values in both copies, as on a new or repaired module: a
  // header that fails its checks is still restored from the mirror.
  for (int damage = 0; damage < 3; ++damage) {
    Wipe();
    Settings fresh;
    fresh.Init(false);
    fresh.SaveState();
    uint8_t header[8];
    Peek(header, kSector, sizeof(header));
    header[damage == 0 ? 0 : (damage == 1 ? 4 : 6)] ^= 1;
    Poke(kSector, header, sizeof(header));
    Settings s;
    CHECK(s.Init(false) && !s.write_locked());
    CHECK(SameCalibration(s, Factory()));
    CHECK(HoldsCalibration(kSector, Factory()));
  }
  // SEED held as well: the state and the options reset.
  {
    Setup(calibration, 5);
    TestFlashDamage(kSector);
    Settings s;
    CHECK(s.Init(true));
    CHECK(s.repair() == STORAGE_REPAIR_NONE);
    CHECK(SameState(s, defaults));
    CHECK(SameOptions(s, defaults));
    CHECK(SameCalibration(s, calibration));
    Settings loaded;
    CHECK(loaded.Init(false) && SameOptions(loaded, defaults));
  }
  // An intact calibration out of range, and a mirror that isn't: the
  // mirror's.
  {
    PersistentData wild = calibration;
    wild.adc_calibration[CV_ADC_CHANNEL_PITCH].scale *= 1.5f;
    const Saved saved = Setup(calibration, 5);
    PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
    Settings s;
    CHECK(s.Init(false) && !s.write_locked());
    CHECK(SameCalibration(s, calibration));
    CHECK(SameState(s, saved));
    CHECK(SameOptions(s, saved));
    CHECK(StockLoads(calibration, saved.state));
    // Out of range in both copies: use RAM defaults and leave flash alone.
    Setup(calibration, 5);
    PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
    PokeChunk(kMirror, PersistentData::tag, &wild, sizeof(wild));
    Take(&snapshot);
    Settings both;
    CHECK(both.Init(false) && both.write_locked());
    CHECK(SameCalibration(both, Factory()));
    CHECK(Unchanged(snapshot));
  }
}

// No valid state, an intact calibration out of range and a good mirror:
// the mirror's calibration is chosen before anything is written.
void TestRestoredCalibrationChoice() {
  test_checker.Start("TestRestoredCalibrationChoice");
  const Saved defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  PersistentData wild = calibration;
  wild.adc_calibration[CV_ADC_CHANNEL_PITCH].scale *= 1.5f;
  for (int unreadable = 0; unreadable < 2; ++unreadable) {
    const Saved saved = Setup(calibration, 5);
    PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
    if (unreadable) {
      TestFlashDamage(StateChunk(1));
    } else {
      uint8_t byte = 0;
      Poke(StateChunk(1) + 12, &byte, 1);
    }
    static uint8_t mirror[kSectorSize];
    Peek(mirror, kMirror, kSectorSize);
    Settings s;
    CHECK(s.Init(false) && !s.write_locked());
    CHECK(SameCalibration(s, calibration));
    CHECK(SameState(s, defaults));
    CHECK(SameOptions(s, saved));  // The options are in the mirror sector.
    static uint8_t now[kSectorSize];
    Peek(now, kMirror, kSectorSize);
    CHECK(!memcmp(now, mirror, kSectorSize));
    uint8_t chunk[kCalibrationChunk];
    Peek(chunk, kSector, sizeof(chunk));
    CHECK(!memcmp(chunk, mirror, sizeof(chunk)));
    CHECK(StockLoads(calibration, defaults.state));
  }
}

void TestCalibrationLost() {
  test_checker.Start("TestCalibrationLost");
  const Saved defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  // The calibration unreadable or invalid, and the mirror unreadable,
  // invalid, out of range or blank: the guarded state.
  for (int mirror = 0; mirror < 4; ++mirror) {
    for (int damage = 0; damage < 2; ++damage) {
      PersistentData wild = calibration;
      wild.adc_calibration[CV_ADC_CHANNEL_SHAPE].scale *= 1.5f;
      Setup(calibration, 5);
      if (mirror == 2) {
        PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
        PokeChunk(kMirror, PersistentData::tag, &wild, sizeof(wild));
      }
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
      CHECK(SameOptions(s, defaults));
      CHECK(SameCalibration(s, Factory()));
      CHECK(Unchanged(snapshot));
    }
  }
}

void TestImplausibleCalibrationWithoutMirror() {
  test_checker.Start("TestImplausibleCalibrationWithoutMirror");
  const Saved defaults = Defaults();
  for (int nan = 0; nan < 2; ++nan) {
    PersistentData wild = Factory();
    wild.adc_calibration[CV_ADC_CHANNEL_PITCH].offset =
        nan ? __builtin_nanf("") : 84.0f;
    for (int mirror = 0; mirror < 2; ++mirror) {
      Setup(Calibration(0.1f), 5);
      PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
      PokeChunk(kMirror, PersistentData::tag, &wild, sizeof(wild));
      if (!mirror) {
        static uint8_t blank[kSectorSize];
        memset(blank, 255, sizeof(blank));
        Poke(kMirror, blank, sizeof(blank));
      }
      Take(&snapshot);
      Settings s;
      CHECK(s.Init(false) && s.write_locked());
      CHECK(SameState(s, defaults));
      CHECK(SameOptions(s, defaults));
      CHECK(SameCalibration(s, Factory()));
      s.SaveState();
      CHECK(Unchanged(snapshot));

      Settings repair;
      CHECK(repair.Init(true) && !repair.write_locked());
      CHECK(repair.repair() == STORAGE_REPAIR_CALIBRATION_RESET);
      CHECK(StockLoads(Factory(), defaults.state));
    }
  }
}

void TestFactoryPrimaryInterruptedRecalibration() {
  test_checker.Start("TestFactoryPrimaryInterruptedRecalibration");
  const Saved defaults = Defaults();
  const PersistentData factory = Factory();
  const PersistentData old_calibration = Calibration(0.1f);
  const PersistentData new_calibration = Calibration(-0.1f);
  for (int cut = 0; cut <= 24; ++cut) {
    Setup(old_calibration, 5);
    PokeChunk(kSector, PersistentData::tag, &factory, sizeof(factory));
    TestFlashBudget() = 0;
    Settings s;
    CHECK(s.Init(false) && SameCalibration(s, old_calibration));
    *s.mutable_persistent_data() = new_calibration;
    TestFlashBudget() = cut;
    const bool saved = s.SavePersistentData();
    TestFlashBudget() = -1;

    if (cut >= 1 && cut <= 4) {
      // Stock rebuilds the torn primary with factory values.
      PersistentData stock_calibration = factory;
      State stock_state = defaults.state;
      StockStorage stock;
      const int wide_sectors = TestFlash().wide_sectors;
      CHECK(!stock.Init(&stock_calibration, &stock_state));
      TestFlash().wide_sectors = wide_sectors;
      CHECK(stock_calibration.SameCalibration(factory));
      CHECK(MirrorHolds(old_calibration));
    }
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameCalibration(
        loaded, cut <= 12 ? old_calibration : new_calibration));
    if (cut == 24) {
      CHECK(saved);
    }
  }
}

void TestAppendAfterBlankGap() {
  test_checker.Start("TestAppendAfterBlankGap");
  const PersistentData calibration = Calibration(0.1f);
  Setup(calibration, 3);
  uint8_t blank[kStateChunk];
  memset(blank, 255, sizeof(blank));
  Poke(StateChunk(3), blank, sizeof(blank));

  Settings s;
  CHECK(s.Init(false) && !s.write_locked());
  CHECK(s.state().gain == 1);
  s.mutable_state()->gain = 99;
  s.SaveState();
  CHECK(StockLoads(calibration, s.state()));
}

void TestRefusedRecalibrationPreservesMeasuredMirror() {
  test_checker.Start("TestRefusedRecalibrationPreservesMeasuredMirror");
  const PersistentData factory = Factory();
  const PersistentData measured = Calibration(0.1f);
  Setup(measured, 5);
  PokeChunk(kSector, PersistentData::tag, &factory, sizeof(factory));
  TestFlashBudget() = 0;
  Settings s;
  CHECK(s.Init(false) && SameCalibration(s, measured));
  TestFlashBudget() = -1;
  Take(&snapshot);
  s.mutable_persistent_data()->adc_calibration[CV_ADC_CHANNEL_PITCH].offset =
      84.0f;
  CHECK(!s.SavePersistentData());
  CHECK(SameCalibration(s, measured));
  CHECK(Unchanged(snapshot));
}

void TestAppendBeforeUnreadableState() {
  test_checker.Start("TestAppendBeforeUnreadableState");
  const PersistentData calibration = Calibration(0.1f);
  Setup(calibration, 2);
  Blank(StateChunk(2), kStateChunk);
  TestFlashDamage(StateChunk(3));
  Settings s;
  CHECK(s.Init(false) && !s.write_locked());
  s.mutable_state()->gain = 99;
  s.SaveState();
  CHECK(TestFlash().num_damaged == 0);
  if (!TestFlash().num_damaged) {
    CHECK(StockLoads(calibration, s.state()));
  }
}

void TestRetryIncompleteErase() {
  test_checker.Start("TestRetryIncompleteErase");
  const PersistentData calibration = Calibration(0.1f);
  Setup(calibration, 5);
  Settings s;
  CHECK(s.Init(false));
  const uint32_t erases = TestFlashErases();
  TestFlashStuckErases() = 1;
  CHECK(s.SavePersistentData());
  CHECK(TestFlashErases() == erases + 2);
  CHECK(StockLoads(calibration, s.state()));
}

void TestStateTerminator() {
  test_checker.Start("TestStateTerminator");
  const Saved defaults = Defaults();
  const PersistentData calibration = Calibration(-0.1f);
  // An unreadable record ends the states, as a torn one does: what an
  // interrupted append leaves. After none, one or many valid records.
  const int saves[] = { 0, 0, 10, 10 };
  const int damaged[] = { 1, 2, 12, 6 };
  for (int i = 0; i < 4; ++i) {
    Setup(calibration, saves[i]);
    Saved last = defaults;
    Peek(&last.state, StateChunk(damaged[i] - 1) + 8, sizeof(last.state));
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
    CHECK(SameState(loaded, Of(s)));
    CHECK(StockLoads(calibration, s.state()));
  }
  // Past the terminator nothing is read at boot.
  const int unread[] = { 13, 14, 40, kStateChunks };
  for (size_t i = 0; i < sizeof(unread) / sizeof(unread[0]); ++i) {
    const Saved saved = Setup(calibration, 10);
    TestFlashDamage(StateChunk(unread[i]));
    Take(&snapshot);
    Settings s;
    CHECK(s.Init(false));
    CHECK(!s.write_locked());
    CHECK(SameState(s, saved));
    CHECK(SameCalibration(s, calibration));
    CHECK(Unchanged(snapshot));
    // Rewrite before the next save would expose the damaged word to stock.
    for (int j = 0; j < unread[i] - 12; ++j) {
      s.mutable_state()->gain = uint8_t(100 + j);
      s.SaveState();
    }
    CHECK(TestFlash().num_damaged == 0);
    CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameState(loaded, Of(s)));
  }
}

void TestInterruptedAppend() {
  test_checker.Start("TestInterruptedAppend");
  const PersistentData calibration = Calibration(0.05f);
  // Power lost before the program: the previous state loads.
  {
    const Saved saved = Setup(calibration, 4);
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
  const Saved saved = Setup(calibration, 4);
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
  CHECK(loaded.Init(false) && SameState(loaded, Of(s)));
  CHECK(StockLoads(calibration, s.state()));
}

void TestInterruptedRewrite() {
  test_checker.Start("TestInterruptedRewrite");
  const Saved defaults = Defaults();
  const PersistentData old_calibration = Calibration(0.1f);
  const PersistentData new_calibration = Calibration(-0.1f);
  // A calibration save: mirror erase, four mirror words, the two words of the
  // record put back, sector erase, four calibration words, the state.
  for (int cut = 0; cut <= 13; ++cut) {
    const Saved saved = Setup(old_calibration, 6);
    Settings s;
    CHECK(s.Init(false));
    memcpy(s.mutable_persistent_data()->adc_calibration,
        new_calibration.adc_calibration,
        sizeof(new_calibration.adc_calibration));
    Keep(old_calibration, new_calibration);
    TestFlashBudget() = cut;
    s.SavePersistentData();
    TestFlashBudget() = -1;
    CHECK(Release());

    Settings loaded;
    CHECK(loaded.Init(false));  // Never the factory test.
    CHECK(!loaded.write_locked());
    if (cut <= 7) {
      // The settings sector wasn't touched.
      CHECK(SameState(loaded, saved));
      CHECK(SameCalibration(loaded, old_calibration));
    } else if (cut <= 12) {
      // Erased, the calibration partly or wholly written: from the mirror.
      CHECK(SameState(loaded, defaults));
      CHECK(SameCalibration(loaded, new_calibration));
    } else {
      CHECK(SameState(loaded, saved));
      CHECK(SameCalibration(loaded, new_calibration));
    }
    // A cut inside the mirror sector before its record is back resets the
    // options; past it they come back from that sector. Page 5 and Rings'
    // polyphony sit in the record's second word, so both words complete it.
    CHECK(SameOptions(loaded, cut >= 1 && cut <= 6 ? defaults : saved));
    // Whatever came back is stable.
    Settings again;
    CHECK(again.Init(false) && !again.write_locked());
    CHECK(SameState(again, Of(loaded)));
    CHECK(SameOptions(again, Of(loaded)));
    CHECK(SameCalibration(again, loaded.persistent_data()));
  }
}

// #317: a restore that failed at power-up leaves the calibration in the
// mirror only. A recalibration copies it back to the settings sector before
// the mirror is erased, so a cut at any point keeps a calibration.
void TestMirrorOnlyCopy() {
  test_checker.Start("TestMirrorOnlyCopy");
  const Saved defaults = Defaults();
  const PersistentData old_calibration = Calibration(0.1f);
  const PersistentData new_calibration = Calibration(-0.1f);
  // Sector erase, four copied words, mirror erase, four mirror words, the
  // record, sector erase, four calibration words, the state.
  for (int cut = 0; cut <= 18; ++cut) {
    const Saved saved = Setup(old_calibration, 6);
    Blank(kSector, kSectorSize);
    Settings s;
    TestFlashBudget() = 0;  // The restore fails.
    CHECK(s.Init(false));
    TestFlashBudget() = -1;
    CHECK(SameCalibration(s, old_calibration));
    CHECK(SameOptions(s, saved));
    CHECK(Flash::IsBlank(kSector, kSectorSize));
    memcpy(s.mutable_persistent_data()->adc_calibration,
        new_calibration.adc_calibration,
        sizeof(new_calibration.adc_calibration));
    Keep(old_calibration, new_calibration);
    TestFlashBudget() = cut;
    s.SavePersistentData();
    TestFlashBudget() = -1;
    CHECK(Release());

    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(!loaded.write_locked());
    CHECK(SameCalibration(
        loaded, cut <= 12 ? old_calibration : new_calibration));
    CHECK(SameOptions(loaded, cut >= 6 && cut <= 11 ? defaults : saved));
    Settings again;
    CHECK(again.Init(false) && !again.write_locked());
    CHECK(SameCalibration(again, loaded.persistent_data()));
  }
}

void TestCalibrationWithoutState() {
  test_checker.Start("TestCalibrationWithoutState");
  const Saved defaults = Defaults();
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
  CHECK(StockLoads(calibration, defaults.state));
}

void TestBlankSector() {
  test_checker.Start("TestBlankSector");
  const Saved defaults = Defaults();
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
    CHECK(StockLoads(Factory(), defaults.state));
    Wipe();
    Settings seed;
    CHECK(seed.Init(true));  // SEED held: no factory test, as stock.
  }
  // Erased, then the power went: the mirror's calibration and the options
  // come back.
  {
    const Saved saved = Setup(calibration, 3);
    Poke(kSector, blank, kSectorSize);
    Settings s;
    CHECK(s.Init(false));
    CHECK(!s.write_locked());
    CHECK(SameState(s, defaults));
    CHECK(SameOptions(s, saved));
    CHECK(SameCalibration(s, calibration));
    CHECK(StockLoads(calibration, defaults.state));
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
  // With a readable sector, a damaged mirror is ignored, with the options
  // after it, then rewritten.
  for (int damage = 0; damage < 2; ++damage) {
    const Saved saved = Setup(calibration, 3);
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
    CHECK(SameOptions(s, defaults));
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
  s.SaveOptions();
  // Every way in to the flash.
  CHECK(!s.chunk_storage_.SaveRecord());
  CHECK(!s.chunk_storage_.CompactMirror());
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
  const Saved defaults = Defaults();
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
    CHECK(SameOptions(s, defaults));
    CHECK(MirrorHolds(Factory()));
    CHECK(StockLoads(Factory(), defaults.state));
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(loaded.repair() == STORAGE_REPAIR_NONE);
    CHECK(SameOptions(loaded, defaults));
    // Saves work again.
    loaded.mutable_state()->media = 1;
    loaded.SaveState();
    CHECK(loaded.chunk_storage_.next_state_chunk_index_ == 3);
    CHECK(TestFlash().wide_sectors == 0);
  }
  // A healthy sector: as stock, the calibration stays and the state resets;
  // the options reset too, appended without erasing the mirror sector.
  {
    Setup(calibration, 5);
    const uint32_t erases = TestFlashErases();
    Settings s;
    Keep(calibration, calibration);
    CHECK(s.Init(true));
    CHECK(Release());
    CHECK(TestFlashErases() == erases + 1);
    CHECK(s.repair() == STORAGE_REPAIR_NONE);
    CHECK(SameCalibration(s, calibration));
    CHECK(SameState(s, defaults));
    CHECK(SameOptions(s, defaults));
    CHECK(MirrorHolds(calibration));
    CHECK(StockLoads(calibration, defaults.state));
    Settings loaded;
    CHECK(loaded.Init(false) && SameOptions(loaded, defaults));
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
  const Saved defaults = Defaults();
  const PersistentData calibration = Calibration(0.1f);
  // A cut after each repair step: mirror erase, four words, the record, sector
  // erase, four words, the state. Once the mirror holds the factory values,
  // every boot restores them; before that the module stays guarded.
  for (int cut = 0; cut <= 13; ++cut) {
    const Saved saved = Setup(calibration, 5);
    LoseCalibration(0);
    TestFlashBudget() = cut;
    Settings s;
    CHECK(s.Init(true));
    TestFlashBudget() = -1;
    CHECK(s.repair() == (cut == 13
        ? STORAGE_REPAIR_CALIBRATION_RESET
        : STORAGE_REPAIR_FAILED));
    Settings loaded;
    CHECK(loaded.Init(false));  // Never the factory test.
    CHECK(loaded.write_locked() == (cut < 5));
    CHECK(SameCalibration(loaded, Factory()));
    CHECK(SameState(loaded, cut >= 5 && cut <= 7 ? saved : defaults));
    CHECK(SameOptions(loaded, defaults));
    // Another try always succeeds.
    Settings again;
    CHECK(again.Init(true));
    CHECK(!again.write_locked());
    CHECK(StockLoads(Factory(), defaults.state));
  }
}

void TestMirrorWrites() {
  test_checker.Start("TestMirrorWrites");
  const PersistentData calibration = Calibration(0.1f);
  // A module updated from a firmware without the mirror loads as before, with
  // the options at their defaults.
  Setup(calibration, 5);
  static uint8_t blank[kSectorSize];
  memset(blank, 255, sizeof(blank));
  Poke(kMirror, blank, kSectorSize);
  Take(&snapshot);
  Settings s;
  CHECK(s.Init(false));
  CHECK(!s.write_locked());
  CHECK(Unchanged(snapshot));
  // Its first save writes the mirror with the options; its next rewrite
  // finds it in place.
  uint32_t erases = TestFlashErases();
  while (s.chunk_storage_.next_state_chunk_index_ <= kStateChunks) {
    s.SaveState();
  }
  CHECK(TestFlashErases() == erases + 1);
  CHECK(MirrorHolds(calibration));
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
  TestFlashBudget() = 8;
  current.SavePersistentData();
  TestFlashBudget() = -1;
  CHECK(MirrorHolds(calibration));
  Settings restored;
  CHECK(restored.Init(false) && !restored.write_locked());
  CHECK(SameCalibration(restored, calibration));
}

void TestLowSupply() {
  test_checker.Start("TestLowSupply");
  const PersistentData calibration = Calibration(0.1f);
  const PersistentData new_calibration = Calibration(-0.1f);
  // Init starts the PVD at level 6, and leaves the rest of PWR_CR1 alone.
  TestFlashPowerUp();
  Setup(calibration, 4);
  CHECK(TestFlash().pwr.CR1 == \
      (PWR_CR1_RESET | PWR_CR1_PLS_LEV6 | PWR_CR1_PVDEN));
  // A new module powered up on a low supply: nothing written, the factory
  // test runs.
  {
    Wipe();
    Take(&snapshot);
    TestFlashPowerUp();
    TestFlash().supply_low = true;
    Settings s;
    CHECK(!s.Init(false));
    CHECK(Unchanged(snapshot));
    TestFlash().supply_low = false;
  }
  // State saves are skipped while the supply is low, and resume where they
  // left off when it comes back.
  {
    Setup(calibration, 4);
    Settings s;
    CHECK(s.Init(false));
    const uint16_t next = s.chunk_storage_.next_state_chunk_index_;
    Take(&snapshot);
    TestFlash().supply_low = true;
    for (int i = 0; i < kStateChunks + 5; ++i) {
      s.mutable_state()->gain = uint8_t(i);
      s.SaveState();
    }
    CHECK(Unchanged(snapshot));
    CHECK(s.chunk_storage_.next_state_chunk_index_ == next);
    TestFlash().supply_low = false;
    s.mutable_state()->gain = 99;
    s.SaveState();
    CHECK(s.chunk_storage_.next_state_chunk_index_ == next + 1);
    CHECK(TestFlashErases() == snapshot.erases);
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameState(loaded, Of(s)));
    CHECK(StockLoads(calibration, s.state()));
  }
  // A calibration save refused before the mirror erase: nothing written,
  // and the next save once the supply is back rewrites the sector.
  {
    const Saved saved = Setup(calibration, 4);
    Settings s;
    CHECK(s.Init(false));
    memcpy(s.mutable_persistent_data()->adc_calibration,
        new_calibration.adc_calibration,
        sizeof(new_calibration.adc_calibration));
    Take(&snapshot);
    TestFlash().supply_low = true;
    s.SavePersistentData();
    CHECK(Unchanged(snapshot));
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameCalibration(loaded, calibration));
    CHECK(SameState(loaded, saved));
    TestFlash().supply_low = false;
    s.SaveState();
    CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
    Settings again;
    CHECK(again.Init(false) && SameCalibration(again, new_calibration));
    CHECK(SameState(again, Of(s)));
  }
  CHECK(TestFlash().low_supply_writes == 0);
}

void TestSupplyDrop() {
  test_checker.Start("TestSupplyDrop");
  const Saved defaults = Defaults();
  const PersistentData old_calibration = Calibration(0.1f);
  const PersistentData new_calibration = Calibration(-0.1f);
  TestFlashPowerUp();
  // A supply drop after each step of a calibration save reads back as a
  // power cut at the same point.
  for (int drop = 1; drop <= 12; ++drop) {
    const Saved saved = Setup(old_calibration, 6);
    Settings s;
    CHECK(s.Init(false));
    memcpy(s.mutable_persistent_data()->adc_calibration,
        new_calibration.adc_calibration,
        sizeof(new_calibration.adc_calibration));
    TestFlash().supply_budget = drop;
    s.SavePersistentData();
    CHECK(TestFlash().supply_low);
    CHECK(s.chunk_storage_.next_state_chunk_index_ == 0xffff);
    TestFlash().supply_low = false;
    TestFlash().supply_budget = 0;

    Settings loaded;
    CHECK(loaded.Init(false));  // Never the factory test.
    CHECK(!loaded.write_locked());
    if (drop <= 7) {
      CHECK(SameState(loaded, saved));
      CHECK(SameCalibration(loaded, old_calibration));
    } else {
      CHECK(SameState(loaded, defaults));
      CHECK(SameCalibration(loaded, new_calibration));
    }
    // Page 5 and Rings' polyphony sit in the record's second word: both words complete it.
    CHECK(SameOptions(loaded, drop <= 6 ? defaults : saved));
    // The interrupted save completes on the next one, the options with the
    // next save after it at the latest.
    s.SaveState();
    CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
    s.SaveState();
    Settings again;
    CHECK(again.Init(false) && !again.write_locked());
    CHECK(SameCalibration(again, new_calibration));
    CHECK(SameState(again, Of(s)));
    CHECK(SameOptions(again, saved));
    CHECK(StockLoads(new_calibration, s.state()));
  }
  // A repair with the supply low fails without writing, and stays guarded;
  // the next one with the supply back succeeds.
  {
    Setup(old_calibration, 5);
    LoseCalibration(0);
    Take(&snapshot);
    TestFlash().supply_low = true;
    Settings s;
    CHECK(s.Init(true));
    CHECK(s.repair() == STORAGE_REPAIR_FAILED);
    CHECK(s.write_locked());
    CHECK(Unchanged(snapshot));
    TestFlash().supply_low = false;
    Settings again;
    CHECK(again.Init(true));
    CHECK(again.repair() == STORAGE_REPAIR_CALIBRATION_RESET);
    CHECK(!again.write_locked());
  }
  CHECK(TestFlash().low_supply_writes == 0);
}

void TestProfileLocks() {
  test_checker.Start("TestProfileLocks");
  uint8_t b[kProfileSize];
  ProfileTemplate(b);
  b[0] = 0;
  const PersistentData calibration = Calibration(0.05f);
  for (int storage = 0; storage < 8; ++storage) {
    for (int seed = 0; seed < 2; ++seed) {
      for (int fail = 0; fail < 2; ++fail) {
        Setup(calibration, 2);
        State saved;
        Peek(&saved, StateChunk(3) + 8, sizeof(saved));
        if (storage == 1 || storage == 2 || storage == 5) {
          TestFlashExpose(true);
          memset(reinterpret_cast<void*>(kSector), 255, kSectorSize);
          if (storage == 2) {
            memset(reinterpret_cast<void*>(kMirror), 255, kSectorSize);
          }
          TestFlashExpose(false);
          if (storage == 5) {
            TestFlashDamage(kMirror);
          }
        } else if (storage == 3) {
          TestFlashExpose(true);
          memset(reinterpret_cast<void*>(StateChunk(1)), 255,
              kSectorSize - kCalibrationChunk);
          TestFlashExpose(false);
        } else if (storage == 4) {
          TestFlashDamage(kSector);
          TestFlashDamage(kMirror);
        } else if (storage == 6) {
          TestFlashDamage(kSector);
        }
        if (storage == 7) {
          PersistentData wild = calibration;
          wild.adc_calibration[0].scale = 0.0f;
          Setup(calibration, 2);
          PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
        }
        Take(&snapshot);
        TestFlashFailures() = fail ? 1000 : 0;
        Settings locked;
        locked.Init(seed, b);
        CHECK(locked.profile_locked());
        CHECK(locked.write_locked());
        if (!storage) {
          CHECK(locked.state().media == 1 && locked.state().gain == 2);
        }
        if (!storage || ((storage == 4 || storage == 5) && !seed)) {
          CHECK(Unchanged(snapshot));
        }
        if (!fail && storage && ((storage != 4 && storage != 5) || seed)) {
          State wire;
          Peek(&wire, StateChunk(1) + 8, sizeof(wire));
          if (storage == 6 || storage == 7) {
            CHECK(!memcmp(&wire, &saved, sizeof(wire)));
          } else {
            CHECK(wire.padding[0] == 0);
            CHECK(wire.media == 3 && wire.gain == 0);
          }
          CHECK(!locked.chunk_storage_.storage_damaged());
        }
        TestFlashFailures() = 0;
        Take(&snapshot);
        locked.chunk_storage_.Recover(
            &locked.persistent_data_, &locked.stored_state_);
        CHECK(Unchanged(snapshot));
        locked.set_pitch_scale(PITCH_SCALE_SHRI);
        locked.SaveOptions();
        locked.SaveState();
        locked.SavePersistentData();
        CHECK(Unchanged(snapshot));
        CHECK(locked.profile_locked());
      }
    }
  }
}

// #347: with writes locked by a refused profile or by damage, a
// recalibration reports its refusal, nothing is written, and the stored
// calibration loads again.
void TestLockedCalibrationRefused() {
  test_checker.Start("TestLockedCalibrationRefused");
  const PersistentData calibration = Calibration(0.07f);
  const PersistentData fresh = Calibration(0.03f);
  uint8_t bad[kProfileSize];
  memset(bad, 0, sizeof(bad));
  for (int damaged = 0; damaged < 2; ++damaged) {
    Setup(calibration, 5);
    if (damaged) {
      LoseCalibration(0);
    }
    Take(&snapshot);
    Settings s;
    if (damaged) {
      s.Init(false);
    } else {
      s.Init(false, bad);
    }
    CHECK(s.profile_locked() == !damaged && s.write_locked());
    CHECK(s.chunk_storage_.storage_damaged() == (damaged == 1));
    memcpy(s.mutable_persistent_data()->adc_calibration, fresh.adc_calibration,
        sizeof(fresh.adc_calibration));
    CHECK(!s.SavePersistentData());
    CHECK(Unchanged(snapshot));
    if (!damaged) {
      Settings loaded;
      CHECK(loaded.Init(false) && SameCalibration(loaded, calibration));
    }
  }
}

void TestProfileSettings() {
  test_checker.Start("TestProfileSettings");
  uint8_t b[kProfileSize];
  ProfileTemplate(b);
  b[18] = PITCH_SCALE_SHRI;
  b[42] = OUTPUT_LIMITER_FAST;
  b[78] = 127;
  b[84] = 200;
  ProfileChecksum(b);
  Wipe();
  Settings s;
  CHECK(!s.Init(false, b));
  CHECK(s.pitch_scale() == PITCH_SCALE_SHRI);
  CHECK(s.stereo_width() == 127 && s.FeatureValue(FEATURE_DIFFUSION) == 200);
  CHECK(s.output_limiter_mode() == OUTPUT_LIMITER_FAST);
  s.set_pitch_scale(PITCH_SCALE_CHROMATIC);
  s.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false, b));
  CHECK(loaded.pitch_scale() == PITCH_SCALE_CHROMATIC);
  const PersistentData calibration = loaded.persistent_data();
  CHECK(loaded.Init(true, b));
  CHECK(loaded.pitch_scale() == PITCH_SCALE_SHRI);
  CHECK(SameCalibration(loaded, calibration));
  // State as stock writes it: zero after monophonic mode.
  State wire;
  Peek(&wire, StateChunk(1) + 8, sizeof(wire));
  for (size_t i = 0; i < sizeof(wire.padding); ++i) {
    CHECK(wire.padding[i] == 0);
  }
}

// Each saved option has a byte of its own in the record, at an offset frozen
// for the record's version; State keeps stock's bytes only.
void TestSavedFeatures() {
  test_checker.Start("TestSavedFeatures");
  CHECK(kOptionsRecordVersion == 1);
  CHECK(kRecordOffsets[FEATURE_PITCH] == 8);
  CHECK(kRecordOffsets[FEATURE_INTERVALS] == 17);
  CHECK(kRecordOffsets[FEATURE_WIDTH] == 18);
  CHECK(kRecordOffsets[FEATURE_DIFFUSION] == 19);
  CHECK(kRecordOffsets[FEATURE_BRIGHTNESS] == 20);
  CHECK(kRecordOffsets[FEATURE_SEED_LATCH] == 21);
  CHECK(kRecordOffsets[FEATURE_TIME_SCAN] == 22);
  CHECK(kRecordOffsets[FEATURE_PLAITS_BANK] == 23);
  CHECK(kRecordOffsets[FEATURE_RINGS_POLYPHONY] == 27);
  CHECK(!kRecordOffsets[FEATURE_MONOPHONIC]);
  CHECK(kRecordOffsets[FEATURE_RESONATOR] == 24);
  CHECK(kRecordOffsets[FEATURE_RESONATOR_MATERIAL] == 25);
  CHECK(kRecordOffsets[FEATURE_RESONATOR_RING] == 26);
  for (int i = 1; i < FEATURE_LAST; ++i) {
    // Saved: the options a user changes, Monophonic aside (stock State).
    const bool second_word = i == FEATURE_RESONATOR ||
        i == FEATURE_RESONATOR_MATERIAL || i == FEATURE_RESONATOR_RING ||
        i == FEATURE_RINGS_POLYPHONY || i == FEATURE_GAIN_CEILING;
    const bool unsaved = kFeatureDefinitions[i].field == kNoField ||
        kFeatureDefinitions[i].field == kReservedField;
    CHECK(!kRecordOffsets[i] == (unsaved &&
        i != FEATURE_WIDTH && i != FEATURE_DIFFUSION && !second_word));
  }
  uint8_t b[kProfileSize];
  ProfileTemplate(b);
  Wipe();
  Settings s;
  s.Init(false);
  for (int pitch = 0; pitch < 10; ++pitch) {
    s.set_pitch_scale(PitchScale(pitch));
    s.set_clouds_style(true);
    s.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
    s.set_self_play_on_request(true);
    s.set_synth(SYNTH_PLAITS);
    s.set_attack_mode(ATTACK_MODE_SNAP_AND_TRIGGER);
    s.set_evolving_feedback(true);
    s.set_delay_mode(DELAY_MODE_WIDE_DETENT);
    s.set_random_intervals(RANDOM_INTERVALS_FIFTHS);
    s.set_spectral(true);
    s.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.pitch_scale() == pitch);
    CHECK(loaded.clouds_style() && loaded.synth() && loaded.spectral());
    CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    CHECK(loaded.self_play_on_request() && loaded.random_intervals());
    CHECK(loaded.attack_mode() == ATTACK_MODE_SNAP_AND_TRIGGER);
    const uint8_t* raw =
        reinterpret_cast<const uint8_t*>(&loaded.stored_record_);
    CHECK(raw[0] == kOptionsRecordVersion);
    CHECK(raw[8] == pitch && raw[9] == 1 && raw[10] == DELAY_MODE_WIDE_DETENT);
    CHECK(raw[11] == 1 && raw[12] == OUTPUT_LIMITER_OFF && raw[13] == 1);
    CHECK(raw[14] == 1 && raw[15] == ATTACK_MODE_SNAP_AND_TRIGGER);
    CHECK(raw[16] == 1 && raw[17] == RANDOM_INTERVALS_FIFTHS);
    // Page 5 at the factory values: Off, String, Medium; one voice.
    CHECK(raw[24] == 0 && raw[25] == 1 && raw[26] == 1 && raw[27] == 0);
    for (size_t i = 28; i < sizeof(OptionsRecord); ++i) {
      CHECK(raw[i] == 0xff);
    }
    for (size_t i = 0; i < sizeof(State().padding); ++i) {
      CHECK(loaded.stored_state_.padding[i] == 0);
    }
  }
  // Removing and fixing a feature keeps its saved byte.
  b[17] = PROFILE_OFF;
  b[19] = 0;
  ProfileChecksum(b);
  Settings off;
  CHECK(off.Init(false, b));
  CHECK(off.pitch_scale() == PITCH_SCALE_OFF);
  off.SaveState();
  Settings restored;
  CHECK(restored.Init(false));
  CHECK(restored.pitch_scale() == PITCH_SCALE_SHRI);
  b[17] = PROFILE_FIXED;
  b[18] = PITCH_SCALE_CHROMATIC;
  ProfileChecksum(b);
  CHECK(off.Init(false, b));
  CHECK(off.pitch_scale() == PITCH_SCALE_CHROMATIC);
  off.SaveState();
  CHECK(restored.Init(false));
  CHECK(restored.pitch_scale() == PITCH_SCALE_SHRI);
  // A byte past the feature's values is not set: the profile's default.
  ProfileTemplate(b);
  b[42] = OUTPUT_LIMITER_FAST;
  ProfileChecksum(b);
  const uint8_t unset[] = { 3, 0x80, 0xff };
  for (size_t i = 0; i < sizeof(unset); ++i) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&restored.record_);
    bytes[kRecordOffsets[FEATURE_LIMITER]] = unset[i];
    CHECK(restored.chunk_storage_.SaveRecord());
    CHECK(restored.Init(false, b));
    CHECK(restored.output_limiter_mode() == OUTPUT_LIMITER_FAST);
    CHECK(restored.clouds_style());
  }
}

// Rings is Synth value 2 and its polyphony a byte of its own; a byte past
// either's values is not set, as Rings' 2 is for a build without it.
void TestRingsSettings() {
  test_checker.Start("TestRingsSettings");
  CHECK(kFeatureDefinitions[FEATURE_SYNTH].values == SYNTH_LAST);
  Wipe();
  Settings s;
  s.Init(false);
  CHECK(s.synth() == SYNTH_BEADS && s.rings_polyphony() == 1);
  const uint8_t voices[] = { 1, 2, 4 };
  for (int poly = 0; poly < 3; ++poly) {
    s.set_synth(SYNTH_RINGS);
    s.SetFeatureValue(FEATURE_RINGS_POLYPHONY, poly);
    CHECK(s.rings_polyphony() == voices[poly]);
    s.SaveOptions();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.synth() == SYNTH_RINGS);
    CHECK(loaded.rings_polyphony() == voices[poly]);
    const uint8_t* raw =
        reinterpret_cast<const uint8_t*>(&loaded.stored_record_);
    CHECK(raw[kRecordOffsets[FEATURE_SYNTH]] == SYNTH_RINGS);
    CHECK(raw[kRecordOffsets[FEATURE_RINGS_POLYPHONY]] == poly);
  }
  // Past the values, each takes the profile's start: Plaits, four voices.
  uint8_t b[kProfileSize];
  ProfileTemplate(b);
  uint8_t* synth = b + 16 + 6 * (FEATURE_SYNTH - 1);
  uint8_t* poly = b + 16;
  while (poly[0] != FEATURE_RINGS_POLYPHONY) {
    poly += 6;
  }
  CHECK(synth[0] == FEATURE_SYNTH && poly[0] == FEATURE_RINGS_POLYPHONY);
  synth[2] = SYNTH_PLAITS;
  poly[2] = 2;
  ProfileChecksum(b);
  const uint8_t unset[] = { 3, 0x80, 0xff };
  for (size_t i = 0; i < sizeof(unset); ++i) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&s.record_);
    bytes[kRecordOffsets[FEATURE_SYNTH]] = unset[i];
    bytes[kRecordOffsets[FEATURE_RINGS_POLYPHONY]] = unset[i];
    CHECK(s.chunk_storage_.SaveRecord());
    CHECK(s.Init(false, b));
    CHECK(s.synth() == SYNTH_PLAITS && s.rings_polyphony() == 4);
  }
  // Fixed or off, the profile's Synth holds and the saved byte stays.
  s.set_synth(SYNTH_RINGS);
  s.SaveOptions();
  synth[1] = PROFILE_FIXED;
  synth[3] = 0;  // A fixed setting has no page.
  ProfileChecksum(b);
  CHECK(s.Init(false, b) && s.synth() == SYNTH_PLAITS);
  s.SaveOptions();
  Settings restored;
  CHECK(restored.Init(false) && restored.synth() == SYNTH_RINGS);
}

// Page 5 saves its three settings; exact values and the CV target do not.
void TestResonatorSaved() {
  test_checker.Start("TestResonatorSaved");
  Wipe();
  Settings s;
  s.Init(false);
  for (int placement = 0; placement < 5; ++placement) {
    for (int value = 0; value < 8; ++value) {
      s.SetFeatureValue(FEATURE_RESONATOR, placement);
      s.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, value);
      s.SetFeatureValue(FEATURE_RESONATOR_RING, 3 - value % 4);
      s.SaveOptions();
      Settings loaded;
      CHECK(loaded.Init(false));
      CHECK(loaded.FeatureValue(FEATURE_RESONATOR) == placement);
      CHECK(loaded.FeatureValue(FEATURE_RESONATOR_MATERIAL) == value);
      CHECK(loaded.FeatureValue(FEATURE_RESONATOR_RING) == 3 - value % 4);
      const uint8_t* raw =
          reinterpret_cast<const uint8_t*>(&loaded.stored_record_);
      CHECK(raw[24] == placement && raw[25] == value &&
          raw[26] == 3 - value % 4);
    }
  }
  // A byte past the values is not set: the profile's default.
  const uint8_t bad[3][2] = { { 5, 8 }, { 0x80, 0x80 }, { 0xff, 0xff } };
  for (int i = 0; i < 3; ++i) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&s.record_);
    bytes[24] = bytes[26] = bad[i][0];
    bytes[25] = bad[i][1];
    CHECK(s.chunk_storage_.SaveRecord());
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.FeatureValue(FEATURE_RESONATOR) == 0);
    CHECK(loaded.FeatureValue(FEATURE_RESONATOR_MATERIAL) == 1);
    CHECK(loaded.FeatureValue(FEATURE_RESONATOR_RING) == 1);
  }
}

void Pitch15Profile(uint8_t* block, uint8_t value) {
  ProfileTemplate(block);
  block[18] = value;
  const size_t offset = 16 + 6 * block[5];
  const size_t bytes = 6 * block[5] + kScaleRecordSize;
  block[6] = bytes & 255;
  block[7] = bytes >> 8;
  block[12] = block[13] = 1;
  uint8_t* scale = block + offset;
  memset(scale, 0, kScaleRecordSize);
  scale[0] = 15;
  scale[1] = 2;
  memcpy(scale + 12, "Saved ID", 8);
  const float period = 12.0f;
  const float fifth = 7.0f;
  memcpy(scale + 8, &period, 4);
  memcpy(scale + 48, &fifth, 4);
  ProfileChecksum(block);
}

// The values a saved feature takes under this test's stock-bank profile:
// development builds size the Plaits bank by the profile.
uint8_t FieldValues(ProfileFeature feature) {
#ifdef PLAITS_BLOBS
  if (feature == FEATURE_PLAITS_BANK) {
    return kNumStockPlaitsBanks;
  }
#endif  // PLAITS_BLOBS
  return kFeatureDefinitions[feature].values;
}

void TestSavedFeaturePairs() {
  test_checker.Start("TestSavedFeaturePairs");
  ProfileFeature features[FEATURE_LAST];
  size_t count = 0;
  uint8_t block[kProfileSize];
  Pitch15Profile(block, 0);
  uint8_t scale[kScaleRecordSize];
  memcpy(scale, block + 16 + 6 * block[5], sizeof(scale));
  for (int id = 1; id < FEATURE_LAST; ++id) {
    if (kFeatureDefinitions[id].field == kNoField ||
        kFeatureDefinitions[id].field == kReservedField) {
      continue;
    }
    features[count] = ProfileFeature(id);
    uint8_t* entry = block + 16 + 6 * count++;
    memset(entry, 0, 6);
    entry[0] = id;
    entry[1] = PROFILE_ADJUSTABLE;
    // Synth start needs its gesture.
    entry[5] = id == FEATURE_SYNTH_START ? ACTION_HOLD : ACTION_NONE;
  }
  CHECK(count == 14);
  block[5] = count;
  const size_t bytes = 6 * count + sizeof(scale);
  block[6] = bytes & 255;
  block[7] = bytes >> 8;
  memcpy(block + 16 + 6 * count, scale, sizeof(scale));
  ProfileChecksum(block);
  Wipe();
  Settings dense;
  dense.Init(false, block);
  CHECK(!dense.profile_locked());
  for (size_t i = 0; i < count; ++i) {
    dense.SetFeatureValue(
        features[i], FieldValues(features[i]) - 1);
  }
  dense.SaveState();
  for (size_t changed = 0; changed < count; ++changed) {
    CHECK(dense.Init(false, block));
    for (size_t i = 0; i < count; ++i) {
      CHECK(dense.FeatureValue(features[i]) ==
          FieldValues(features[i]) - 1);
    }
    dense.SetFeatureValue(features[changed], 0);
    dense.SaveState();
    CHECK(dense.Init(false, block));
    for (size_t i = 0; i < count; ++i) {
      CHECK(dense.FeatureValue(features[i]) ==
          (i == changed ? 0 : FieldValues(features[i]) - 1));
    }
    dense.SetFeatureValue(
        features[changed], FieldValues(features[changed]) - 1);
    dense.SaveState();
  }
  size_t pairs = 0;
  for (size_t a = 0; a < count; ++a) {
    for (size_t b = a + 1; b < count; ++b) {
      ++pairs;
      Wipe();
      Settings initial;
      initial.Init(false, block);
      CHECK(!initial.profile_locked());
      initial.mutable_state()->gain = 137;
      initial.mutable_state()->assignable_cv_amount[1] = 173;
      initial.SaveState();
      for (int state = PROFILE_OFF; state <= PROFILE_FIXED; ++state) {
        Settings both;
        CHECK(both.Init(false, block));
        both.SetFeatureValue(
            features[a], FieldValues(features[a]) - 1);
        both.SetFeatureValue(
            features[b], FieldValues(features[b]) - 1);
        both.SaveOptions();
        Settings loaded;
        CHECK(loaded.Init(false, block));
        for (size_t i = 0; i < count; ++i) {
          CHECK(loaded.FeatureValue(features[i]) ==
              (i == a || i == b
                  ? FieldValues(features[i]) - 1 : 0));
        }
        uint8_t dormant[kProfileSize];
        memcpy(dormant, block, sizeof(dormant));
        dormant[16 + 6 * a + 1] = state;
        dormant[16 + 6 * a + 5] = ACTION_NONE;
        ProfileChecksum(dormant);
        CHECK(loaded.Init(false, dormant));
        CHECK(loaded.FeatureValue(features[a]) == 0);
        loaded.SetFeatureValue(features[b], 0);
        loaded.SaveOptions();
        CHECK(loaded.Init(false, block));
        CHECK(loaded.FeatureValue(features[a]) ==
            FieldValues(features[a]) - 1);
        CHECK(loaded.FeatureValue(features[b]) == 0);
        CHECK(loaded.state().gain == 137);
        CHECK(loaded.state().assignable_cv_amount[1] == 173);
      }
    }
  }
  CHECK(pairs == 91);
}

// Custom scale 15 is a plain value of the pitch byte.
void TestPitchScale15() {
  test_checker.Start("TestPitchScale15");
  uint8_t custom[kProfileSize];
  uint8_t plain[kProfileSize];
  ProfileTemplate(plain);
  for (int value = 0; value < 2; ++value) {
    Wipe();
    Pitch15Profile(custom, value);
    Settings available;
    available.Init(false, custom);
    CHECK(available.pitch_scale() == value);
  }
  Wipe();
  Pitch15Profile(custom, 0);
  Settings s;
  s.Init(false, custom);
  s.set_pitch_scale(PitchScale(15));
  CHECK(s.pitch_scale() == 15);
  s.SaveOptions();
  CHECK(s.stored_record_.values[0] == 15);
  CHECK(s.Init(false, custom) && s.pitch_scale() == 15);
  // A profile without it plays Off.
  CHECK(s.Init(false, plain) && s.pitch_scale() == PITCH_SCALE_OFF);
  // Fixed or Off profiles keep the selection dormant.
  uint8_t fixed[kProfileSize];
  ProfileTemplate(fixed);
  fixed[17] = PROFILE_FIXED;
  fixed[18] = PITCH_SCALE_CHROMATIC;
  fixed[19] = 0;
  ProfileChecksum(fixed);
  CHECK(s.Init(false, custom) && s.pitch_scale() == 15);
  CHECK(s.Init(false, fixed) && s.pitch_scale() == PITCH_SCALE_CHROMATIC);
  s.SaveState();
  CHECK(s.Init(false, custom) && s.pitch_scale() == 15);
  // Past the scale IDs: not set, the profile's default.
  Pitch15Profile(custom, 1);
  const uint8_t unset[] = { 16, 0x9f, 0xff };
  for (size_t i = 0; i < sizeof(unset); ++i) {
    s.record_.values[0] = unset[i];
    CHECK(s.chunk_storage_.SaveRecord());
    CHECK(s.Init(false, custom));
    CHECK(s.pitch_scale() == PITCH_SCALE_CHROMATIC);
  }
}

// Stock bytes 0-9 carry over from any earlier firmware; what Beta 1 tagged
// or Beta 2 and 3 packed after them is ignored, so their options start at
// the profile's defaults, and the first save writes zero there as stock.
void TestStockSettingsCarryOver() {
  test_checker.Start("TestStockSettingsCarryOver");
  uint8_t b[kProfileSize];
  for (int custom = 0; custom < 2; ++custom) {
    ProfileTemplate(b);
    if (custom) {
      b[18] = PITCH_SCALE_SHRI;
      b[24] = 1;
      b[42] = OUTPUT_LIMITER_FAST;
      b[78] = 127;
      b[84] = 200;
      ProfileChecksum(b);
    }
    for (int earlier = 0; earlier < 3; ++earlier) {
      for (int destination = 0; destination < 4; ++destination) {
        Wipe();
        Settings stock;
        stock.Init(false, b);
        const Saved defaults = Of(stock);
        const PersistentData calibration = Calibration(0.05f);
        stock.persistent_data_ = calibration;
        stock.SavePersistentData();
        // No record yet, as after an upgrade.
        Blank(RecordChunk(1), kSectorSize - kCalibrationChunk);
        State& saved = stock.stored_state_;
        memset(&saved, 0, sizeof(saved));
        saved.media = 2;
        saved.assignable_cv_destination = destination;
        saved.assignable_cv_amount[0] = 77;
        saved.assignable_cv_amount[1] = 33;
        saved.assignable_cv_amount[2] = 111;
        saved.gain = 40;
        saved.gate_output_mode = 1;
        saved.frozen = 1;
        saved.disable_autosave = 1;
        saved.monophonic_mode = 1;
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&saved);
        if (earlier == 1) {
          // Beta 1: tags from 0xa0, width 200, width assigned.
          const uint8_t beta1[] = { 0xa1, 0xa1, 0xa9, 0xa2, 200, 0xa2, 1 };
          memcpy(bytes + 10, beta1, sizeof(beta1));
        } else if (earlier == 2) {
          // Beta 2 and 3: nibbles, width 200, Clouds-style, the 0xc1 marker.
          const uint8_t beta3[] = { 0x93, 0x91, 0x12, 0x21, 200, 0x91,
              1, 180, 0x91, 0x92, 0x9e, 0x95, 0x93, 0xc1 };
          memcpy(bytes + 10, beta3, sizeof(beta3));
        }
        Saved expected = defaults;
        memset(&expected.state, 0, sizeof(State));
        memcpy(&expected.state, &saved, offsetof(State, padding));
        stock.chunk_storage_.SaveState();
        Settings loaded;
        CHECK(loaded.Init(false, b));
        CHECK(SameState(loaded, expected));
        CHECK(SameOptions(loaded, defaults));
        CHECK(SameCalibration(loaded, calibration));
        CHECK(!loaded.width_assigned() && !loaded.diffusion_assigned());
        // The profile's own width and diffusion, not older bytes.
        CHECK(loaded.stereo_width() == (custom ? 127 : 0));
        CHECK(loaded.FeatureValue(FEATURE_DIFFUSION) == (custom ? 200 : 0));
        // The first save writes stock's bytes, then the record.
        loaded.SaveState();
        CHECK(!memcmp(&loaded.stored_state_, &expected.state, sizeof(State)));
        CHECK(StockLoads(calibration, expected.state));
        CHECK(loaded.Init(false, b));
        CHECK(SameState(loaded, expected));
        CHECK(SameOptions(loaded, defaults));
        CHECK(SameCalibration(loaded, calibration));
      }
    }
  }
}

void TestProfileNewAssignment() {
  test_checker.Start("TestProfileNewAssignment");
  uint8_t b[kProfileSize];
  ProfileTemplate(b);
  b[16 + 6 * 10 + 1] = PROFILE_OFF;  // Width.
  b[16 + 6 * 10 + 4] = 0;
  ProfileChecksum(b);
  for (int choice = 0; choice < 2; ++choice) {
    Wipe();
    Settings s;
    s.Init(false);
    s.set_stereo_width(129);
    s.set_diffusion(0.75f);
    s.set_width_assigned(true);
    s.SaveState();
    Settings p;
    CHECK(p.Init(false, b));
    CHECK(!p.width_assigned());
    if (choice) {
      p.mutable_state()->assignable_cv_destination = 2;
    } else {
      p.set_diffusion_assigned(true);
      CHECK(p.diffusion_assigned());
    }
    p.SaveState();
    CHECK(p.stored_record_.cv_target == (choice ? 0 : 2));
    CHECK(s.Init(false));
    CHECK(!s.width_assigned());
    CHECK(choice ? s.state().assignable_cv_destination == 2
        : s.diffusion_assigned());
  }
}

void TestProfileDormantAmounts() {
  test_checker.Start("TestProfileDormantAmounts");
  uint8_t b[kProfileSize];
  for (int combination = 0; combination < 4; ++combination) {
    const int fixed = combination & 1;
    const int destination = 1 + combination / 2;
    Wipe();
    Settings s;
    s.Init(false);
    s.set_stereo_width(129);
    s.set_diffusion(0.75f);
    s.mutable_state()->monophonic_mode = 1;
    if (destination == 1) {
      s.set_width_assigned(true);
    } else {
      s.set_diffusion_assigned(true);
    }
    s.SaveState();
    const uint8_t diffusion = s.FeatureValue(FEATURE_DIFFUSION);
    ProfileTemplate(b);
    for (int i = 10; i <= 12; ++i) {
      b[16 + 6 * i + 1] = fixed ? PROFILE_FIXED : PROFILE_OFF;
      b[16 + 6 * i + 4] = b[16 + 6 * i + 5] = 0;
    }
    b[78] = 111;
    b[84] = 112;
    b[90] = 0;
    ProfileChecksum(b);
    Settings dormant;
    CHECK(dormant.Init(false, b));
    CHECK(dormant.stereo_width() == (fixed ? 111 : 0));
    CHECK(dormant.FeatureValue(FEATURE_DIFFUSION) == (fixed ? 112 : 1));
    CHECK(!dormant.state().monophonic_mode);
    CHECK(!dormant.width_assigned() && !dormant.diffusion_assigned());
    dormant.SaveState();
    const uint8_t* record =
        reinterpret_cast<const uint8_t*>(&dormant.stored_record_);
    CHECK(record[kRecordOffsets[FEATURE_WIDTH]] == 129);
    CHECK(record[kRecordOffsets[FEATURE_DIFFUSION]] == diffusion);
    CHECK(dormant.stored_state_.monophonic_mode == 1);
    CHECK(dormant.stored_record_.cv_target == destination);
    CHECK(s.Init(false));
    CHECK(s.stereo_width() == 129);
    CHECK(s.FeatureValue(FEATURE_DIFFUSION) == diffusion);
    CHECK(s.state().monophonic_mode);
    CHECK(destination == 1 ? s.width_assigned() : s.diffusion_assigned());
    // Bytes past their values read as not set and are saved valid again;
    // the reserved bytes stay erased.
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&s.record_);
    bytes[kRecordOffsets[FEATURE_BRIGHTNESS]] = 0x8e;
    bytes[kRecordOffsets[FEATURE_SEED_LATCH]] = 0x8e;
    CHECK(s.chunk_storage_.SaveRecord());
    CHECK(s.Init(false));
    CHECK(s.FeatureValue(FEATURE_BRIGHTNESS) == 0);
    CHECK(s.FeatureValue(FEATURE_SEED_LATCH) == 0);
    s.SaveState();
    const uint8_t* saved =
        reinterpret_cast<const uint8_t*>(&s.stored_record_);
    CHECK(saved[kRecordOffsets[FEATURE_BRIGHTNESS]] == 0);
    CHECK(saved[kRecordOffsets[FEATURE_SEED_LATCH]] == 0);
    CHECK(saved[2] == 0xff && saved[3] == 0xff);
    // Page 5 (bytes 24-26) and Rings' polyphony (27) are saved; the rest
    // stays erased.
    for (size_t i = 28; i < sizeof(OptionsRecord); ++i) {
      CHECK(saved[i] == 0xff);
    }
  }
}

void TestProfileDiffusionOff() {
  test_checker.Start("TestProfileDiffusionOff");
  Wipe();
  Settings s;
  s.Init(false);
  s.set_clouds_style(true);
  s.SaveState();
  uint8_t b[kProfileSize];
  ProfileTemplate(b);
  b[83] = PROFILE_OFF;
  b[86] = 0;
  ProfileChecksum(b);
  CHECK(s.Init(false, b));
  CHECK(s.clouds_style());
  CHECK(s.diffusion_set() && s.diffusion() == 0.0f);
  s.SaveState();
  CHECK(s.stored_record_.values[kRecordOffsets[FEATURE_DIFFUSION] - 8] == 0);
}

// Start in quality (#287): a newly installed profile starts in its quality
// once; the saved identity keeps the user's later choice.
void TestStartQuality() {
  test_checker.Start("TestStartQuality");
  CHECK(FEATURE_START_QUALITY == 39);
  CHECK(kFeatureDefinitions[FEATURE_START_QUALITY].values == 5);
  uint8_t factory[kProfileSize];
  uint8_t scorched[kProfileSize];
  uint8_t sunny[kProfileSize];
  uint8_t keep[kProfileSize];
  uint8_t off[kProfileSize];
  ProfileTemplate(factory);
  ProfileWithEntry(scorched, FEATURE_START_QUALITY, PROFILE_FIXED, 4);
  ProfileWithEntry(sunny, FEATURE_START_QUALITY, PROFILE_FIXED, 3);
  ProfileWithEntry(keep, FEATURE_START_QUALITY, PROFILE_FIXED, 0);
  ProfileWithEntry(off, FEATURE_START_QUALITY, PROFILE_OFF, 4);
  Profile p;
  CHECK(p.Init(scorched) && p.Init(sunny) && p.Init(keep) && p.Init(off));
  // The identity is the profile's whole CRC.
  Profile a, b;
  a.Init(scorched);
  b.Init(sunny);
  const size_t size = 16 + (scorched[6] | scorched[7] << 8);
  CHECK(a.identity() == Profile::Crc32(scorched, size));
  CHECK(a.identity() != b.identity());
  // Adjustable has no page or saved field to change it.
  uint8_t adjustable[kProfileSize];
  ProfileWithEntry(adjustable, FEATURE_START_QUALITY, PROFILE_ADJUSTABLE, 1);
  CHECK(!p.Init(adjustable));

  // A module in use on the standard profile, in Cold digital.
  Wipe();
  Settings s;
  s.Init(false, factory);
  CHECK(s.state().media == 3);
  s.mutable_state()->media = 2;
  s.SaveState();
  // Keep current, or an Off entry, never changes it.
  CHECK(s.Init(false, keep) && s.state().media == 2);
  CHECK(s.Init(false, off) && s.state().media == 2);
  // A new profile starts in its quality, at every start until a save.
  CHECK(s.Init(false, scorched) && s.state().media == 0);
  CHECK(s.Init(false, scorched) && s.state().media == 0);
  // The user's quality, once saved, wins on the same profile.
  s.mutable_state()->media = 1;
  s.SaveState();
  CHECK(s.stored_record_.profile_identity == a.identity());
  CHECK(s.Init(false, scorched) && s.state().media == 1);
  // Another profile applies its own quality once more.
  CHECK(s.Init(false, sunny) && s.state().media == 1);
  s.mutable_state()->media = 3;
  s.SaveState();
  CHECK(s.Init(false, sunny) && s.state().media == 3);
  CHECK(s.Init(false, scorched) && s.state().media == 0);

  // No record, as after an upgrade, or another version's: a new profile.
  for (int version = 0; version < 2; ++version) {
    s.Init(false, scorched);
    s.mutable_state()->media = 2;
    s.SaveState();
    if (version) {
      s.record_.version = kOptionsRecordVersion + 1;
      CHECK(s.chunk_storage_.SaveRecord());
    } else {
      Blank(RecordChunk(1), kSectorSize - kCalibrationChunk);
    }
    CHECK(s.Init(false, scorched) && s.state().media == 0);
  }

  // A settings reset starts in the profile's quality.
  s.mutable_state()->media = 2;
  s.SaveState();
  CHECK(s.Init(true, scorched) && s.state().media == 0);
  CHECK(s.Init(true, keep) && s.state().media == 3);

  // A frozen granular recording that returns from flash keeps its quality;
  // without autosave or in spectral nothing returns.
  for (int variant = 0; variant < 3; ++variant) {
    s.Init(false, factory);
    s.mutable_state()->media = 2;
    s.mutable_state()->frozen = 1;
    s.mutable_state()->disable_autosave = variant == 1;
    s.set_spectral(variant == 2);
    s.SaveState();
    CHECK(s.Init(false, scorched));
    CHECK(s.state().media == (variant ? 0 : 2));
  }

  // A new module starts in it too.
  Wipe();
  Settings fresh;
  fresh.Init(false, sunny);
  CHECK(fresh.state().media == 1);
}

#ifdef PLAITS_BLOBS
// The saved bank is kept while the profile has that many banks; past them it
// takes the profile's start bank.
void TestPlaitsBanks() {
  test_checker.Start("TestPlaitsBanks");
  uint8_t banks[5 * 8];
  for (int i = 0; i < 5 * 8; ++i) {
    banks[i] = i % 24;
  }
  uint8_t b[kProfileSize];
  Wipe();
  ProfileV3(b, banks, 5, NULL, 0, 1);
  Settings s;
  s.Init(false, b);
  CHECK(s.FeatureValue(FEATURE_PLAITS_BANK) == 1);
  s.SetFeatureValue(FEATURE_PLAITS_BANK, 4);
  s.SaveOptions();
  Settings five;
  five.Init(false, b);
  CHECK(five.FeatureValue(FEATURE_PLAITS_BANK) == 4);
  ProfileV3(b, banks, 3, NULL, 0, 2);
  Settings three;
  three.Init(false, b);
  CHECK(three.FeatureValue(FEATURE_PLAITS_BANK) == 2);
  ProfileV3(b, banks, 0, NULL, 0, 0);
  Settings stock;
  stock.Init(false, b);
  CHECK(stock.FeatureValue(FEATURE_PLAITS_BANK) == 0);
  // The same saved bank within a smaller table is kept.
  stock.SetFeatureValue(FEATURE_PLAITS_BANK, 2);
  stock.SaveOptions();
  ProfileV3(b, banks, 5, NULL, 0, 0);
  Settings again;
  again.Init(false, b);
  CHECK(again.FeatureValue(FEATURE_PLAITS_BANK) == 2);
}
#endif  // PLAITS_BLOBS

int main() {
  if (!CHECK(MapBank2())) {
    return 1;
  }
  TestMaskedReader();
  TestReadIntoRecords();
  TestCalibrationFromMirror();
  TestRestoredCalibrationChoice();
  TestCalibrationLost();
  TestImplausibleCalibrationWithoutMirror();
  TestFactoryPrimaryInterruptedRecalibration();
  TestAppendAfterBlankGap();
  TestRefusedRecalibrationPreservesMeasuredMirror();
  TestAppendBeforeUnreadableState();
  TestRetryIncompleteErase();
  TestStateTerminator();
  TestInterruptedAppend();
  TestInterruptedRewrite();
  TestMirrorOnlyCopy();
  TestCalibrationWithoutState();
  TestBlankSector();
  TestWriteLock();
  TestRepair();
  TestRepairFailures();
  TestRepairPowerLoss();
  TestMirrorWrites();
  TestLowSupply();
  TestSupplyDrop();
  TestProfileLocks();
  TestLockedCalibrationRefused();
  TestProfileSettings();
  TestSavedFeatures();
  TestRingsSettings();
  TestResonatorSaved();
  TestSavedFeaturePairs();
  TestPitchScale15();
  TestStockSettingsCarryOver();
  TestProfileDormantAmounts();
  TestProfileNewAssignment();
  TestProfileDiffusionOff();
  TestStartQuality();
#ifdef PLAITS_BLOBS
  TestPlaitsBanks();
#endif  // PLAITS_BLOBS
  test_checker.Start("main");
  CHECK(TestFlash().violations == 0);
  CHECK(TestFlash().wide_sectors == 0);
  return test_checker.failures() ? 1 : 0;
}
