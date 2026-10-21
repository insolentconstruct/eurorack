// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// C version of CMSIS-DSP's arm_bitreversal_32, which the firmware assembles
// from arm_bitreversal2.s. The table holds pairs of byte offsets of complex
// values to swap.

#include "arm_math.h"

void arm_bitreversal_32(
    uint32_t* pSrc,
    const uint16_t bitRevLen,
    const uint16_t* pBitRevTable) {
  for (uint16_t i = 0; i < bitRevLen; i += 2) {
    const uint32_t a = pBitRevTable[i] >> 2;
    const uint32_t b = pBitRevTable[i + 1] >> 2;
    uint32_t tmp = pSrc[a];
    pSrc[a] = pSrc[b];
    pSrc[b] = tmp;
    tmp = pSrc[a + 1];
    pSrc[a + 1] = pSrc[b + 1];
    pSrc[b + 1] = tmp;
  }
}
