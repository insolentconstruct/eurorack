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
// Bank 2 flash and the fault mask for the recovery tests.

#ifndef BEADS_TEST_RECOVERY_STUBS_STM32H7XX_HAL_H_
#define BEADS_TEST_RECOVERY_STUBS_STM32H7XX_HAL_H_

#include <stdint.h>
#include <cstring>

// Bank 2, mapped at its real address by the test. Sectors are numbered 0-7,
// or 8-15 as stmlib does. Programming can only clear bits.
struct FLASH_EraseInitTypeDef {
  uint32_t TypeErase;
  uint32_t Banks;
  uint32_t Sector;
  uint32_t NbSectors;
  uint32_t VoltageRange;
};
struct SCB_Type {
  uint32_t CCR;
};
extern SCB_Type test_scb;
extern bool test_locked;
extern bool test_ecc_error;  // Every masked read hits a double ECC error.
extern int test_ecc_read;  // Only this masked read (counting from 1) does.
extern int test_masked_reads;
extern int test_fail_programs;  // Writes that report failure, writing nothing.
extern int test_silent_programs;  // Writes that report success, writing nothing.
extern uint32_t test_faultmask;
extern uint32_t test_sr2;  // Sticky error flags.
extern int test_fail_sector;  // The sector number given, or -1 for any.
extern int test_fail_erases;  // Failures left before an erase succeeds.
extern int test_silent_erases;  // Erases that report success, erasing nothing.
extern int test_erase_calls;
extern uint32_t test_erased;  // Bit mask of the sector numbers erased.
extern int test_programs;
#define SCB (&test_scb)
#define SCB_CCR_BFHFNMIGN_Msk (1UL << 8)
#define FLASH_FLAG_SNECCERR_BANK2 (1UL << 25)
#define FLASH_FLAG_DBECCERR_BANK2 (1UL << 26)
#define FLASH_FLAG_ALL_ERRORS_BANK2 (0x01ee0000UL | (3UL << 25))
#define __HAL_FLASH_CLEAR_FLAG_BANK2(flag) (test_sr2 &= ~(flag))
#define __HAL_FLASH_GET_FLAG_BANK2(flag) ((test_sr2 & (flag)) == (flag))
// As FLASH_WaitForLastOperation: a pending error fails the operation and
// is cleared.
inline bool TestPendingError() {
  if (test_sr2 & FLASH_FLAG_ALL_ERRORS_BANK2) {
    test_sr2 &= ~FLASH_FLAG_ALL_ERRORS_BANK2;
    return true;
  }
  return false;
}
inline void HAL_FLASH_Unlock() { test_locked = false; }
inline void HAL_FLASH_Lock() { test_locked = true; }
inline int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* e, uint32_t*) {
  ++test_erase_calls;
  if (TestPendingError() || test_locked || e->Banks != 2 || \
      e->NbSectors != 1 || e->Sector > 15) {
    return 1;
  }
  const bool selected = test_fail_sector < 0 || \
      uint32_t(test_fail_sector) == e->Sector;
  if (test_fail_erases && selected) {
    --test_fail_erases;
    return 1;
  }
  if (test_silent_erases && selected) {
    --test_silent_erases;
    return 0;
  }
  test_erased |= 1 << e->Sector;
  // SNB is three bits wide: stmlib's sector 15 is bank 2 sector 7.
  memset(reinterpret_cast<void*>(0x08100000 + (e->Sector & 7) * 0x20000),
      255, 0x20000);
  return 0;
}
inline int HAL_FLASH_Program(int, uint32_t address, uint64_t source) {
  if (TestPendingError() || test_locked) {
    return 1;
  }
  ++test_programs;
  if (test_fail_programs) {
    --test_fail_programs;
    return 1;
  }
  if (test_silent_programs) {
    --test_silent_programs;
    return 0;
  }
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  const uint8_t* src = reinterpret_cast<const uint8_t*>(uintptr_t(source));
  for (int i = 0; i < 32; ++i) {
    dst[i] &= src[i];
  }
  return 0;
}
// Reads made with bus faults masked raise the error flag.
inline void __DSB() {
  if (test_faultmask && (test_scb.CCR & SCB_CCR_BFHFNMIGN_Msk) && \
      (++test_masked_reads == test_ecc_read || test_ecc_error)) {
    test_sr2 |= FLASH_FLAG_DBECCERR_BANK2;
  }
}
inline void __ISB() { }
inline void __set_FAULTMASK(uint32_t mask) { test_faultmask = mask; }
#define HAL_OK 0
#define FLASH_TYPEERASE_SECTORS 0
#define FLASH_BANK_1 1
#define FLASH_BANK_2 2
#define FLASH_VOLTAGE_RANGE_4 0
#define FLASH_TYPEPROGRAM_FLASHWORD 0

#endif  // BEADS_TEST_RECOVERY_STUBS_STM32H7XX_HAL_H_
