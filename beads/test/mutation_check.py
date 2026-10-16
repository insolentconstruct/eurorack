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
# -----------------------------------------------------------------------------
#
# Firmware mutation checks in a disposable host test copy.

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
  ('TestCloudsStorage', 'options', 'beads/settings.h',
    'state_.padding[3] == 0xa2', 'state_.padding[3] != 0'),
  ('TestCloudsOptions', 'options', 'beads/options.h',
    'reverb > 0.5f + 0.03f', 'reverb > 0.9f + 0.03f'),
  ('TestStockHashes', 'clouds', 'beads/dsp/processor.cc',
    'const float post_gain_wet = 1.5f;', 'const float post_gain_wet = 1.4f;'),
  ('TestCloudsOutput', 'clouds', 'beads/dsp/processor.cc',
    'reverb_.ProcessClouds(out, size);', 'reverb_.Process(out, size);'),
  ('TestCloudsOutput', 'clouds', 'beads/dsp/processor.cc',
    'diffuser_.Process(out, size);', '(void)out;'),
  ('TestCloudsOutput', 'clouds', 'beads/dsp/fx/reverb.cc',
    '4680.0f, LFO_2', '4600.0f, LFO_2'),
  ('TestDiffuserImpulse', 'clouds', 'beads/dsp/fx/diffuser.cc',
    'c.WriteAllPass(apl1, -kap);', 'c.WriteAllPass(apl1, kap);'),
  ('TestCloudsSwitch', 'firmware', 'beads/beads.cc',
    'if (reverb_style_previous_gain == 0.0f) {', 'if (true) {'),
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
    '  if (next_interval_ >= max_interval_) {\n'
    '    next_interval_ = max_interval_;\n  }\n',
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
    '  seed_button_state_ = false;\n'
    '  seed_button_locked_ = true;',
    '  seed_button_state_ = false;\n'
    '  seed_button_locked_ = false;'),
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
  ('TestSynthBankSelection', 'wavetable', 'beads/dsp/synth.cc',
    'bank_quantizer_.Init(kNumBanks, 0.048f, false);',
    'bank_quantizer_.Init(kNumBanks, 0.1f, false);'),
  ('TestBrightRecording', 'processor', 'beads/dsp/processor.h',
    'float kSampleRates[4] = { 24000.0f, 48000.0f, 32000.0f, 48000.0f };',
    'float kSampleRates[4] = { 24000.0f, 48000.0f, 32000.0f, 24000.0f };'),
  ('TestColdRecording', 'processor', 'beads/dsp/processor.h',
    'float kSampleRates[4] = { 24000.0f, 48000.0f, 32000.0f, 48000.0f };',
    'float kSampleRates[4] = { 24000.0f, 48000.0f, 24000.0f, 48000.0f };'),
  ('TestSunnyRecording', 'processor', 'beads/dsp/processor.h',
    'int kRatios[4] = { 1, 2, 1, 1 };', 'int kRatios[4] = { 1, 1, 1, 1 };'),
  ('TestCassetteRecording', 'processor', 'beads/dsp/processor.h',
    '24000.0f, 48000.0f, 32000.0f', '32000.0f, 48000.0f, 32000.0f'),
  ('TestFreezeButtonRecording', 'processor', 'beads/ui.cc',
    'freeze_button_state_ = !freeze_button_state_;',
    'freeze_button_state_ = false;'),
  ('TestFreezeGateRecording', 'processor', 'beads/dsp/delay_line.h',
    '    if (frozen) {', '    if (false) {'),
  ('TestFrozenGrains', 'processor', 'beads/dsp/granular_processor.h',
    '      if (triggers) {\n        ++triggers;',
    '      if (*freeze) { out->l = out->r = 0.0f; }\n'
    '      if (triggers) {\n        ++triggers;'),
  ('TestUnfreezeCrossfade', 'processor', 'beads/dsp/delay_line.h',
    'const float gain = stmlib::SmoothStep(t);', 'const float gain = t;'),
  ('TestProcessorGrainLatch', 'granular', 'beads/dsp/delay_granular_player.h',
    'if (trigger.trigger) {', 'if (true) {'),
  ('TestProcessorEnvelopeCompletion', 'granular', 'beads/dsp/grain.h',
    'return envelope_.done() || voice_stealing_envelope_.done();',
    'return voice_stealing_envelope_.done();'),
  ('TestProcessorVoiceRelease', 'granular',
    'beads/dsp/voice_stealing_envelope.h',
    'decrement_ = 1.0f / duration;', 'decrement_ = 1.0f;'),
  ('TestProcessorVoiceSaturation', 'granular',
    'beads/dsp/delay_granular_player.h',
    'GranularPlayer<DelayGrain, 32>', 'GranularPlayer<DelayGrain, 31>'),
  ('TestProcessorRandomPan', 'granular', 'beads/dsp/granular_player.h',
    '*pan = BoundedRandomFloat(pan_spread);',
    '*pan = BoundedRandomFloat(pan_spread * 0.0f);'),
  ('TestPitchCvImmediate', 'granular', 'beads/cv_reader.cc',
    'cv_channel_[CV_ADC_CHANNEL_PITCH].Init(\n      1.0f, -60.0f, 60.0f,',
    'cv_channel_[CV_ADC_CHANNEL_PITCH].Init(\n      0.5f, -60.0f, 60.0f,'),
  ('TestCvChannelIsolation', 'granular', 'beads/cv_reader.cc',
    'cv_channel_[i].Process(cv_.value(CvAdcChannel(i)));',
    'cv_channel_[i].Process(cv_.value(\n'
    '        CvAdcChannel((i + 1) % CV_ADC_CHANNEL_LAST)));'),
  ('TestFeedbackRecording', 'mix', 'beads/dsp/processor.cc',
    'const float feedback = mix_parameters.feedback;',
    'const float feedback = 0.0f;'),
  ('TestDryWetEndpoints', 'mix', 'beads/dsp/processor.cc',
    'const float post_gain_dry = 0.75f;', 'const float post_gain_dry = 0.5f;'),
  ('TestMonoRecordingStereoOutput', 'mix', 'beads/dsp/granular_player.h',
    '*pan = BoundedRandomFloat(pan_spread);',
    '*pan = 0.5f + BoundedRandomFloat(pan_spread) * 0.0f;'),
  ('TestReverbAfterDryWet', 'mix', 'beads/dsp/processor.cc',
    'const float reverb_mix = reverb * reverb * 0.9f;',
    'const float reverb_mix = 0.0f;'),
  ('TestReverbSendOff', 'mix', 'beads/dsp/fx/reverb.h',
    'const float wet = output_gain;',
    'const float wet = mix_ == 0.0f ? 0.0f : output_gain;'),
  ('TestFeedbackQualityCurves', 'mix', 'beads/dsp/granular_processor.h',
    'const float soft_limit_gains[RESOLUTION_LAST] = { 1.75f, 1.3f, 0.0f };',
    'const float soft_limit_gains[RESOLUTION_LAST] = { 0.0f, 0.0f, 0.0f };'),
  ('TestFeedbackDelayReturn', 'mix', 'beads/dsp/granular_processor.h',
    'const float max_feedback_gain = taps ? 1.5f : 3.0f;',
    'const float max_feedback_gain = taps ? 0.0f : 3.0f;'),
  ('TestContinuousDelayPlayback', 'delay_mode', 'beads/dsp/processor.cc',
    'const float direction = parameters.size.base > threshold ? 1.0f : -1.0f;',
    'const float direction = '
    'parameters.size.base > threshold ? -1.0f : -1.0f;'),
  ('TestDelayModeFade', 'delay_mode', 'beads/dsp/processor.cc',
    'kDelayModeFadeTime = 0.75f;', 'kDelayModeFadeTime = 0.5f;'),
  ('TestDelayRotaryPitch', 'delay_mode', 'beads/dsp/multitap_delay.h',
    'float gain = 100.0f * (fabsf(speed - 1.0f) - 0.01f);',
    'float gain = 0.0f;'),
  ('TestDelayBandwidthAndCv', 'delay_mode', 'beads/dsp/granular_processor.h',
    '(parameters.size.has_cv ? parameters.size.cv : 0.0f)',
    '(parameters.size.has_cv ? 0.0f : 0.0f)'),
  ('TestSynthMixEndpoints', 'wavetable', 'beads/dsp/synth.cc',
    'const float post_gain_dry = 0.5f;', 'const float post_gain_dry = 0.25f;'),
  ('TestSynthFreeze', 'wavetable', 'beads/dsp/wavetable_grain.h',
    'if (!freeze) {', 'if (true) {'),
  ('TestSynthResolution', 'wavetable', 'beads/dsp/synth.cc',
    'wavetable_bit_shift[4] = { 4, 6, 7, 16 }',
    'wavetable_bit_shift[4] = { 4, 4, 7, 16 }'),
  ('TestSynthResolution', 'wavetable', 'beads/dsp/wavetable_grain.h',
    'sample = float(int(sample * scale_up_)) * scale_down_;',
    'sample = sample * scale_up_ * scale_down_;'),
  ('TestSynthPitchCvAndSpread', 'wavetable', 'beads/dsp/synth.cc',
    '(p.pitch.has_cv ? p.pitch.cv : 0.0f)',
    '(p.pitch.has_cv ? p.pitch.cv * 0.5f : 0.0f)'),
  ('TestSynthAmplitudeSweep', 'wavetable', 'beads/dsp/wavetable_voice.h',
    'const float scale = 1.0f / (frequency * 131072.0f);',
    'const float scale = 0.5f / (frequency * 131072.0f);'),
  ('TestAudioPatchDetection', 'options', 'beads/normalization_detector.h',
    'return normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT) ||',
    'return normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT) &&'),
  ('TestFrozenStateChangeGuard', 'options', 'beads/ui.h',
    'return frozen || calibrating || testing;',
    'return false && (frozen || calibrating || testing);'),
  ('TestGateOutputChordPersistence', 'options', 'beads/ui.cc',
    'state->gate_output_mode = !state->gate_output_mode;',
    'state->gate_output_mode = state->gate_output_mode;'),
  ('TestOverwriteReserveDisplay', 'options', 'beads/ui.cc',
    'const float brightness = overwrite_reserve_ *',
    'const float brightness = 1.0f *'),
  ('TestCorruptCalibrationRewritesOnce', 'options',
    'stmlib/system/sector_storage_h7xx.h',
    '    RewriteSector();\n    return false;', '    return false;'),
  ('TestMonophonicGesture', 'options', 'beads/ui.cc',
    'state->monophonic_mode = !state->monophonic_mode;',
    'state->monophonic_mode = state->monophonic_mode;'),
  ('TestMonophonicGrains', 'granular', 'beads/dsp/granular_player.h',
    'if (monophonic_mode) {', 'if (false && monophonic_mode) {'),
  ('TestSelfPlayOptions', 'options', 'beads/options.h',
    'pot > 0.5f + 0.03f', 'pot > 0.9f + 0.03f'),
  ('TestSelfPlayStorage', 'options', 'beads/settings.h',
    'state_.padding[1] == 0xa1', 'state_.padding[1] != 0'),
  ('TestSelfPlayHold', 'options', 'beads/ui.cc',
    'settings_->self_play_on_request() && no_input_',
    'settings_->self_play_on_request()'),
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
  ('TestMainLoopMatchesHarness', 'firmware', 'beads/beads.cc',
    'processor.WasteCPUCycles(16);',
    'processor.WasteCPUCycles(15);'),
  ('TestAutomaticSelfPlay', 'firmware', 'beads/beads.cc',
    ') >= 10000)',
    ') >= 9900)'),
  ('TestRequestedSelfPlay', 'firmware', 'beads/beads.cc',
    'if (ui.self_play_requested()) {',
    'if (false) {'),
  ('TestSelfPlayRequestLifecycle', 'firmware', 'beads/beads.cc',
    '    ui.ClearSelfPlayRequest();\n    return;',
    '    return;'),
  ('TestMonoStereo', 'firmware', 'beads/beads.cc',
    'bool mono = normalization_detector.mono();',
    'bool mono = false;'),
  ('TestFrozenQualityLock', 'firmware', 'beads/beads.cc',
    'if (ui.should_ignore_state_change() || codec_transition_frames) {',
    'if ((ui.should_ignore_state_change() && !ui.freeze_led_state()) || '
    'codec_transition_frames) {'),
  ('TestOverwriteIndicator', 'firmware', 'beads/beads.cc',
    'synth_mode ? 1.0f : processor.overwrite_reserve()',
    '1.0f'),
  ('TestTriggerOutput', 'firmware', 'beads/beads.cc',
    'm.output_gate = settings.state().gate_output_mode;',
    'm.output_gate = false;'),
  ('TestStartupBranches', 'firmware', 'beads/beads.cc',
    'const bool recovery_mode = ui.pressed_immediate(SWITCH_SEED);',
    'const bool recovery_mode = false;'),
  ('TestCallbackCvRouting', 'firmware', 'beads/beads.cc',
    'p.time.cv = cv_reader.cv(CV_ADC_CHANNEL_TIME);',
    'p.time.cv = cv_reader.cv(CV_ADC_CHANNEL_SIZE);'),
  ('TestPitchChain', 'firmware', 'beads/beads.cc',
    'p.pitch.cv = cv_reader.cv(CV_ADC_CHANNEL_PITCH);',
    'p.pitch.cv = 0.5f * cv_reader.cv(CV_ADC_CHANNEL_PITCH);'),
  ('TestSelfPlayFrozenBackup', 'firmware',
    'beads/background_buffer_backup.cc',
    'if (previous_freeze_state_) {',
    'if (false) {'),
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
        # Drop the duplicated final row to expose the overread.
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
        run = subprocess.run(
            ['nice', '-n', '10', 'make', '-s', '-f',
                'beads/test/' + suite + '.mk', 'test'],
            cwd=work, stdout=stream, stderr=subprocess.STDOUT,
            timeout=240 if suite in ('firmware', 'clouds') else 60)
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
