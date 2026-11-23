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
// Host-only simulated flash and image I/O for the compatibility drivers.

#ifndef BEADS_TEST_RECOVERY_COMPAT_RUNTIME_H_
#define BEADS_TEST_RECOVERY_COMPAT_RUNTIME_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <stm32h7xx_hal.h>

inline void Require(bool ok, const char* message) {
  if (!ok) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
  }
}

#ifdef OLD_STUBS
SCB_Type test_scb;
PWR_TypeDef test_pwr;
bool test_locked = true;
bool test_ecc_error, test_supply_low;
int test_ecc_read, test_masked_reads, test_fail_programs;
int test_silent_programs, test_fail_erases, test_silent_erases;
int test_erase_calls, test_programs, test_low_supply_writes;
int test_cut_at, test_cut_model, test_operations, test_low_after;
int test_sector_programs[8], test_damage_programs;
int test_fail_sector = -1;
uint32_t test_faultmask, test_sr2, test_erased;
void TestProtect() { }
void TestErase(uint32_t address, size_t size) {
  memset(reinterpret_cast<void*>(address), 255, size);
}
void TestProgram(uint32_t address, uint64_t source) {
  uint8_t* dest = reinterpret_cast<uint8_t*>(address);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(source);
  for (int i = 0; i < 32; ++i) {
    Require(dest[i] == 255, "program onto nonblank simulated flash");
    dest[i] = src[i];
  }
}
inline uint32_t Programs() { return test_programs; }
inline uint32_t Erases() { return test_erase_calls; }
#else
bool test_down[4];
uint32_t test_writes;
inline uint32_t Programs() { return test_writes; }
inline uint32_t Erases() { return TestFlashErases(); }
#endif

inline void* Bank() { return reinterpret_cast<void*>(0x08100000); }
inline void MapBank() {
  Require(mmap(Bank(), 0x100000, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == Bank(),
      "map bank 2");
  memset(Bank(), 255, 0x100000);
}
inline void Image(const char* path, bool save) {
  FILE* f = fopen(path, save ? "wb" : "rb");
  Require(f != NULL, "open image");
  size_t size = save ? fwrite(Bank(), 1, 0x100000, f)
      : fread(Bank(), 1, 0x100000, f);
  Require(size == 0x100000, "image size");
  if (!save) Require(fgetc(f) == EOF, "trailing image bytes");
  Require(fclose(f) == 0, "close image");
}

#endif  // BEADS_TEST_RECOVERY_COMPAT_RUNTIME_H_
