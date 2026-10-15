#!/usr/bin/env python3
# Run from the repository root. Mutations touch only a disposable test copy.
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
out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else
                   'beads/test/.runs/mutations').resolve()
out.mkdir(parents=True, exist_ok=True)
cases = [
    ('TestStockAssignRouting', 'options', 'beads/ui.cc',
     '(state->assignable_cv_destination + 1) % 3',
     '(state->assignable_cv_destination + 2) % 3'),
    ('TestStockAssignAmounts', 'options', 'beads/ui.cc',
     'p->hidden_parameter() * 256.0f', 'p->hidden_parameter() * 128.0f'),
    ('TestAutosaveChordAndDisplay', 'options', 'beads/ui.cc',
     'state->disable_autosave = !state->disable_autosave;',
     'state->disable_autosave = state->disable_autosave;'),
    ('TestSeedLatchGestures', 'options', 'beads/ui.cc',
     '4000, 4000, 1500,', '4000, 4100, 1500,'),
    ('TestGainHoldThresholdAndReload', 'options', 'beads/ui.cc',
     '4000, 4000, 1500,', '4000, 4000, 1600,'),
    ('TestFreezeDeadlineAndCancel', 'backup',
     'beads/background_buffer_backup.cc', 'kBackupDelay = 10000;',
     'kBackupDelay = 9999;'),
    ('TestBackupLedAndRestore', 'options', 'beads/ui.cc',
     'busy() ? 128 : 255', 'busy() ? 255 : 255'),
    ('TestFrozenSliceBoundarySelection', 'delay',
     'beads/dsp/delay_controller.cc',
     'if (play_head_[i].done()) {', 'if (true) {'),
    ('TestClockDividerTable', 'delay', 'beads/dsp/delay_controller.cc',
     'tap_ratio[flip] = kRatio[ratio_integral];',
     'tap_ratio[flip] = kRatio[ratio_integral] * 2.0f;'),
    ('TestFinalBankRendering', 'wavetable', 'beads/resources.cc',
     None, None),
    ('TestLimiterLinkedGainAndModeSwitch', 'options', 'beads/dsp/limiter.h',
     'in_out->r *= gain;', 'in_out->r *= gain * 0.5f;'),
    ('TestLimiterLinkedGainAndModeSwitch', 'options', 'beads/dsp/limiter.h',
     'peak_ = peak_lp_ = 0.5f;', '/* Detector reset removed. */'),
]
started = time.monotonic()
results = []
with tempfile.TemporaryDirectory(prefix='.mutation-', dir='beads/test') as tmp:
    work = pathlib.Path(tmp).resolve()
    for directory in ('beads', 'stmlib'):
        shutil.copytree(root / directory, work / directory,
                        ignore=shutil.ignore_patterns('.runs', '.mutation-*',
                                                     '__pycache__', 'core*'))
    for index, (name, suite, source, before, after) in enumerate(cases, 1):
        path = work / source
        original = path.read_text()
        if before is None:
            start = original.index('const int16_t wav_integrated_waves[] = {')
            end = original.index('};', start)
            values = list(re.finditer(r'-?\d+', original[start:end]))
            # Drop the duplicated final row, recreating the pre-#35 overread.
            cut = start + values[-260].start()
            line = original.count('\n', 0, cut) + 1
            mutated = original[:cut] + original[end:]
            change = 'remove the final 260-sample guard row'
        else:
            assert original.count(before) == 1, (name, before)
            line = original.count('\n', 0, original.index(before)) + 1
            mutated = original.replace(before, after)
            change = before + ' -> ' + after
        path.write_text(mutated)
        test = work / ('beads/test/' + suite + '_test.cc')
        original_test = test.read_text()
        if suite != 'wavetable':
            first = {'options': 'TestStockAssignRouting();',
                     'backup': 'TestFreezeDeadlineAndCancel();',
                     'delay': 'TestFrozenSliceBoundarySelection();'}[suite]
            main = original_test.index('int main(')
            cut = original_test.index(first, main)
            test.write_text(original_test[:cut] + name + '();\n' +
                            '  return test_checker.failures() ? 1 : 0;\n}\n')
        log = out / ('%02d-%s.log' % (index, name))
        try:
            with log.open('w') as stream:
                run = subprocess.run(['make', '-s', '-f', 'beads/test/' + suite + '.mk',
                                      'test'], cwd=work,
                                     stdout=stream, stderr=subprocess.STDOUT,
                                     timeout=60)
            failure = log.read_text()
            assert run.returncode != 0, name + ' survived mutation'
            if suite == 'wavetable':
                assert 'ERROR: AddressSanitizer:' in failure, failure
                report = failure.split('ERROR: AddressSanitizer:', 1)[1]
                stack = report.split('SUMMARY:', 1)[0]
                assert re.search(r'^\s*#\d+ .*beads::Wavetable(?:Grain|Voice)::',
                                 stack, re.MULTILINE), failure
            else:
                assert 'ERROR: ' + name + ':' in failure, failure
            evidence = next(x for x in failure.splitlines()
                            if 'ERROR: ' in x)
            results.append(f'{name}: {source}:{line}: {change}\n  {evidence}')
            print(results[-1], flush=True)
        finally:
            path.write_text(original)
            test.write_text(original_test)
(out / 'mutations.txt').write_text('\n'.join(results) + '\n')
print('Mutation runtime: %.2f s' % (time.monotonic() - started))
