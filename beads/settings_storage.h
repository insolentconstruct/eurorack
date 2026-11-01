// Copyright 2019 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// stmlib's ChunkStorage for the settings sector, checking every flash
// operation. A corrected ECC error left over from an earlier read makes the
// HAL refuse the next erase or program; stmlib ignores the result, and the
// records are then written over unerased flash, which reads back as an
// uncorrectable error and crashes the module at every start. Here the error
// flags are cleared first, every result is checked, and a record is only
// programmed onto blank flash and read back, past the D-cache. Same layout
// and records as stmlib's.
//
// Every read is masked (beads/drivers/flash.h), so an uncorrectable ECC word
// no longer crashes the module. Before each erase of the sector, the
// calibration about to be written is verified in a mirror sector, so a power
// cut after the erase can't lose it. Settings decides what a lost
// calibration means; Lock stops all writes to bank 2 until a repair. An
// erase or program refused for a low supply fails like any other.

#ifndef BEADS_SETTINGS_STORAGE_H_
#define BEADS_SETTINGS_STORAGE_H_

#include <stm32h7xx_hal.h>

#include <algorithm>
#include <cstring>

#include "stmlib/stmlib.h"
#include "stmlib/system/storage.h"

#include "beads/drivers/flash.h"

namespace beads {

const int kFlashAttempts = 3;

// What Init found in the settings sector.
enum StorageStatus {
  STORAGE_OK,
  // Blank sector and mirror: a new module. The defaults were written.
  STORAGE_NEW,
  // The calibration survived, in the sector or in its mirror, but no state
  // did. Nothing written.
  STORAGE_RESTORED,
  // The calibration is unreadable or invalid in a sector that isn't blank;
  // the states before the first bad one loaded. Nothing written.
  STORAGE_CALIBRATION_LOST,
  // Blank sector, and a mirror that isn't blank but holds no calibration.
  // Nothing written.
  STORAGE_DAMAGED
};

template<
    uint32_t sector_index,
    uint32_t mirror_sector_index,
    typename PersistentData,
    typename StateData>
class ChunkStorage {
 private:
  struct ChunkHeader {
    uint32_t tag;
    uint16_t size;
    uint16_t checksum;
  };

  enum {
    FLASH_STORAGE_BASE = stmlib::Sector<sector_index>::start,
    FLASH_STORAGE_LAST = stmlib::Sector<sector_index + 1>::start,
    MIRROR_BASE = stmlib::Sector<mirror_sector_index>::start,
    MIRROR_LAST = stmlib::Sector<mirror_sector_index + 1>::start,
  };

  enum {
    BLOCK_SIZE = 32,  // Must be a power of 2!
  };

  enum ChunkStatus {
    CHUNK_VALID,
    CHUNK_INVALID,
    CHUNK_UNREADABLE
  };

 public:
  ChunkStorage() { }
  ~ChunkStorage() { }
  // Loads the latest saved data. A new module's blank sector is written;
  // anything else is left for the caller to write once it has chosen the
  // calibration.
  StorageStatus Init(PersistentData* persistent_data, StateData* state_data) {
    persistent_data_ = persistent_data;
    state_data_ = state_data;
    locked_ = false;

    const StorageStatus status = Load(
        persistent_data, state_data, &next_state_chunk_index_);
    if (status == STORAGE_NEW) {
      RewriteSector();
    }
    return status;
  }

  // Damaged storage: nothing is erased or programmed until Repair.
  void Lock() {
    locked_ = true;
    next_state_chunk_index_ = REWRITE_NEEDED;
  }

  void Recover(PersistentData* persistent_data, StateData* state_data) {
    persistent_data_ = persistent_data;
    state_data_ = state_data;
    ReadChunk(chunk_address(0), persistent_data_);
    RewriteSector();
  }

  // Rewrites a damaged sector from RAM, past the lock, and checks that it
  // loads back as written. Writes stay locked if it fails.
  bool Repair() {
    locked_ = false;
    RewriteSector();
    PersistentData persistent_data;
    StateData state_data;
    uint16_t next_index;
    locked_ = next_state_chunk_index_ != 2 || \
        Load(&persistent_data, &state_data, &next_index) != STORAGE_OK || \
        next_index != 2 || \
        memcmp(&persistent_data, persistent_data_, sizeof(PersistentData)) || \
        memcmp(&state_data, state_data_, sizeof(StateData)) || \
        !Flash::IsBlank(
            chunk_address(2), FLASH_STORAGE_LAST - chunk_address(2));
    if (locked_) {
      next_state_chunk_index_ = REWRITE_NEEDED;
    }
    return !locked_;
  }

  void SaveState() {
    if (Flash::SupplyLow()) {
      return;  // Kept in RAM; the next save writes it.
    }
    if (chunk_address(next_state_chunk_index_ + 1) > FLASH_STORAGE_LAST) {
      RewriteSector();
    } else {
      HAL_FLASH_Unlock();
      if (WriteChunk(chunk_address(next_state_chunk_index_), state_data_)) {
        next_state_chunk_index_++;
        HAL_FLASH_Lock();
      } else {
        // The next free chunk can't take the record: start the sector over.
        RewriteSector();
      }
    }
  }

  void SavePersistentData() {
    RewriteSector();
  }

  // The mirrored calibration, if it reads clean and passes its checks.
  bool ReadMirror(PersistentData* persistent_data) {
    return ReadChunk(MIRROR_BASE, persistent_data) == CHUNK_VALID;
  }

  inline bool locked() const { return locked_; }

 private:
  // Reads the sector as stmlib's loader does: the calibration, then the
  // states up to the first one that fails its checks or can't be read, as
  // an interrupted save leaves it. Nothing past that one is read.
  StorageStatus Load(
      PersistentData* persistent_data,
      StateData* state_data,
      uint16_t* next_index) {
    *next_index = REWRITE_NEEDED;
    const ChunkStatus calibration = ReadChunk(
        chunk_address(0), persistent_data);
    if (calibration == CHUNK_INVALID && Flash::IsBlank(
            FLASH_STORAGE_BASE, FLASH_STORAGE_LAST - FLASH_STORAGE_BASE)) {
      // Erased, then the power went: the mirror holds the calibration.
      const ChunkStatus mirror = ReadChunk(MIRROR_BASE, persistent_data);
      if (mirror == CHUNK_VALID) {
        return STORAGE_RESTORED;
      }
      // A mirror that isn't blank may have been the only copy left.
      return mirror == CHUNK_INVALID && \
          Flash::IsBlank(MIRROR_BASE, MIRROR_LAST - MIRROR_BASE)
          ? STORAGE_NEW
          : STORAGE_DAMAGED;
    }

    uint16_t index = 1;
    for (; chunk_address(index + 1) <= FLASH_STORAGE_LAST; ++index) {
      if (ReadChunk(chunk_address(index), state_data) != CHUNK_VALID) {
        break;
      }
    }
    if (calibration != CHUNK_VALID) {
      return STORAGE_CALIBRATION_LOST;
    } else if (index == 1) {
      // Rewritten, then the power went before the first state.
      return STORAGE_RESTORED;
    }
    *next_index = index;
    return STORAGE_OK;
  }

  void RewriteSector() {
    HAL_FLASH_Unlock();
    if (MirrorPersistentData() && \
        EraseSector(sector_index, FLASH_STORAGE_BASE) && \
        WriteChunk(chunk_address(0), persistent_data_) && \
        WriteChunk(chunk_address(1), state_data_)) {
      next_state_chunk_index_ = 2;
    } else {
      // Settings stay in RAM; the next save tries the whole sector again.
      next_state_chunk_index_ = REWRITE_NEEDED;
    }
    HAL_FLASH_Lock();
  }

  // Leaves a verified copy of the calibration about to be written in the
  // mirror sector; only writes it if it is missing or different.
  bool MirrorPersistentData() {
    PersistentData mirror;
    if (ReadChunk(MIRROR_BASE, &mirror) == CHUNK_VALID && \
        !memcmp(&mirror, persistent_data_, sizeof(PersistentData))) {
      return true;
    }
    return EraseSector(mirror_sector_index, MIRROR_BASE) && \
        WriteChunk(MIRROR_BASE, persistent_data_) && \
        ReadChunk(MIRROR_BASE, &mirror) == CHUNK_VALID && \
        !memcmp(&mirror, persistent_data_, sizeof(PersistentData));
  }

  bool EraseSector(uint32_t index, uint32_t address) {
    if (locked_) {
      return false;
    }
    for (int attempt = 0; attempt < kFlashAttempts; ++attempt) {
      if (Flash::SupplyLow()) {
        return false;
      }
      ClearErrors();
      FLASH_EraseInitTypeDef erase_params;
      erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
      erase_params.Banks = (index < 8) ? FLASH_BANK_1 : FLASH_BANK_2;
      erase_params.Sector = index % 8;  // Within its bank.
      erase_params.NbSectors = 1;
      erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;

      uint32_t sector_error;
      if (HAL_FLASHEx_Erase(&erase_params, &sector_error) == HAL_OK) {
        // The D-cache may still hold the records read before the erase.
        SCB_InvalidateDCache_by_Addr(
            (uint32_t*)(address),
            FLASH_STORAGE_LAST - FLASH_STORAGE_BASE);
        return true;
      }
    }
    return false;
  }

  // Reads a whole chunk in one masked read.
  template<typename T>
  ChunkStatus ReadChunk(uint32_t address, T* data) {
    struct {
      ChunkHeader h;
      T data;
    } chunk;
    if (!Flash::Read(address, &chunk, sizeof(chunk))) {
      return CHUNK_UNREADABLE;
    }
    if (chunk.h.tag == T::tag &&
        chunk.h.size == sizeof(T) &&
        Checksum(&chunk.data, sizeof(T)) == chunk.h.checksum) {
      memcpy(data, &chunk.data, sizeof(T));
      return CHUNK_VALID;
    } else {
      return CHUNK_INVALID;
    }
  }

  // Programs the chunk only onto blank flash, and checks it reads back.
  template<typename T>
  bool WriteChunk(uint32_t flash_address, const T* data) {
    ChunkHeader h;
    h.tag = T::tag;
    h.size = sizeof(T);
    h.checksum = Checksum(data, sizeof(T));

    size_t num_flash_words = ChunkSize<T>() / BLOCK_SIZE;

    size_t data_size = sizeof(T);
    const uint8_t* data_source = (const uint8_t*)(data);

    for (size_t i = 0; i < num_flash_words; ++i) {
      size_t header_size = i == 0 ? sizeof(ChunkHeader) : 0;
      size_t size = std::min(data_size, BLOCK_SIZE - header_size);

      uint8_t buffer[BLOCK_SIZE];
      memcpy(&buffer[0], &h, header_size);
      memcpy(&buffer[header_size], data_source, size);

      if (!ProgramWord(flash_address, &buffer[0])) {
        return false;
      }

      data_size -= size;
      data_source += size;
      flash_address += BLOCK_SIZE;
    }
    return true;
  }

  bool ProgramWord(uint32_t address, const uint8_t* buffer) {
    if (locked_) {
      return false;
    }
    for (int attempt = 0; attempt < kFlashAttempts; ++attempt) {
      if (!Flash::IsBlank(address, BLOCK_SIZE)) {
        return false;
      }
      if (Flash::SupplyLow()) {
        return false;
      }
      ClearErrors();
      if (HAL_FLASH_Program(
              FLASH_TYPEPROGRAM_FLASHWORD,
              address,
              (uint64_t)(&buffer[0])) == HAL_OK) {
        uint32_t written[BLOCK_SIZE / 4];
        return Flash::Read(address, written, BLOCK_SIZE) && \
            !memcmp(written, buffer, BLOCK_SIZE);
      }
    }
    return false;
  }

  void ClearErrors() const {
    __HAL_FLASH_CLEAR_FLAG(sector_index < 8
        ? FLASH_FLAG_ALL_ERRORS_BANK1
        : FLASH_FLAG_ALL_ERRORS_BANK2);
  }

  template<typename T>
  inline size_t ChunkSize() {
    return (sizeof(ChunkHeader) + sizeof(T) + BLOCK_SIZE - 1) & \
        ~(BLOCK_SIZE - 1);
  }

  uint32_t chunk_address(size_t index) {
    if (index == 0) {
      return FLASH_STORAGE_BASE;
    } else {
      return FLASH_STORAGE_BASE + ChunkSize<PersistentData>() + ChunkSize<StateData>() * (index - 1);
    }
  }

  uint16_t Checksum(const void* data, size_t size) const {
    uint16_t sum = 0;
    const char* bytes = static_cast<const char*>(data);
    while (size--) {
      sum += *bytes++;
    }
    return sum ^ 0xffff;
  }

  enum {
    // Past the end of the sector: the next save rewrites it.
    REWRITE_NEEDED = 0xffff
  };

  PersistentData* persistent_data_;
  StateData* state_data_;
  uint16_t next_state_chunk_index_;
  // Damaged storage found at power-up: nothing is erased or programmed
  // until a repair succeeds.
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(ChunkStorage);
};

}  // namespace beads

#endif  // BEADS_SETTINGS_STORAGE_H_
