// Copyright 2026 Insolent Construct.
//
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
// Flash operations for the settings storage test: the settings sector only,
// mapped at its real address by the test.

#ifndef BEADS_TEST_STORAGE_STUBS_STM32H7XX_HAL_H_
#define BEADS_TEST_STORAGE_STUBS_STM32H7XX_HAL_H_

#include <stdint.h>
#include <cstring>

struct GPIO_TypeDef { };
struct GPIO_InitTypeDef {
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Pin;
};
struct FLASH_EraseInitTypeDef {
  uint32_t TypeErase;
  uint32_t Banks;
  uint32_t Sector;
  uint32_t NbSectors;
  uint32_t VoltageRange;
};

// Error flags left by an earlier read or operation. As in the HAL, an erase
// or program then fails without touching flash, and clears them.
extern uint32_t test_errors;
// Operations that fail without touching flash, and erases that report
// success but leave the sector as it was.
extern int test_failures;
extern int test_stuck_erases;
extern unsigned test_erases;
extern unsigned test_refusals;
// Programs onto flash that isn't blank: an uncorrectable ECC error on the
// chip.
extern unsigned test_overwrites;

inline bool TestRefused() {
  if (test_errors) {
    test_errors = 0;
    ++test_refusals;
    return true;
  }
  if (test_failures > 0) {
    --test_failures;
    ++test_refusals;
    return true;
  }
  return false;
}

inline void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*) { }
inline void HAL_FLASH_Unlock() { }
inline void HAL_FLASH_Lock() { }
inline int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef*, uint32_t*) {
  if (TestRefused()) {
    return 1;
  }
  ++test_erases;
  if (test_stuck_erases > 0) {
    --test_stuck_erases;
    return 0;
  }
  memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
  return 0;
}
inline int HAL_FLASH_Program(int, uint32_t address, uint64_t source) {
  if (TestRefused()) {
    return 1;
  }
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  uint8_t* src = reinterpret_cast<uint8_t*>(uintptr_t(source));
  bool blank = true;
  for (int i = 0; i < 32; ++i) {
    blank = blank && dst[i] == 0xff;
    dst[i] &= src[i];
  }
  if (!blank) {
    ++test_overwrites;
  }
  return 0;
}
#define HAL_OK 0
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP 0
#define FLASH_TYPEERASE_SECTORS 0
#define FLASH_BANK_1 0
#define FLASH_BANK_2 1
#define FLASH_VOLTAGE_RANGE_4 0
#define FLASH_TYPEPROGRAM_FLASHWORD 0
inline void SCB_InvalidateDCache_by_Addr(uint32_t*, int32_t) { }
#define FLASH_SR_SNECCERR 0x02000000U
#define FLASH_FLAG_ALL_ERRORS_BANK1 0x07ee0000U
#define FLASH_FLAG_ALL_ERRORS_BANK2 0x87ee0000U
#define __HAL_FLASH_CLEAR_FLAG(flags) (test_errors &= ~((flags) & 0x7fffffffU))

#endif  // BEADS_TEST_STORAGE_STUBS_STM32H7XX_HAL_H_
