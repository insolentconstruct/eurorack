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
    ('TestClockIdleCounter', 'delay', 'beads/dsp/delay_clock.cc',
     '  if (next_interval_ >= max_interval_) {\n    next_interval_ = max_interval_;\n  }\n',
     ''),
    ('TestFinalBankRendering', 'wavetable', 'beads/resources.cc',
     None, None),
    ('TestLimiterLinkedGainAndModeSwitch', 'options', 'beads/dsp/limiter.h',
     'in_out->r *= gain;', 'in_out->r *= gain * 0.5f;'),
    ('TestLimiterLinkedGainAndModeSwitch', 'options', 'beads/dsp/limiter.h',
     'peak_ = peak_lp_ = 0.5f;', '/* Detector reset removed. */'),
    ('TestQualityTapCycle', 'options', 'beads/ui.cc',
     '(state->media + 3) % 4',
     '(state->media + 1) % 4'),
    ('TestManualGainGestureAndMeter', 'options', 'beads/ui.cc',
     '1.0f + p->hidden_parameter() * 255.0f',
     '1.0f + p->hidden_parameter() * 127.0f'),
    ('TestSeedStartupLatch', 'options', 'beads/ui.cc',
     '  seed_button_state_ = false;\n  seed_button_locked_ = true;',
     '  seed_button_state_ = false;\n  seed_button_locked_ = false;'),
    ('TestAssignableAmountLeds', 'options', 'beads/ui.cc',
     'state.assignable_cv_amount[i] >> 1',
     'state.assignable_cv_amount[i] >> 2'),
    ('TestAgcAdjustmentBlink', 'options', 'beads/ui.cc',
     '(system_clock.milliseconds() & 128) ? 128 : 0',
     '(system_clock.milliseconds() & 128) ? 0 : 128'),
    ('TestRecoveryPreservesCalibration', 'options', 'beads/settings.cc',
     'if (recovery_mode) {',
     'if (false) {'),
    ('TestGainLearningWindow', 'gain', 'beads/gain_control.cc',
     'kActiveTime = 5.0f;',
     'kActiveTime = 4.0f;'),
    ('TestGainHeadroomAndRise', 'gain', 'beads/gain_control.cc',
     '0.9f / min(agc_peak_, 0.9f)',
     '0.8f / min(agc_peak_, 0.9f)'),
    ('TestGainDigitalClipping', 'gain', 'beads/gain_control.cc',
     'l *= gain;',
     'l *= gain * 0.5f;'),
    ('TestGainTapeClipping', 'gain', 'beads/gain_control.cc',
     'soft_clipping_amount = 1.0f;',
     'soft_clipping_amount = 0.0f;'),
    ('TestGainNoiseAmplification', 'gain', 'beads/gain_control.cc',
     'kMaxGain = 40.0f;',
     'kMaxGain = 20.0f;'),
    ('TestDensityModulation', 'trigger', 'beads/dsp/trigger_generator.cc',
     'density += linear_fm;',
     'density -= linear_fm;'),
    ('TestLatchedNoonSilence', 'trigger', 'beads/dsp/trigger_generator.cc',
     'o.trigger = false;',
     'o.trigger = true;'),
    ('TestRegularDensityRate', 'trigger', 'beads/dsp/trigger_generator.cc',
     'kMaxGrainFrequency = 130.81278f;',
     'kMaxGrainFrequency = 100.0f;'),
    ('TestRandomDensityRate', 'trigger', 'beads/dsp/trigger_generator.cc',
     'if (randomize) {\n        ratio_ =',
     'if (false) {\n        ratio_ ='),
    ('TestClockedNoonSilence', 'trigger', 'beads/dsp/trigger_generator.cc',
     'max((density - 0.52f) * 2.15f, 0.0f)',
     'max((density - 0.40f) * 2.15f, 0.0f)'),
    ('TestClockProbability', 'trigger', 'beads/dsp/trigger_generator.cc',
     'const float probability = probability_sqrt * probability_sqrt;',
     'const float probability = probability_sqrt;'),
    ('TestClockDivisions', 'trigger', 'beads/dsp/trigger_generator.cc',
     '1, 2, 3, 4, 6, 8, 12, 16',
     '1, 2, 3, 4, 6, 8, 12, 12'),
    ('TestSeedGateEnable', 'trigger', 'beads/dsp/trigger_generator.cc',
     'bool gate = seed_button_state != SEED_BUTTON_STATE_OFF;',
     'bool gate = true;'),
    ('TestManualNoonEdges', 'trigger', 'beads/dsp/trigger_generator.cc',
     'out[i].trigger = tap;',
     'out[i].trigger = merged_gate_flags[i] & GATE_FLAG_HIGH;'),
    ('TestExponentialTriggerFm', 'trigger', 'beads/dsp/trigger_generator.cc',
     'SemitonesToRatio(exponential_fm * 60.0f)',
     'SemitonesToRatio(exponential_fm * 30.0f)'),
    ('TestGrainTimeAndDirection', 'granular', 'beads/dsp/delay_granular_player.h',
     'p.time.value() * float(max_delay)',
     '(1.0f - p.time.value()) * float(max_delay)'),
    ('TestGrainDuration', 'granular', 'beads/dsp/granular_player.h',
     'shortness * -108.0f',
     'shortness * -96.0f'),
    ('TestGrainShape', 'granular', 'beads/dsp/grain_envelope.h',
     'const float attack = shape;',
     'const float attack = 0.5f * shape;'),
    ('TestPitchNotches', 'granular', 'beads/cv_reader.h',
     '0.0f, 1.0f, 3.0f, 4.0f, 7.0f, 12.0f, 24.0f, 24.0f',
     '0.0f, 2.0f, 3.0f, 4.0f, 7.0f, 12.0f, 24.0f, 24.0f'),
    ('TestPatchedAndNoon', 'granular', 'beads/dsp/randomized_parameter.h',
     'result += attenuverter * cv;',
     'result += 0.5f * attenuverter * cv;'),
    ('TestPatchedRandomDepth', 'granular', 'beads/dsp/randomized_parameter.h',
     'std::max(cv, 0.0f) * -attenuverter',
     'fabsf(cv) * -attenuverter'),
    ('TestInternalDistributions', 'granular', 'beads/dsp/randomized_parameter.h',
     'return 2.0f * stmlib::Random::GetFloat() - 1.0f;',
     'return stmlib::Random::GetFloat() - 0.5f;'),
    ('TestAttenurandomizerLaw', 'granular', 'beads/cv_reader.h',
     'fabsf(v) - 0.03f',
     'fabsf(v) - 0.01f'),
    ('TestSharedRandomStream', 'granular', 'beads/dsp/randomized_parameter.h',
     'stmlib::Random::GetFloat() * range - result',
     '0.5f * range - result'),
    ('TestFreeDelayDensityAndCv', 'delay', 'beads/dsp/delay_controller.cc',
     'fabsf(density - 0.5f) * 216.0f',
     'fabsf(density - 0.5f) * 200.0f'),
    ('TestSecondaryTapModulation', 'delay', 'beads/dsp/delay_controller.cc',
     'tap_ratio[1] = 0.77f;',
     'tap_ratio[1] = 0.5f;'),
    ('TestSeedClockIntervalsAndCrossfade', 'delay', 'beads/dsp/delay_clock.cc',
     'next_interval_ < max_interval_',
     'next_interval_ <= max_interval_'),
    ('TestTimeMultipliersAndCrossfade', 'delay', 'beads/dsp/delay_controller.cc',
     'multiplier_lp_ * 15.0f + 1.0f',
     'multiplier_lp_ * 14.0f + 1.0f'),
    ('TestDelayShapeEnvelope', 'delay', 'beads/dsp/delay_controller.h',
     'const float square = 1.0f;',
     'const float square = 0.5f;'),
    ('TestDelayRandomModulation', 'delay', 'beads/dsp/delay_controller.cc',
     'position_modulation_.Init(0.1f / sample_rate);',
     'position_modulation_.Init(0.0f);'),
    ('TestFmBankRatios', 'wavetable', 'beads/resources.cc', None, None),
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
        if before is not None:
            assert original.count(before) == 1, (name, before)
            line = original.count('\n', 0, original.index(before)) + 1
            mutated = original.replace(before, after)
            change = before + ' -> ' + after
        else:
            start = original.index('const int16_t wav_integrated_waves[] = {')
            end = original.index('};', start)
            values = list(re.finditer(r'-?\d+', original[start:end]))
            if name == 'TestFmBankRatios':
                # Negate bank 8, wave 15: the 1:2 FM ratio's last row.
                row = values[(8 * 16 + 15) * 260:(8 * 16 + 16) * 260]
                line = original.count('\n', 0, start + row[0].start()) + 1
                mutated = original
                for value in reversed(row):
                    cut = start + value.start()
                    mutated = (mutated[:cut] + str(-int(value.group(0))) +
                               mutated[start + value.end():])
                change = 'negate the last 1:2 FM wave'
            else:
                # Drop the duplicated final row, recreating the pre-#35 overread.
                cut = start + values[-260].start()
                line = original.count('\n', 0, cut) + 1
                mutated = original[:cut] + original[end:]
                change = 'remove the final 260-sample guard row'
        path.write_text(mutated)
        test = work / ('beads/test/' + suite + '_test.cc')
        original_test = test.read_text()
        if suite != 'wavetable':
            main = original_test.index('int main(')
            first = re.compile(r'\n  Test\w+\(\);').search(
                original_test, main).group(0)[1:]
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
            if name == 'TestFinalBankRendering':
                assert 'ERROR: AddressSanitizer:' in failure, failure
                report = failure.split('ERROR: AddressSanitizer:', 1)[1]
                stack = report.split('SUMMARY:', 1)[0]
                assert re.search(r'^\s*#\d+ .*beads::Wavetable(?:Grain|Voice)::',
                                 stack, re.MULTILINE), failure
            elif name == 'TestClockIdleCounter':
                assert re.search(r'delay_clock\.cc:\d+:\d+: runtime error: '
                                 r'signed integer overflow', failure), failure
            else:
                assert 'ERROR: ' + name + ':' in failure, failure
            evidence = next(x for x in failure.splitlines()
                            if 'ERROR: ' in x or 'runtime error: ' in x)
            results.append(f'{name}: {source}:{line}: {change}\n  {evidence}')
            print(results[-1], flush=True)
        finally:
            path.write_text(original)
            test.write_text(original_test)
(out / 'mutations.txt').write_text('\n'.join(results) + '\n')
print('Mutation runtime: %.2f s' % (time.monotonic() - started))
