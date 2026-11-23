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
//
// Per-input exception flags and the transitive expm1 result.
#ifndef BEADS_TEST_LIBM_BLOB_EMU_DIAGNOSTICS_H_
#define BEADS_TEST_LIBM_BLOB_EMU_DIAGNOSTICS_H_

#include <stdint.h>

typedef struct {
  uint32_t control;
  float expm1[128];
  uint32_t flags[3][128];
} Diagnostics;

static inline void ClearExceptions(uint32_t control) {
  __asm__ volatile ("vmsr fpscr, %0" : : "r" (control) : "memory");
}

static inline uint32_t Exceptions(void) {
  uint32_t flags;
  __asm__ volatile ("vmrs %0, fpscr" : "=r" (flags) : : "memory");
  return flags & 0x0fffffff;
}

#endif  // BEADS_TEST_LIBM_BLOB_EMU_DIAGNOSTICS_H_
