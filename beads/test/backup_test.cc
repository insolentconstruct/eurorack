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

#include "beads/test/test_checker.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#define private public
#include "beads/background_buffer_backup.h"
#include "beads/settings.h"
#undef private
#include "beads/dsp/processor.h"
#include "stmlib/system/system_clock.h"

bool test_locked = true;
int test_budget = -1;
int test_fail_at = 0;
int test_ops = 0;
bool test_half_erase = false;
namespace stmlib { SystemClock system_clock; }
using namespace beads;
using namespace stmlib;

typedef BackgroundBufferBackup::BufferBackupHeader Header;
static_assert(sizeof(Header) == 32, "one flash word");
static_assert(offsetof(Header, tag) == 0, "tag");
static_assert(offsetof(Header, write_ptr) == 4, "write_ptr");
static_assert(offsetof(Header, media) == 8, "media");
static_assert(offsetof(Header, mono) == 9, "mono");
static_assert(offsetof(Header, crc) == 12, "crc");
static_assert(offsetof(Header, crc_tag) == 16, "crc tag");
static_assert(offsetof(Header, reserved) == 20, "reserved");

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
    for (int i = 0; i < 8; ++i) crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
  }
  return ~crc;
}

void Fill(uint8_t seed) {
  for (size_t i = 0; i < kAudio; ++i) processor.data_[i] = uint8_t(i * 7 + seed + (i >> 9));
}

bool Restored(uint8_t seed) {
  for (size_t i = 0; i < kAudio; ++i) {
    if (processor.data_[i] != uint8_t(i * 7 + seed + (i >> 9))) return false;
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
  bool Freeze(int save_every = 0) {
    for (int i = 0; i < 10000; ++i) system_clock.Tick();
    backup.Process(true);
    for (int i = 1; backup.busy(); ++i) {
      if (test_budget == 0) return false;
      if (save_every && i % save_every == 0) settings.SaveState();
      backup.Process(true);
    }
    return test_budget != 0;
  }
};

void Wipe() {
  memset(kFlash, 255, 0x100000);
  test_ops = 0;
}

void Save(Processor::Media media, bool mono, int write_ptr, uint8_t seed) {
  Rig rig;
  rig.Boot();
  processor.Configure(media, mono);
  processor.write_ptr_ = write_ptr;
  Fill(seed);
  CHECK(rig.Freeze());
  CHECK(rig.settings.state().frozen);
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
  for (int i = 10; i < 32; ++i) kFlash[i] = uint8_t(0xa5 ^ (i * 29));
  kHeader->tag = 0x5a5a5246;
  kHeader->write_ptr = write_ptr;
  kHeader->media = media;
  kHeader->mono = mono;
}

void TestStock() {
  test_checker.Start("TestStock");
  WriteStock(2, 0, 12345, 9);
  Rig rig;
  CHECK(rig.Boot());
  CHECK(processor.media_ == 2 && !processor.mono_ && processor.write_ptr_ == 12345);
  CHECK(Restored(9));
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
  const size_t offsets[] = { 32, 32 + 12345, 32 + kAudio - 1, 4, 8, 9 };
  for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
    Wipe();
    Save(Processor::MEDIA_DIGITAL, false, 100, 5);
    kFlash[offsets[i]] ^= 1;  // Header fields stay in range: only the CRC sees it.
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
    rig.Freeze();
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

void TestFlashErrors() {
  test_checker.Start("TestFlashErrors");
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
    rig.Freeze();
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
    CHECK(after.Freeze());
    Rig again;
    CHECK(again.Boot() && Restored(4));
    ++checked;
  }
  printf("PASS flash errors at %d points abandon the backup cleanly\n", checked);
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
  for (int i = 0; i < 10; ++i) r.backup.Process(true);
  CHECK(r.backup.remaining_bytes_ > 0 && r.backup.write_offset_ > 0);
  r.backup.Process(false);
  CHECK(!r.backup.busy() && kHeader->tag == 0xffffffff);
  for (size_t i = 0; i < sizeof(Header); ++i) CHECK(kFlash[i] == 255);
  CHECK(!r.settings.state().frozen);
  CHECK(r.Freeze());
  CHECK(kHeader->tag == 0x5a5a5246 && r.settings.state().frozen);
  Rig after;
  CHECK(after.Boot() && Restored(17));
}

void TestPowerLoss() {
  test_checker.Start("TestPowerLoss");
  for (int i = 0; i < 4; ++i) CheckPowerLoss(i & 1, i & 2);
}

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) return 1;
  TestFreezeDeadlineAndCancel();
  TestFormat();
  TestStock();
  TestCorruption();
  TestPowerLoss();
  TestFlashErrors();
  TestSettingsSaves();
  return test_checker.failures() ? 1 : 0;
}
