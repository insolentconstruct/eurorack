#ifndef BACKUP_TEST_HAL_H_
#define BACKUP_TEST_HAL_H_
#include <cstdint>
#include <cstring>
// Bank 2 flash, mapped at its real address by the test. Programming can only
// clear bits, like the hardware; a locked or failing operation does nothing.
struct FLASH_EraseInitTypeDef {
  uint32_t TypeErase, Banks, Sector, NbSectors, VoltageRange;
};
extern bool test_locked;
extern int test_budget;    // Operations left before power is lost; -1: none.
extern int test_fail_at;   // Operation number that fails; 0: none.
extern int test_ops;
extern bool test_half_erase;  // Power goes halfway through an erase.
inline bool TestOperation() {
  if (test_locked || test_budget == 0) return false;
  if (test_budget > 0) --test_budget;
  return ++test_ops != test_fail_at;
}
inline void HAL_FLASH_Unlock() { test_locked = false; }
inline void HAL_FLASH_Lock() { test_locked = true; }
inline int HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* e, uint32_t*) {
  if (test_half_erase && test_budget == 1 && !test_locked) {
    memset(reinterpret_cast<void*>(0x08100000 + (e->Sector - 8) * 0x20000),
           255, 0x10000);
    test_budget = 0;
    return 1;
  }
  if (!TestOperation()) return 1;
  memset(reinterpret_cast<void*>(0x08100000 + (e->Sector - 8) * 0x20000),
         255, 0x20000 * e->NbSectors);
  return 0;
}
inline int HAL_FLASH_Program(int, uint32_t address, uint64_t source) {
  if (!TestOperation()) return 1;
  uint8_t* dst = reinterpret_cast<uint8_t*>(uintptr_t(address));
  const uint8_t* src = reinterpret_cast<const uint8_t*>(uintptr_t(source));
  for (int i = 0; i < 32; ++i) dst[i] &= src[i];
  return 0;
}
#define HAL_OK 0
#define FLASH_TYPEERASE_SECTORS 0
#define FLASH_BANK_1 0
#define FLASH_BANK_2 1
#define FLASH_VOLTAGE_RANGE_4 0
#define FLASH_TYPEPROGRAM_FLASHWORD 0
#endif
