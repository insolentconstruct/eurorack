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
// Frozen buffer backup tests.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

#include "beads/test/test_checker.h"

#define private public
#include "stmlib/system/system_clock.h"
#include "beads/background_buffer_backup.h"
#include "beads/settings.h"
#undef private

#include "beads/dsp/processor.h"

bool test_locked = true;
int test_budget = -1;
int test_fail_at = 0;
int test_ops = 0;
int test_erases = 0;
bool test_half_erase = false;
bool test_stale_flags = false;
int test_stuck_erases = 0;
namespace stmlib {

SystemClock system_clock;

}  // namespace stmlib

using namespace beads;
using namespace stmlib;

typedef BackgroundBufferBackup::BufferBackupHeader Header;
STATIC_ASSERT(sizeof(Header) == 32, one_flash_word);
STATIC_ASSERT(offsetof(Header, tag) == 0, tag);
STATIC_ASSERT(offsetof(Header, write_ptr) == 4, write_ptr);
STATIC_ASSERT(offsetof(Header, media) == 8, media);
STATIC_ASSERT(offsetof(Header, mono) == 9, mono);
STATIC_ASSERT(offsetof(Header, crc) == 12, crc);
STATIC_ASSERT(offsetof(Header, crc_tag) == 16, crc_tag);
STATIC_ASSERT(offsetof(Header, trimmed_frames) == 20, trimmed_frames);
STATIC_ASSERT(offsetof(Header, reserved) == 24, reserved);

const size_t kAudio = kDelayBufferSizeBits / 8;
uint8_t* const kFlash = reinterpret_cast<uint8_t*>(0x08100000);
Header* const kHeader = reinterpret_cast<Header*>(0x08100000);
const int kMaxDelay[] = { 384000, 256000, 256000, 192000 };
static Processor processor;

// Bitwise CRC-32 (ISO-HDLC), independent of stmlib's table.
uint32_t Crc(uint32_t crc, const void* data, size_t size) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  crc = ~crc;
  while (size--) {
    crc ^= *p++;
    for (int i = 0; i < 8; ++i) {
      crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
    }
  }
  return ~crc;
}

void Fill(uint8_t seed) {
  for (size_t i = 0; i < kAudio; ++i) {
    processor.data_[i] = uint8_t(i * 7 + seed + (i >> 9));
  }
}

bool Restored(uint8_t seed) {
  for (size_t i = 0; i < kAudio; ++i) {
    if (processor.data_[i] != uint8_t(i * 7 + seed + (i >> 9))) {
      return false;
    }
  }
  return true;
}

struct Rig {
  Settings settings;
  BackgroundBufferBackup backup;
  // Power-up: settings from flash, then the backup, as in Init().
  bool Boot() {
    test_budget = -1;
    test_fail_at = 0;
    settings.Init(false);
    processor.configured_ = 0;
    processor.write_ptr_ = 0;
    memset(processor.data_, 0, kAudio);
    system_clock.Init();
    backup.Init(&processor, &settings);
    return backup.Load();
  }
  void Autosave(bool on) {
    settings.mutable_state()->disable_autosave = !on;
    settings.SaveState();
  }
  // Freeze for 10 s, then run the main loop until the backup ends or the
  // power goes. Returns false on power loss.
  bool Freeze(int save_every) {
    for (int i = 0; i < 10000; ++i) {
      system_clock.Tick();
    }
    backup.Process(true);
    for (int i = 1; backup.busy(); ++i) {
      if (test_budget == 0) {
        return false;
      }
      if (save_every && i % save_every == 0) {
        settings.SaveState();
      }
      backup.Process(true);
    }
    return test_budget != 0;
  }
};

void Wipe() {
  TestFlashRepairAll();
  memset(kFlash, 255, 0x100000);
  test_ops = 0;
  test_erases = 0;
}

void Save(Processor::Media media, bool mono, int write_ptr, uint8_t seed) {
  Rig rig;
  rig.Boot();
  processor.Configure(media, mono);
  processor.write_ptr_ = write_ptr;
  Fill(seed);
  CHECK(rig.Freeze(0));
  CHECK(rig.settings.state().frozen);
}

void TestTrimBackup() {
  test_checker.Start("TestTrimBackup");
  for (int media = 0; media < 4; ++media) {
    for (int mono = 0; mono < 2; ++mono) {
      const int capacity = kMaxDelay[media] << mono;
      int lengths[4];
      lengths[0] = 0;
      lengths[1] = 1;
      lengths[2] = capacity / 2;
      lengths[3] = capacity;
      for (int i = 0; i < 4; ++i) {
        Wipe();
        Rig r;
        r.Boot();
        processor.Configure(Processor::Media(media), mono);
        processor.write_ptr_ = 123;
        processor.duration_ = lengths[i];
        Fill(3);
        CHECK(r.Freeze(0));
        CHECK(kHeader->trimmed_frames == uint32_t(capacity - lengths[i]));
        CHECK(r.Boot());
        CHECK(processor.duration_ == lengths[i]);
        CHECK(Restored(3));
      }
      kHeader->trimmed_frames = capacity + 1;
      kHeader->crc = 0;
      kHeader->crc = Crc(Crc(0, kFlash + 32, kAudio), kHeader, sizeof(Header));
      Rig invalid;
      CHECK(!invalid.Boot());
      CHECK(processor.configured_ == 0);
    }
  }
}

void TestFormat() {
  test_checker.Start("TestFormat");
  for (int media = 0; media < 4; ++media) {
    for (int mono = 0; mono < 2; ++mono) {
      Wipe();
      const int last = (kMaxDelay[media] << mono) - 1;
      Save(Processor::Media(media), mono, last, uint8_t(media * 2 + mono));
      // The fields the stock firmware reads.
      CHECK(kHeader->tag == 0x5a5a5246);
      CHECK(kHeader->write_ptr == last);
      CHECK(kHeader->media == media && kHeader->mono == mono);
      // Ours: all defined.
      CHECK(kHeader->padding[0] == 0 && kHeader->padding[1] == 0);
      CHECK(kHeader->crc_tag == 0x31435243);
      for (size_t i = 0; i < sizeof(kHeader->reserved); ++i) {
        CHECK(kHeader->reserved[i] == 0);
      }
      Header h = *kHeader;
      h.crc = 0;
      CHECK(kHeader->crc == Crc(Crc(0, kFlash + 32, kAudio), &h, sizeof(h)));
      Rig rig;
      CHECK(rig.Boot());
      CHECK(processor.media_ == media && processor.mono_ == mono);
      CHECK(processor.write_ptr_ == last);
      CHECK(Restored(uint8_t(media * 2 + mono)));
    }
  }
  printf("PASS format, all media, mono and stereo\n");
}

// A backup written by the stock firmware: bytes 10-31 are stack leftovers.
void WriteStock(int media, int mono, int write_ptr, uint8_t seed) {
  Wipe();
  Rig rig;
  rig.Boot();
  rig.settings.mutable_state()->frozen = true;
  rig.settings.SaveState();
  Fill(seed);
  memcpy(kFlash + 32, processor.data_, kAudio);
  for (int i = 10; i < 32; ++i) {
    kFlash[i] = uint8_t(0xa5 ^ (i * 29));
  }
  kHeader->tag = 0x5a5a5246;
  kHeader->write_ptr = write_ptr;
  kHeader->media = media;
  kHeader->mono = mono;
}

void TestFrozenTrimBackup() {
  test_checker.Start("TestFrozenTrimBackup");
  for (int stock = 0; stock < 2; ++stock) {
    Wipe();
    if (stock) {
      WriteStock(3, 0, 123, 7);
    } else {
      Save(Processor::MEDIA_DIGITAL, false, 123, 7);
    }
    Rig r;
    CHECK(r.Boot());
    CHECK(processor.duration_ == kMaxDelay[3]);
    processor.duration_ = 0;
    r.backup.Process(true);
    CHECK(!r.settings.state().frozen);
    CHECK(!r.backup.busy());
    CHECK(!r.backup.previous_freeze_state_);
    CHECK(r.Freeze(0));
    CHECK(kHeader->trimmed_frames == uint32_t(kMaxDelay[3]));
    // The saved trim is kept: no save follows while the gate holds.
    for (int i = 0; i < 10; ++i) {
      r.backup.Process(true);
    }
    CHECK(r.settings.state().frozen && !r.backup.busy());
    CHECK(r.Boot());
    CHECK(processor.duration_ == 0);
    CHECK(Restored(7));
  }
}

void TestStock() {
  test_checker.Start("TestStock");
  for (int media = 0; media < 4; ++media) {
    for (int mono = 0; mono < 2; ++mono) {
      WriteStock(media, mono, 123, 9);
      Rig r;
      CHECK(r.Boot());
      CHECK(processor.duration_ == kMaxDelay[media] << mono);
    }
  }
  WriteStock(2, 0, 12345, 9);
  Rig rig;
  CHECK(rig.Boot());
  CHECK(processor.media_ == 2 && !processor.mono_ &&
      processor.write_ptr_ == 12345);
  CHECK(Restored(9));
  CHECK(processor.duration_ == kMaxDelay[2]);
  const int bad[][3] = {
    { 4, 0, 0 }, { 255, 0, 0 }, { 0, 2, 0 }, { 3, 0, -1 },
    { 3, 0, 192000 }, { 3, 1, 384000 }, { 0, 1, 768000 }, { 1, 0, 256000 },
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
    WriteStock(bad[i][0], bad[i][1], bad[i][2], 1);
    Rig rig;
    CHECK(!rig.Boot());
    CHECK(processor.configured_ == 0);
  }
  WriteStock(0, 1, 767999, 3);
  Rig edge;
  CHECK(edge.Boot() && Restored(3));
  printf("PASS stock backups load; out-of-range headers are refused\n");
}

void TestCorruption() {
  test_checker.Start("TestCorruption");
  const size_t offsets[] = { 20, 32, 32 + 12345, 32 + kAudio - 1, 4, 8, 9 };
  for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
    Wipe();
    Save(Processor::MEDIA_DIGITAL, false, 100, 5);
    // Header fields stay in range; only the CRC sees the change.
    kFlash[offsets[i]] ^= 1;
    Rig rig;
    CHECK(!rig.Boot());
  }
  // A half-erased sector in the middle of the audio.
  Wipe();
  Save(Processor::MEDIA_DIGITAL, false, 100, 5);
  memset(kFlash + 3 * 0x20000, 255, 0x10000);
  Rig rig;
  CHECK(!rig.Boot());
  printf("PASS corrupted audio or header is refused\n");
}

// An old, complete backup is on flash and the frozen flag is set but stale
// (autosave was off at power-up). The new backup loses power after `budget`
// flash operations, with a half-done erase if `half`; at the next power-up,
// either complete backup may load, never a mix. The old backup has no CRC
// if `stock` wrote it.
void CheckPowerLoss(bool half, bool stock) {
  int checked = 0, loaded[3] = { 0, 0, 0 };
  for (int budget = 0; budget <= 24010; budget += budget < 40 ? 1 :
       budget < 23960 ? 997 : 1) {
    Wipe();
    if (stock) {
      WriteStock(Processor::MEDIA_DIGITAL, 0, 100, 1);
    } else {
      Save(Processor::MEDIA_DIGITAL, false, 100, 1);
    }
    {
      Rig rig;
      rig.Boot();
      rig.Autosave(false);
    }
    Rig rig;
    CHECK(!rig.Boot());  // Autosave off: not loaded, flag left set.
    CHECK(rig.settings.state().frozen);
    rig.Autosave(true);
    processor.Configure(Processor::MEDIA_DIGITAL, false);
    processor.write_ptr_ = 200;
    Fill(2);
    test_budget = budget;
    test_half_erase = half;
    rig.Freeze(0);
    test_half_erase = false;
    Rig after;
    if (after.Boot()) {
      // Every byte of one backup or the other.
      CHECK((processor.write_ptr_ == 200 && Restored(2)) ||
             (processor.write_ptr_ == 100 && Restored(1)));
      ++loaded[processor.write_ptr_ / 100];
    } else {
      ++loaded[0];
    }
    ++checked;
  }
  printf("PASS power loss at %d points%s%s: never restores a partial backup "
         "(old %d, new %d, none %d)\n", checked,
         half ? ", half-erased sectors" : "", stock ? ", stock old backup" : "",
         loaded[1], loaded[2], loaded[0]);
}

void TestFailedOperations() {
  test_checker.Start("TestFailedOperations");
  int checked = 0;
  for (int fail = 1; fail <= 24010; fail += fail < 40 ? 1 :
       fail < 23960 ? 997 : 1) {
    Wipe();
    Rig rig;
    rig.Boot();
    processor.Configure(Processor::MEDIA_CLEAN_TAPE, true);
    processor.write_ptr_ = 7;
    Fill(4);
    test_fail_at = test_ops + fail;
    rig.Freeze(0);
    if (rig.settings.state().frozen) {
      continue;  // The failure came after the backup: nothing to check.
    }
    CHECK(!rig.backup.busy());
    Rig after;
    CHECK(!after.Boot());
    // Unfreezing and freezing again saves normally.
    after.backup.Process(false);
    processor.Configure(Processor::MEDIA_CLEAN_TAPE, true);
    processor.write_ptr_ = 7;
    Fill(4);
    CHECK(after.Freeze(0));
    Rig again;
    CHECK(again.Boot() && Restored(4));
    ++checked;
  }
  printf("PASS flash errors at %d points abandon the backup cleanly\n",
      checked);
}

void TestSettingsSaves() {
  test_checker.Start("TestSettingsSaves");
  Wipe();
  Rig rig;
  rig.Boot();
  processor.Configure(Processor::MEDIA_DIRTY_TAPE, false);
  processor.write_ptr_ = 383999;
  Fill(6);
  CHECK(rig.Freeze(1000));  // A settings save every 1000 blocks.
  CHECK(rig.settings.state().frozen);
  Rig after;
  CHECK(after.Boot() && Restored(6));
  printf("PASS settings saves during a backup\n");
}

void TestFreezeDeadlineAndCancel() {
  test_checker.Start("TestFreezeDeadlineAndCancel");
  Wipe();
  Rig r;
  CHECK(!r.Boot());
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  Fill(17);
  r.backup.Process(false);
  const int ops = test_ops;
  for (int i = 1; i < 10000; ++i) {
    system_clock.Tick();
    r.backup.Process(true);
    CHECK(!r.backup.busy() && test_ops == ops);
  }
  system_clock.Tick();
  r.backup.Process(true);
  CHECK(r.backup.busy() && test_ops == ops + 1);
  for (int i = 0; i < 10; ++i) {
    r.backup.Process(true);
  }
  CHECK(r.backup.remaining_bytes_ > 0 && r.backup.write_offset_ > 0);
  r.backup.Process(false);
  CHECK(!r.backup.busy() && kHeader->tag == 0xffffffff);
  for (size_t i = 0; i < sizeof(Header); ++i) {
    CHECK(kFlash[i] == 255);
  }
  CHECK(!r.settings.state().frozen);
  for (int i = 0; i < 10000; ++i) {
    system_clock.Tick();
  }
  CHECK(r.Freeze(0));
  CHECK(kHeader->tag == 0x5a5a5246 && r.settings.state().frozen);
  Rig after;
  CHECK(after.Boot() && Restored(17));
}

void Run(Rig& rig, bool freeze, uint32_t milliseconds) {
  for (uint32_t i = 0; i < milliseconds; ++i) {
    system_clock.Tick();
    rig.backup.Process(freeze);
  }
}

void TestModulatedFreeze() {
  test_checker.Start("TestModulatedFreeze");
  for (int on = 15000; on <= 25000; on += 10000) {
    Wipe();
    Rig rig;
    rig.Boot();
    for (int cycle = 0; cycle < 40; ++cycle) {
      Run(rig, true, on);
      Run(rig, false, 5000);
      CHECK(test_erases <= (on == 15000 ? 6 : 12));
      if (cycle >= 2) {
        CHECK(test_erases == (on == 15000 ? 6 : 12));
      }
    }
  }
}

void TestBackupBackoff() {
  test_checker.Start("TestBackupBackoff");
  Wipe();
  Rig rig;
  rig.Boot();
  uint32_t delay = 10000;
  for (int attempt = 0; attempt < 9; ++attempt) {
    const int erases = test_erases;
    Run(rig, true, delay - 1);
    CHECK(test_erases == erases);
    Run(rig, true, 1);
    CHECK(test_erases == erases + 1);
    rig.backup.Process(false);
    delay = delay < 320000 ? delay * 2 : 320000;
    CHECK(rig.backup.backup_delay_ == delay);
  }
  CHECK(!rig.Boot());
  CHECK(rig.backup.backup_delay_ == 10000);
}

void TestBackupCompletionReset() {
  test_checker.Start("TestBackupCompletionReset");
  Wipe();
  Rig rig;
  rig.Boot();
  Run(rig, true, 10000);
  rig.backup.Process(false);
  Run(rig, true, 20000);
  while (rig.backup.busy()) {
    rig.backup.Process(true);
  }
  CHECK(rig.settings.state().frozen);
  CHECK(rig.backup.backup_delay_ == 10000);
  rig.backup.Process(false);
  const int erases = test_erases;
  Run(rig, true, 10001);
  CHECK(test_erases == erases + 2);
}

void TestBackupThawReset() {
  test_checker.Start("TestBackupThawReset");
  Wipe();
  Rig rig;
  rig.Boot();
  Run(rig, true, 10000);
  rig.backup.Process(false);
  Run(rig, false, 59000);
  CHECK(rig.backup.backup_delay_ == 20000);
  Run(rig, true, 10000);
  CHECK(test_erases == 1);
  rig.backup.Process(false);
  Run(rig, false, 59999);
  CHECK(rig.backup.backup_delay_ == 20000);
  Run(rig, false, 1);
  CHECK(rig.backup.backup_delay_ == 10000);
  Run(rig, true, 10000);
  CHECK(test_erases == 2);
}

void TestBackupEarlyCancel() {
  test_checker.Start("TestBackupEarlyCancel");
  Wipe();
  Rig rig;
  rig.Boot();
  for (int i = 0; i < 8; ++i) {
    Run(rig, true, 9999);
    rig.backup.Process(false);
    CHECK(test_erases == 0);
    CHECK(rig.backup.backup_delay_ == 10000);
  }
  Run(rig, true, 10001);
  while (rig.backup.busy()) {
    rig.backup.Process(true);
  }
  CHECK(test_erases == 6 && rig.settings.state().frozen);
}

void TestBackupClockWrap() {
  test_checker.Start("TestBackupClockWrap");
  Wipe();
  Rig rig;
  rig.Boot();
  // Start just before wrap; the doubled deadline crosses it.
  system_clock.count_ = UINT32_MAX - 15000;
  rig.backup.Process(false);
  Run(rig, true, 9999);
  CHECK(test_erases == 0 && !rig.backup.busy());
  Run(rig, true, 1);
  CHECK(test_erases == 1 && rig.backup.busy());
  rig.backup.Process(false);
  CHECK(!rig.backup.busy() && rig.backup.backup_delay_ == 20000);
  // The doubled deadline also crosses wrap, without starting early.
  Run(rig, true, 19999);
  CHECK(test_erases == 1 && !rig.backup.busy());
  Run(rig, true, 1);
  CHECK(test_erases == 2 && rig.backup.busy());
  rig.backup.Process(false);
  CHECK(rig.backup.backup_delay_ == 40000);
  // A fresh boot lets the thaw interval cross wrap as well.
  Wipe();
  CHECK(!rig.Boot());
  system_clock.count_ = UINT32_MAX - 15000;
  rig.backup.Process(false);
  Run(rig, true, 10000);
  rig.backup.Process(false);
  Run(rig, false, 59999);
  CHECK(rig.backup.backup_delay_ == 20000);
  Run(rig, false, 1);
  CHECK(rig.backup.backup_delay_ == 10000);
  Run(rig, true, 9999);
  CHECK(test_erases == 1 && !rig.backup.busy());
  Run(rig, true, 1);
  CHECK(test_erases == 2 && rig.backup.busy());
  while (rig.backup.busy()) {
    rig.backup.Process(true);
  }
  CHECK(rig.settings.state().frozen);
  CHECK(rig.backup.backup_delay_ == 10000);
}

void TestBackupRestoredThaw() {
  test_checker.Start("TestBackupRestoredThaw");
  Wipe();
  Save(Processor::MEDIA_DIGITAL, false, 123, 19);
  Rig rig;
  CHECK(rig.Boot() && Restored(19));
  const int erases = test_erases;
  Run(rig, true, 70000);
  CHECK(test_erases == erases && !rig.backup.busy());
  rig.backup.Process(false);
  const uint32_t released = system_clock.milliseconds();
  CHECK(rig.backup.thaw_start_time_ == released);
  CHECK(!rig.settings.state().frozen);
  CHECK(rig.backup.backup_delay_ == 10000);
  Run(rig, false, 59999);
  CHECK(rig.backup.thaw_start_time_ == released);
  CHECK(rig.backup.backup_delay_ == 10000);
  Run(rig, false, 1);
  CHECK(rig.backup.backup_delay_ == 10000);
  Fill(20);
  Run(rig, true, 9999);
  CHECK(test_erases == erases && !rig.backup.busy());
  Run(rig, true, 1);
  CHECK(test_erases == erases + 1 && rig.backup.busy());
  while (rig.backup.busy()) {
    rig.backup.Process(true);
  }
  CHECK(rig.settings.state().frozen);
  CHECK(rig.backup.backup_delay_ == 10000);
  Rig after;
  CHECK(after.Boot() && Restored(20));
}

bool Cleared() {
  for (size_t i = 0; i < kAudio; ++i) {
    if (processor.data_[i]) {
      return false;
    }
  }
  return true;
}

void TestDamagedBuffer() {
  test_checker.Start("TestDamagedBuffer");
  // An uncorrectable word in the header, in the audio (found by the CRC
  // pass), and in a stock backup's audio (found by the copy).
  const uint32_t words[] = { 0, 32, 32 * 1000, 32 + kAudio - 32 };
  for (int stock = 0; stock < 2; ++stock) {
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
      Wipe();
      if (stock) {
        WriteStock(3, 0, 123, 7);
      } else {
        Save(Processor::MEDIA_DIGITAL, false, 123, 7);
      }
      TestFlashDamage(0x08100000 + words[i]);
      Rig r;
      memset(processor.data_, 0x55, kAudio);
      CHECK(!r.Boot());
      CHECK(!r.settings.write_locked());
      CHECK(Cleared());
      // Caught before the processor is touched, except by a stock copy.
      CHECK(processor.configured_ == (stock && words[i] ? 1 : 0));
      // The next freeze saves over it.
      processor.Configure(Processor::MEDIA_DIGITAL, false);
      Fill(8);
      CHECK(r.Freeze(0));
      CHECK(TestFlash().num_damaged == 0);
      Rig after;
      CHECK(after.Boot() && Restored(8));
    }
  }
  CHECK(TestFlash().violations == 0);
}

void TestFrozenNoReads() {
  test_checker.Start("TestFrozenNoReads");
  // Frozen after a load, or after a save, the main loop reads no flash, and
  // an unchanged trim (here not the full length) saves nothing.
  for (int stock = 0; stock < 2; ++stock) {
    Wipe();
    if (stock) {
      WriteStock(3, 0, 123, 7);
    } else {
      Rig r;
      r.Boot();
      processor.Configure(Processor::MEDIA_DIGITAL, false);
      processor.duration_ = 1000;
      Fill(7);
      CHECK(r.Freeze(0));
    }
    Rig r;
    CHECK(r.Boot());
    const int reads = TestFlash().masked_reads;
    const int ops = test_ops;
    for (int i = 0; i < 100; ++i) {
      r.backup.Process(true);
    }
    CHECK(TestFlash().masked_reads == reads);
    CHECK(test_ops == ops);
    CHECK(r.settings.state().frozen);
    CHECK(!r.backup.busy());
  }
}

void TestStaleFlags() {
  test_checker.Start("TestStaleFlags");
  // An error flag left by an earlier operation: the HAL would refuse the
  // next erase or program, and the save be abandoned.
  Wipe();
  Rig r;
  r.Boot();
  processor.Configure(Processor::MEDIA_CLEAN_TAPE, false);
  Fill(5);
  const int erases = test_erases;
  test_stale_flags = true;
  CHECK(r.Freeze(0));
  test_stale_flags = false;
  CHECK(r.settings.state().frozen);
  CHECK(test_erases == erases + 6);  // None refused and retried.
  Rig after;
  CHECK(after.Boot() && Restored(5));
}

void TestLockedBackup() {
  test_checker.Start("TestLockedBackup");
  Wipe();
  Save(Processor::MEDIA_DIGITAL, false, 123, 7);
  // Both copies of the calibration lost.
  TestFlashDamage(0x081e0000 + 32);
  TestFlashDamage(0x081c0000 + 32);
  Rig r;
  CHECK(!r.Boot());  // The stored frozen flag isn't trusted.
  CHECK(r.settings.write_locked());
  const int erases = test_erases;
  const int ops = test_ops;
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  Fill(9);
  r.Freeze(0);
  CHECK(test_erases == erases && test_ops == ops);
  CHECK(!r.settings.state().frozen);
  CHECK(!r.backup.busy());
  // Nor a program, even mid-save.
  r.backup.write_offset_ = 0;
  r.backup.remaining_bytes_ = kAudio;
  r.backup.WriteBlock();
  CHECK(test_ops == ops);
  TestFlashRepairAll();
}

void TestEraseDamagedSectors() {
  test_checker.Start("TestEraseDamagedSectors");
  Wipe();
  Save(Processor::MEDIA_DIGITAL, false, 123, 7);
  TestFlashDamage(0x08120000 + 4096);
  TestFlashDamage(0x08180000 + 0x1ffe0);
  Rig r;
  r.Boot();
  uint8_t invalid[kProfileSize] = { 0 };
  r.settings.Init(false, invalid);
  CHECK(r.settings.profile_locked() && !r.settings.storage_damaged());
  int erases = test_erases;
  test_locked = true;
  // A refused erase, then one that does nothing: retried until blank.
  test_fail_at = test_ops + 1;
  test_stuck_erases = 1;
  r.backup.EraseDamagedSectors();
  test_fail_at = 0;
  CHECK(test_erases == erases + 4);
  CHECK(TestFlash().num_damaged == 0);
  CHECK(kFlash[0x20000] == 0xff && kFlash[0x80000] == 0xff);
  CHECK(TestFlash().wide_sectors == 0);
  // The other sectors are left as they were.
  CHECK(kHeader->tag == 0x5a5a5246 && kFlash[0x40000] != 0xff);
  // An erase that never works leaves the damage.
  TestFlashDamage(0x08140000);
  erases = test_erases;
  test_stuck_erases = 100;
  r.backup.EraseDamagedSectors();
  test_stuck_erases = 0;
  CHECK(test_erases == erases + 3);
  CHECK(TestFlash().num_damaged == 1);
  TestFlashRepairAll();
}

void TestRetriedBufferErase() {
  test_checker.Start("TestRetriedBufferErase");
  // A refused erase is retried, and the save completes.
  Wipe();
  Rig r;
  r.Boot();
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  Fill(6);
  test_fail_at = test_ops + 1;
  CHECK(r.Freeze(0));
  test_fail_at = 0;
  CHECK(r.settings.state().frozen);
  Rig after;
  CHECK(after.Boot() && Restored(6));
}

void TestStuckBufferErase() {
  test_checker.Start("TestStuckBufferErase");
  // An erase that reports success but leaves the old backup: the save is
  // abandoned before any audio is written over it.
  Wipe();
  Save(Processor::MEDIA_DIGITAL, false, 123, 7);
  Rig r;
  CHECK(r.Boot());
  r.backup.Process(false);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  Fill(9);
  const int ops = test_ops;
  test_stuck_erases = 3;  // Every attempt.
  r.Freeze(0);
  test_stuck_erases = 0;
  CHECK(test_ops == ops + 3);
  CHECK(!r.settings.state().frozen);
  CHECK(!r.backup.busy());
  CHECK(TestFlash().wide_sectors == 0);
}

void TestPowerLoss() {
  test_checker.Start("TestPowerLoss");
  for (int i = 0; i < 4; ++i) {
    CheckPowerLoss(i & 1, i & 2);
  }
}

void TestLowSupplyBackup() {
  test_checker.Start("TestLowSupplyBackup");
  TestFlashPowerUp();
  // A low supply when the backup starts: no erase or program, and the save
  // is abandoned as a failed erase abandons it.
  Wipe();
  Save(Processor::MEDIA_DIGITAL, false, 123, 7);
  Rig r;
  CHECK(r.Boot());
  r.backup.Process(false);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  Fill(9);
  const int ops = test_ops;
  const int erases = test_erases;
  TestFlash().supply_low = true;
  r.Freeze(0);
  CHECK(test_ops == ops && test_erases == erases);
  CHECK(!r.backup.busy());
  CHECK(!r.settings.state().frozen);
  // Nor a program, even mid-save.
  r.backup.write_offset_ = 0;
  r.backup.remaining_bytes_ = kAudio;
  r.backup.WriteBlock();
  CHECK(test_ops == ops);
  CHECK(!r.backup.busy());
  // The old backup is still in flash, but no longer flagged.
  CHECK(kHeader->tag == 0x5a5a5246);
  Rig after;
  CHECK(!after.Boot());
  // With the supply back, thawing and freezing again saves normally.
  TestFlash().supply_low = false;
  after.backup.Process(false);
  processor.Configure(Processor::MEDIA_DIGITAL, false);
  Fill(9);
  CHECK(after.Freeze(0));
  Rig again;
  CHECK(again.Boot() && Restored(9));
  CHECK(TestFlash().low_supply_writes == 0);
}

void TestSupplyDropBackup() {
  test_checker.Start("TestSupplyDropBackup");
  TestFlashPowerUp();
  // The supply drops after an erase, an audio word, the header or the
  // settings save that flags it: the save stops there and doesn't load
  // unless it completed, and the next one completes.
  const int last = 6 + 24000 + 1 + int((8 + sizeof(State) + 31) / 32);
  const int drops[] = { 1, 5, 6, 7, 12000, 24005, 24006, 24007, last - 1,
      last };
  for (size_t i = 0; i < sizeof(drops) / sizeof(drops[0]); ++i) {
    Wipe();
    Save(Processor::MEDIA_DIGITAL, false, 100, 1);
    Rig r;
    CHECK(r.Boot());
    r.backup.Process(false);
    processor.Configure(Processor::MEDIA_DIGITAL, false);
    processor.write_ptr_ = 200;
    Fill(2);
    TestFlash().supply_budget = drops[i];
    r.Freeze(0);
    CHECK(TestFlash().supply_low);
    CHECK(!r.backup.busy());
    TestFlash().supply_low = false;
    TestFlash().supply_budget = 0;
    Rig after;
    if (drops[i] == last) {
      CHECK(after.Boot() && processor.write_ptr_ == 200 && Restored(2));
    } else {
      CHECK(!after.Boot());
    }
    after.backup.Process(false);
    processor.Configure(Processor::MEDIA_DIGITAL, false);
    processor.write_ptr_ = 300;
    Fill(3);
    CHECK(after.Freeze(0));
    Rig again;
    CHECK(again.Boot() && processor.write_ptr_ == 300 && Restored(3));
  }
  CHECK(TestFlash().low_supply_writes == 0);
}

void OnUnmaskedRead(int) {
  printf("ERROR: %s: unmasked read of a damaged flash word\n",
      test_checker.test());
  _exit(1);
}

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  TestFlashGuard(&OnUnmaskedRead);
  TestFreezeDeadlineAndCancel();
  TestModulatedFreeze();
  TestBackupBackoff();
  TestBackupCompletionReset();
  TestBackupThawReset();
  TestBackupEarlyCancel();
  TestBackupClockWrap();
  TestBackupRestoredThaw();
  TestTrimBackup();
  TestFrozenTrimBackup();
  TestFormat();
  TestStock();
  TestCorruption();
  TestPowerLoss();
  TestFailedOperations();
  TestSettingsSaves();
  TestDamagedBuffer();
  TestFrozenNoReads();
  TestStaleFlags();
  TestLockedBackup();
  TestEraseDamagedSectors();
  TestRetriedBufferErase();
  TestStuckBufferErase();
  TestLowSupplyBackup();
  TestSupplyDropBackup();
  return test_checker.failures() ? 1 : 0;
}
