#ifndef OPTIONS_TEST_HAL_H_
#define OPTIONS_TEST_HAL_H_
#include <cstdint>
#include <cstring>
struct GPIO_TypeDef { };
struct GPIO_InitTypeDef { uint32_t Mode, Pull, Pin; };
struct FLASH_EraseInitTypeDef {
  uint32_t TypeErase, Banks, Sector, NbSectors, VoltageRange;
};
extern bool test_down[4];
extern unsigned test_writes, test_erases;
extern bool test_interrupt;
inline int HAL_GPIO_ReadPin(GPIO_TypeDef*, uint16_t pin) {
  return !test_down[pin];
}
inline void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*) { }
inline void HAL_FLASH_Unlock() { }
inline void HAL_FLASH_Lock() { }
inline int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef*, uint32_t*) {
  ++test_erases;
  memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
  return 0;
}
inline int HAL_FLASH_Program(int, uint32_t address, uint64_t source) {
  ++test_writes;
  auto dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  auto src = reinterpret_cast<uint8_t*>(uintptr_t(source));
  for (int i = 0; i < (test_interrupt ? 8 : 32); ++i) dst[i] &= src[i];
  return 0;
}
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP 0
#define FLASH_TYPEERASE_SECTORS 0
#define FLASH_BANK_1 0
#define FLASH_BANK_2 1
#define FLASH_VOLTAGE_RANGE_4 0
#define FLASH_TYPEPROGRAM_FLASHWORD 0
#endif
