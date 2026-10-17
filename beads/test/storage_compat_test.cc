// Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
// The same program links against either the base or the original stock reader.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include "stmlib/system/system_clock.h"
#include "beads/background_buffer_backup.h"
#include "beads/settings.h"
#include "beads/dsp/processor.h"

bool test_locked = true;
int test_budget = -1;
int test_fail_at = 0;
int test_ops = 0;
int test_erases = 0;
bool test_half_erase = false;
namespace stmlib { SystemClock system_clock; }
using namespace beads;
using namespace stmlib;
static Processor processor;

int main(int argc, char** argv) {
  if (argc != 5) return 2;
  const bool save = strcmp(argv[1], "save") == 0;
  const int media = atoi(argv[3]);
  const int mono = atoi(argv[4]);
  void* flash = mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (flash == MAP_FAILED) return 3;
  memset(flash, 255, 0x100000);
  if (!save) {
    FILE* f = fopen(argv[2], "rb");
    if (!f || fread(flash, 1, 0x100000, f) != 0x100000) return 4;
    fclose(f);
  }
  system_clock.Init();
  Settings settings;
  settings.Init(false);
  BackgroundBufferBackup backup;
  backup.Init(&processor, &settings);
  const bool loaded = backup.Load();
  const int write_ptr = 1234 + media * 2 + mono;
  const size_t size = kDelayBufferSizeBits / 8;
  if (save) {
    processor.Configure(Processor::Media(media), mono);
    processor.write_ptr_ = write_ptr;
    for (size_t i = 0; i < size; ++i) processor.data_[i] = uint8_t(i * 7 + media * 2 + mono);
    settings.mutable_state()->media = media;
    settings.mutable_state()->gain = 37;
    settings.mutable_state()->assignable_cv_amount[1] = 79;
    settings.SaveState();
    for (int i = 0; i < 10000; ++i) system_clock.Tick();
    backup.Process(true);
    while (backup.busy()) backup.Process(true);
    if (!settings.state().frozen) return 5;
    FILE* f = fopen(argv[2], "wb");
    if (!f || fwrite(flash, 1, 0x100000, f) != 0x100000) return 6;
    fclose(f);
  } else {
    if (!loaded || processor.media_ != media || processor.mono_ != bool(mono) ||
        processor.write_ptr_ != write_ptr || settings.state().gain != 37 ||
        settings.state().media != media || settings.state().assignable_cv_amount[1] != 79) return 7;
    for (size_t i = 0; i < size; ++i) {
      if (processor.data_[i] != uint8_t(i * 7 + media * 2 + mono)) return 8;
    }
  }
  return 0;
}
