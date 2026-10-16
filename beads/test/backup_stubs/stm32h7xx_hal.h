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
// Flash operations with power loss injection.

#ifndef BEADS_TEST_BACKUP_STUBS_STM32H7XX_HAL_H_
#define BEADS_TEST_BACKUP_STUBS_STM32H7XX_HAL_H_

#include <stdint.h>
#include <cstring>

// Bank 2 flash, mapped at its real address by the test. Programming can only
// clear bits, like the hardware; a locked or failing operation does nothing.
struct FLASH_EraseInitTypeDef {
  uint32_t TypeErase;
  uint32_t Banks;
  uint32_t Sector;
  uint32_t NbSectors;
  uint32_t VoltageRange;
};
extern bool test_locked;
extern int test_budget;    // Operations left before power is lost; -1: none.
extern int test_fail_at;   // Operation number that fails; 0: none.
extern int test_ops;
extern int test_erases;
extern bool test_half_erase;  // Power goes halfway through an erase.
inline bool TestOperation() {
  if (test_locked || test_budget == 0) {
    return false;
  }
  if (test_budget > 0) {
    --test_budget;
  }
  return ++test_ops != test_fail_at;
}
inline void HAL_FLASH_Unlock() { test_locked = false; }
inline void HAL_FLASH_Lock() { test_locked = true; }
inline int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* e, uint32_t*) {
  if (e->Sector >= 8 && e->Sector < 14) {
    test_erases += e->NbSectors;
  }
  if (test_half_erase && test_budget == 1 && !test_locked) {
    memset(reinterpret_cast<void*>(0x08100000 + (e->Sector - 8) * 0x20000),
      255, 0x10000);
    test_budget = 0;
    return 1;
  }
  if (!TestOperation()) {
    return 1;
  }
  memset(reinterpret_cast<void*>(0x08100000 + (e->Sector - 8) * 0x20000),
      255, 0x20000 * e->NbSectors);
  return 0;
}
inline int HAL_FLASH_Program(int, uint32_t address, uint64_t source) {
  if (!TestOperation()) {
    return 1;
  }
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  const uint8_t* src = reinterpret_cast<const uint8_t*>(uintptr_t(source));
  for (int i = 0; i < 32; ++i) {
    dst[i] &= src[i];
  }
  return 0;
}
#define HAL_OK 0
#define FLASH_TYPEERASE_SECTORS 0
#define FLASH_BANK_1 0
#define FLASH_BANK_2 1
#define FLASH_VOLTAGE_RANGE_4 0
#define FLASH_TYPEPROGRAM_FLASHWORD 0

#endif  // BEADS_TEST_BACKUP_STUBS_STM32H7XX_HAL_H_
