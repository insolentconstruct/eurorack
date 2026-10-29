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
    ('erase_params.Sector = 8 + kNumSectors - remaining_sectors_;',
     'SaveState(false);  // Stale: its backup was not loaded at boot.'),
    ('erase_params.Sector = 8 + remaining_sectors_ - 1;',
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
    '      kDelayBufferSizeBits / 8);\n'
    '  return crc32(crc, &h',
    '      kDelayBufferSizeBits / 16);\n'
    '  return crc32(crc, &h'),
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
    '      if (!IsBlank(address, BLOCK_SIZE)) {\n        return false;\n      }\n',
    ''),
  ('TestFailedProgram', 'storage', 'beads/settings_storage.h',
    'const int kFlashAttempts = 3;',
    'const int kFlashAttempts = 1;'),
  ('TestNonBlankSlot', 'storage', 'beads/settings_storage.h',
    '      if (WriteChunk(next_state_chunk_index_, state_data_)) {',
    '      WriteChunk(next_state_chunk_index_, state_data_);\n      if (true) {'),
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
if '--patterns-only' in sys.argv:
  print('All %d mutation patterns occur exactly once' % len(cases))
  raise SystemExit(0)

if '--only' in sys.argv:
  selected = sys.argv[sys.argv.index('--only') + 1]
  cases = [case for case in cases if case[0] == selected]
  assert cases, selected
  del sys.argv[sys.argv.index('--only'):sys.argv.index('--only') + 2]

out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else
    'beads/test/.runs/mutations').resolve()
out.mkdir(parents=True, exist_ok=True)
jobs = '-j4'
started = time.monotonic()
results = []
survivors = []
with tempfile.TemporaryDirectory(prefix='.mutation-', dir='beads/test') as tmp:
  work = pathlib.Path(tmp).resolve()
  for directory in ('beads', 'stmlib'):
    shutil.copytree(root / directory, work / directory,
        ignore=shutil.ignore_patterns('.runs', '.mutation-*',
            '__pycache__', 'core*', 'build'))
  for index, (name, suite, sources, befores, afters) in enumerate(cases, 1):
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
        main = original_test.index('int main(')
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
      print(results[-1], flush=True)
    finally:
      for path, original in zip(paths, originals):
        path.write_text(original)
      test.write_text(original_test)
(out / 'mutations.txt').write_text('\n'.join(results) + '\n')
print('Mutation runtime: %.2f s' % (time.monotonic() - started))
if survivors:
  print('%d mutations survived their tests' % len(survivors), file=sys.stderr)
  raise SystemExit(1)
