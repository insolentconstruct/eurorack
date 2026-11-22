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
// The options record in the calibration mirror sector: its journal, versions
// and checksum, the compactor and its power cuts, the rules that keep a
// calibration in flash, earlier firmware on the same sectors, and foreign
// records in either sector.

#include <sys/wait.h>

#include "beads/test/storage_fixtures.h"

const PersistentData kCalibration = Calibration(0.07f);

// A module in use, then its next save forced to compact: a word of junk in
// the next free slot, as the bench writes one.
Saved CompactionPending(Settings* s) {
  const Saved saved = Setup(kCalibration, 5);
  CHECK(s->Init(false));
  const uint8_t junk[32] = { 0 };
  Poke(RecordChunk(s->chunk_storage_.next_record_index_), junk, 32);
  return saved;
}

// Options saves append in the mirror sector, never in the settings sector;
// with a state, the record goes first.
void TestRecordJournal() {
  test_checker.Start("TestRecordJournal");
  const Saved saved = Setup(kCalibration, 5);
  OptionsRecord record;
  CHECK(RecordAt(1, &record) && RecordAt(2, &record));
  CHECK(!RecordAt(3, &record));
  Settings s;
  CHECK(s.Init(false));
  CHECK(SameOptions(s, saved));
  CHECK(s.chunk_storage_.next_record_index_ == 3);
  static uint8_t sector[kSectorSize];
  static uint8_t now[kSectorSize];
  Peek(sector, kSector, kSectorSize);
  const uint32_t erases = TestFlashErases();
  for (int i = 0; i < 6; ++i) {
    const uint32_t writes = test_writes;
    s.set_pitch_scale(PitchScale(1 + i));
    s.SaveOptions();
    CHECK(test_writes == writes + 2);
    CHECK(RecordAt(3 + i, &record) && record.values[0] == 1 + i);
  }
  // Nothing changed: nothing written.
  const uint32_t writes = test_writes;
  s.SaveOptions();
  CHECK(test_writes == writes);
  Peek(now, kSector, kSectorSize);
  CHECK(!memcmp(now, sector, kSectorSize));
  CHECK(TestFlashErases() == erases);
  // A state save with nothing else changed is one word, stock's bytes.
  s.mutable_state()->media = 1;
  s.SaveState();
  CHECK(test_writes == writes + 1);
  State wire;
  Peek(&wire, StateChunk(7) + 8, sizeof(wire));
  CHECK(!memcmp(&wire, &s.state(), sizeof(State)));
  for (size_t i = 0; i < sizeof(wire.padding); ++i) {
    CHECK(wire.padding[i] == 0);
  }
  // Both changed, cut after one, two and three words: the record comes
  // first, so a cut never leaves the new state with the old options. Its
  // second word holds only erased reserved bytes: one word completes it.
  for (int cut = 1; cut <= 3; ++cut) {
    const Saved before = Setup(kCalibration, 5);
    Settings both;
    CHECK(both.Init(false));
    both.set_pitch_scale(PITCH_SCALE_MINOR);
    both.mutable_state()->media = 0;
    TestFlashBudget() = cut;
    both.SaveState();
    TestFlashBudget() = -1;
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.pitch_scale() == PITCH_SCALE_MINOR);
    CHECK(loaded.state().media == (cut == 3 ? 0 : before.state.media));
    CHECK(before.values[FEATURE_PITCH] != PITCH_SCALE_MINOR);
  }
}

// The firmware's checksum is the unsigned byte sum, as Python tools compute.
void TestRecordChecksum() {
  test_checker.Start("TestRecordChecksum");
  Setup(kCalibration, 0);
  Settings s;
  CHECK(s.Init(false));
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&s.record_);
  for (size_t i = 0; i < sizeof(OptionsRecord); ++i) {
    bytes[i] = uint8_t(i * 37 + 200);
  }
  const int index = s.chunk_storage_.next_record_index_;
  CHECK(s.chunk_storage_.SaveRecord());
  uint8_t header[8];
  Peek(header, RecordChunk(index), sizeof(header));
  CHECK(header[0] == 'D' && header[1] == 'R' && header[2] == 'Z');
  CHECK(header[3] == 'L' && header[4] == 56 && header[5] == 0);
  CHECK(header[6] == 0xab && header[7] == 0xe2);
}

// The latest structurally valid slot decides; another version, or a bad
// tag, size or checksum in the only slot, gives the profile's defaults.
void TestRecordVersions() {
  test_checker.Start("TestRecordVersions");
  const Saved defaults = Defaults();
  Settings writer;
  Setup(kCalibration, 0);
  CHECK(writer.Init(false));
  writer.set_output_limiter_mode(OUTPUT_LIMITER_FAST);
  writer.PackRecord();
  const OptionsRecord ours = writer.record_;
  OptionsRecord other = ours;
  other.version = kOptionsRecordVersion + 1;
  other.values[kRecordOffsets[FEATURE_LIMITER] - 8] = OUTPUT_LIMITER_OFF;
  const uint8_t torn[32] = { 0x44, 0x52, 0x5a, 0x4c, 56, 0, 0, 0 };
  for (int fixture = 0; fixture < 3; ++fixture) {
    Setup(kCalibration, 0);
    Blank(RecordChunk(1), kSectorSize - kCalibrationChunk);
    if (fixture == 0) {
      // Ours, then a newer firmware's.
      PokeChunk(RecordChunk(1), OptionsRecord::tag, &ours, 56);
      PokeChunk(RecordChunk(2), OptionsRecord::tag, &other, 56);
    } else if (fixture == 1) {
      // A newer firmware's, then a torn slot.
      PokeChunk(RecordChunk(1), OptionsRecord::tag, &other, 56);
      Poke(RecordChunk(2), torn, sizeof(torn));
    } else {
      // A newer firmware's, then ours after a downgrade.
      PokeChunk(RecordChunk(1), OptionsRecord::tag, &other, 56);
      PokeChunk(RecordChunk(2), OptionsRecord::tag, &ours, 56);
    }
    Settings s;
    CHECK(s.Init(false));
    CHECK(s.output_limiter_mode() ==
        (fixture == 2 ? OUTPUT_LIMITER_FAST : OUTPUT_LIMITER_STOCK));
    if (fixture < 2) {
      CHECK(SameOptions(s, defaults));
    }
    // Our next save appends our version after it.
    s.set_output_limiter_mode(OUTPUT_LIMITER_FAST);
    s.SaveOptions();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_FAST);
  }
  // One slot with each fault.
  for (int fault = 0; fault < 5; ++fault) {
    Setup(kCalibration, 0);
    Blank(RecordChunk(1), kSectorSize - kCalibrationChunk);
    OptionsRecord r = ours;
    uint32_t tag = OptionsRecord::tag;
    uint16_t size = 56;
    if (fault == 0) {
      tag = 0x4c5a5245;
    } else if (fault == 1) {
      size = 55;
    } else if (fault == 3) {
      r.version = 0;
    } else if (fault == 4) {
      r.version = 0xff;
    }
    PokeChunk(RecordChunk(1), tag, &r, size);
    if (fault == 2) {
      uint8_t byte = 0x12;
      Poke(RecordChunk(1) + 20, &byte, 1);  // Checksum fails.
    }
    Settings s;
    CHECK(s.Init(false));
    CHECK(SameOptions(s, defaults));
  }
}

// A torn slot, failing its checksum, ends the journal: the slot before it
// loads, and the next save compacts.
void TestTornRecord() {
  test_checker.Start("TestTornRecord");
  const Saved saved = Setup(kCalibration, 5);
  uint8_t word[32];
  Peek(word, RecordChunk(2), sizeof(word));
  word[9] ^= 0x40;
  Poke(RecordChunk(3), word, sizeof(word));
  Settings s;
  CHECK(s.Init(false));
  CHECK(SameOptions(s, saved));
  CHECK(s.chunk_storage_.next_record_index_ == 3);
  const uint32_t erases = TestFlashErases();
  s.set_pitch_scale(PITCH_SCALE_MAJOR);
  Keep(kCalibration, kCalibration);
  s.SaveOptions();
  CHECK(Release());
  CHECK(TestFlashErases() == erases + 1);
  CHECK(s.chunk_storage_.next_record_index_ == 2);
  CHECK(MirrorHolds(kCalibration));
  OptionsRecord record;
  CHECK(RecordAt(1, &record) && !RecordAt(2, &record));
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.pitch_scale() == PITCH_SCALE_MAJOR);
  CHECK(SameCalibration(loaded, kCalibration));
}

// The journal fills, then one compaction starts it over.
void TestSector14Full() {
  test_checker.Start("TestSector14Full");
  CHECK(kRecordSlots == 2046);
  Setup(kCalibration, 0);
  Settings s;
  CHECK(s.Init(false));
  const uint32_t erases = TestFlashErases();
  int saves = 0;
  while (s.chunk_storage_.next_record_index_ <= kRecordSlots) {
    s.set_pitch_scale(PitchScale(1 + saves % 9));
    s.SaveOptions();
    ++saves;
  }
  CHECK(saves == kRecordSlots - 1);
  CHECK(TestFlashErases() == erases);
  Settings full;
  CHECK(full.Init(false));
  CHECK(full.pitch_scale() == s.pitch_scale());
  s.set_pitch_scale(PITCH_SCALE_SHRI);
  Keep(kCalibration, kCalibration);
  s.SaveOptions();
  CHECK(Release());
  CHECK(TestFlashErases() == erases + 1);
  CHECK(s.chunk_storage_.next_record_index_ == 2);
  CHECK(MirrorHolds(kCalibration));
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.pitch_scale() == PITCH_SCALE_SHRI);
}

// A cut at each step of a compaction (erase, four calibration words, two
// record words): the calibration is the old one, never lost or locked; the
// options are the last saved, the defaults inside the window, or the new
// ones once the record's first word is in.
void TestInterruptedCompaction() {
  test_checker.Start("TestInterruptedCompaction");
  const Saved defaults = Defaults();
  for (int cut = 0; cut <= 7; ++cut) {
    Settings s;
    const Saved saved = CompactionPending(&s);
    s.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
    Keep(kCalibration, kCalibration);
    TestFlashBudget() = cut;
    s.SaveOptions();
    TestFlashBudget() = -1;
    CHECK(Release());
    Settings loaded;
    CHECK(loaded.Init(false));  // Never the factory test.
    CHECK(!loaded.write_locked());
    CHECK(SameCalibration(loaded, kCalibration));
    CHECK(SameState(loaded, saved));
    if (cut == 0) {
      CHECK(SameOptions(loaded, saved));
    } else if (cut < 6) {
      CHECK(SameOptions(loaded, defaults));
    } else {
      CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    }
    // Saves work again, and repeated boots change nothing.
    loaded.set_pitch_scale(PITCH_SCALE_MINOR);
    loaded.SaveState();
    Settings again;
    CHECK(again.Init(false) && !again.write_locked());
    CHECK(again.pitch_scale() == PITCH_SCALE_MINOR);
    CHECK(SameCalibration(again, kCalibration));
    CHECK(MirrorHolds(kCalibration));
  }
}

// Rules 3, 6 and 7: the mirror sector is erased only to copy the settings
// sector's calibration, read from flash, usable and the one in use.
void TestCompactionGuards() {
  test_checker.Start("TestCompactionGuards");
  PersistentData wild = kCalibration;
  wild.adc_calibration[CV_ADC_CHANNEL_PITCH].scale *= 1.5f;
  const PersistentData recalibrated = Calibration(-0.05f);
  for (int fault = 0; fault < 5; ++fault) {
    Settings s;
    CompactionPending(&s);
    static uint8_t mirror[kSectorSize];
    if (fault == 4) {
      // Out of range in both copies, and in use.
      Setup(kCalibration, 5);
      PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
      PokeChunk(kMirror, PersistentData::tag, &wild, sizeof(wild));
      CHECK(s.Init(false) && SameCalibration(s, wild));
      const uint8_t junk[32] = { 0 };
      Poke(RecordChunk(s.chunk_storage_.next_record_index_), junk, 32);
    } else if (fault == 0) {
      TestFlashDamage(kSector + 32);  // Unreadable.
    } else if (fault == 1) {
      PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
    } else if (fault == 2) {
      // Factory values the boot couldn't replace with the mirror's.
      const PersistentData factory = Factory();
      Blank(kSector, kSectorSize);
      PokeChunk(kSector, PersistentData::tag, &factory, sizeof(factory));
      TestFlashBudget() = 0;
      CHECK(s.Init(false));
      TestFlashBudget() = -1;
      CHECK(SameCalibration(s, kCalibration));
    } else {
      // A new calibration in RAM, not saved yet.
      memcpy(s.mutable_persistent_data()->adc_calibration,
          recalibrated.adc_calibration, sizeof(recalibrated.adc_calibration));
    }
    Peek(mirror, kMirror, kSectorSize);
    const uint32_t erases = TestFlashErases();
    s.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
    s.SaveOptions();
    CHECK(TestFlashErases() == erases);
    static uint8_t now[kSectorSize];
    Peek(now, kMirror, kSectorSize);
    CHECK(!memcmp(now, mirror, kSectorSize));
    CHECK(MirrorHolds(fault == 4 ? wild : kCalibration));
    CHECK(s.chunk_storage_.record_pending());  // Held in RAM.
    TestFlashRepairAll();
  }
  // A stale mirror (stock recalibrated after a downgrade) is refreshed from
  // the settings sector before the next record.
  Setup(kCalibration, 5);
  const Saved saved = Setup(kCalibration, 5);
  uint8_t chunk[kCalibrationChunk];
  PokeChunk(kSector, PersistentData::tag, &recalibrated, sizeof(recalibrated));
  Peek(chunk, kSector, sizeof(chunk));
  Settings s;
  CHECK(s.Init(false));
  CHECK(SameCalibration(s, recalibrated));
  CHECK(SameOptions(s, saved));
  const uint32_t erases = TestFlashErases();
  s.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  Keep(recalibrated, recalibrated);
  s.SaveOptions();
  CHECK(Release());
  CHECK(TestFlashErases() == erases + 1);
  CHECK(MirrorHolds(recalibrated));
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);
}

// After an interrupted rewrite, a stock-storage firmware rewrites the
// settings sector with the factory values. Their exact values are never a
// module's own: the mirror's calibration comes back.
void TestFactoryCaliGuard() {
  test_checker.Start("TestFactoryCaliGuard");
  const PersistentData factory = Factory();
  const Saved saved = Setup(kCalibration, 5);
  PokeChunk(kSector, PersistentData::tag, &factory, sizeof(factory));
  Settings s;
  Keep(kCalibration, kCalibration);
  CHECK(s.Init(false));
  CHECK(Release());
  CHECK(!s.write_locked());
  CHECK(SameCalibration(s, kCalibration));
  CHECK(SameState(s, saved) && SameOptions(s, saved));
  CHECK(HoldsCalibration(kSector, kCalibration));
  // Factory values in both, as on a new or repaired module: kept, and
  // nothing written at each boot.
  Wipe();
  Settings fresh;
  fresh.Init(false);
  Take(&snapshot);
  Settings again;
  CHECK(again.Init(false) && SameCalibration(again, factory));
  CHECK(Unchanged(snapshot));
}

// Sector 15 checksum-valid but out of range, a usable mirror, the boot's
// restore refused, then a recalibration cut at every step: a usable
// calibration is never lost.
void TestRule6ImplausibleSector15() {
  test_checker.Start("TestRule6ImplausibleSector15");
  PersistentData wild = kCalibration;
  wild.adc_calibration[CV_ADC_CHANNEL_SHAPE].offset *= 2.0f;
  const PersistentData recalibrated = Calibration(-0.1f);
  for (int cut = 0; cut <= 18; ++cut) {
    Setup(kCalibration, 5);
    PokeChunk(kSector, PersistentData::tag, &wild, sizeof(wild));
    Settings s;
    TestFlashBudget() = 0;
    CHECK(s.Init(false));
    TestFlashBudget() = -1;
    CHECK(SameCalibration(s, kCalibration));
    memcpy(s.mutable_persistent_data()->adc_calibration,
        recalibrated.adc_calibration, sizeof(recalibrated.adc_calibration));
    Keep(kCalibration, recalibrated);
    TestFlashBudget() = cut;
    s.SavePersistentData();
    TestFlashBudget() = -1;
    CHECK(Release());
    Settings loaded;
    CHECK(loaded.Init(false) && !loaded.write_locked());
    CHECK(SameCalibration(loaded, cut <= 12 ? kCalibration : recalibrated));
    Settings again;
    CHECK(again.Init(false) && !again.write_locked());
    CHECK(SameCalibration(again, loaded.persistent_data()));
  }
}

// SEED at power-up resets the options too: appended when a slot is free,
// otherwise through a guarded compaction.
void TestRecoveryResetsOptions() {
  test_checker.Start("TestRecoveryResetsOptions");
  const Saved defaults = Defaults();
  for (int journal = 0; journal < 3; ++journal) {
    Settings s;
    if (journal == 0) {
      Setup(kCalibration, 5);
    } else if (journal == 1) {
      CompactionPending(&s);  // A torn next slot.
    } else {
      Setup(kCalibration, 0);
      CHECK(s.Init(false));
      while (s.chunk_storage_.next_record_index_ <= kRecordSlots) {
        s.set_pitch_scale(PitchScale(1 + s.pitch_scale() % 9));
        s.SaveOptions();
      }
    }
    const uint32_t erases = TestFlashErases();
    Settings reset;
    Keep(kCalibration, kCalibration);
    CHECK(reset.Init(true));
    CHECK(Release());
    CHECK(TestFlashErases() == erases + (journal ? 2 : 1));
    CHECK(SameOptions(reset, defaults) && SameState(reset, defaults));
    CHECK(SameCalibration(reset, kCalibration));
    CHECK(MirrorHolds(kCalibration));
    Settings loaded;
    CHECK(loaded.Init(false) && SameOptions(loaded, defaults));
  }
}

// A calibration out of range never replaces a usable one: the save keeps
// the stored calibration, in RAM too, and the states go on saving.
void TestImplausibleSaveKept() {
  test_checker.Start("TestImplausibleSaveKept");
  PersistentData wild = kCalibration;
  wild.adc_calibration[CV_ADC_CHANNEL_SIZE].offset *= 2.0f;
  for (int cut = 0; cut <= 13; ++cut) {
    Setup(kCalibration, 5);
    Settings s;
    CHECK(s.Init(false));
    *s.mutable_persistent_data() = wild;
    Keep(kCalibration, kCalibration);
    TestFlashBudget() = cut;
    s.SavePersistentData();
    TestFlashBudget() = -1;
    CHECK(Release());
    CHECK(SameCalibration(s, kCalibration));
    s.mutable_state()->gain = 33;
    s.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false) && SameCalibration(loaded, kCalibration));
    CHECK(loaded.state().gain == 33);
  }
}

// SEED at power-up over a settings sector without states, blank or with the
// calibration alone: the options reset too.
void TestRecoveryRestoredStorage() {
  test_checker.Start("TestRecoveryRestoredStorage");
  const Saved defaults = Defaults();
  for (int blank = 0; blank < 2; ++blank) {
    Setup(kCalibration, 5);
    Blank(blank ? kSector : StateChunk(1),
        blank ? kSectorSize : kSectorSize - kCalibrationChunk);
    Settings reset;
    Keep(kCalibration, kCalibration);
    CHECK(reset.Init(true));
    CHECK(Release());
    CHECK(SameOptions(reset, defaults) && SameCalibration(reset, kCalibration));
    Settings loaded;
    CHECK(loaded.Init(false) && SameOptions(loaded, defaults));
  }
}

// The record refused after a mirror rewrite: the calibration save goes on,
// and the next options save puts the record back.
int refused_after = -1;

void RefuseOnce() {
  if (refused_after > 0 && --refused_after == 0) {
    TestFlashFailures() = kFlashAttempts;  // Every retry of its first word.
  }
  CheckKept();
}

void TestRecordAppendRefused() {
  test_checker.Start("TestRecordAppendRefused");
  const PersistentData recalibrated = Calibration(-0.1f);
  const Saved saved = Setup(kCalibration, 5);
  Settings s;
  CHECK(s.Init(false));
  memcpy(s.mutable_persistent_data()->adc_calibration,
      recalibrated.adc_calibration, sizeof(recalibrated.adc_calibration));
  Keep(kCalibration, recalibrated);
  TestFlash().after_operation = &RefuseOnce;
  refused_after = 5;  // Mirror erase and calibration; its record refused.
  s.SavePersistentData();
  CHECK(Release());
  CHECK(s.chunk_storage_.next_state_chunk_index_ == 2);
  CHECK(s.chunk_storage_.record_pending());
  CHECK(HoldsCalibration(kSector, recalibrated) && MirrorHolds(recalibrated));
  s.SaveOptions();
  CHECK(!s.chunk_storage_.record_pending());
  Settings loaded;
  CHECK(loaded.Init(false) && SameOptions(loaded, saved));
  CHECK(SameCalibration(loaded, recalibrated));
}

// Saves refused for a low supply keep the options in RAM; the next save
// once it is back writes them.
void TestRecordLowSupply() {
  test_checker.Start("TestRecordLowSupply");
  TestFlashPowerUp();
  const Saved saved = Setup(kCalibration, 5);
  Settings s;
  CHECK(s.Init(false));
  Take(&snapshot);
  TestFlash().supply_low = true;
  s.set_pitch_scale(PITCH_SCALE_PELOG);
  s.SaveOptions();
  s.SaveState();
  CHECK(Unchanged(snapshot));
  Settings unchanged;
  TestFlash().supply_low = false;
  CHECK(unchanged.Init(false) && SameOptions(unchanged, saved));
  s.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.pitch_scale() == PITCH_SCALE_PELOG);
  CHECK(TestFlash().low_supply_writes == 0);
}

// Hidden width and diffusion edits, the CV target and its clearing, a
// Plaits bank or page commit: each survives a reboot.
void TestGesturePersistence() {
  test_checker.Start("TestGesturePersistence");
  Setup(kCalibration, 0);
  Settings s;
  CHECK(s.Init(false));
  s.set_stereo_width(150);
  s.SaveState();
  s.set_diffusion(0.5f);
  s.SaveState();
  s.set_diffusion_assigned(true);
  s.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.stereo_width() == 150 && loaded.diffusion_assigned());
  CHECK(loaded.FeatureValue(FEATURE_DIFFUSION) == s.FeatureValue(
      FEATURE_DIFFUSION));
  CHECK(!loaded.state().assignable_cv_destination);
  loaded.set_width_assigned(true);
  loaded.SaveState();
  CHECK(s.Init(false) && s.width_assigned());
  // Stock routing on [A] clears the target.
  s.set_width_assigned(false);
  s.mutable_state()->assignable_cv_destination = 3;
  s.SaveState();
  CHECK(loaded.Init(false) && !loaded.width_assigned());
  CHECK(loaded.state().assignable_cv_destination == 3);
  // A page commit or [A] toggle saves the options alone.
  const uint32_t writes = test_writes;
  loaded.SetFeatureValue(FEATURE_SEED_LATCH, 3);
  loaded.SetFeatureValue(FEATURE_BRIGHTNESS, 2);
  loaded.SaveOptions();
  CHECK(test_writes == writes + 2);
  CHECK(s.Init(false));
  CHECK(s.FeatureValue(FEATURE_SEED_LATCH) == 3);
  CHECK(s.FeatureValue(FEATURE_BRIGHTNESS) == 2);
}

// Beta 2 and 3 after a downgrade: their state saves leave the record, which
// wins at the next upgrade; their recalibration erases it.
void TestEarlierFirmware() {
  test_checker.Start("TestEarlierFirmware");
  const Saved defaults = Defaults();
  for (int recalibrate = 0; recalibrate < 2; ++recalibrate) {
    const Saved saved = Setup(kCalibration, 5);
    // Beta 3 appends its packed state: other options, marker 0xc1.
    State beta3 = saved.state;
    beta3.gain = 99;
    const uint8_t packed[] = { 0x92, 0x91, 0x12, 0x22, 200, 0x91,
        1, 180, 0x91, 0x92, 0x9e, 0x95, 0x93, 0xc1 };
    memcpy(beta3.padding, packed, sizeof(packed));
    PokeChunk(StateChunk(7), State::tag, &beta3, sizeof(beta3));
    const PersistentData recalibrated = Calibration(-0.05f);
    if (recalibrate) {
      // As its mirror rewrite does: the new calibration alone.
      Blank(kMirror, kSectorSize);
      PokeChunk(kMirror, PersistentData::tag, &recalibrated,
          sizeof(recalibrated));
      Blank(kSector, kSectorSize);
      PokeChunk(kSector, PersistentData::tag, &recalibrated,
          sizeof(recalibrated));
      PokeChunk(StateChunk(1), State::tag, &beta3, sizeof(beta3));
    }
    Settings s;
    CHECK(s.Init(false));
    CHECK(s.state().gain == 99);
    for (size_t i = 0; i < sizeof(s.state().padding); ++i) {
      CHECK(!s.state().padding[i]);
    }
    CHECK(SameOptions(s, recalibrate ? defaults : saved));
    CHECK(SameCalibration(s, recalibrate ? recalibrated : kCalibration));
  }
}

// A firmware never writes a tag other than STAT after the calibration in
// the settings sector, through options, states, compaction and
// recalibration.
void TestNoForeignTagInSector15() {
  test_checker.Start("TestNoForeignTagInSector15");
  Settings s;
  CompactionPending(&s);
  for (int i = 0; i < 40; ++i) {
    s.set_pitch_scale(PitchScale(i % 10));
    s.SaveOptions();
    s.mutable_state()->gain = uint8_t(i);
    s.SaveState();
  }
  memcpy(s.mutable_persistent_data()->adc_calibration,
      Calibration(-0.02f).adc_calibration, sizeof(PersistentData().
          adc_calibration));
  s.SavePersistentData();
  s.SaveState();
  for (int i = 1; i <= kStateChunks; ++i) {
    uint32_t tag;
    Peek(&tag, StateChunk(i), sizeof(tag));
    CHECK(tag == State::tag || tag == 0xffffffff);
  }
}

// Foreign records, kept from the study (STUDY.md): in the settings sector
// stock firmware programs over them; in the mirror sector it never reads
// them.
const uint32_t kForeignTag = 0x5a52445a;

void PutForeign(uint32_t address, size_t payload_size, uint8_t fill) {
  uint8_t payload[56];
  memset(payload, fill, payload_size);
  PokeChunk(address, kForeignTag, payload, uint16_t(payload_size));
}

void QuietExit(int) {
  _exit(1);
}

bool StockInit(PersistentData* data, State* state, StockStorage* stock) {
  memset(data, 0, sizeof(*data));
  memset(state, 0, sizeof(*state));
  return stock->Init(data, state);
}

bool SameCal(const PersistentData& a, const PersistentData& b) {
  return !memcmp(a.adc_calibration, b.adc_calibration,
      sizeof(a.adc_calibration));
}

State StateAt(int index) {
  State s;
  Peek(&s, StateChunk(index) + 8, sizeof(s));
  return s;
}

void TestStockForeignInSector15() {
  test_checker.Start("TestStockForeignInSector15");
  // S1: after the newest State, stock's next save programs over it.
  const size_t payloads[] = { 24, 56, 0 };  // 0: the control.
  for (int k = 0; k < 3; ++k) {
    Setup(kCalibration, 5);
    const State newest = StateAt(6);
    if (payloads[k]) {
      PutForeign(StateChunk(7), payloads[k], 0x5a);
    }
    PersistentData data;
    State state;
    StockStorage stock;
    CHECK(StockInit(&data, &state, &stock) && SameCal(data, kCalibration));
    CHECK(!memcmp(&state, &newest, sizeof(State)));
    const uint32_t overwrites = TestFlashOverwrites();
    state.gain = 200;
    stock.SaveState();
    PersistentData d2;
    State s2;
    StockStorage again;
    CHECK(StockInit(&d2, &s2, &again) && SameCal(d2, kCalibration));
    CHECK((TestFlashOverwrites() - overwrites) == (payloads[k] ? 1u : 0u));
    CHECK((s2.gain == 200) == !payloads[k]);
  }
  // S2: an overwritten word left uncorrectable faults stock's boot.
  for (int k = 0; k < 2; ++k) {
    Setup(kCalibration, 5);
    if (k) {
      TestFlashDamage(StateChunk(7));
    }
    const pid_t pid = fork();
    if (pid == 0) {
      TestFlashGuard(&QuietExit);
      PersistentData d;
      State st;
      StockStorage stock;
      StockInit(&d, &st, &stock);
      _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK((!WIFEXITED(status) || WEXITSTATUS(status) != 0) == (k == 1));
    TestFlashRepairAll();
  }
  // S3: a newer State after it is never read.
  for (int k = 0; k < 2; ++k) {
    Setup(kCalibration, 5);
    State newer = StateAt(6);
    newer.gain = 123;
    if (k) {
      PutForeign(StateChunk(7), 24, 0x5a);
    }
    PokeChunk(StateChunk(k ? 8 : 7), State::tag, &newer, sizeof(newer));
    PersistentData d;
    State st;
    StockStorage stock;
    CHECK(StockInit(&d, &st, &stock) && SameCal(d, kCalibration));
    CHECK((st.gain == 123) == (k == 0));
  }
  // S4: directly after the calibration, stock's Init fails: factory test.
  for (int k = 0; k < 2; ++k) {
    Setup(kCalibration, 0);
    if (k) {
      Blank(StateChunk(1), kSectorSize - kCalibrationChunk);
      PutForeign(StateChunk(1), 24, 0x5a);
    }
    PersistentData d;
    State st;
    StockStorage stock;
    CHECK(StockInit(&d, &st, &stock) == (k == 0));
    PersistentData d2;
    State s2;
    StockStorage again;
    CHECK(StockInit(&d2, &s2, &again) && SameCal(d2, kCalibration));
  }
  // D1: this firmware loads the newest State and rewrites the sector.
  for (int k = 0; k < 2; ++k) {
    Setup(kCalibration, 5);
    if (k) {
      PutForeign(StateChunk(7), 24, 0x5a);
    }
    Settings s;
    CHECK(s.Init(false) && SameCalibration(s, kCalibration));
    const uint32_t erases = TestFlashErases();
    const uint32_t overwrites = TestFlashOverwrites();
    s.mutable_state()->gain = 201;
    s.SaveState();
    CHECK(TestFlashOverwrites() == overwrites);
    CHECK(TestFlashErases() - erases == (k ? 1u : 0u));
    Settings loaded;
    CHECK(loaded.Init(false) && loaded.state().gain == 201);
    CHECK(MirrorHolds(kCalibration));
  }
  TestFlash().wide_sectors = 0;  // Stock names sector 15 to the HAL.
}

void TestRecordsInMirrorSector() {
  test_checker.Start("TestRecordsInMirrorSector");
  static uint8_t mirror[kSectorSize];
  static uint8_t now[kSectorSize];
  // a) Stock, through two of its own rewrites: the mirror sector untouched.
  const Saved saved = Setup(kCalibration, 5);
  Peek(mirror, kMirror, kSectorSize);
  {
    PersistentData d;
    State st;
    StockStorage stock;
    CHECK(StockInit(&d, &st, &stock) && SameCal(d, kCalibration));
    const uint32_t overwrites = TestFlashOverwrites();
    for (int i = 0; i < 2 * kStateChunks + 3; ++i) {
      st.gain = uint8_t(i);
      stock.SaveState();
    }
    Peek(now, kMirror, kSectorSize);
    CHECK(!memcmp(now, mirror, kSectorSize));
    CHECK(TestFlashOverwrites() == overwrites);
    PersistentData d2;
    State s2;
    StockStorage again;
    CHECK(StockInit(&d2, &s2, &again) && SameCal(d2, kCalibration));
    // b) Then this firmware: the options are still there.
    Settings s;
    CHECK(s.Init(false) && SameOptions(s, saved));
  }
  TestFlash().wide_sectors = 0;
  // b) This firmware's state saves, through a rewrite, leave it too.
  Setup(kCalibration, 5);
  Peek(mirror, kMirror, kSectorSize);
  {
    Settings s;
    CHECK(s.Init(false));
    for (int i = 0; i < kStateChunks + 3; ++i) {
      s.mutable_state()->gain = uint8_t(i);
      s.SaveState();
    }
    Peek(now, kMirror, kSectorSize);
    CHECK(!memcmp(now, mirror, kSectorSize));
  }
  // c) Recalibration puts the record back after the new calibration.
  {
    const PersistentData recalibrated = Calibration(-0.05f);
    const Saved before = Setup(kCalibration, 5);
    Settings s;
    CHECK(s.Init(false));
    memcpy(s.mutable_persistent_data()->adc_calibration,
        recalibrated.adc_calibration, sizeof(recalibrated.adc_calibration));
    s.SavePersistentData();
    Settings loaded;
    CHECK(loaded.Init(false) && SameCalibration(loaded, recalibrated));
    CHECK(MirrorHolds(recalibrated) && SameOptions(loaded, before));
  }
  // d) A blank settings sector: calibration and options from the mirror.
  {
    const Saved before = Setup(kCalibration, 5);
    Blank(kSector, kSectorSize);
    Settings s;
    CHECK(s.Init(false) && !s.write_locked());
    CHECK(SameCalibration(s, kCalibration) && SameOptions(s, before));
  }
  // e) An uncorrectable record word: no unmasked read; the slots before it
  // load, and the next save compacts it away.
  for (int k = 0; k < 2; ++k) {
    const Saved before = Setup(kCalibration, 5);
    const Saved defaults = Defaults();
    Setup(kCalibration, 5);
    TestFlashDamage(RecordChunk(1 + k));
    const int violations = TestFlash().violations;
    Settings s;
    CHECK(s.Init(false) && SameCalibration(s, kCalibration));
    CHECK(SameOptions(s, k ? Of(s) : defaults));
    CHECK(k == 0 || s.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
    s.set_pitch_scale(PITCH_SCALE_FIFTHS);
    s.SaveOptions();
    CHECK(TestFlash().violations == violations);
    CHECK(TestFlash().num_damaged == 0);
    Settings loaded;
    CHECK(loaded.Init(false) && loaded.pitch_scale() == PITCH_SCALE_FIFTHS);
    PersistentData d;
    State st;
    StockStorage stock;
    CHECK(StockInit(&d, &st, &stock) && SameCal(d, kCalibration));
    (void)before;
  }
  TestFlash().wide_sectors = 0;
  // f) Records without a mirrored calibration and a blank settings sector:
  // damaged, never a new module. This firmware never leaves that.
  for (int k = 0; k < 2; ++k) {
    Wipe();
    if (k) {
      PutForeign(RecordChunk(1), 24, 1);
    }
    Settings s;
    s.Init(false);
    CHECK(s.write_locked() == (k == 1));
  }
}

int main() {
  if (!CHECK(MapBank2())) {
    return 1;
  }
  TestRecordJournal();
  TestRecordChecksum();
  TestRecordVersions();
  TestTornRecord();
  TestSector14Full();
  TestInterruptedCompaction();
  TestCompactionGuards();
  TestFactoryCaliGuard();
  TestRule6ImplausibleSector15();
  TestRecoveryResetsOptions();
  TestImplausibleSaveKept();
  TestRecoveryRestoredStorage();
  TestRecordAppendRefused();
  TestRecordLowSupply();
  TestGesturePersistence();
  TestEarlierFirmware();
  TestNoForeignTagInSector15();
  TestStockForeignInSector15();
  TestRecordsInMirrorSector();
  test_checker.Start("main");
  CHECK(TestFlash().violations == 0);
  CHECK(TestFlash().wide_sectors == 0);
  return test_checker.failures() ? 1 : 0;
}
