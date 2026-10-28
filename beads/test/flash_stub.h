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
// Bank 2 error flags, uncorrectable ECC errors and the core registers the
// masked reader uses, shared by the HAL stubs.
//
// A read covering a damaged flash word sets DBECCERR, and counts as a fault
// unless FAULTMASK and BFHFNMIGN are set. With the guard on, the pages
// holding a damaged word are inaccessible outside a masked read, so a plain
// read of one stops the test (TestFlashGuardHandler).

#ifndef BEADS_TEST_FLASH_STUB_H_
#define BEADS_TEST_FLASH_STUB_H_

#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>

#define FLASH_SR_SNECCERR 0x02000000U
#define FLASH_SR_DBECCERR 0x04000000U
#define FLASH_FLAG_ALL_ERRORS_BANK1 0x07ee0000U
#define FLASH_FLAG_ALL_ERRORS_BANK2 0x87ee0000U
#define FLASH_FLAG_SNECCERR_BANK2 (FLASH_SR_SNECCERR | 0x80000000U)
#define FLASH_FLAG_DBECCERR_BANK2 (FLASH_SR_DBECCERR | 0x80000000U)
#define SCB_CCR_BFHFNMIGN_Msk (1U << 8)

struct SCB_Type {
  uint32_t CCR;
};

const int kTestDamagedWordsMax = 64;
const uint32_t kTestMaskedReadMax = 4096 + 32;  // Plus line alignment.

struct TestFlashState {
  uint32_t errors;  // Bank 2 status flags.
  uint32_t damaged[kTestDamagedWordsMax];
  int num_damaged;
  bool guard;
  bool exposed;
  uint32_t faultmask;
  SCB_Type scb;
  // The range last invalidated in the D-cache: the reader's.
  uint32_t range_start;
  uint32_t range_end;
  // A failed read whose lines are still cached.
  bool stale;
  uint32_t stale_start;
  uint32_t stale_end;
  // Reads of damaged words that would fault, masked intervals that are too
  // long, flash operations started with faults masked.
  int violations;
  int masked_reads;
};

inline TestFlashState& TestFlash() {
  static TestFlashState state;
  return state;
}

// Error flags left by an earlier read or operation. As in the HAL, an erase
// or program then fails without touching flash, and clears them.
inline uint32_t& TestFlashErrors() {
  return TestFlash().errors;
}

#define SCB (&TestFlash().scb)

inline void TestFlashExpose(bool exposed) {
  TestFlashState& f = TestFlash();
  if (!f.guard || f.exposed == exposed) {
    return;
  }
  f.exposed = exposed;
  for (int i = 0; i < f.num_damaged; ++i) {
    mprotect(reinterpret_cast<void*>(uintptr_t(f.damaged[i] & ~4095U)), 4096,
        exposed ? PROT_READ | PROT_WRITE : PROT_NONE);
  }
}

// Makes the flash word at address uncorrectable, as a program over unerased
// flash does.
inline void TestFlashDamage(uint32_t address) {
  TestFlashState& f = TestFlash();
  TestFlashExpose(true);
  if (f.num_damaged < kTestDamagedWordsMax) {
    f.damaged[f.num_damaged++] = address & ~31U;
  }
  TestFlashExpose(false);
}

inline void TestFlashRepairAll() {
  TestFlashExpose(true);
  TestFlash().num_damaged = 0;
}

inline bool TestFlashDamaged(uint32_t start, uint32_t end) {
  const TestFlashState& f = TestFlash();
  for (int i = 0; i < f.num_damaged; ++i) {
    if (f.damaged[i] < end && f.damaged[i] + 32 > start) {
      return true;
    }
  }
  return false;
}

// An erase clears the damage in its sectors.
inline void TestFlashErased(uint32_t start, uint32_t size) {
  TestFlashState& f = TestFlash();
  int kept = 0;
  for (int i = 0; i < f.num_damaged; ++i) {
    if (f.damaged[i] < start || f.damaged[i] >= start + size) {
      f.damaged[kept++] = f.damaged[i];
    }
  }
  f.num_damaged = kept;
}

// Brackets the flash writes of the HAL stubs.
inline void TestFlashBeginOperation() {
  if (TestFlash().faultmask) {
    ++TestFlash().violations;
  }
  TestFlashExpose(true);
}

inline void TestFlashEndOperation() {
  TestFlashExpose(false);
}

inline uint32_t __get_FAULTMASK() {
  return TestFlash().faultmask;
}

inline void __set_FAULTMASK(uint32_t faultmask) {
  TestFlashState& f = TestFlash();
  f.faultmask = faultmask;
  // Damaged words read as undefined data only with faults masked.
  const bool masked = faultmask && (f.scb.CCR & SCB_CCR_BFHFNMIGN_Msk);
  if (masked) {
    ++f.masked_reads;
  }
  TestFlashExpose(masked);
}

inline void __DSB() { }
inline void __ISB() { }

inline void SCB_InvalidateDCache_by_Addr(uint32_t* address, int32_t size) {
  TestFlashState& f = TestFlash();
  f.range_start = uint32_t(uintptr_t(address));
  f.range_end = f.range_start + uint32_t(size);
  if (f.stale && f.range_start <= f.stale_start &&
      f.range_end >= f.stale_end) {
    f.stale = false;
  }
}

inline bool TestFlashGetFlag(uint32_t flag) {
  TestFlashState& f = TestFlash();
  if (TestFlashDamaged(f.range_start, f.range_end)) {
    if (!f.faultmask || !(f.scb.CCR & SCB_CCR_BFHFNMIGN_Msk)) {
      ++f.violations;
    }
    f.errors |= FLASH_SR_DBECCERR;
    f.stale = true;
    f.stale_start = f.range_start;
    f.stale_end = f.range_end;
  }
  if (f.range_end - f.range_start > kTestMaskedReadMax) {
    ++f.violations;
  }
  return (f.errors & flag & 0x7fffffffU) == (flag & 0x7fffffffU);
}

#define __HAL_FLASH_GET_FLAG_BANK2(flag) TestFlashGetFlag(flag)
#define __HAL_FLASH_CLEAR_FLAG_BANK2(flag) \
    (TestFlashErrors() &= ~((flag) & 0x7fffffffU))
#define __HAL_FLASH_CLEAR_FLAG(flag) \
    (TestFlashErrors() &= ~((flag) & 0x7fffffffU))

// Installed by a test's main with the guard on.
inline void TestFlashGuard(void (*handler)(int)) {
  TestFlash().guard = true;
  TestFlash().exposed = true;
  signal(SIGSEGV, handler);
  signal(SIGBUS, handler);
}

#endif  // BEADS_TEST_FLASH_STUB_H_
