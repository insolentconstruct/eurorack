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
// The friendly state machine that saves the buffer in the background.

#include "beads/background_buffer_backup.h"

#include "stmlib/system/system_clock.h"
#include "stmlib/utils/crc32.h"

#include "beads/dsp/processor.h"
#include "beads/settings.h"

namespace beads {

using namespace std;
using namespace stmlib;

const size_t kFlashWordSize = 32;
const uint32_t kBackupDelay = 10000; // milliseconds
const size_t kNumSectors = 6;
const uint32_t kCrcTag = 0x31435243;  // CRC1

// Stereo frames in the buffer, by media. Mono has twice as many.
const int kMaxDelay[] = {
  kDelayBufferSizeBits / 16,
  kDelayBufferSizeBits / 24,
  kDelayBufferSizeBits / 24,
  kDelayBufferSizeBits / 32
};

void BackgroundBufferBackup::Init(Processor* processor, Settings* settings) {
  STATIC_ASSERT(sizeof(BufferBackupHeader) == kFlashWordSize, one_flash_word);
  processor_ = processor;
  settings_ = settings;
  
  previous_freeze_state_ = false;
  remaining_sectors_ = 0;
  remaining_bytes_ = 0;
  write_offset_ = 0;
}

bool BackgroundBufferBackup::Load() {
  bool frozen = settings_->state().frozen && \
      !settings_->state().disable_autosave;
  if (frozen) {
    const char* flash_ptr = (const char*)(Sector<8>::start);
    const BufferBackupHeader* header = (const BufferBackupHeader*)(flash_ptr);
    if (Valid(*header)) {
      processor_->Configure(
          Processor::Media(header->media),
          bool(header->mono));

      for (size_t i = 0; i < kDelayBufferSizeBits / 8; i += kFlashWordSize) {
        uint32_t address = Sector<8>::start + uint32_t(i) + kFlashWordSize;
        const uint8_t* flash_ptr = (const uint8_t*)(address);
        copy(
            &flash_ptr[0],
            &flash_ptr[kFlashWordSize],
            processor_->delay_buffer_raw_bytes(i));
      }

      *processor_->mutable_delay_write_ptr() = header->write_ptr;
    } else {
      frozen = false;
    }
  }
  freeze_start_time_ = system_clock.milliseconds();
  previous_freeze_state_ = frozen;
  return frozen;
}

bool BackgroundBufferBackup::Valid(const BufferBackupHeader& header) const {
  if (header.tag != 0x5a5a5246 ||  // FRZZ
      header.media > Processor::MEDIA_DIGITAL ||
      header.mono > 1 ||
      header.write_ptr < 0 ||
      header.write_ptr >= kMaxDelay[header.media] << header.mono) {
    return false;
  }
  if (header.crc_tag != kCrcTag) {
    return true;  // Saved by the stock firmware: no CRC.
  }
  BufferBackupHeader h = header;
  h.crc = 0;
  uint32_t crc = crc32(
      0,
      (const void*)(Sector<8>::start + kFlashWordSize),
      kDelayBufferSizeBits / 8);
  return crc32(crc, &h, sizeof(h)) == header.crc;
}

void BackgroundBufferBackup::SaveState(bool frozen) {
  settings_->mutable_state()->frozen = frozen;
  settings_->SaveState();
}

void BackgroundBufferBackup::EraseSector() {
  if (remaining_sectors_) {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_params;
    erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_params.Banks = FLASH_BANK_2;
    // The header's sector first: an interrupted backup is never loaded.
    erase_params.Sector = 8 + kNumSectors - remaining_sectors_;
    erase_params.NbSectors = 1;
    erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;
    uint32_t sector_error;
    if (HAL_FLASHEx_Erase(&erase_params, &sector_error) != HAL_OK) {
      remaining_sectors_ = remaining_bytes_ = 0;
      return;
    }
    --remaining_sectors_;
  
    if (!remaining_sectors_) {
      write_offset_ = 0;
      crc_ = 0;
      remaining_bytes_ = kDelayBufferSizeBits / 8;
    }
  }
}

void BackgroundBufferBackup::WriteBlock() {
  if (remaining_bytes_) {
    HAL_FLASH_Unlock();  // A settings save may have locked it.
    uint8_t* source = processor_->delay_buffer_raw_bytes(write_offset_);
    uint8_t buffer[kFlashWordSize];
    copy(&source[0], &source[kFlashWordSize], &buffer[0]);
        
    uint32_t flash_address = \
        Sector<8>::start + uint32_t(write_offset_) + kFlashWordSize;
    if (HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_FLASHWORD,
        flash_address,
        (uint64_t)(&buffer[0])) != HAL_OK) {
      remaining_sectors_ = remaining_bytes_ = 0;
      return;
    }
    crc_ = crc32(crc_, &buffer[0], kFlashWordSize);
    write_offset_ += kFlashWordSize;
    remaining_bytes_ -= kFlashWordSize;
    
    if (!remaining_bytes_) {
      // The header goes last, once all the audio is in.
      BufferBackupHeader header;
      header.tag = 0x5a5a5246;  // FRZZ
      header.write_ptr = *processor_->mutable_delay_write_ptr();
      header.media = processor_->media();
      header.mono = processor_->mono();
      fill(&header.padding[0], &header.padding[sizeof(header.padding)], 0);
      header.crc = 0;
      header.crc_tag = kCrcTag;
      fill(&header.reserved[0], &header.reserved[sizeof(header.reserved)], 0);
      header.crc = crc32(crc_, &header, sizeof(header));
      if (HAL_FLASH_Program(
          FLASH_TYPEPROGRAM_FLASHWORD,
          Sector<8>::start,
          (uint64_t)(&header)) != HAL_OK) {
        return;
      }
      // We've saved everything, we are now sure that we can restart with
      // the FREEZE button enabled!
      SaveState(true);
    }
  }
}

void BackgroundBufferBackup::Process(bool freeze_state) {
  if (!freeze_state) {
    if (previous_freeze_state_) {
      // Abort current save op.
      remaining_sectors_ = remaining_bytes_ = 0;
      SaveState(false);
    }
    freeze_start_time_ = system_clock.milliseconds();
    previous_freeze_state_ = false;
  } else {
    if ((system_clock.milliseconds() - freeze_start_time_) >= kBackupDelay &&
        !previous_freeze_state_ && !settings_->state().disable_autosave) {
      previous_freeze_state_ = true;
      remaining_sectors_ = kNumSectors;
      if (settings_->state().frozen) {
        SaveState(false);  // Stale: its backup was not loaded at boot.
      }
    }
  }
  EraseSector();
  WriteBlock();
}


}  // namespace beads