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
// Rebuilds the bank 2 storage without trusting it.

#include "beads/recovery/settings_recovery.h"

#include <stm32h7xx_hal.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace beads {

using namespace std;

// stmlib::ChunkStorage's layout: chunks of 32-byte flash words, each starting
// with this header; the calibration first, then the states. The mirror holds
// the calibration, then Drizzlet's options.
struct ChunkHeader {
  uint32_t tag;
  uint16_t size;
  uint16_t checksum;
};

const size_t kFlashWordSize = 32;
const size_t kCalibrationChunkSize = (sizeof(ChunkHeader) +
    sizeof(PersistentData) + kFlashWordSize - 1) & ~(kFlashWordSize - 1);
const size_t kStateChunkSize = (sizeof(ChunkHeader) +
    sizeof(State) + kFlashWordSize - 1) & ~(kFlashWordSize - 1);
const size_t kMaxChunkSize = sizeof(ChunkHeader) + sizeof(PersistentData);
const size_t kBlankCheckWords = 64;

struct ChunkLayoutAssertions {
  STATIC_ASSERT(sizeof(ChunkHeader) == 8, chunk_header_size);
  STATIC_ASSERT(sizeof(PersistentData) % 4 == 0, calibration_size);
  STATIC_ASSERT(sizeof(State) % 4 == 0, state_size);
  STATIC_ASSERT(sizeof(State) <= sizeof(PersistentData), state_fits);
  STATIC_ASSERT(kMaxChunkSize == kCalibrationChunkSize, calibration_words);
};

// As in stmlib's ChunkStorage, with the bytes unsigned as on the module.
static uint16_t Checksum(const void* data, size_t size) {
  uint16_t sum = 0;
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  while (size--) {
    sum += *bytes++;
  }
  return sum ^ 0xffff;
}

static void ClearErrors() {
  __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_ALL_ERRORS_BANK2);
}

static uint32_t SectorStart(uint32_t sector) {
  return kBank2Start + sector * kBank2SectorSize;
}

RecoveryResult SettingsRecovery::Run() {
  // PVD level 6: 2.69-2.83 V falling.
  MODIFY_REG(PWR->CR1, PWR_CR1_PLS, PWR_CR1_PLS_LEV6);
  SET_BIT(PWR->CR1, PWR_CR1_PVDEN);

  PersistentData settings;
  PersistentData mirror;
  const bool settings_usable = Usable(kSettingsSector, &settings);
  const bool mirror_usable = Usable(kMirrorSector, &mirror);

  // As the firmware chooses: the settings sector's, unless it holds the
  // factory values and the mirror a calibration of its own.
  const bool use_mirror = mirror_usable && (!settings_usable || \
      (IsFactory(settings) && !IsFactory(mirror)));
  LoadDefaults(&persistent_data_, &state_);
  if (use_mirror || settings_usable) {
    const PersistentData& kept = use_mirror ? mirror : settings;
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      persistent_data_.adc_calibration[i] = kept.adc_calibration[i];
    }
  }

  bool success = true;
  for (uint32_t s = kFirstBufferSector; s <= kLastBufferSector; ++s) {
    success = EraseSector(s) && success;
  }

  // The sector holding the calibration kept is rewritten last. With none,
  // the mirror goes first, as in the firmware.
  const bool bootstrap = !settings_usable && !mirror_usable;
  const uint32_t first = use_mirror ? kSettingsSector : kMirrorSector;
  const uint32_t second = use_mirror ? kMirrorSector : kSettingsSector;
  if (!RewriteSector(first, second, bootstrap) || \
      !RewriteSector(second, first, false) || \
      !success) {
    return RECOVERY_FAILED;
  }
  return IsFactory(persistent_data_)
      ? RECOVERY_CALIBRATION_RESET
      : RECOVERY_CALIBRATION_KEPT;
}

bool SettingsRecovery::RewriteSector(
    uint32_t sector,
    uint32_t other,
    bool bootstrap) {
  const uint32_t start = SectorStart(sector);
  for (int attempt = 0; attempt < 2; ++attempt) {
    PersistentData data;
    if (!bootstrap && !Usable(other, &data)) {
      return false;
    }
    if (EraseSector(sector) && \
        WriteChunk(
            start,
            PersistentData::tag,
            &persistent_data_,
            sizeof(PersistentData)) && \
        (sector != kSettingsSector || WriteChunk(
            start + kCalibrationChunkSize,
            State::tag,
            &state_,
            sizeof(State))) && \
        Verify(sector)) {
      return true;
    }
  }
  return false;
}

bool SettingsRecovery::EraseSector(uint32_t sector) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (SupplyLow()) {
      return false;
    }
    HAL_FLASH_Unlock();
    ClearErrors();
    FLASH_EraseInitTypeDef erase_params;
    erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_params.Banks = FLASH_BANK_2;
    erase_params.Sector = sector;
    erase_params.NbSectors = 1;
    erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;
    uint32_t sector_error;
    const bool erased = \
        HAL_FLASHEx_Erase(&erase_params, &sector_error) == HAL_OK;
    HAL_FLASH_Lock();
    if (erased && IsBlank(SectorStart(sector), kBank2SectorSize)) {
      return true;
    }
  }
  return false;
}

bool SettingsRecovery::WriteChunk(
    uint32_t address,
    uint32_t tag,
    const void* data,
    size_t size) {
  uint8_t chunk[kMaxChunkSize];
  ChunkHeader h;
  h.tag = tag;
  h.size = size;
  h.checksum = Checksum(data, size);
  const size_t chunk_size = sizeof(ChunkHeader) + size;
  fill(&chunk[0], &chunk[kMaxChunkSize], 0xff);
  memcpy(&chunk[0], &h, sizeof(ChunkHeader));
  memcpy(&chunk[sizeof(ChunkHeader)], data, size);
  for (size_t offset = 0; offset < chunk_size; offset += kFlashWordSize) {
    uint32_t words[kFlashWordSize / 4];
    memcpy(words, &chunk[offset], kFlashWordSize);
    if (!ProgramWord(address + offset, words)) {
      return false;
    }
  }
  return true;
}

bool SettingsRecovery::ProgramWord(uint32_t address, const uint32_t* words) {
  // A word is programmed once, onto erased flash (RM0433 4.3.9).
  if (!IsBlank(address, kFlashWordSize) || SupplyLow()) {
    return false;
  }
  HAL_FLASH_Unlock();
  ClearErrors();
  const bool programmed = HAL_FLASH_Program(
      FLASH_TYPEPROGRAM_FLASHWORD,
      address,
      (uint64_t)(&words[0])) == HAL_OK;
  HAL_FLASH_Lock();
  uint32_t written[kFlashWordSize / 4];
  return programmed && \
      ReadMasked(address, written, kFlashWordSize) && \
      !memcmp(written, words, kFlashWordSize);
}

bool SettingsRecovery::ReadMasked(
    uint32_t address,
    void* destination,
    size_t size) {
  const volatile uint32_t* source = (const volatile uint32_t*)(address);
  uint32_t* words = static_cast<uint32_t*>(destination);
  size_t num_words = size / 4;

  __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_DBECCERR_BANK2);
  // With FAULTMASK set, a load that faults returns undefined data instead
  // of entering the HardFault handler.
  SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk;
  __DSB();
  __ISB();
  __set_FAULTMASK(1);
  while (num_words--) {
    *words++ = *source++;
  }
  __DSB();
  const bool clean = !__HAL_FLASH_GET_FLAG_BANK2(FLASH_FLAG_DBECCERR_BANK2);
  __set_FAULTMASK(0);
  SCB->CCR &= ~SCB_CCR_BFHFNMIGN_Msk;
  __DSB();
  __ISB();
  __HAL_FLASH_CLEAR_FLAG_BANK2(
      FLASH_FLAG_DBECCERR_BANK2 | FLASH_FLAG_SNECCERR_BANK2);
  return clean;
}

bool SettingsRecovery::IsBlank(uint32_t address, size_t size) {
  uint32_t words[kBlankCheckWords];
  for (size_t offset = 0; offset < size; offset += sizeof(words)) {
    const size_t block_size = min(size - offset, sizeof(words));
    if (!ReadMasked(address + offset, words, block_size)) {
      return false;
    }
    for (size_t i = 0; i < block_size / 4; ++i) {
      if (words[i] != 0xffffffff) {
        return false;
      }
    }
  }
  return true;
}

bool SettingsRecovery::ReadCalibration(uint32_t sector, PersistentData* data) {
  return ReadChunk(
      SectorStart(sector), data, PersistentData::tag, sizeof(PersistentData));
}

bool SettingsRecovery::IsPlausible(const PersistentData& data) const {
  PersistentData defaults;
  State state;
  LoadDefaults(&defaults, &state);
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    const AdcCalibrationData& c = data.adc_calibration[i];
    const AdcCalibrationData& d = defaults.adc_calibration[i];
    // Written so that NaN fails.
    if (!(fabsf(c.scale / d.scale - 1.0f) <= 0.25f) || \
        !(fabsf(c.offset / d.offset - 1.0f) <= 0.25f)) {
      return false;
    }
  }
  return true;
}

bool SettingsRecovery::Usable(uint32_t sector, PersistentData* data) {
  return ReadCalibration(sector, data) && IsPlausible(*data);
}

/* static */
bool SettingsRecovery::IsFactory(const PersistentData& data) {
  PersistentData defaults;
  State state;
  LoadDefaults(&defaults, &state);
  return !memcmp(
      data.adc_calibration,
      defaults.adc_calibration,
      sizeof(defaults.adc_calibration));
}

/* static */
bool SettingsRecovery::SupplyLow() {
  return READ_BIT(PWR->CSR1, PWR_CSR1_PVDO);
}

bool SettingsRecovery::Verify(uint32_t sector) {
  const uint32_t start = SectorStart(sector);
  size_t used = kCalibrationChunkSize;
  PersistentData data;
  if (!ReadChunk(start, &data, PersistentData::tag, sizeof(PersistentData)) || \
      memcmp(&data, &persistent_data_, sizeof(PersistentData))) {
    return false;
  }
  if (sector == kSettingsSector) {
    State state;
    if (!ReadChunk(start + used, &state, State::tag, sizeof(State)) || \
        memcmp(&state, &state_, sizeof(State))) {
      return false;
    }
    used += kStateChunkSize;
  }
  return IsBlank(start + used, kBank2SectorSize - used);
}

/* static */
void SettingsRecovery::LoadDefaults(PersistentData* data, State* state) {
  // As in Settings::Init, with the padding cleared as Settings::Save* does.
  AdcCalibrationData calibration;
  calibration.scale = -2.0f;
  calibration.offset = 1.0f;
  data->adc_calibration[CV_ADC_CHANNEL_ASSIGN] = calibration;
  data->adc_calibration[CV_ADC_CHANNEL_DENSITY] = calibration;

  calibration.scale = -3.2f;
  calibration.offset = 1.6f;
  data->adc_calibration[CV_ADC_CHANNEL_TIME] = calibration;
  data->adc_calibration[CV_ADC_CHANNEL_SIZE] = calibration;
  data->adc_calibration[CV_ADC_CHANNEL_SHAPE] = calibration;

  calibration.scale = -120.0f;
  calibration.offset = 60.0f;
  data->adc_calibration[CV_ADC_CHANNEL_PITCH] = calibration;
  fill(&data->padding[0], &data->padding[sizeof(data->padding)], 0);

  state->media = 3;
  state->assignable_cv_destination = 0;
  state->assignable_cv_amount[0] = 255;
  state->assignable_cv_amount[1] = 0;
  state->assignable_cv_amount[2] = 128;
  state->gain = 0;
  state->gate_output_mode = 0;
  state->frozen = 0;
  state->disable_autosave = 0;
  state->monophonic_mode = 0;
  fill(&state->padding[0], &state->padding[sizeof(state->padding)], 0);
}

bool SettingsRecovery::ReadChunk(
    uint32_t address,
    void* data,
    uint32_t tag,
    size_t size) {
  uint32_t words[kMaxChunkSize / 4];
  const size_t chunk_size = sizeof(ChunkHeader) + size;
  if (!ReadMasked(address, words, chunk_size)) {
    return false;
  }
  ChunkHeader h;
  memcpy(&h, words, sizeof(ChunkHeader));
  const uint8_t* payload = (const uint8_t*)(words) + sizeof(ChunkHeader);
  if (h.tag != tag || h.size != size || h.checksum != Checksum(payload, size)) {
    return false;
  }
  memcpy(data, payload, size);
  return true;
}

}  // namespace beads
