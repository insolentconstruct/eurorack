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
// Recovery firmware tests: the rebuilt sector must load in the firmware.

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#include "beads/recovery/settings_recovery.h"
#include "beads/settings.h"

SCB_Type test_scb;
bool test_locked = true;
bool test_ecc_error = false;
uint32_t test_faultmask = 0;
uint32_t test_sr2 = 0;
int test_fail_sector = -1;
int test_fail_erases = 0;
int test_silent_erases = 0;
int test_erase_calls = 0;
uint32_t test_erased = 0;
int test_programs = 0;

using namespace beads;

const char* test_name = "main";
int test_failures = 0;

bool Check(bool condition, const char* message) {
  if (!condition) {
    printf("ERROR: %s: %s\n", test_name, message);
    ++test_failures;
  }
  return condition;
}

#define CHECK(condition) Check((condition), #condition)

uint8_t* const kBank2 = reinterpret_cast<uint8_t*>(0x08100000);
const size_t kBufferSize = 7 * 0x20000;
const size_t kSettingsOffset = 7 * 0x20000;

void Reset() {
  test_ecc_error = false;
  test_faultmask = 0;
  test_scb.CCR = 0;
  test_sr2 = 0;
  test_fail_sector = -1;
  test_fail_erases = 0;
  test_silent_erases = 0;
  test_erase_calls = 0;
  test_erased = 0;
  test_programs = 0;
}

// A frozen buffer and settings as the firmware saved them, with the
// calibration scaled by the given factors.
void Prepare(float scale, float offset) {
  memset(kBank2, 0x5a, kBufferSize);
  memset(kBank2 + kSettingsOffset, 255, 0x20000);
  Settings saved;
  saved.Init(false);
  PersistentData* data = saved.mutable_persistent_data();
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    data->adc_calibration[i].scale *= scale;
    data->adc_calibration[i].offset *= offset;
  }
  saved.SavePersistentData();
  saved.mutable_state()->frozen = 1;
  saved.mutable_state()->media = 1;
  saved.mutable_state()->monophonic_mode = 1;
  saved.SaveState();
  Reset();
}

bool IsErased(size_t offset, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (kBank2[offset + i] != 255) {
      return false;
    }
  }
  return true;
}

// Loads the result the way the module firmware does at startup.
bool Loads(Settings* loaded) {
  const int programs = test_programs;
  return loaded->Init(false) && test_programs == programs;
}

// The firmware's defaults, from a blank sector.
void LoadDefaults(Settings* defaults) {
  static uint8_t sector[0x20000];
  memcpy(sector, kBank2 + kSettingsOffset, sizeof(sector));
  memset(kBank2 + kSettingsOffset, 255, sizeof(sector));
  defaults->Init(false);
  memcpy(kBank2 + kSettingsOffset, sector, sizeof(sector));
}

// Settings::Init leaves the padding as it was: compare the named fields.
bool SameState(const State& a, const State& b) {
  return !memcmp(&a, &b, offsetof(State, padding));
}

RecoveryResult RunRecovery() {
  SettingsRecovery recovery;
  return recovery.Run();
}

void TestDefaults() {
  test_name = "TestDefaults";
  PersistentData data;
  State state;
  memset(&data, 0x5a, sizeof(data));
  memset(&state, 0x5a, sizeof(state));
  SettingsRecovery::LoadDefaults(&data, &state);
  Settings defaults;
  LoadDefaults(&defaults);
  CHECK(!memcmp(
      data.adc_calibration,
      defaults.persistent_data().adc_calibration,
      sizeof(data.adc_calibration)));
  CHECK(SameState(state, defaults.state()));
  for (size_t i = 0; i < sizeof(data.padding); ++i) {
    CHECK(data.padding[i] == 0);
  }
  for (size_t i = 0; i < sizeof(state.padding); ++i) {
    CHECK(state.padding[i] == 0);
  }
}

void TestKeepsCalibration() {
  test_name = "TestKeepsCalibration";
  Prepare(1.2f, 0.8f);
  CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);
  CHECK(IsErased(0, kBufferSize));
  // Bank 2 sector numbers for the recovery's own erases.
  CHECK((test_erased & 0xff) == 0xff);
  CHECK(test_locked);
  Settings loaded;
  Settings defaults;
  CHECK(Loads(&loaded));
  LoadDefaults(&defaults);
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    const AdcCalibrationData& c = loaded.persistent_data().adc_calibration[i];
    const AdcCalibrationData& d = defaults.persistent_data().adc_calibration[i];
    CHECK(c.scale == d.scale * 1.2f);
    CHECK(c.offset == d.offset * 0.8f);
  }
  CHECK(SameState(loaded.state(), defaults.state()));
  CHECK(loaded.state().frozen == 0);
}

void TestResetsCalibration() {
  const float outliers[][2] = {
    { 1.3f, 1.0f },
    { 1.0f, 0.7f },
    { 1.0f, NAN },
    { -1.0f, 1.0f },
  };
  test_name = "TestResetsCalibration";
  for (size_t i = 0; i < sizeof(outliers) / sizeof(outliers[0]); ++i) {
    Prepare(outliers[i][0], outliers[i][1]);
    CHECK(RunRecovery() == RECOVERY_CALIBRATION_RESET);
    CHECK(IsErased(0, kBufferSize));
    Settings loaded;
    Settings defaults;
    CHECK(Loads(&loaded));
    LoadDefaults(&defaults);
    CHECK(!memcmp(
        loaded.persistent_data().adc_calibration,
        defaults.persistent_data().adc_calibration,
        sizeof(defaults.persistent_data().adc_calibration)));
    CHECK(SameState(loaded.state(), defaults.state()));
  }
}

void TestCorruptSettings() {
  test_name = "TestCorruptSettings";
  // A bad checksum, a bad tag, and garbage.
  const size_t offsets[] = { 40, 0, 0 };
  for (int i = 0; i < 3; ++i) {
    Prepare(1.0f, 1.0f);
    if (i == 2) {
      memset(kBank2 + kSettingsOffset, 0x5a, 0x20000);
    } else {
      kBank2[kSettingsOffset + offsets[i]] ^= 0x10;
    }
    CHECK(RunRecovery() == RECOVERY_CALIBRATION_RESET);
    Settings loaded;
    CHECK(Loads(&loaded));
  }
}

void TestEccError() {
  test_name = "TestEccError";
  Prepare(1.0f, 1.0f);
  test_ecc_error = true;
  SettingsRecovery recovery;
  PersistentData data;
  CHECK(!recovery.ReadCalibration(&data));
  CHECK(!recovery.IsBlank(0x08100000 + kSettingsOffset + 0x10000, 32));
  CHECK(test_faultmask == 0 && test_scb.CCR == 0);
  test_ecc_error = false;
  CHECK(recovery.ReadCalibration(&data));
  CHECK(recovery.IsBlank(0x08100000 + kSettingsOffset + 0x10000, 32));

  // The flag is cleared, or the next erase would fail on it.
  test_ecc_error = true;
  CHECK(!recovery.ReadCalibration(&data));
  test_ecc_error = false;
  CHECK(!test_sr2);
  test_erase_calls = 0;
  CHECK(recovery.EraseSector(0));
  CHECK(test_erase_calls == 1);

  // A flag left by an earlier read affects neither a read nor an erase.
  test_sr2 = FLASH_FLAG_DBECCERR_BANK2 | FLASH_FLAG_SNECCERR_BANK2;
  CHECK(recovery.ReadCalibration(&data));
  test_sr2 = FLASH_FLAG_SNECCERR_BANK2;
  test_erase_calls = 0;
  CHECK(recovery.EraseSector(0));
  CHECK(test_erase_calls == 1);

  // An error that an erase does not clear: the settings are never loaded.
  test_ecc_error = true;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(test_programs == 0);
}

void TestEraseErrors() {
  test_name = "TestEraseErrors";
  Prepare(1.0f, 1.0f);
  test_fail_erases = 1;
  CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);

  // An erase that reports success but leaves the data is retried.
  Prepare(1.0f, 1.0f);
  test_silent_erases = 1;
  CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);
  CHECK(IsErased(0, kBufferSize));
  Prepare(1.0f, 1.0f);
  test_silent_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);

  // Two failures of the first buffer sector.
  Prepare(1.0f, 1.0f);
  test_fail_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(IsErased(0x20000, kBufferSize - 0x20000));

  // The settings sector is never written if it was not erased.
  Prepare(1.0f, 1.0f);
  test_fail_sector = kSettingsSector;
  test_fail_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(IsErased(0, kBufferSize));
  CHECK(test_programs == 0);

  // Nor read back if it was erased but not blank.
  Prepare(1.0f, 1.0f);
  test_fail_sector = kSettingsSector;
  test_silent_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(test_programs == 0);
}

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  TestDefaults();
  TestKeepsCalibration();
  TestResetsCalibration();
  TestCorruptSettings();
  TestEccError();
  TestEraseErrors();
  return test_failures ? 1 : 0;
}
