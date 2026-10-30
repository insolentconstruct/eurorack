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
// Reads of the bank 2 flash (frozen buffer, calibration mirror, settings)
// that survive an uncorrectable ECC error. A plain read of such a word faults
// and the module restarts in a loop; here it is read with faults masked and
// the error flag is checked instead. Writes check the supply first: nothing
// is erased or programmed while VDD is below the PVD threshold.

#ifndef BEADS_DRIVERS_FLASH_H_
#define BEADS_DRIVERS_FLASH_H_

#include <algorithm>

#include "stmlib/stmlib.h"

#include <stm32h7xx_hal.h>

namespace beads {

// Interrupts are held off for one read: a few microseconds at most, well
// within the watchdog's 2 s.
const size_t kFlashMaxReadSize = 4096;
const size_t kFlashCheckBlockSize = 256;

class Flash {
 public:
  Flash() { }
  ~Flash() { }

  // Starts the programmable voltage detector at level 6: 2.69 to 2.83 V
  // falling, 100 mV of hysteresis. Programs at x64 parallelism are rated
  // down to 1.8 V. Not an option byte: nothing persists.
  static void Init() {
    MODIFY_REG(PWR->CR1, PWR_CR1_PLS, PWR_CR1_PLS_LEV6);
    SET_BIT(PWR->CR1, PWR_CR1_PVDEN);
  }

  // True while VDD is below the threshold: no erase or program may start.
  // One already under way can't be stopped.
  static bool SupplyLow() {
    return READ_BIT(PWR->CSR1, PWR_CSR1_PVDO);
  }

  // Copies size bytes (a multiple of 4, at most kFlashMaxReadSize) from bank
  // 2 to a word-aligned destination. Returns false if the read hit an
  // uncorrectable ECC error; the words read are then undefined.
  static bool Read(uint32_t address, void* destination, size_t size) {
    // Read the flash itself, not lines cached before an erase or program.
    Invalidate(address, size);
    const uint32_t faultmask = __get_FAULTMASK();
    const uint32_t ccr = SCB->CCR;
    __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_DBECCERR_BANK2);
    // With FAULTMASK set, a load that faults returns undefined data instead
    // of entering the HardFault handler.
    SCB->CCR = ccr | SCB_CCR_BFHFNMIGN_Msk;
    __DSB();
    __ISB();
    __set_FAULTMASK(1);
    const volatile uint32_t* source =
        (const volatile uint32_t*)(uintptr_t)(address);
    uint32_t* words = static_cast<uint32_t*>(destination);
    size_t num_words = size / 4;
    while (num_words--) {
      *words++ = *source++;
    }
    __DSB();
    const bool clean = !__HAL_FLASH_GET_FLAG_BANK2(FLASH_FLAG_DBECCERR_BANK2);
    if (!clean) {
      // The ignored bus fault still latched its status: clear it (write 1
      // to clear) before unmasking, so that later fault diagnostics aren't
      // misled.
      SCB->CFSR = SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk;
      __DSB();
    }
    __set_FAULTMASK(faultmask);
    SCB->CCR = ccr;
    __DSB();
    __ISB();
    __HAL_FLASH_CLEAR_FLAG_BANK2(
        FLASH_FLAG_DBECCERR_BANK2 | FLASH_FLAG_SNECCERR_BANK2);
    if (!clean) {
      // Keep the undefined words out of the cache.
      Invalidate(address, size);
    }
    return clean;
  }

  // True if the range reads back erased, with no ECC error.
  static bool IsBlank(uint32_t address, size_t size) {
    uint32_t words[kFlashCheckBlockSize / 4];
    for (size_t offset = 0; offset < size; offset += sizeof(words)) {
      const size_t block_size = std::min(size - offset, sizeof(words));
      if (!Read(address + offset, words, block_size)) {
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

  // True if the range reads with no ECC error.
  static bool IsReadable(uint32_t address, size_t size) {
    uint32_t words[kFlashCheckBlockSize / 4];
    for (size_t offset = 0; offset < size; offset += sizeof(words)) {
      if (!Read(address + offset, words,
              std::min(size - offset, sizeof(words)))) {
        return false;
      }
    }
    return true;
  }

  // A flag left by an earlier read, even a corrected error, makes the HAL
  // refuse the next erase or program.
  static void ClearErrors() {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);
  }

 private:
  static void Invalidate(uint32_t address, size_t size) {
    // Whole cache lines: CMSIS steps 32 bytes from the address given.
    const uint32_t start = address & ~uint32_t(31);
    SCB_InvalidateDCache_by_Addr(
        (uint32_t*)(uintptr_t)(start),
        int32_t(address + size - start));
  }

  DISALLOW_COPY_AND_ASSIGN(Flash);
};

}  // namespace beads

#endif  // BEADS_DRIVERS_FLASH_H_
