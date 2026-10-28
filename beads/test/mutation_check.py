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
cases = [
  ('TestOptionsSeed', 'options', 'beads/options.h',
    'return stage_ == STAGE_EDITING && page_ == kPitchScalePage &&',
    'return qualified_[0] && stage_ == STAGE_EDITING && page_ == kPitchScalePage &&'),
  ('TestOptionsTablesAndLifetime', 'options', 'beads/options.cc',
    'page_ = 0;',
    'page_ = 2;'),
  ('TestOptionsKnobsAndSettling', 'options', 'beads/options.cc',
    'origin_[i] = previous_[i] = pots[i];',
    'origin_[i] = previous_[i] = 0.0f;'),
  ('TestPitchGrids', 'pitch_quantizer', 'beads/dsp/pitch_quantizer.cc',
    '0.0000f, 2.0004f, 3.9996f, 6.9996f, 9.0000f,',
    '0.0000f, 2.0004f, 5.0004f, 6.9996f, 9.0000f,'),
  ('TestPitchGrids', 'pitch_quantizer', 'beads/dsp/pitch_quantizer.cc',
    '0.0000f, 3.0000f, 5.0004f, 6.9996f, 9.9996f,',
    '0.0000f, 3.0000f, 5.0004f, 6.9996f, 9.0000f,'),
  ('TestPitchStorage', 'options', 'beads/settings.h',
    'value <= (kOptionTag + 9)',
    'value <= 0xa8'),
  ('TestPitchStorage', 'options', 'beads/settings.h',
    'value <= (kOptionTag + 9)',
    'value <= 0xaa'),
  ('TestOptionsSeed', 'options', 'beads/options.cc',
    'scale_ == PITCH_SCALE_SHRI',
    'scale_ == PITCH_SCALE_BHAIRAV'),
  ('TestOptionsPalette', 'options', 'beads/options.cc',
    '    { 255, 255, 0 }, PULSE_SLOW },\n'
    '  { OPTION_PITCH, 5',
    '    { 0, 255, 255 }, PULSE_SLOW },\n'
    '  { OPTION_PITCH, 5'),
  ('TestCloudsStorage', 'options', 'beads/settings.h',
    'state_.reverb_style == (kOptionTag + 2)',
    'state_.reverb_style != 0'),
  ('TestOptionsZonesAndSaveBatch', 'options', 'beads/options.cc',
    'if (pot > kOptionsHalfHigh) {\n            clouds_preview_ = true;',
    'if (pot > 0.93f) {\n            clouds_preview_ = true;'),
  ('TestStockHashes', 'clouds', 'beads/dsp/processor.cc',
    'const float post_gain_wet = 1.5f;',
    'const float post_gain_wet = 1.4f;'),
  ('TestCloudsOutput', 'clouds', 'beads/dsp/processor.cc',
    'reverb_.ProcessClouds(out, size);',
    'reverb_.Process(out, size);'),
  ('TestCloudsOutput', 'clouds', 'beads/dsp/processor.cc',
    'diffuser_.Process(out, size);',
    '(void)out;'),
  ('TestCloudsOutput', 'clouds', 'beads/dsp/fx/reverb.cc',
    '4680.0f, LFO_2',
    '4600.0f, LFO_2'),
  ('TestDiffuserImpulse', 'clouds', 'beads/dsp/fx/diffuser.h',
    'return tail - sum * coefficient;',
    'return tail + sum * coefficient;'),
  ('TestCloudsSwitch', 'firmware', 'beads/beads.cc',
    'if (reverb_style_previous_gain == 0.0f) {',
    'if (true) {'),
  ('TestQuantizedPitchChain', 'firmware', 'beads/beads.cc',
    'processor.set_pitch_scale(ui.pitch_scale());',
    'processor.set_pitch_scale(PITCH_SCALE_OFF);'),
  ('TestPitchGrids', 'pitch_quantizer', 'beads/dsp/pitch_quantizer.cc',
    'floorf(pitch / 12.0f)',
    'ceilf(pitch / 12.0f)'),
  ('TestRandomizedPitchGrid', 'pitch_quantizer', 'beads/dsp/randomized_parameter.h',
    'pitch = QuantizePitch(pitch, scale);',
    'pitch = pitch;'),
  ('TestQuantizedGrainPitch', 'granular', 'beads/dsp/delay_granular_player.h',
    'p.pitch.pitch_value(pitch_scale_, random_intervals_)',
    'p.pitch.pitch_value()'),
  ('TestQuantizedSynthGrains', 'wavetable', 'beads/dsp/wavetable_granular_player.h',
    'p.pitch.pitch_value(pitch_scale_, random_intervals_)',
    'p.pitch.pitch_value()'),
  ('TestOptionsSeed', 'options', 'beads/ui.cc',
    'const bool accept_switch_events = !was_active && !options_.active() &&\n'
    '      options_.stage() != Options::STAGE_DRAIN && !showing_storage_status();',
    'const bool accept_switch_events = true;'),
  ('TestPitchStorage', 'options', 'beads/settings.cc',
    'void Settings::SaveState() {\n'
    '  NormalizeOptions();\n'
    '  fill(&state_.padding[0],',
    'void Settings::SaveState() {\n'
    '  NormalizeOptions();\n'
    '  fill(&state_.padding[1],'),
  ('TestTrimGesture', 'options', 'beads/ui.cc',
    'const bool accept_switch_events = !was_active && !options_.active() &&\n'
    '      options_.stage() != Options::STAGE_DRAIN && !showing_storage_status();',
    'const bool accept_switch_events = true;'),
  ('TestTrimGesture', 'options', 'beads/ui.cc',
    '    ignore_release_[SWITCH_MEDIA] = ignore_release_[SWITCH_FREEZE] = true;',
    '    ignore_release_[SWITCH_MEDIA] = ignore_release_[SWITCH_FREEZE] = false;'),
  ('TestTrimGesture', 'options', 'beads/ui.cc',
    '    trim_flag_ = true;',
    '    trim_flag_ = false;'),
  ('TestTrimBackup', 'backup', 'beads/background_buffer_backup.cc',
    'header.trimmed_frames >',
    'false && header.trimmed_frames >'),
  ('TestTrimBackup', 'backup', 'beads/background_buffer_backup.cc',
    '*processor_->mutable_delay_duration() =',
    '*processor_->mutable_delay_write_ptr() ='),
  ('TestPlayingGrainSurvivesTrim', 'granular', 'beads/dsp/delay_grain.h',
    'const int32_t tail = DelayLineType::MAX_DELAY << 10;',
    'const int32_t tail = delay_line->duration() << 10;'),
  ('TestUntrimmedGrainRender', 'granular', 'beads/dsp/delay_grain.h',
    'float consumption_speed = speed -',
    'float consumption_speed = speed * 0.5f -'),
  ('TestTrimGrainBounds', 'granular', 'beads/dsp/delay_grain.h',
    'delay_line->duration() / fabsf(consumption_speed)',
    'DelayLineType::MAX_DELAY / fabsf(consumption_speed)'),
  ('TestTrimGrainBounds', 'granular', 'beads/dsp/delay_grain.h',
    'delay_line->duration() + duration * consumption_speed',
    'DelayLineType::MAX_DELAY + duration * consumption_speed'),
  ('TestTrimGrowth', 'processor', 'beads/dsp/delay_line.h',
    '    ++duration_;',
    '    duration_ += 2;'),
  ('TestTrimDispatch', 'processor', 'beads/dsp/processor.h',
    'processor_dirty_tape_.Trim(mono_);\n        break;',
    'processor_dirty_tape_.Trim(mono_);'),
  ('TestTrimBackup', 'backup', 'beads/background_buffer_backup.cc',
    '      header.trimmed_frames = (kMaxDelay[header.media] << header.mono) -\n'
    '          *processor_->mutable_delay_duration();',
    '      header.trimmed_frames = 0;'),
  ('TestCorruption', 'backup', 'beads/background_buffer_backup.cc',
    '  h.crc = 0;',
    '  h.crc = 0; h.trimmed_frames = 0;'),
  ('TestFrozenTrimBackup', 'backup', 'beads/background_buffer_backup.cc',
    'trimmed_frames != saved_trimmed_frames_',
    '(false && trimmed_frames != saved_trimmed_frames_)'),
  ('TestStockAssignRouting', 'options', 'beads/ui.cc',
    '(state->assignable_cv_destination + 1) % 3',
    '(state->assignable_cv_destination + 2) % 3'),
  ('TestStockAssignAmounts', 'options', 'beads/ui.cc',
    'p->hidden_parameter() * 256.0f',
    'p->hidden_parameter() * 128.0f'),
  ('TestAutosaveChordAndDisplay', 'options', 'beads/ui.cc',
    'state->disable_autosave = !state->disable_autosave;',
    'state->disable_autosave = state->disable_autosave;'),
  ('TestSeedLatchGestures', 'options', 'beads/ui.cc',
    '4000, 4000, kSelfPlayHoldTime,',
    '4000, 4100, kSelfPlayHoldTime,'),
  ('TestGainHoldThresholdAndReload', 'options', 'beads/ui.cc',
    '4000, 4000, kSelfPlayHoldTime,',
    '4000, 4000, 1600,'),
  ('TestFreezeDeadlineAndCancel', 'backup', 'beads/background_buffer_backup.cc',
    'kBackupDelay = 10000;',
    'kBackupDelay = 9999;'),
  ('TestBackupLedAndRestore', 'options', 'beads/ui.cc',
    'busy() ? 128 : 255',
    'busy() ? 255 : 255'),
  ('TestFrozenSliceBoundarySelection', 'delay', 'beads/dsp/delay_controller.cc',
    'if (play_head_[i].done()) {',
    'if (true) {'),
  ('TestClockDividerTable', 'delay', 'beads/dsp/delay_controller.cc',
    'tap_ratio[flip] = kRatio[ratio_integral];',
    'tap_ratio[flip] = kRatio[ratio_integral] * 2.0f;'),
  ('TestClockIdleCounter', 'delay', 'beads/dsp/delay_clock.cc',
    '  if (next_interval_ >= max_interval_) {\n'
    '    next_interval_ = max_interval_;\n'
    '  }\n',
    ''),
  ('TestFinalBankRendering', 'wavetable', 'beads/resources.cc',
    None,
    None),
  ('TestLimiterLinkedGainAndModeSwitch', 'options', 'beads/dsp/limiter.h',
    'in_out->r *= gain;',
    'in_out->r *= gain * 0.5f;'),
  ('TestLimiterLinkedGainAndModeSwitch', 'options', 'beads/dsp/limiter.h',
    'peak_ = peak_lp_ = 0.5f;',
    '/* Detector reset removed. */'),
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
    '    if (recovery_mode) {\n      state_ = defaults;',
    '    if (false) {\n      state_ = defaults;'),
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
    ': p.time.value() * float(max_delay)',
    ': (1.0f - p.time.value()) * float(max_delay)'),
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
  ('TestFmBankRatios', 'wavetable', 'beads/resources.cc',
    None,
    None),
  ('TestSynthBankSelection', 'wavetable', 'beads/dsp/synth.cc',
    'bank_quantizer_.Init(kNumBanks, 0.048f, false);',
    'bank_quantizer_.Init(kNumBanks, 0.1f, false);'),
  ('TestBrightRecording', 'processor', 'beads/dsp/processor.h',
    'float sr[4] = { 24000.0f, 48000.0f, 32000.0f, 48000.0f };',
    'float sr[4] = { 24000.0f, 48000.0f, 32000.0f, 24000.0f };'),
  ('TestColdRecording', 'processor', 'beads/dsp/processor.h',
    'float sr[4] = { 24000.0f, 48000.0f, 32000.0f, 48000.0f };',
    'float sr[4] = { 24000.0f, 48000.0f, 24000.0f, 48000.0f };'),
  ('TestSunnyRecording', 'processor', 'beads/dsp/processor.h',
    'int ratio[4] = { 1, 2, 1, 1 };',
    'int ratio[4] = { 1, 1, 1, 1 };'),
  ('TestCassetteRecording', 'processor', 'beads/dsp/processor.h',
    '24000.0f, 48000.0f, 32000.0f',
    '32000.0f, 48000.0f, 32000.0f'),
  ('TestFreezeButtonRecording', 'processor', 'beads/ui.cc',
    'freeze_button_state_ = !freeze_button_state_;',
    'freeze_button_state_ = false;'),
  ('TestFreezeGateRecording', 'processor', 'beads/dsp/delay_line.h',
    '    if (frozen) {',
    '    if (false) {'),
  ('TestFrozenGrains', 'processor', 'beads/dsp/granular_processor.h',
    '      if (triggers) {\n        ++triggers;',
    '      if (*freeze) { out->l = out->r = 0.0f; }\n'
    '      if (triggers) {\n'
    '        ++triggers;'),
  ('TestUnfreezeCrossfade', 'processor', 'beads/dsp/delay_line.h',
    'const float gain = stmlib::SmoothStep(t);',
    'const float gain = t;'),
  ('TestProcessorGrainLatch', 'granular', 'beads/dsp/delay_granular_player.h',
    'if (trigger.trigger) {',
    'if (true) {'),
  ('TestProcessorEnvelopeCompletion', 'granular', 'beads/dsp/grain.h',
    'return envelope_.done() || voice_stealing_envelope_.done();',
    'return voice_stealing_envelope_.done();'),
  ('TestProcessorVoiceRelease', 'granular', 'beads/dsp/voice_stealing_envelope.h',
    'decrement_ = 1.0f / duration;',
    'decrement_ = 1.0f;'),
  ('TestProcessorVoiceSaturation', 'granular', 'beads/dsp/delay_granular_player.h',
    'GranularPlayer<DelayGrain, 32> {\n'
    ' public:\n'
    '  DelayGranularPlayer() { }\n'
    '  ~DelayGranularPlayer() { }\n'
    '  \n'
    '  inline void Init(float sample_rate) {\n'
    '    GranularPlayer<DelayGrain, 32>::Init',
    'GranularPlayer<DelayGrain, 31> {\n'
    ' public:\n'
    '  DelayGranularPlayer() { }\n'
    '  ~DelayGranularPlayer() { }\n'
    '  \n'
    '  inline void Init(float sample_rate) {\n'
    '    GranularPlayer<DelayGrain, 31>::Init'),
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
    'const float post_gain_dry = 0.75f;',
    'const float post_gain_dry = 0.5f;'),
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
    'delay_mode_ != DELAY_MODE_NEVER &&\n'
    '      parameters.size.base > threshold ? 1.0f : -1.0f;',
    'delay_mode_ != DELAY_MODE_NEVER &&\n'
    '      parameters.size.base > threshold ? -1.0f : -1.0f;'),
  ('TestDelayModeFade', 'delay_mode', 'beads/dsp/processor.cc',
    'kDelayModeFadeTime = 0.75f;',
    'kDelayModeFadeTime = 0.5f;'),
  ('TestDelayRotaryPitch', 'delay_mode', 'beads/dsp/multitap_delay.h',
    'float gain = 100.0f * (fabsf(speed - 1.0f) - 0.01f);',
    'float gain = 0.0f;'),
  ('TestDelayBandwidthAndCv', 'delay_mode', 'beads/dsp/granular_processor.h',
    '(parameters.size.has_cv ? parameters.size.cv : 0.0f)',
    '(parameters.size.has_cv ? 0.0f : 0.0f)'),
  ('TestSynthMixEndpoints', 'wavetable', 'beads/dsp/synth.cc',
    'const float post_gain_dry = 0.5f;',
    'const float post_gain_dry = 0.25f;'),
  ('TestSynthFreeze', 'wavetable', 'beads/dsp/wavetable_grain.h',
    'if (!freeze) {',
    'if (true) {'),
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
  ('TestMonophonicGesture', 'options', 'beads/ui.cc',
    'state->monophonic_mode = !state->monophonic_mode;',
    'state->monophonic_mode = state->monophonic_mode;'),
  ('TestMonophonicGrains', 'granular', 'beads/dsp/granular_player.h',
    'if (monophonic_mode) {',
    'if (false && monophonic_mode) {'),
  ('TestEngineOption', 'options', 'beads/options.cc',
    'if (pot > kOptionsHalfHigh) {\n            spectral_preview_ = true;',
    'if (pot > 0.93f) {\n            spectral_preview_ = true;'),
  ('TestSynthStartOption', 'options', 'beads/options.cc',
    'if (pot > kOptionsHalfHigh) {\n            manual_start_preview_ = true;',
    'if (pot > 0.93f) {\n            manual_start_preview_ = true;'),
  ('TestEngineOption', 'options', 'beads/ui.cc',
    'settings_->set_spectral(options_.spectral_preview());',
    'settings_->set_spectral(false);'),
  ('TestOptionsPreview', 'options', 'beads/options.cc',
    '  selected_ = kOptionPages[page_].knobs[knob];\n  indication_time_ = now;',
    '  selected_ = kOptionPages[page_].knobs[knob];'),
  ('TestRandomIntervals', 'pitch_quantizer', 'beads/dsp/randomized_parameter.h',
    'if (intervals && attenuverter < 0.0f) {',
    'if (intervals) {'),
  ('TestRandomizedPitchGrid', 'pitch_quantizer',
    'beads/dsp/randomized_parameter.h',
    'if (intervals && attenuverter < 0.0f) {',
    'if (intervals && attenuverter < 0.0f && !has_cv) {'),
  ('TestRandomIntervals', 'pitch_quantizer', 'beads/dsp/randomized_parameter.h',
    'pitch = base + QuantizePitch(pitch - base, PITCH_SCALE_FIFTHS);',
    'pitch = QuantizePitch(pitch, PITCH_SCALE_FIFTHS);'),
  ('TestRandomIntervalGrains', 'granular', 'beads/dsp/delay_granular_player.h',
    'p.pitch.pitch_value(pitch_scale_, random_intervals_)',
    'p.pitch.pitch_value(pitch_scale_, false)'),
  ('TestRandomIntervalSynthGrains', 'wavetable',
    'beads/dsp/wavetable_granular_player.h',
    'p.pitch.pitch_value(pitch_scale_, random_intervals_)',
    'p.pitch.pitch_value(pitch_scale_, false)'),
  ('TestRandomIntervalSynthGrains', 'wavetable', 'beads/dsp/synth.h',
    'wavetable_granular_player_.set_random_intervals(intervals);',
    '(void)intervals;'),
  ('TestRandomIntervalGrains', 'granular', 'beads/dsp/processor.h',
    'processor_clean_tape_.set_random_intervals(intervals);',
    '(void)intervals;'),
  ('TestRandomIntervalsChain', 'firmware', 'beads/beads.cc',
    'processor.set_random_intervals(ui.random_intervals());',
    'processor.set_random_intervals(false);'),
  ('TestIntervalsStorage', 'options', 'beads/settings.h',
    'return state_.random_intervals == (kOptionTag + 1);',
    'return state_.random_intervals != 0;'),
  ('TestIntervalsOption', 'options', 'beads/options.cc',
    'if (pot > kOptionsHalfHigh) {\n            intervals_preview_ = true;',
    'if (pot > 0.93f) {\n            intervals_preview_ = true;'),
  ('TestIntervalsOption', 'options', 'beads/ui.cc',
    'settings_->set_random_intervals(options_.intervals_preview());',
    '(void)0;'),
  ('TestOptionsPreview', 'options', 'beads/options.cc',
    'StartPreview(now, preview_knob_ + 1);',
    'StartPreview(now, MIX_PARAMETER_LAST);'),
  ('TestOptionsPreview', 'options', 'beads/ui.cc',
    'leds_.set(Led(LED_ASSIGN_1 + i), i == options_.preview_knob() ? 255',
    'leds_.set(Led(LED_ASSIGN_1 + i), false ? 255'),
  ('TestOptionsPalette', 'options', 'beads/ui.cc',
    'const uint32_t now = system_clock.milliseconds() -\n'
    '        options_.indication_time();',
    'const uint32_t now = system_clock.milliseconds();'),
  ('TestOptionsConfirmations', 'options', 'beads/ui.cc',
    '      if (page_led == MIX_PARAMETER_LAST || i == page_led) {',
    '      if (i == page_led) {'),
  ('TestWidthGestureAndMeter', 'options', 'beads/ui.cc',
    '          settings_->set_stereo_width(StereoWidthFromPot(',
    '          (void)(StereoWidthFromPot('),
  ('TestWidthGestureAndMeter', 'options', 'beads/ui.cc',
    '      assignable_cv_pot_[MIX_PARAMETER_DRY_WET].Lock();',
    '      (void)0;'),
  ('TestWidthGestureAndMeter', 'options', 'beads/options.cc',
    '  if (pot < kWidthStockThreshold) {',
    '  if (pot < 0.05f) {'),
  ('TestEngineStorage', 'options', 'beads/settings.h',
    'state_.engine == (kOptionTag + 1)',
    'state_.engine != 0'),
  ('TestSelfPlayStorage', 'options', 'beads/settings.h',
    'state_.self_play == (kOptionTag + 1)',
    'state_.self_play != 0'),
  ('TestSelfPlayHold', 'options', 'beads/ui.cc',
    '!spectral_ &&\n                  no_input_ &&',
    '!spectral_ &&'),
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
  ('TestAutomaticSelfPlay', 'firmware', 'beads/beads.cc',
    ') >= 10000)',
    ') >= 9900)'),
  ('TestRequestedSelfPlay', 'firmware', 'beads/beads.cc',
    'if (ui.self_play_requested()) {',
    'if (false) {'),
  ('TestSelfPlayRequestLifecycle', 'firmware', 'beads/beads.cc',
    '    ui.ClearSelfPlayRequest();\n    codec_fade = false;\n    return;',
    '    codec_fade = false;\n    return;'),
  ('TestMonoStereo', 'firmware', 'beads/beads.cc',
    'bool mono = normalization_detector.mono();',
    'bool mono = false;'),
  ('TestFrozenQualityLock', 'firmware', 'beads/beads.cc',
    'if (ui.should_ignore_state_change() || codec_transition_frames) {',
    'if ((ui.should_ignore_state_change() && !ui.freeze_led_state()) || codec_transition_frames) {'),
  ('TestOverwriteIndicator', 'firmware', 'beads/beads.cc',
    'synth_mode || processor.spectral()\n      ? 1.0f : processor.overwrite_reserve()',
    '1.0f'),
  ('TestSpectralEngine', 'firmware', 'beads/beads.cc',
    'if (!processor.spectral() && background_buffer_backup.Load()) {',
    'if (false) {'),
  ('TestSpectralEngine', 'firmware', 'beads/beads.cc',
    'if (normalization_detector.no_input() && !processor.spectral()) {',
    'if (normalization_detector.no_input()) {'),
  ('TestCloudsReference', 'spectral', 'beads/dsp/pvoc/stft.cc',
    'fft_out_[i + fft_size_ / 2] = i ? -fft_in_[2 * i + 1] : fft_in_[1];',
    'fft_out_[i + fft_size_ / 2] = fft_in_[2 * i + 1];'),
  ('TestCloudsReference', 'spectral', 'beads/dsp/pvoc/frame_transformation.cc',
    'float warped = 4.0f * x * (1.0f - x) * (1.0f - x) * (1.0f - x);',
    'float warped = 4.0f * x * (1.0f - x) * (1.0f - x);'),
  ('TestFrameControls', 'spectral', 'beads/dsp/spectral_processor.cc',
    'draw_counter_ += kSpectralHopSize;',
    'draw_counter_ += kSpectralHopSize / 2;'),
  ('TestSpectralBypasses', 'spectral', 'beads/dsp/processor.cc',
    'const float direction = !spectral_ && ',
    'const float direction = '),
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
  ('TestSelfPlayFrozenBackup', 'firmware', 'beads/background_buffer_backup.cc',
    'if (previous_freeze_state_) {',
    'if (false) {'),
  ('TestSynthPitchScaleChain', 'firmware', 'beads/beads.cc',
    '    synth.set_pitch_scale(ui.pitch_scale());',
    '    synth.set_pitch_scale(PITCH_SCALE_OFF);'),
  ('TestPlaitsSynthSelection', 'firmware', 'beads/dsp/delay_line.h',
    '  inline void Reset() {\n    Reset(true);',
    '  inline void Reset() {\n    Reset(false);'),
  ('TestPlaitsSynthSelection', 'firmware', 'beads/beads.cc',
    'if (synth_mode && ui.plaits_synth()) {',
    'if (synth_mode && settings.state().synth_engine == 0) {'),
  ('TestSynthEngineStorage', 'options', 'beads/settings.h',
    'return state_.synth_engine == (kOptionTag + 1);',
    'return state_.synth_engine != 0;'),
  ('TestVoiceAllocation', 'plaits_synth', 'beads/dsp/plaits_synth.cc',
    'active_voice_ = (active_voice_ + 1) % kNumPlaitsVoices;',
    'active_voice_ = (active_voice_ + 1) % (kNumPlaitsVoices - 1);'),
  ('TestVoiceAllocation', 'plaits_synth', 'beads/dsp/plaits_synth.cc',
    'drone_ = drone_ && noon && !trigger;',
    'drone_ = drone_ && noon;'),
  ('TestModelDetents', 'plaits_synth', 'beads/dsp/plaits_synth.cc',
    'const float kModelHysteresis = 0.048f;',
    'const float kModelHysteresis = 0.06f;'),
  ('TestFreezeSustains', 'plaits_synth', 'beads/dsp/plaits_voice.cc',
    'sustain_level_ = level_;',
    'sustain_level_ = 0.0f;'),
  ('TestEveryModelRenders', 'plaits_synth', 'beads/dsp/plaits_voice.cc',
    '? fm_patches_table[patch.model - 2]',
    '? fm_patches_table[0]'),
  ('TestSynthStereoWidth', 'wavetable', 'beads/dsp/wavetable_granular_player.h',
    '          pan,\n'
    '          trigger.fractional_sample);',
    '          0.5f,\n'
    '          trigger.fractional_sample);'),
  ('TestSettingsRollover', 'options', 'beads/settings_storage.h',
    '        WriteChunk(chunk_address(1), state_data_)) {',
    '        true) {'),
  ('TestStorageStaleErrorFlag', 'options', 'beads/settings_storage.h',
    '      ClearErrors();\n      FLASH_EraseInitTypeDef',
    '      FLASH_EraseInitTypeDef'),
  ('TestStorageStaleErrorFlag', 'options', 'beads/settings_storage.h',
    '      ClearErrors();\n      if (HAL_FLASH_Program(',
    '      if (HAL_FLASH_Program('),
  ('TestStorageNonBlankSlot', 'options', 'beads/settings_storage.h',
    '      if (!Flash::IsBlank(address, BLOCK_SIZE)) {\n        return false;\n      }\n',
    ''),
  ('TestStorageFailedProgram', 'options', 'beads/settings_storage.h',
    'const int kFlashAttempts = 3;',
    'const int kFlashAttempts = 1;'),
  ('TestStorageFailedProgram', 'options', 'beads/settings_storage.h',
    '      if (WriteChunk(chunk_address(next_state_chunk_index_), state_data_)) {',
    '      WriteChunk(chunk_address(next_state_chunk_index_), state_data_);\n'
    '      if (true) {'),
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
  ('TestCalibrationErrors', 'storage', 'beads/settings_storage.h',
    '    if (calibration == CHUNK_UNREADABLE) {\n      return STORAGE_DAMAGED;',
    '    if (calibration == CHUNK_UNREADABLE) {\n      return STORAGE_RESTORED;'),
  ('TestCalibrationErrors', 'storage', 'beads/settings_storage.h',
    'FLASH_STORAGE_LAST - FLASH_STORAGE_BASE)) {\n        return STORAGE_DAMAGED;',
    'FLASH_STORAGE_LAST - FLASH_STORAGE_BASE)) {\n        return STORAGE_NEW;'),
  ('TestCorruptCalibrationLocks', 'options', 'beads/settings_storage.h',
    'FLASH_STORAGE_LAST - FLASH_STORAGE_BASE)) {\n        return STORAGE_DAMAGED;',
    'FLASH_STORAGE_LAST - FLASH_STORAGE_BASE)) {\n        return STORAGE_NEW;'),
  ('TestBlankSector', 'storage', 'beads/settings_storage.h',
    '      if (mirror == CHUNK_VALID) {\n        return STORAGE_RESTORED;',
    '      if (mirror == CHUNK_VALID) {\n        return STORAGE_NEW;'),
  ('TestBlankSector', 'storage', 'beads/settings_storage.h',
    '          ? STORAGE_NEW\n          : STORAGE_DAMAGED;',
    '          ? STORAGE_NEW\n          : STORAGE_NEW;'),
  ('TestStateErrors', 'storage', 'beads/settings_storage.h',
    '        return STORAGE_DAMAGED_STATE;',
    '        break;'),
  ('TestCalibrationWithoutState', 'storage', 'beads/settings_storage.h',
    '      // Rewritten, then the power went before the first state.\n'
    '      return STORAGE_RESTORED;',
    '      // Rewritten, then the power went before the first state.\n'
    '      return STORAGE_NEW;'),
  ('TestWriteLock', 'storage', 'beads/settings_storage.h',
    '      locked_ = true;\n    } else if',
    '      locked_ = false;\n    } else if'),
  ('TestWriteLock', 'storage', 'beads/settings_storage.h',
    '  bool EraseSector(uint32_t index, uint32_t address) {\n    if (locked_) {',
    '  bool EraseSector(uint32_t index, uint32_t address) {\n    if (false) {'),
  ('TestWriteLock', 'storage', 'beads/settings_storage.h',
    '  bool ProgramWord(uint32_t address, const uint8_t* buffer) {\n'
    '    if (locked_) {',
    '  bool ProgramWord(uint32_t address, const uint8_t* buffer) {\n'
    '    if (false) {'),
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
  ('TestRepair', 'storage', 'beads/settings.cc',
    '              ? STORAGE_REPAIR_CALIBRATION_KEPT\n',
    '              ? STORAGE_REPAIR_CALIBRATION_RESET\n'),
  ('TestStateErrors', 'storage', 'beads/settings.cc',
    '    bool keep = status == STORAGE_DAMAGED_STATE && \\\n'
    '        CalibrationPlausible(factory);',
    '    bool keep = status == STORAGE_DAMAGED_STATE;'),
  ('TestRepairCalibrationOrder', 'storage', 'beads/settings.cc',
    '    bool keep = status == STORAGE_DAMAGED_STATE && \\\n',
    '    bool keep = false && \\\n'),
  ('TestRepairCalibrationOrder', 'storage', 'beads/settings.cc',
    '      keep = chunk_storage_.ReadMirror(&persistent_data_) && \\\n'
    '          CalibrationPlausible(factory);',
    '      keep = false;'),
  ('TestRepairCalibrationOrder', 'storage', 'beads/settings.cc',
    '      keep = chunk_storage_.ReadMirror(&persistent_data_) && \\\n'
    '          CalibrationPlausible(factory);',
    '      keep = chunk_storage_.ReadMirror(&persistent_data_);'),
  ('TestRepairCalibrationOrder', 'storage', 'beads/settings_storage.h',
    '    return ReadChunk(MIRROR_BASE, persistent_data) == CHUNK_VALID;',
    '    return ReadChunk(MIRROR_BASE, persistent_data) != CHUNK_INVALID;'),
  ('TestStateErrors', 'storage', 'beads/settings.cc',
    '    if (!(fabsf(c.scale / f.scale - 1.0f) <= 0.25f) || \\',
    '    if (!(fabsf(c.scale / f.scale - 1.0f) <= 0.6f) || \\'),
  ('TestStateErrors', 'storage', 'beads/settings.cc',
    '    state_ = defaults;\n    bool keep',
    '    bool keep'),
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
  ('TestFrozenNoReads', 'backup', 'beads/background_buffer_backup.cc',
    '        saved_trimmed_frames_ = header.crc_tag == kCrcTag\n'
    '            ? header.trimmed_frames : 0;',
    '        saved_trimmed_frames_ = 0;'),
  ('TestFrozenTrimBackup', 'backup', 'beads/background_buffer_backup.cc',
    '      saved_trimmed_frames_ = header.trimmed_frames;',
    '      saved_trimmed_frames_ = 0;'),
  ('TestStaleFlags', 'backup', 'beads/background_buffer_backup.cc',
    '    Flash::ClearErrors();\n    if (settings_->write_locked() || \\',
    '    if (settings_->write_locked() || \\'),
  ('TestStaleFlags', 'backup', 'beads/background_buffer_backup.cc',
    '    Flash::ClearErrors();\n    if (settings_->write_locked() || HAL_FLASH_Program(',
    '    if (settings_->write_locked() || HAL_FLASH_Program('),
  ('TestStaleFlags', 'backup', 'beads/background_buffer_backup.cc',
    '      Flash::ClearErrors();\n      if (settings_->write_locked() || HAL_FLASH_Program(',
    '      if (settings_->write_locked() || HAL_FLASH_Program('),
  ('TestLockedBackup', 'backup', 'beads/background_buffer_backup.cc',
    '    if (settings_->write_locked() || \\\n        HAL_FLASHEx_Erase(',
    '    if (false || \\\n        HAL_FLASHEx_Erase('),
  ('TestLockedBackup', 'backup', 'beads/background_buffer_backup.cc',
    '    if (settings_->write_locked() || HAL_FLASH_Program(\n'
    '        FLASH_TYPEPROGRAM_FLASHWORD,\n'
    '        flash_address,',
    '    if (false || HAL_FLASH_Program(\n'
    '        FLASH_TYPEPROGRAM_FLASHWORD,\n'
    '        flash_address,'),
  ('TestEraseDamagedSectors', 'backup', 'beads/background_buffer_backup.cc',
    '    if (Flash::IsReadable(Sector<8>::start + i * kSectorSize, kSectorSize)) {',
    '    if (false) {'),
  ('TestStorageRepair', 'firmware', 'beads/background_buffer_backup.cc',
    '    if (!settings_->write_locked()) {\n      HAL_FLASHEx_Erase(',
    '    if (true) {\n      HAL_FLASHEx_Erase('),
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
    '      options_.stage() != Options::STAGE_DRAIN && !showing_storage_status();',
    '      options_.stage() != Options::STAGE_DRAIN;'),
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
    '          if (mode_ == MODE_REPAIR_CALIBRATION_RESET) {',
    '          if (false) {'),
  ('TestStorageRepair', 'firmware', 'beads/ui.cc',
    '          color.r = fast_blink ? 255 : 0;',
    '          color.r = 255;'),
  ('TestHermiteRawMatchesScaled', 'delay', 'beads/dsp/delay_line.h',
    'const int32_t b_neg = w + a;',
    'const int32_t b_neg = w + a + 1;'),
  ('TestHermiteRawMatchesScaled', 'delay', 'beads/dsp/delay_line.h',
    'result.l = result.r = i ? odd : even;',
    'result.l = result.r = even;'),
]
# Validate every replacement before creating a disposable source tree.
for name, suite, source, before, after in cases:
  pattern = before if before is not None else (
      'const int16_t wav_integrated_waves[] = {')
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
started = time.monotonic()
results = []
indexed = []
with tempfile.TemporaryDirectory(prefix='.mutation-', dir='beads/test') as tmp:
  work = pathlib.Path(tmp).resolve()
  for directory in ('beads', 'plaits', 'stmlib', 'clouds'):
    shutil.copytree(root / directory, work / directory,
        ignore=shutil.ignore_patterns('.runs', '.mutation-*',
            '__pycache__', 'core*'))
  for index, (name, suite, source, before, after) in enumerate(cases, 1):
    if index not in selected_indices:
      continue
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
      table = original_test.find('const HostTest tests[]', main)
      if table >= 0:
        # Table-style main (clouds_test.cc, delay_mode_test.cc).
        cut = original_test.rindex('\n', 0, table) + 1
      else:
        first = re.compile(r'\n  Test\w+\(\);').search(
            original_test, main).group(0)[1:]
        cut = original_test.index(first, main)
      test.write_text(original_test[:cut] + name + '();\n' +
          '  return test_checker.failures() ? 1 : 0;\n}\n')
    log = out / ('%02d-%s.log' % (index, name))
    try:
      with log.open('w') as stream:
        run = subprocess.run(
            ['nice', '-n', '10', 'make', '-s', '-j4', '-f',
                'beads/test/' + suite + '.mk', 'test'],
            cwd=work, stdout=stream, stderr=subprocess.STDOUT,
            timeout=240 if suite in ('firmware', 'clouds', 'granular',
                'plaits_synth', 'spectral') else 60)
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
      indexed.append((index, results[-1]))
      print(results[-1], flush=True)
    finally:
      path.write_text(original)
      test.write_text(original_test)
(out / 'mutations.txt').write_text('\n'.join(results) + '\n')
(out / 'results.json').write_text(json.dumps(indexed) + '\n')
print('Mutation runtime: %.2f s' % (time.monotonic() - started))
