#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
#
# Author: Insolent Construct (insolentconstruct@gmail.com)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# See http://creativecommons.org/licenses/MIT/ for more information.
#
# Firmware mutation checks in a disposable host test copy: each recorded bug
# fix or optimization is broken in turn and the named test must fail.

import json
import pathlib
import re
import resource
import shutil
import subprocess
import sys
import tempfile
import time

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
root = pathlib.Path.cwd()
# (test, suite, file, text, replacement): breaking the text must make the
# named test fail. Tuples of equal length in the last three give several edits.
cases = [
  # #33: frozen delay slices stay inside the recording and at least one sample.
  ('TestFrozenSliceBoundarySelection', 'delay', 'beads/dsp/delay_controller.cc',
    'CONSTRAIN(slice_duration, 1, max_delay_);',
    'CONSTRAIN(slice_duration, 0, max_delay_);'),
  # #57: the idle counter saturates.
  ('TestHermiteRawMatchesScaled', 'delay', 'beads/dsp/delay_line.h',
    'const int32_t b_neg = w + a;',
    'const int32_t b_neg = w + a + 1;'),
  ('TestHermiteRawMatchesScaled', 'delay', 'beads/dsp/delay_line.h',
    'result.l = result.r = i ? odd : even;',
    'result.l = result.r = even;'),
  ('TestClockIdleCounter', 'delay', 'beads/dsp/delay_clock.cc',
    '  if (next_interval_ >= max_interval_) {\n'
    '    next_interval_ = max_interval_;\n'
    '  }\n',
    ''),
  # #36: backups are ordered, checked and never half-valid.
  # Header-sector-first and clearing the stale flag each protect an old stock
  # backup against an interrupted replacement: only both together are visible.
  ('TestPowerLoss', 'backup',
    ('beads/background_buffer_backup.cc', 'beads/background_buffer_backup.cc'),
    ('const uint32_t sector = kNumSectors - remaining_sectors_;',
     'SaveState(false);  // Stale: its backup was not loaded at boot.'),
    ('const uint32_t sector = remaining_sectors_ - 1;',
     '(void)0;  // Stale: its backup was not loaded at boot.')),
  ('TestSettingsSaves', 'backup', 'beads/background_buffer_backup.cc',
    '    HAL_FLASH_Unlock();  // A settings save may have locked it.\n',
    ''),
  ('TestStock', 'backup', 'beads/background_buffer_backup.cc',
    '      header.write_ptr >= kMaxDelay[header.media] << header.mono) {',
    '      header.write_ptr > kMaxDelay[header.media] << header.mono) {'),
  ('TestStock', 'backup', 'beads/background_buffer_backup.cc',
    '      header.write_ptr < 0 ||',
    '      false ||'),
  ('TestStock', 'backup', 'beads/background_buffer_backup.cc',
    '      header.mono > 1 ||',
    '      header.mono > 2 ||'),
  ('TestCorruption', 'backup', 'beads/background_buffer_backup.cc',
    '  return crc32(crc, &h, sizeof(h)) == header.crc;',
    '  return crc32(crc, &h, sizeof(h)) == header.crc || true;'),
  ('TestFormat', 'backup', 'beads/background_buffer_backup.cc',
    '  h.crc = 0;',
    '  h.crc = 0; h.write_ptr = 0;'),
  ('TestFormat', 'backup', 'beads/background_buffer_backup.cc',
    'i < kDelayBufferSizeBits / 8; i += sizeof(block)) {',
    'i < kDelayBufferSizeBits / 16; i += sizeof(block)) {'),
  ('TestFormat', 'backup', 'beads/background_buffer_backup.cc',
    '      header.crc = crc32(crc_, &header, sizeof(header));',
    '      header.crc = crc_;'),
  ('TestStock', 'backup', 'beads/background_buffer_backup.cc',
    '  if (header.crc_tag != kCrcTag) {\n'
    '    return true;',
    '  if (header.crc_tag != kCrcTag) {\n'
    '    return false;'),
  # #56: failed saves back off, completion and a long thaw reset the wait.
  ('TestFreezeDeadlineAndCancel', 'backup', 'beads/background_buffer_backup.cc',
    'kBackupDelay = 10000;',
    'kBackupDelay = 9999;'),
  ('TestModulatedFreeze', 'backup', 'beads/background_buffer_backup.cc',
    '(now - freeze_start_time_) >= backup_delay_',
    '(now - freeze_start_time_) >= kBackupDelay'),
  ('TestBackupBackoff', 'backup', 'beads/background_buffer_backup.cc',
    'kMaxBackupDelay = 320000',
    'kMaxBackupDelay = 640000'),
  ('TestBackupCompletionReset', 'backup', 'beads/background_buffer_backup.cc',
    '      backup_delay_ = kBackupDelay;\n      SaveState(true);',
    '      SaveState(true);'),
  ('TestBackupThawReset', 'backup', 'beads/background_buffer_backup.cc',
    'kBackupResetDelay = 60000',
    'kBackupResetDelay = 59000'),
  ('TestBackupEarlyCancel', 'backup', 'beads/background_buffer_backup.cc',
    '    if (save_started_) {',
    '    if (freeze_state_) {'),
  ('TestBackupClockWrap', 'backup', 'beads/background_buffer_backup.cc',
    '(now - freeze_start_time_) >= backup_delay_',
    'now >= freeze_start_time_ + backup_delay_'),
  ('TestBackupRestoredThaw', 'backup', 'beads/background_buffer_backup.cc',
    '    if (freeze_state_) {',
    '    if (freeze_state_ && save_started_) {'),
  # #35: the wavetable ends with a duplicated guard row.
  ('TestGuardRow', 'wavetable', 'beads/resources.cc', None, None),
  # Synth grains computed in pairs mix each grain's own sample.
  ('TestSynthGrainPairs', 'wavetable',
    'beads/dsp/wavetable_granular_player.h',
    'b->Mix(sample_b, out);',
    'b->Mix(sample_a, out);'),
  # Grains are summed locally and added once: both channels (the audio
  # reference comparison does not reach this path).
  ('TestDryWetEndpoints', 'mix', 'beads/dsp/delay_granular_player.h',
    '    *out += sum;',
    '    out->l += sum.l;'),
  # #149: settings saves check every flash operation and only program erased
  # flash.
  ('TestStaleErrorFlag', 'storage', 'beads/settings_storage.h',
    '      ClearErrors();\n      FLASH_EraseInitTypeDef',
    '      FLASH_EraseInitTypeDef'),
  ('TestStaleErrorFlag', 'storage', 'beads/settings_storage.h',
    '      ClearErrors();\n      if (HAL_FLASH_Program(',
    '      if (HAL_FLASH_Program('),
  ('TestFailedErase', 'storage', 'beads/settings_storage.h',
    '      if (!Flash::IsBlank(address, BLOCK_SIZE)) {\n        return false;\n      }\n',
    ''),
  ('TestFailedProgram', 'storage', 'beads/settings_storage.h',
    'const int kFlashAttempts = 3;',
    'const int kFlashAttempts = 1;'),
  ('TestNonBlankSlot', 'storage', 'beads/settings_storage.h',
    '      if (WriteChunk(chunk_address(next_state_chunk_index_), state_data_)) {',
    '      WriteChunk(chunk_address(next_state_chunk_index_), state_data_);\n'
    '      if (true) {'),
  # #148: the boot guard (masked reads, calibration mirror, write lock, SEED
  # repair, storage warning) and damaged frozen buffers; #165: no flash
  # writes on a low supply.
  ('TestLowSupply', 'storage', 'beads/settings.cc',
    '  Flash::Init();  // Every write checks the supply from here on.',
    '  // Every write checks the supply from here on.'),
  ('TestLowSupply', 'storage', 'beads/drivers/flash.h',
    '    MODIFY_REG(PWR->CR1, PWR_CR1_PLS, PWR_CR1_PLS_LEV6);',
    '    MODIFY_REG(PWR->CR1, PWR_CR1_PLS, PWR_CR1_PLS);'),
  ('TestLowSupply', 'storage', 'beads/drivers/flash.h',
    '    MODIFY_REG(PWR->CR1, PWR_CR1_PLS, PWR_CR1_PLS_LEV6);',
    '    PWR->CR1 = PWR_CR1_PLS_LEV6;'),
  ('TestLowSupply', 'storage', 'beads/drivers/flash.h',
    '    return READ_BIT(PWR->CSR1, PWR_CSR1_PVDO);',
    '    return false;'),
  ('TestLowSupply', 'storage', 'beads/settings_storage.h',
    '    if (Flash::SupplyLow()) {\n'
    '      return;  // Kept in RAM; the next save writes it.\n'
    '    }\n',
    ''),
  ('TestSupplyDrop', 'storage', 'beads/settings_storage.h',
    '      if (Flash::SupplyLow()) {\n'
    '        return false;\n'
    '      }\n'
    '      ClearErrors();\n'
    '      FLASH_EraseInitTypeDef',
    '      ClearErrors();\n'
    '      FLASH_EraseInitTypeDef'),
  ('TestSupplyDrop', 'storage', 'beads/settings_storage.h',
    '      if (Flash::SupplyLow()) {\n'
    '        return false;\n'
    '      }\n'
    '      ClearErrors();\n'
    '      if (HAL_FLASH_Program(',
    '      ClearErrors();\n'
    '      if (HAL_FLASH_Program('),
  ('TestLowSupplyBackup', 'backup', 'beads/background_buffer_backup.cc',
    '    if (Flash::SupplyLow()) {\n'
    '      return false;\n'
    '    }\n',
    ''),
  ('TestLowSupplyBackup', 'backup', 'beads/background_buffer_backup.cc',
    '    if (Flash::SupplyLow()) {\n'
    '      remaining_sectors_ = remaining_bytes_ = 0;\n'
    '      return;\n'
    '    }\n',
    ''),
  ('TestSupplyDropBackup', 'backup', 'beads/background_buffer_backup.cc',
    '      if (Flash::SupplyLow()) {\n'
    '        return;\n'
    '      }\n',
    ''),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '      SCB->CFSR = SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk;',
    '      SCB->CFSR = SCB_CFSR_PRECISERR_Msk;'),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '      SCB->CFSR = SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk;\n'
    '      __DSB();\n'
    '    }\n'
    '    __set_FAULTMASK(faultmask);',
    '      __DSB();\n'
    '    }\n'
    '    __set_FAULTMASK(faultmask);\n'
    '    if (!clean) {\n'
    '      SCB->CFSR = SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk;\n'
    '    }'),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '    __set_FAULTMASK(1);',
    '    __set_FAULTMASK(0);'),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '    SCB->CCR = ccr | SCB_CCR_BFHFNMIGN_Msk;',
    '    SCB->CCR = ccr;'),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '    __set_FAULTMASK(faultmask);',
    '    __set_FAULTMASK(0 * faultmask);'),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '    SCB->CCR = ccr;\n    __DSB();',
    '    __DSB();'),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '    __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_DBECCERR_BANK2);\n    // With',
    '    // With'),
  ('TestMaskedReader', 'storage', 'beads/drivers/flash.h',
    '      // Keep the undefined words out of the cache.\n'
    '      Invalidate(address, size);',
    '      // Keep the undefined words out of the cache.'),
  ('TestCalibrationFromMirror', 'storage', 'beads/settings_storage.h',
    '    if (calibration != CHUNK_VALID) {\n      return STORAGE_CALIBRATION_LOST;',
    '    if (calibration == CHUNK_UNREADABLE) {\n      return STORAGE_CALIBRATION_LOST;'),
  ('TestCalibrationLost', 'storage', 'beads/settings_storage.h',
    '    if (calibration != CHUNK_VALID) {\n      return STORAGE_CALIBRATION_LOST;',
    '    if (calibration == CHUNK_UNREADABLE) {\n      return STORAGE_CALIBRATION_LOST;'),
  ('TestBlankSector', 'storage', 'beads/settings_storage.h',
    '      if (mirror == CHUNK_VALID) {\n        return STORAGE_RESTORED;',
    '      if (mirror == CHUNK_VALID) {\n        return STORAGE_NEW;'),
  ('TestBlankSector', 'storage', 'beads/settings_storage.h',
    '          ? STORAGE_NEW\n          : STORAGE_DAMAGED;',
    '          ? STORAGE_NEW\n          : STORAGE_NEW;'),
  ('TestStateTerminator', 'storage', 'beads/settings_storage.h',
    '      if (ReadChunk(chunk_address(index), state_data) != CHUNK_VALID) {',
    '      if (ReadChunk(chunk_address(index), state_data) == CHUNK_INVALID) {'),
  ('TestCalibrationWithoutState', 'storage', 'beads/settings_storage.h',
    '      // Rewritten, then the power went before the first state.\n'
    '      return STORAGE_RESTORED;',
    '      // Rewritten, then the power went before the first state.\n'
    '      return STORAGE_NEW;'),
  ('TestWriteLock', 'storage', 'beads/settings_storage.h',
    '  void Lock() {\n    locked_ = true;',
    '  void Lock() {\n    locked_ = false;'),
  ('TestWriteLock', 'storage', 'beads/settings_storage.h',
    '  bool EraseSector(uint32_t index, uint32_t address) {\n    if (locked_) {',
    '  bool EraseSector(uint32_t index, uint32_t address) {\n    if (false) {'),
  ('TestWriteLock', 'storage', 'beads/settings_storage.h',
    '  bool ProgramWord(uint32_t address, const uint8_t* buffer) {\n'
    '    if (locked_) {',
    '  bool ProgramWord(uint32_t address, const uint8_t* buffer) {\n'
    '    if (false) {'),
  ('TestMirrorWrites', 'storage', 'beads/settings_storage.h',
    '      erase_params.Sector = index % 8;  // Within its bank.',
    '      erase_params.Sector = index;  // Within its bank.'),
  ('TestInterruptedRewrite', 'storage', 'beads/settings_storage.h',
    '    if (MirrorPersistentData() && \\',
    '    if (true && \\'),
  ('TestMirrorWrites', 'storage', 'beads/settings_storage.h',
    '        !memcmp(&mirror, persistent_data_, sizeof(PersistentData))) {\n'
    '      return true;',
    '        false) {\n'
    '      return true;'),
  ('TestMirrorWrites', 'storage', 'beads/settings_storage.h',
    '        WriteChunk(MIRROR_BASE, persistent_data_) && \\',
    '        true && \\'),
  ('TestRepair', 'storage', 'beads/settings_storage.h',
    '    locked_ = false;\n    RewriteSector();',
    '    RewriteSector();'),
  ('TestRepairFailures', 'storage', 'beads/settings_storage.h',
    '    return !locked_;\n  }',
    '    return true;\n  }'),
  ('TestCalibrationFromMirror', 'storage', 'beads/settings.cc',
    '      (status == STORAGE_OK || status == STORAGE_RESTORED) && \\',
    '      false && \\'),
  ('TestRestoredCalibrationChoice', 'storage', 'beads/settings.cc',
    '      (status == STORAGE_OK || status == STORAGE_RESTORED) && \\',
    '      status == STORAGE_OK && \\'),
  ('TestCalibrationWithoutState', 'storage', 'beads/settings.cc',
    '  if (status == STORAGE_RESTORED) {\n    chunk_storage_.SavePersistentData();',
    '  if (false) {\n    chunk_storage_.SavePersistentData();'),
  ('TestRetriedBufferErase', 'backup', 'beads/background_buffer_backup.cc',
    '    if (!EraseBufferSector(sector, kFlashAttempts)) {',
    '    if (!EraseBufferSector(sector, 1)) {'),
  ('TestCalibrationLost', 'storage', 'beads/settings.cc',
    '      CalibrationPlausible(mirror, factory)) {',
    '      true) {'),
  ('TestCalibrationFromMirror', 'storage', 'beads/settings.cc',
    '    if (recovery_mode) {\n      state_ = defaults;\n    }\n'
    '    status = STORAGE_RESTORED;',
    '    status = STORAGE_RESTORED;'),
  ('TestCalibrationLost', 'storage', 'beads/settings.cc',
    '  } else if (lost) {\n    status = STORAGE_DAMAGED;',
    '  } else if (lost) {\n    status = STORAGE_OK;'),
  ('TestCalibrationLost', 'storage', 'beads/settings.cc',
    '    chunk_storage_.Lock();\n',
    ''),
  ('TestCalibrationLost', 'storage', 'beads/settings.cc',
    '    chunk_storage_.Lock();\n    state_ = defaults;',
    '    chunk_storage_.Lock();'),
  ('TestCalibrationFromMirror', 'storage', 'beads/settings.cc',
    '    if (!(fabsf(c.scale / f.scale - 1.0f) <= 0.25f) || \\',
    '    if (!(fabsf(c.scale / f.scale - 1.0f) <= 0.8f) || \\'),
  ('TestRepair', 'storage', 'beads/settings.cc',
    '      repair_ = chunk_storage_.Repair()\n',
    '      repair_ = !chunk_storage_.Repair()\n'),
  ('TestInterruptedRewrite', 'storage', 'beads/settings.cc',
    '  return status != STORAGE_NEW || recovery_mode;',
    '  return status == STORAGE_OK || recovery_mode;'),
  ('TestBlankSector', 'storage', 'beads/settings.cc',
    '  return status != STORAGE_NEW || recovery_mode;',
    '  return status != STORAGE_NEW;'),
  ('TestDamagedBuffer', 'backup', 'beads/background_buffer_backup.cc',
    '    if (Flash::Read(Sector<8>::start, &header, sizeof(header)) && \\',
    '    if ((memcpy(&header, (const void*)(Sector<8>::start), sizeof(header)),'
    ' true) && \\'),
  ('TestDamagedBuffer', 'backup', 'beads/background_buffer_backup.cc',
    '            Sector<8>::start + kFlashWordSize + uint32_t(i), block, size)) {\n'
    '      return false;',
    '            Sector<8>::start + kFlashWordSize + uint32_t(i), block, size)) {\n'
    '      return true;'),
  ('TestDamagedBuffer', 'backup', 'beads/background_buffer_backup.cc',
    '        readable = Flash::Read(address, words, kFlashWordSize);',
    '        Flash::Read(address, words, kFlashWordSize);'),
  ('TestDamagedBuffer', 'backup', 'beads/background_buffer_backup.cc',
    '          fill(&bytes[0], &bytes[kFlashWordSize], 0);',
    '          (void)bytes;'),
  ('TestStaleFlags', 'backup', 'beads/background_buffer_backup.cc',
    '    Flash::ClearErrors();\n'
    '    if (HAL_FLASHEx_Erase(&erase_params, &sector_error) == HAL_OK && \\',
    '    if (HAL_FLASHEx_Erase(&erase_params, &sector_error) == HAL_OK && \\'),
  ('TestStaleFlags', 'backup', 'beads/background_buffer_backup.cc',
    '    Flash::ClearErrors();\n    if (settings_->write_locked() || HAL_FLASH_Program(',
    '    if (settings_->write_locked() || HAL_FLASH_Program('),
  ('TestStaleFlags', 'backup', 'beads/background_buffer_backup.cc',
    '      Flash::ClearErrors();\n      if (settings_->write_locked() || HAL_FLASH_Program(',
    '      if (settings_->write_locked() || HAL_FLASH_Program('),
  ('TestLockedBackup', 'backup', 'beads/background_buffer_backup.cc',
    'int attempts) {\n  if (settings_->write_locked()) {',
    'int attempts) {\n  if (false) {'),
  ('TestLockedBackup', 'backup', 'beads/background_buffer_backup.cc',
    '    if (settings_->write_locked() || HAL_FLASH_Program(\n'
    '        FLASH_TYPEPROGRAM_FLASHWORD,\n'
    '        flash_address,',
    '    if (false || HAL_FLASH_Program(\n'
    '        FLASH_TYPEPROGRAM_FLASHWORD,\n'
    '        flash_address,'),
  ('TestEraseDamagedSectors', 'backup', 'beads/background_buffer_backup.cc',
    '    if (!Flash::IsReadable(Sector<8>::start + i * kSectorSize, kSectorSize)) {',
    '    if (true) {'),
  ('TestEraseDamagedSectors', 'backup', 'beads/background_buffer_backup.cc',
    '      EraseBufferSector(i, kFlashAttempts);',
    '      EraseBufferSector(i, 1);'),
  ('TestStuckBufferErase', 'backup', 'beads/background_buffer_backup.cc',
    ' HAL_OK && \\\n'
    '        Flash::IsBlank(Sector<8>::start + sector * kSectorSize, kSectorSize)) {',
    ' HAL_OK) {'),
  ('TestStuckBufferErase', 'backup', 'beads/background_buffer_backup.cc',
    '    erase_params.Sector = sector;',
    '    erase_params.Sector = sector + 8;'),
  ('TestStorageRepair', 'firmware', 'beads/beads.cc',
    '    background_buffer_backup.EraseDamagedSectors();',
    '    (void)0;'),
  ('TestStorageWarning', 'firmware', 'beads/beads.cc',
    '    ui.ShowStorageStatus();\n',
    ''),
  ('TestStorageWarning', 'firmware', 'beads/ui.cc',
    '      mode_ = MODE_STORAGE_WARNING;',
    '      mode_ = MODE_NORMAL;'),
  ('TestStorageWarning', 'firmware', 'beads/ui.cc',
    '  const bool accept_switch_events = !showing_storage_status();',
    '  const bool accept_switch_events = true;'),
  ('TestStorageWarning', 'firmware', 'beads/ui.cc',
    'const uint32_t kStorageStatusTime = 3000;',
    'const uint32_t kStorageStatusTime = 4000;'),
  ('TestStorageWarning', 'firmware', 'beads/ui.cc',
    '    case MODE_PANIC:\n    case MODE_STORAGE_WARNING:\n',
    '    case MODE_STORAGE_WARNING:\n      break;\n    case MODE_PANIC:\n'),
  ('TestStorageWarning', 'firmware', 'beads/ui.cc',
    '  if (mode_ != MODE_FACTORY_TEST && !showing_storage_status()) {',
    '  if (mode_ != MODE_FACTORY_TEST) {'),
  ('TestStorageRepair', 'firmware', 'beads/ui.cc',
    '          color.g = blink ? 255 : 0;',
    '          color.g = blink ? 255 : 255;'),
  ('TestStorageRepair', 'firmware', 'beads/ui.cc',
    '          color.g = blink ? 255 : 0;\n'
    '          for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {\n'
    '            leds_.set(Led(LED_ASSIGN_1 + i), 255);',
    '          color.g = blink ? 255 : 0;\n'
    '          for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {\n'
    '            leds_.set(Led(LED_ASSIGN_1 + i), 0);'),
  ('TestStorageRepair', 'firmware', 'beads/ui.cc',
    '          color.r = fast_blink ? 255 : 0;',
    '          color.r = 255;'),
  ('TestRepair', 'storage', 'beads/settings.cc',
    '    if (recovery_mode && status == STORAGE_OK) {',
    '    if (false) {'),
  # #53: codec restarts fade out first, FREEZE cancels the fade, and leaving
  # the synth clears its reverb.
  ('TestCodecRestartFade', 'firmware', 'beads/beads.cc',
    '      if (!FadedOut()) {\n        return;\n      }\n      // elapsed',
    '      // elapsed'),
  ('TestCodecRestartFade', 'firmware', 'beads/beads.cc',
    '    codec_fade = false;\n    return;',
    '    return;'),
  ('TestSynthStopClearsReverb', 'firmware', 'beads/beads.cc',
    '        processor.ClearReverb();\n',
    ''),
]
GUARD = 'const int16_t wav_integrated_waves[] = {'
# A case with several edits gives tuples of equal length.
def listed(value):
  return value if isinstance(value, tuple) else (value,)
cases = [(n, s, listed(f), listed(b), listed(a)) for n, s, f, b, a in cases]
# Validate every replacement before creating a disposable source tree.
for name, suite, sources, befores, afters in cases:
  assert len(sources) == len(befores) == len(afters), name
  for source, before in zip(sources, befores):
    pattern = before if before is not None else GUARD
    assert (root / source).read_text().count(pattern) == 1, (name, pattern)


def shard_indices(count, shard, shards):
  # Shard K of N runs every case whose 1-based index is K modulo N, so the
  # N shards together run each case exactly once.
  return [i for i in range(1, count + 1) if (i - 1) % shards == shard - 1]


for shards in range(1, 17):
  indices = sorted(i for shard in range(1, shards + 1)
      for i in shard_indices(len(cases), shard, shards))
  assert indices == list(range(1, len(cases) + 1)), shards
if '--patterns-only' in sys.argv:
  print('All %d mutation patterns occur exactly once' % len(cases))
  raise SystemExit(0)

if '--merge' in sys.argv:
  # Joins shard reports into one, as a serial run writes it; every case must
  # appear exactly once.
  arguments = sys.argv[sys.argv.index('--merge') + 1:]
  out = pathlib.Path(arguments[0]).resolve()
  out.mkdir(parents=True, exist_ok=True)
  merged = {}
  for directory in map(pathlib.Path, arguments[1:]):
    for index, result in json.loads((directory / 'results.json').read_text()):
      assert index not in merged, index
      merged[index] = result
    for log in directory.glob('*.log'):
      shutil.copy(log, out / log.name)
  missing = sorted(set(range(1, len(cases) + 1)) - set(merged))
  assert not missing, 'cases not run: %s' % missing
  assert set(merged) == set(range(1, len(cases) + 1)), 'unexpected case indices'
  results = [merged[index] for index in sorted(merged)]
  (out / 'mutations.txt').write_text('\n'.join(results) + '\n')
  print('Merged %d mutation results' % len(results))
  raise SystemExit(0)

assert not ('--only' in sys.argv and '--shard' in sys.argv), \
    '--only and --shard select cases differently; use one'
if '--only' in sys.argv:
  selected = sys.argv[sys.argv.index('--only') + 1]
  cases = [case for case in cases if case[0] == selected]
  assert cases, selected
  del sys.argv[sys.argv.index('--only'):sys.argv.index('--only') + 2]

selected_indices = set(range(1, len(cases) + 1))
if '--shard' in sys.argv:
  shard, shards = map(int,
      sys.argv[sys.argv.index('--shard') + 1].split('/'))
  assert 1 <= shard <= shards, (shard, shards)
  selected_indices = set(shard_indices(len(cases), shard, shards))
  del sys.argv[sys.argv.index('--shard'):sys.argv.index('--shard') + 2]

out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else
    'beads/test/.runs/mutations').resolve()
out.mkdir(parents=True, exist_ok=True)
jobs = '-j4'
started = time.monotonic()
results = []
indexed = []
survivors = []
with tempfile.TemporaryDirectory(prefix='.mutation-', dir='beads/test') as tmp:
  work = pathlib.Path(tmp).resolve()
  for directory in ('beads', 'stmlib'):
    shutil.copytree(root / directory, work / directory,
        ignore=shutil.ignore_patterns('.runs', '.mutation-*',
            '__pycache__', 'core*', 'build'))
  for index, (name, suite, sources, befores, afters) in enumerate(cases, 1):
    if index not in selected_indices:
      continue
    paths = [work / source for source in sources]
    originals = [path.read_text() for path in paths]
    source = sources[0]
    line = 0
    change = []
    for path, before, after in zip(paths, befores, afters):
      original = path.read_text()
      if before is not None:
        mutated = original.replace(before, after)
        change.append(before + ' -> ' + after)
      else:
        # Drop the duplicated final row to expose the overread.
        start = original.index(GUARD)
        end = original.index('};', start)
        values = list(re.finditer(r'-?\d+', original[start:end]))
        cut = start + values[-260].start()
        mutated = original[:cut] + original[end:]
        change.append('remove the final 260-sample guard row')
      if not line:
        line = original.count('\n', 0, original.index(
            before if before is not None else GUARD)) + 1
      path.write_text(mutated)
    change = ' AND '.join(change)
    test = work / ('beads/test/' + suite + '_test.cc')
    original_test = test.read_text()
    log = out / ('%02d-%s.log' % (index, name))
    try:
      if suite != 'wavetable':
        # Run only the named test: it must fail by itself.
        main = original_test.index('\nint main(')
        first = re.compile(r'\n  Test\w+\(\);').search(
            original_test, main).group(0)[1:]
        cut = original_test.index(first, main)
        test.write_text(original_test[:cut] + name + '();\n' +
            '  return test_checker.failures() ? 1 : 0;\n}\n')
      command = ['nice', '-n', '10', 'make', '-s', jobs, '-f',
          'beads/test/' + suite + '.mk', 'test']
      with log.open('w') as stream:
        run = subprocess.run(command, cwd=work, stdout=stream,
            stderr=subprocess.STDOUT, timeout=600)
      failure = log.read_text()
      # A mutation that does not compile proves nothing.
      assert not re.search(r'^\S+:\d+:\d+: (fatal )?error:', failure,
          re.MULTILINE), failure
      if run.returncode == 0:
        survivors.append(f'{name}: {source}:{line}: {change}')
        print('SURVIVED ' + survivors[-1], flush=True)
        continue
      # The named test fails, or a sanitizer trips inside it.
      assert re.search('ERROR: ' + name + ':|runtime error: |ERROR: '
          'AddressSanitizer:', failure), failure
      evidence = next(x for x in failure.splitlines()
          if re.search('ERROR: |runtime error: ', x))
      results.append(f'{name}: {source}:{line}: {change}\n  {evidence}')
      indexed.append((index, results[-1]))
      print(results[-1], flush=True)
    finally:
      for path, original in zip(paths, originals):
        path.write_text(original)
      test.write_text(original_test)
(out / 'mutations.txt').write_text('\n'.join(results) + '\n')
(out / 'results.json').write_text(json.dumps(indexed) + '\n')
print('Mutation runtime: %.2f s' % (time.monotonic() - started))
if survivors:
  print('%d mutations survived their tests' % len(survivors), file=sys.stderr)
  raise SystemExit(1)
