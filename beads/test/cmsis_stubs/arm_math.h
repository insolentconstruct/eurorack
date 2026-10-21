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
// Host stand-in for CMSIS-DSP V1.4.4's arm_math.h: the types and real FFT
// declarations the spectral engine uses, so the firmware's CMSIS sources
// build on the desktop. Defining the real header's guard keeps it out when
// arm_common_tables.h and arm_const_structs.h include it.

#ifndef _ARM_MATH_H
#define _ARM_MATH_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALIGN4 __attribute__((aligned(4)))
#define FAST_MATH_TABLE_SIZE 512

typedef int8_t q7_t;
typedef int16_t q15_t;
typedef int32_t q31_t;
typedef int64_t q63_t;
typedef float float32_t;
typedef double float64_t;

typedef enum {
  ARM_MATH_SUCCESS = 0,
  ARM_MATH_ARGUMENT_ERROR = -1,
  ARM_MATH_LENGTH_ERROR = -2,
  ARM_MATH_SIZE_MISMATCH = -3,
  ARM_MATH_NANINF = -4,
  ARM_MATH_SINGULAR = -5,
  ARM_MATH_TEST_FAILURE = -6
} arm_status;

typedef struct {
  uint16_t fftLen;
  const q15_t* pTwiddle;
  const uint16_t* pBitRevTable;
  uint16_t bitRevLength;
} arm_cfft_instance_q15;

typedef struct {
  uint16_t fftLen;
  const q31_t* pTwiddle;
  const uint16_t* pBitRevTable;
  uint16_t bitRevLength;
} arm_cfft_instance_q31;

typedef struct {
  uint16_t fftLen;
  const float32_t* pTwiddle;
  const uint16_t* pBitRevTable;
  uint16_t bitRevLength;
} arm_cfft_instance_f32;

typedef struct {
  arm_cfft_instance_f32 Sint;
  uint16_t fftLenRFFT;
  float32_t* pTwiddleRFFT;
} arm_rfft_fast_instance_f32;

void arm_cfft_f32(
    const arm_cfft_instance_f32* S,
    float32_t* p1,
    uint8_t ifftFlag,
    uint8_t bitReverseFlag);

arm_status arm_rfft_fast_init_f32(
    arm_rfft_fast_instance_f32* S,
    uint16_t fftLen);

void arm_rfft_fast_f32(
    arm_rfft_fast_instance_f32* S,
    float32_t* p,
    float32_t* pOut,
    uint8_t ifftFlag);

#ifdef __cplusplus
}
#endif

#endif  // _ARM_MATH_H
