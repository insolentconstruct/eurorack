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

#include <signal.h>
#include <unistd.h>
#include <vector>
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
int test_ecc_read = 0;
int test_masked_reads = 0;
int test_fail_programs = 0;
int test_silent_programs = 0;
uint32_t test_faultmask = 0;
uint32_t test_sr2 = 0;
int test_fail_sector = -1;
int test_fail_erases = 0;
int test_silent_erases = 0;
int test_erase_calls = 0;
uint32_t test_erased = 0;
int test_programs = 0;
PWR_TypeDef test_pwr;
bool test_supply_low = false;
int test_low_supply_writes = 0;

using namespace beads;

const char* test_name = "main";
int test_failures = 0;
int test_reruns = 0;

bool Check(bool condition, const char* message) {
  if (!condition) {
    printf("ERROR: %s: %s\n", test_name, message);
    ++test_failures;
  }
  return condition;
}

#define CHECK(condition) Check((condition), #condition)

uint8_t* const kBank2 = reinterpret_cast<uint8_t*>(0x08100000);
const size_t kBufferSize = 6 * 0x20000;
const size_t kSettingsOffset = 7 * 0x20000;

int test_cut_at = 0;
int test_cut_model = 0;
int test_operations = 0;
int test_low_after = 0;
int test_lose_after = 0;  // After this operation, a calibration goes bad.
int test_lose_sector = 0;
int test_sector_programs[8];
int test_damage_programs = 0;
bool damaged[256];
bool opened[256];
std::vector<bool> operations;

void TestProtect() {
  for (int i = 0; i < 256; ++i) {
    if (opened[i]) {
      mprotect(kBank2 + i * 4096, 4096, PROT_NONE);
      opened[i] = false;
    }
  }
}

void ClearDamage() {
  mprotect(kBank2, 0x100000, PROT_READ | PROT_WRITE);
  memset(damaged, 0, sizeof(damaged));
  memset(opened, 0, sizeof(opened));
}

void Damage(uint32_t address, size_t size) {
  for (size_t i = 0; i < size; i += 4096) {
    int page = (address + i - 0x08100000) / 4096;
    damaged[page] = true;
    mprotect(kBank2 + page * 4096, 4096, PROT_NONE);
  }
}

void Segv(int, siginfo_t* info, void*) {
  uintptr_t address = reinterpret_cast<uintptr_t>(info->si_addr);
  if (address >= 0x08100000 && address < 0x08200000) {
    int page = (address - 0x08100000) / 4096;
    if (damaged[page] && test_faultmask &&
        (test_scb.CCR & SCB_CCR_BFHFNMIGN_Msk)) {
      test_sr2 |= FLASH_FLAG_DBECCERR_BANK2;
      mprotect(kBank2 + page * 4096, 4096, PROT_READ);
      opened[page] = true;
      return;
    }
  }
  const char message[] = "ERROR: unmasked flash fault\n";
  const ssize_t written = write(2, message, sizeof(message) - 1);
  (void)written;
  _exit(1);
}

bool Operation(bool erase) {
  operations.push_back(erase);
  ++test_operations;
  return test_cut_at && test_operations == test_cut_at;
}

void AfterOperation(bool cut) {
  if (test_low_after && test_operations == test_low_after) {
    test_supply_low = true;
  }
  if (test_lose_after && test_operations == test_lose_after) {
    kBank2[test_lose_sector * 0x20000 + 6] ^= 1;
  }
  if (cut) {
    throw TestPowerCut();
  }
}

void TestErase(uint32_t address, size_t size) {
  bool cut = Operation(true);
  int model = cut ? test_cut_model : 1;
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  if (model == 1 || model == 2 || model == 4) {
    size_t length = model == 1 ? size : size / 2;
    mprotect(dst, length, PROT_READ | PROT_WRITE);
    for (size_t i = 0; i < length; i += 4096) {
      int page = (address + i - 0x08100000) / 4096;
      damaged[page] = opened[page] = false;
    }
    memset(dst, 255, length);
  }
  if (model == 3 || model == 4) {
    Damage(address + (model == 4 ? size / 2 : 0),
        model == 4 ? size / 2 : size);
  }
  AfterOperation(cut);
}

void TestProgram(uint32_t address, uint64_t source) {
  if (damaged[(address - 0x08100000) / 4096]) {
    ++test_damage_programs;
    CHECK(false);
    return;
  }
  bool cut = Operation(false);
  int model = cut ? test_cut_model : 1;
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  const uint8_t* src = reinterpret_cast<const uint8_t*>(uintptr_t(source));
  if (model == 1 || model == 2) {
    for (int i = 0; i < (model == 1 ? 32 : 16); ++i) {
      dst[i] &= src[i];
    }
  } else if (model == 3) {
    Damage(address & ~4095U, 4096);
  }
  AfterOperation(cut);
}

void Reset() {
  test_locked = true;
  test_supply_low = false;
  test_pwr.CR1 = 0;
  test_low_supply_writes = 0;
  test_cut_at = test_cut_model = test_operations = test_low_after = 0;
  test_lose_after = test_lose_sector = 0;
  test_damage_programs = 0;
  memset(test_sector_programs, 0, sizeof(test_sector_programs));
  operations.clear();
  test_ecc_error = false;
  test_ecc_read = 0;
  test_masked_reads = 0;
  test_fail_programs = 0;
  test_silent_programs = 0;
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
  ClearDamage();
  Reset();
  memset(kBank2, 255, 0x100000);
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

bool MirrorMatches() {
  SettingsRecovery recovery;
  PersistentData mirror;
  PersistentData settings;
  if (!recovery.Usable(6, &mirror) || !recovery.Usable(7, &settings) ||
      memcmp(mirror.adc_calibration, settings.adc_calibration,
          sizeof(mirror.adc_calibration))) {
    return false;
  }
  for (size_t i = 0; i < sizeof(mirror.padding); ++i) {
    if (mirror.padding[i]) {
      return false;
    }
  }
  return recovery.IsBlank(0x081c0080, 0x20000 - 128);
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
  CHECK(MirrorMatches());
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
    CHECK(MirrorMatches());
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
    Prepare(1.1f, 0.9f);
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
  Prepare(1.1f, 0.9f);
  test_ecc_error = true;
  SettingsRecovery recovery;
  PersistentData data;
  CHECK(!recovery.ReadCalibration(kSettingsSector, &data));
  CHECK(!recovery.IsBlank(0x08100000 + kSettingsOffset + 0x10000, 32));
  CHECK(test_faultmask == 0 && test_scb.CCR == 0);
  test_ecc_error = false;
  CHECK(recovery.ReadCalibration(kSettingsSector, &data));
  CHECK(recovery.IsBlank(0x08100000 + kSettingsOffset + 0x10000, 32));

  // The flag is cleared, or the next erase would fail on it.
  test_ecc_error = true;
  CHECK(!recovery.ReadCalibration(kSettingsSector, &data));
  test_ecc_error = false;
  CHECK(!test_sr2);
  test_erase_calls = 0;
  CHECK(recovery.EraseSector(0));
  CHECK(test_erase_calls == 1);

  // A flag left by an earlier read affects neither a read nor an erase.
  test_sr2 = FLASH_FLAG_DBECCERR_BANK2 | FLASH_FLAG_SNECCERR_BANK2;
  CHECK(recovery.ReadCalibration(kSettingsSector, &data));
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
  Prepare(1.1f, 0.9f);
  test_fail_erases = 1;
  CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);

  // An erase that reports success but leaves the data is retried.
  Prepare(1.1f, 0.9f);
  test_silent_erases = 1;
  CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);
  CHECK(IsErased(0, kBufferSize));
  CHECK(MirrorMatches());
  Prepare(1.1f, 0.9f);
  test_silent_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);

  // Two failures of the first buffer sector.
  Prepare(1.1f, 0.9f);
  test_fail_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(IsErased(0x20000, kBufferSize - 0x20000));
  CHECK(MirrorMatches());

  // The settings sector is never written if it was not erased.
  Prepare(1.1f, 0.9f);
  test_fail_sector = kSettingsSector;
  test_fail_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(IsErased(0, kBufferSize));
  CHECK(MirrorMatches());
  CHECK(test_sector_programs[7] == 0);
  SettingsRecovery recovery;
  PersistentData kept;
  PersistentData expected;
  State state;
  SettingsRecovery::LoadDefaults(&expected, &state);
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    expected.adc_calibration[i].scale *= 1.1f;
    expected.adc_calibration[i].offset *= 0.9f;
  }
  CHECK((recovery.Usable(7, &kept) &&
      !memcmp(kept.adc_calibration, expected.adc_calibration,
          sizeof(kept.adc_calibration))) ||
      (recovery.Usable(6, &kept) &&
      !memcmp(kept.adc_calibration, expected.adc_calibration,
          sizeof(kept.adc_calibration))));

  // Nor programmed if an erase reports success but leaves data.
  Prepare(1.1f, 0.9f);
  test_fail_sector = kSettingsSector;
  test_silent_erases = 2;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(test_sector_programs[7] == 0);
}

void TestProgramErrors() {
  test_name = "TestProgramErrors";
  Prepare(1.1f, 0.9f);
  test_fail_programs = 1;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  Prepare(1.1f, 0.9f);
  test_silent_programs = 1;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  Prepare(1.1f, 0.9f);
  PersistentData original;
  SettingsRecovery recovery;
  CHECK(recovery.Usable(7, &original));
  test_fail_programs = 100;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  PersistentData kept;
  CHECK(recovery.Usable(7, &kept));
  CHECK(!memcmp(original.adc_calibration, kept.adc_calibration,
      sizeof(kept.adc_calibration)));
}

// One double ECC error at each masked read in turn: recovery fails, or
// leaves a sector the firmware loads with either calibration.
void TestSingleEccError() {
  test_name = "TestSingleEccError";
  Prepare(1.1f, 1.1f);
  Settings original;
  CHECK(Loads(&original));
  Prepare(1.1f, 1.1f);
  CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);
  const int reads = test_masked_reads;
  Settings defaults;
  LoadDefaults(&defaults);
  int resets = 0;
  for (int read = 1; read <= reads; ++read) {
    Prepare(1.1f, 1.1f);
    test_ecc_read = read;
    ++test_reruns;
    const RecoveryResult result = RunRecovery();
    if (result == RECOVERY_FAILED) {
      continue;
    }
    Settings loaded;
    const Settings& expected = result == RECOVERY_CALIBRATION_KEPT
        ? original : defaults;
    resets += result == RECOVERY_CALIBRATION_RESET;
    if (!CHECK(Loads(&loaded) && !memcmp(
        loaded.persistent_data().adc_calibration,
        expected.persistent_data().adc_calibration,
        sizeof(expected.persistent_data().adc_calibration)) &&
        SameState(loaded.state(), defaults.state()))) {
      printf("  at masked read %d of %d\n", read, reads);
      break;
    }
  }
  CHECK(resets > 0);
}

bool SameCalibration(const PersistentData& a, const PersistentData& b) {
  return !memcmp(a.adc_calibration, b.adc_calibration,
      sizeof(a.adc_calibration));
}

void PlaceChunk(uint32_t address, uint32_t tag, const void* payload,
    uint16_t size) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  uint16_t sum = 0;
  const uint8_t* bytes = static_cast<const uint8_t*>(payload);
  for (int i = 0; i < size; ++i) {
    sum += bytes[i];
  }
  sum ^= 0xffff;
  memcpy(dst, &tag, 4);
  memcpy(dst + 4, &size, 2);
  memcpy(dst + 6, &sum, 2);
  memcpy(dst + 8, payload, size);
}

void PlaceCalibration(int sector, const PersistentData& data) {
  CHECK(sizeof(data) == 120);
  PlaceChunk(0x08100000 + sector * 0x20000, 0x494c4143,
      &data, sizeof(data));
}

void Fixture(int scenario) {
  Prepare(1.1f, 0.9f);
  Settings saved;
  CHECK(Loads(&saved));
  saved.mutable_state()->media = 2;
  saved.SaveState();
  PersistentData good;
  memcpy(&good, kBank2 + kSettingsOffset + 8, sizeof(good));
  if (scenario >= 2 && scenario <= 6) {
    PlaceCalibration(6, good);
  }
  if (scenario == 3 || scenario == 4) {
    uint8_t options[56];
    memset(options, 0x5a, sizeof(options));
    for (int i = 0; i < 3; ++i) {
      PlaceChunk(0x081c0080 + 64 * i, 0x4c5a5244,
          options, sizeof(options));
    }
  }
  if (scenario == 4) {
    kBank2[kSettingsOffset + 6] ^= 1;
  } else if (scenario == 5) {
    PersistentData factory;
    State state;
    SettingsRecovery::LoadDefaults(&factory, &state);
    PlaceCalibration(7, factory);
  } else if (scenario == 6) {
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      good.adc_calibration[i].scale *= 1.5f;
    }
    PlaceCalibration(7, good);
  } else if (scenario == 7) {
    memset(kBank2 + 6 * 0x20000, 0x5a, 0x20000);
  } else if (scenario == 8) {
    Damage(0x081c0000, 4096);
  } else if (scenario == 9) {
    PlaceCalibration(6, good);
    memset(kBank2 + kSettingsOffset, 255, 0x20000);
  } else if (scenario == 10) {
    memset(kBank2 + kSettingsOffset, 0x5a, 0x20000);
  } else if (scenario == 11) {
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      good.adc_calibration[i].offset *= 1.1f;
    }
    PlaceCalibration(6, good);
  }
  Reset();
}

void Invariant(bool bootstrap, const PersistentData& kept) {
  SettingsRecovery recovery;
  PersistentData data;
  CHECK(bootstrap ||
      (recovery.Usable(6, &data) && SameCalibration(data, kept)) ||
      (recovery.Usable(7, &data) && SameCalibration(data, kept)));
  CHECK(test_damage_programs == 0);
  CHECK(test_low_supply_writes == 0);
}

void FinalLayout(const PersistentData& kept) {
  SettingsRecovery recovery;
  PersistentData data;
  State state;
  SettingsRecovery::LoadDefaults(&data, &state);
  memcpy(data.adc_calibration, kept.adc_calibration,
      sizeof(data.adc_calibration));
  CHECK(IsErased(0, kBufferSize));
  CHECK(MirrorMatches());
  for (int sector = 6; sector <= 7; ++sector) {
    uint8_t expected[256];
    memset(expected, 255, sizeof(expected));
    // Build the expected chunk independently of recovery's writer.
    uint32_t address = 0x08100000 + sector * 0x20000;
    uint16_t size = sizeof(data);
    uint16_t sum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
    for (size_t i = 0; i < sizeof(data); ++i) {
      sum += bytes[i];
    }
    sum ^= 0xffff;
    uint32_t tag = 0x494c4143;
    memcpy(expected, &tag, 4);
    memcpy(expected + 4, &size, 2);
    memcpy(expected + 6, &sum, 2);
    memcpy(expected + 8, &data, sizeof(data));
    CHECK(!memcmp(reinterpret_cast<void*>(uintptr_t(address)),
        expected, 128));
    if (sector == 7) {
      memset(expected, 255, sizeof(expected));
      size = sizeof(state);
      sum = 0;
      bytes = reinterpret_cast<const uint8_t*>(&state);
      for (size_t i = 0; i < sizeof(state); ++i) {
        sum += bytes[i];
      }
      sum ^= 0xffff;
      tag = State::tag;
      memcpy(expected, &tag, 4);
      memcpy(expected + 4, &size, 2);
      memcpy(expected + 6, &sum, 2);
      memcpy(expected + 8, &state, sizeof(state));
      size_t used = (8 + sizeof(state) + 31) & ~31;
      CHECK(!memcmp(reinterpret_cast<void*>(uintptr_t(address + 128)),
          expected, used));
      CHECK(recovery.IsBlank(address + 128 + used,
          0x20000 - 128 - used));
    }
  }
  CHECK(test_damage_programs == 0);
  CHECK(test_low_supply_writes == 0);
  Settings loaded;
  CHECK(Loads(&loaded));
  CHECK(SameCalibration(loaded.persistent_data(), kept));
}

bool Cut(int at, int model) {
  test_cut_at = at;
  test_cut_model = model;
  try {
    RunRecovery();
  } catch (const TestPowerCut&) {
    return true;
  }
  return false;
}

void TestPowerCuts() {
  for (int scenario = 1; scenario <= 11; ++scenario) {
    char name[40];
    snprintf(name, sizeof(name), "PowerCuts S%d", scenario);
    test_name = name;
    Fixture(scenario);
    SettingsRecovery recovery;
    RecoveryResult result = recovery.Run();
    CHECK(result == (scenario == 10 ? RECOVERY_CALIBRATION_RESET :
        RECOVERY_CALIBRATION_KEPT));
    PersistentData kept = recovery.persistent_data();
    // Verify the selected calibration independently from Run's result.
    PersistentData expected;
    State state;
    SettingsRecovery::LoadDefaults(&expected, &state);
    if (scenario != 10) {
      for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
        expected.adc_calibration[i].scale *= 1.1f;
        expected.adc_calibration[i].offset *= 0.9f;
      }
    }
    CHECK(SameCalibration(kept, expected));
    const int n = test_operations;
    const std::vector<bool> kinds = operations;
    FinalLayout(kept);
    int cuts = 0;
    int reruns = 0;
    for (int k = 1; k <= n; ++k) {
      const int models = kinds[k - 1] ? 5 : 4;
      for (int model = 0; model < models; ++model) {
        Fixture(scenario);
        CHECK(Cut(k, model));
        ++cuts;
        Invariant(scenario == 10, kept);
        Reset();
        CHECK(RunRecovery() == result);
        ++reruns;
        FinalLayout(kept);
      }
      for (int model = 0; model <= 2; model += 2) {
        for (int k2 = 1; k2 <= n; ++k2) {
          for (int model2 = 0; model2 <= 2; model2 += 2) {
            Fixture(scenario);
            CHECK(Cut(k, model));
            ++cuts;
            Invariant(scenario == 10, kept);
            Reset();
            CHECK(Cut(k2, model2));
            ++cuts;
            ++reruns;
            Invariant(scenario == 10, kept);
            Reset();
            CHECK(RunRecovery() == result);
            ++reruns;
            FinalLayout(kept);
          }
        }
      }
    }
    printf("%s: ops=%d cuts=%d reruns=%d\n", name, n, cuts, reruns);
  }
  test_name = "main";
}

void TestFactoryStorage() {
  test_name = "TestFactoryStorage";
  Prepare(1.0f, 1.0f);
  PersistentData original;
  SettingsRecovery recovery;
  CHECK(recovery.Usable(7, &original));
  CHECK(recovery.Run() == RECOVERY_CALIBRATION_RESET);
  FinalLayout(original);
}

void TestLowSupply() {
  test_name = "TestLowSupply";
  Fixture(1);
  PersistentData kept;
  SettingsRecovery recovery;
  CHECK(recovery.Usable(7, &kept));
  static uint8_t before[0x100000];
  memcpy(before, kBank2, sizeof(before));
  test_supply_low = true;
  CHECK(RunRecovery() == RECOVERY_FAILED);
  CHECK(test_low_supply_writes == 0);
  CHECK(test_erase_calls == 0 && test_programs == 0);
  CHECK(!memcmp(before, kBank2, sizeof(before)));
  Reset();
  CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);
  FinalLayout(kept);
  int reruns = 1;
  const int points[] = { 1, 6, 7, 9, 12, 16 };
  for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
    Fixture(1);
    test_low_after = points[i];
    CHECK(RunRecovery() == RECOVERY_FAILED);
    Invariant(false, kept);
    Reset();
    CHECK(RunRecovery() == RECOVERY_CALIBRATION_KEPT);
    ++reruns;
    FinalLayout(kept);
  }
  printf("TestLowSupply: cuts=0 reruns=%d\n", reruns);
}

void RunTest(void (*test)()) {
  test_reruns = 0;
  test();
  printf("%s: cuts=0 reruns=%d\n", test_name, test_reruns);
}

// The copy that should stay goes bad after the buffer erases: the other
// sector is never erased.
void TestOtherCopyLost() {
  test_name = "TestOtherCopyLost";
  const int scenarios[] = { 2, 5 };
  for (int i = 0; i < 2; ++i) {
    Fixture(scenarios[i]);
    const int target = scenarios[i] == 5 ? 7 : 6;
    static uint8_t before[0x20000];
    memcpy(before, kBank2 + target * 0x20000, sizeof(before));
    test_lose_after = 6;
    test_lose_sector = 13 - target;
    CHECK(RunRecovery() == RECOVERY_FAILED);
    CHECK(test_operations == 6);
    CHECK(!memcmp(before, kBank2 + target * 0x20000, sizeof(before)));
    PersistentData kept;
    SettingsRecovery recovery;
    CHECK(recovery.Usable(target, &kept));
  }
}

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = Segv;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  CHECK(sigaction(SIGSEGV, &action, NULL) == 0);
  ClearDamage();
  memset(kBank2, 255, 0x100000);
  RunTest(TestDefaults);
  RunTest(TestKeepsCalibration);
  RunTest(TestResetsCalibration);
  RunTest(TestCorruptSettings);
  RunTest(TestEccError);
  RunTest(TestEraseErrors);
  RunTest(TestProgramErrors);
  RunTest(TestSingleEccError);
  RunTest(TestFactoryStorage);
  RunTest(TestOtherCopyLost);
  TestLowSupply();
  TestPowerCuts();
  return test_failures ? 1 : 0;
}
