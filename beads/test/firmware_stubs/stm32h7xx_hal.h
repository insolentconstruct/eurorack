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
// GPIO and flash operations for UI tests.

#ifndef BEADS_TEST_FIRMWARE_STUBS_STM32H7XX_HAL_H_
#define BEADS_TEST_FIRMWARE_STUBS_STM32H7XX_HAL_H_

#include <cstdint>
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
extern bool test_down[4];
extern unsigned test_writes;
inline int HAL_GPIO_ReadPin(GPIO_TypeDef*, uint16_t pin) {
  return !test_down[pin];
}
inline void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*) { }
inline void HAL_FLASH_Unlock() { }
inline void HAL_FLASH_Lock() { }
inline int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* e, uint32_t*) {
  memset(reinterpret_cast<void*>(0x08100000 + (e->Sector - 8) * 0x20000),
      255, 0x20000 * e->NbSectors);
  return 0;
}
inline int HAL_FLASH_Program(int, uint32_t address, uint64_t source) {
  ++test_writes;
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  uint8_t* src = reinterpret_cast<uint8_t*>(uintptr_t(source));
  for (int i = 0; i < 32; ++i) {
    dst[i] &= src[i];
  }
  return 0;
}
inline void HAL_IncTick() { }
#define HAL_OK 0
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP 0
#define FLASH_TYPEERASE_SECTORS 0
#define FLASH_BANK_1 0
#define FLASH_BANK_2 1
#define FLASH_VOLTAGE_RANGE_4 0
#define FLASH_TYPEPROGRAM_FLASHWORD 0

#endif  // BEADS_TEST_FIRMWARE_STUBS_STM32H7XX_HAL_H_
