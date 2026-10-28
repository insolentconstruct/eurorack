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
// programmed onto blank flash and read back. Same layout and records as
// stmlib's.

#ifndef BEADS_SETTINGS_STORAGE_H_
#define BEADS_SETTINGS_STORAGE_H_

#include <stm32h7xx_hal.h>

#include <algorithm>
#include <cstring>

#include "stmlib/stmlib.h"
#include "stmlib/system/storage.h"

namespace beads {

const int kFlashAttempts = 3;

template<
    uint32_t sector_index,
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
  };
  
  enum {
    BLOCK_SIZE = 32,  // Must be a power of 2!
  };

 public:
  ChunkStorage() { }
  ~ChunkStorage() { }
  // Loads the latest saved data. In case the sector is blank/corrupted,
  // reinitializes the sector and returns false.
  bool Init(PersistentData* persistent_data, StateData* state_data) {
    persistent_data_ = persistent_data;
    state_data_ = state_data;
  
    if (ReadChunk(0, persistent_data)) {
      for (next_state_chunk_index_ = 1;
           chunk_address(next_state_chunk_index_ + 1) <= FLASH_STORAGE_LAST;
           ++next_state_chunk_index_) {
         if (!ReadChunk(next_state_chunk_index_, state_data)) {
           break;
         }
      }
      if (next_state_chunk_index_ != 1) {
        return true;
      }
    }
    RewriteSector();
    return false;
  }
  
  void Recover(PersistentData* persistent_data, StateData* state_data) {
    persistent_data_ = persistent_data;
    state_data_ = state_data;
    ReadChunk(0, persistent_data_);
    RewriteSector();
  }

  void SaveState() {
    if (chunk_address(next_state_chunk_index_ + 1) > FLASH_STORAGE_LAST) {
      RewriteSector();
    } else {
      HAL_FLASH_Unlock();
      if (WriteChunk(next_state_chunk_index_, state_data_)) {
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

 private:
  void RewriteSector() {
    HAL_FLASH_Unlock();
    if (EraseSector() && \
        WriteChunk(0, persistent_data_) && \
        WriteChunk(1, state_data_)) {
      next_state_chunk_index_ = 2;
    } else {
      // Settings stay in RAM; the next save tries the whole sector again.
      next_state_chunk_index_ = kRewriteNeeded;
    }
    HAL_FLASH_Lock();
  }

  bool EraseSector() {
    for (int attempt = 0; attempt < kFlashAttempts; ++attempt) {
      ClearErrors();
      FLASH_EraseInitTypeDef erase_params;
      erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
      erase_params.Banks = (sector_index < 8) ? FLASH_BANK_1 : FLASH_BANK_2;
      erase_params.Sector = sector_index;
      erase_params.NbSectors = 1;
      erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;
    
      uint32_t sector_error;
      if (HAL_FLASHEx_Erase(&erase_params, &sector_error) == HAL_OK) {
        return true;
      }
    }
    return false;
  }

  template<typename T>
  bool ReadChunk(size_t index, T* data) {
    const char* flash_ptr = (const char*)(chunk_address(index));
    ChunkHeader* h = (ChunkHeader*)(flash_ptr);
    if (h->tag == T::tag &&
        h->size == sizeof(T) &&
        Checksum(flash_ptr + sizeof(ChunkHeader), h->size) == h->checksum) {
      memcpy(data, flash_ptr + sizeof(ChunkHeader), h->size);
      return true;
    } else {
      return false;
    }
  }

  // Programs the chunk only onto blank flash, and checks it reads back.
  template<typename T>
  bool WriteChunk(size_t index, const T* data) {
    ChunkHeader h;
    h.tag = T::tag;
    h.size = sizeof(T);
    h.checksum = Checksum(data, sizeof(T));
    
    size_t num_flash_words = ChunkSize<T>() / BLOCK_SIZE;
    uint32_t flash_address = chunk_address(index);

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
    for (int attempt = 0; attempt < kFlashAttempts; ++attempt) {
      if (!IsBlank(address, BLOCK_SIZE)) {
        return false;
      }
      ClearErrors();
      if (HAL_FLASH_Program(
              FLASH_TYPEPROGRAM_FLASHWORD,
              address,
              (uint64_t)(&buffer[0])) == HAL_OK) {
        return !memcmp((const void*)(address), buffer, BLOCK_SIZE);
      }
    }
    return false;
  }

  bool IsBlank(uint32_t address, size_t size) const {
    const uint32_t* words = (const uint32_t*)(address);
    for (size_t i = 0; i < size / 4; ++i) {
      if (words[i] != 0xffffffff) {
        return false;
      }
    }
    return true;
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
    kRewriteNeeded = 0xffff
  };

  PersistentData* persistent_data_;
  StateData* state_data_;
  uint16_t next_state_chunk_index_;

  DISALLOW_COPY_AND_ASSIGN(ChunkStorage);
};

}  // namespace beads

#endif  // BEADS_SETTINGS_STORAGE_H_
