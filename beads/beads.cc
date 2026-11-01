// Copyright 2017-2021 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
// Author: Frédéric Meslin (fred@fredslab.net)
// Author: Oliver Rockstedt (info@sourcebox.de)
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

#include <algorithm>

#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/cv_reader.h"
#include "beads/drivers/codec.h"
#include "beads/drivers/debug_pin.h"
#include "beads/drivers/freeze_input.h"
#include "beads/drivers/seed_input.h"
#include "beads/drivers/system.h"
#include "beads/drivers/watchdog.h"
#include "beads/dsp/plaits_synth.h"
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/dwt_calibration.h"
#include "beads/factory_test.h"
#include "beads/gain_control.h"
#include "beads/normalization_detector.h"
#include "beads/settings.h"
#include "beads/ui.h"

using namespace std;
using namespace beads;
using namespace stmlib;

const int kBlockRate = 4000;
const float kReverbStyleFadeStep = 1.0f / (0.01f * kBlockRate);

const bool skip_factory_test = false;

uint8_t delay_buffer_1[512 * 1024] __attribute__ ((section (".ramd1")));
uint8_t delay_buffer_2[256 * 1024] __attribute__ ((section (".ramd2")));
uint16_t reverb_buffer[32768] __attribute__ ((section (".ramd3")));

// The Plaits voices borrow the first delay bank in synth mode, when nothing
// else reads or writes it. Leaving synth mode clears every delay bank
// (Processor::Configure) before the granular processor or the buffer backup
// use it again. Bank 0's last byte, which 12-bit media never clear, stays out.
STATIC_ASSERT(kPlaitsSynthRamSize < 256 * 1024, plaits_voices_in_bank_0);

BackgroundBufferBackup background_buffer_backup;
Calibrator calibrator;
CvReader cv_reader;
Codec codec;
FactoryTest factory_test;
FreezeInput freeze_input;
GainControl gain_control;
NormalizationDetector normalization_detector;
PlaitsSynth plaits_synth;
Processor processor;
SeedInput seed_input;
Settings settings;
Synth synth;
Ui ui;

volatile bool ready = false;

// #define PROFILE_INTERRUPT

extern "C" {
  
void SysTick_Handler() {
  // Takes about 2µs to complete
  HAL_IncTick();
  Watchdog::Reload();
  if (!ready) {
    return;
  }
  ui.Poll();
  if (!skip_factory_test) {
    factory_test.Poll();
  }
}

}


// This counter is used to temporarily blank the codec output and disable
// the AGC while the normalization detection system is still catching up with
// a codec restart.
int codec_transition_frames;
bool clouds_style;
float reverb_style_gain;
// Set by main before it restarts the codec: the output fades out first.
volatile bool codec_fade;
bool codec_running;
// The rate the converter runs at; 0 before its first start.
int codec_sample_rate;
float reverb_style_previous_gain;
bool synth_mode;
int synth_mode_start_time;

GateFlags previous_seed;
uint8_t seed_latch_mode;
bool seed_latch_enabled;
stmlib::DelayLine<Codec::Event, 256> event_delay;

void Process(
    const Codec::Event* events,
    const Codec::Frame* in,
    Codec::Frame* out,
    size_t size) {
#ifdef PROFILE_INTERRUPT
  ScopedDebugPinToggler toggler;
#endif  // PROFILE_INTERRUPT
#ifdef DWT_CALIBRATION
  dwt_calibration::ScopedBlockTimer calibration_callback;
#endif  // DWT_CALIBRATION
  FloatFrame float_in[kMaxCodecBlockSize];
  FloatFrame float_out[kMaxCodecBlockSize];
  bool freeze[kMaxCodecBlockSize];
  GateFlags seed[kMaxCodecBlockSize];


  // Normalization detection.
  normalization_detector.Process(
      events[size - 1],
      in[size - 1],
      cv_reader.cv_values());
  
  // CV Reading
  cv_reader.Read();
  for (int i = 0; i < Ui::MIX_PARAMETER_LAST; ++i) {
    ui.ProcessPot(
        Ui::MixParameter(i),
        cv_reader.pot(PotsAdcChannel(POTS_ADC_CHANNEL_FEEDBACK + i)));
  }
  
  // Conversion of events into the right format.
  bool freeze_block = false;
  for (size_t i = 0; i < size; ++i) {
    freeze[i] = ui.freeze_button_state() || (events[i] & Codec::EVENT_FREEZE);
    freeze_block |= freeze[i];
  }
  ui.set_freeze_led_state(freeze_block);
  
  const AttackMode attack_mode = ui.attack_mode();
  if (attack_mode != processor.attack_input()->mode()) {
    processor.SetAttacks(attack_mode, kAttackThreshold);
  }
  bool attack_onset = false;
  if (attack_mode && !codec_transition_frames && !freeze_block &&
      !synth_mode && processor.attack_recording()) {
    attack_onset = processor.attack_input()->Process(
        in,
        size,
        1.0f / 2147483648.0f,
        gain_control.peak(),
        processor.sample_rate(),
        normalization_detector.normalized(
            NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT),
        normalization_detector.normalized(
            NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT));
    processor.SetAttackOnset(attack_onset);
  }
  const bool attack_gate = attack_onset &&
      attack_mode == ATTACK_MODE_SNAP_AND_TRIGGER;
  const GateFlags seed_at_block_start = previous_seed;
  bool seed_block = false;
  GateFlags* seed_input = NULL;
  if (!normalization_detector.normalized(NORMALIZATION_DETECTOR_CHANNEL_SEED)) {
    for (size_t i = 0; i < size; ++i) {
      event_delay.Write(events[i]);
      // Delay the seed input by 2.5ms.
      Codec::Event delayed_event = event_delay.Read(size * 10);
      bool has_seed = delayed_event & Codec::EVENT_SEED;
      previous_seed = seed[i] = ExtractGateFlags(previous_seed, has_seed);
      seed_block |= has_seed;
    }
    seed_input = seed;
  } else if (attack_mode == ATTACK_MODE_SNAP_AND_TRIGGER) {
    // Attacks trigger grains as SEED would.
    for (size_t i = 0; i < size; ++i) {
      previous_seed = seed[i] = ExtractGateFlags(
          previous_seed, attack_gate && i == 0);
    }
    seed_input = seed;
  } else {
    previous_seed = GATE_FLAG_LOW;
  }
  if (attack_gate && seed_input) {
    // An attack adds a one-sample gate to SEED: only its first two edges can
    // change, and the SEED input loop above stays as it was.
    seed[0] = ExtractGateFlags(seed_at_block_start, true);
    if (size > 1) {
      seed[1] = ExtractGateFlags(seed[0], seed[1] & GATE_FLAG_HIGH);
    }
    previous_seed = seed[size - 1];
    seed_block = true;
  }
  ui.set_seed_led_state(seed_block && !codec_transition_frames);
  ui.set_overwrite_reserve(synth_mode || processor.spectral()
      ? 1.0f : processor.overwrite_reserve());
  
  // Conversion of input samples into floats, (with automatic gain control).
  gain_control.Process(
      !codec_transition_frames,
      settings.state().gain,
      normalization_detector.normalized(
          NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT),
      normalization_detector.normalized(
          NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT),
      in,
      float_in,
      size);
  if (codec_transition_frames) {
    --codec_transition_frames;
    Codec::Frame silence;
    silence.l = 0;
    silence.r = 0;
    fill(&out[0], &out[size], silence);
    return;
  }
  
  Parameters p;
  p.time.base = cv_reader.pot(POTS_ADC_CHANNEL_TIME);
  p.time.attenuverter = cv_reader.pot(POTS_ADC_CHANNEL_TIME_AMOUNT);
  p.time.has_cv = !normalization_detector.normalized(
      NORMALIZATION_DETECTOR_CHANNEL_TIME_CV);
  p.time.cv = cv_reader.cv(CV_ADC_CHANNEL_TIME);
  
  p.size.base = cv_reader.pot(POTS_ADC_CHANNEL_SIZE);
  p.size.attenuverter = cv_reader.pot(POTS_ADC_CHANNEL_SIZE_AMOUNT);
  p.size.has_cv = !normalization_detector.normalized(
      NORMALIZATION_DETECTOR_CHANNEL_SIZE_CV);
  p.size.cv = cv_reader.cv(CV_ADC_CHANNEL_SIZE);

  p.shape.base = cv_reader.pot(POTS_ADC_CHANNEL_SHAPE);
  p.shape.attenuverter = cv_reader.pot(POTS_ADC_CHANNEL_SHAPE_AMOUNT);
  p.shape.has_cv = !normalization_detector.normalized(
      NORMALIZATION_DETECTOR_CHANNEL_SHAPE_CV);
  p.shape.cv = cv_reader.cv(CV_ADC_CHANNEL_SHAPE);
  
  p.pitch.base = cv_reader.pot(POTS_ADC_CHANNEL_PITCH);
  p.pitch.attenuverter = cv_reader.pot(POTS_ADC_CHANNEL_PITCH_AMOUNT);
  p.pitch.has_cv = !normalization_detector.normalized(
      NORMALIZATION_DETECTOR_CHANNEL_PITCH_CV);
  p.pitch.cv = cv_reader.cv(CV_ADC_CHANNEL_PITCH);
  
  p.monophonic_mode = settings.state().monophonic_mode;

  ModulatedParameter d;
  d.base = cv_reader.pot(POTS_ADC_CHANNEL_DENSITY);
  d.cv = cv_reader.cv(CV_ADC_CHANNEL_DENSITY);
  if (normalization_detector.normalized(
      NORMALIZATION_DETECTOR_CHANNEL_DENSITY_CV)) {
    // Avoid small but annoying modulation due to noise on the density
    // CV input.
    d.cv = 0.0f;
  }

  const bool width_assigned = settings.width_assigned();
  const bool diffusion_assigned = settings.diffusion_assigned();
  const bool mix_assigned = !width_assigned && !diffusion_assigned;
  const float assign_cv = cv_reader.cv(CV_ADC_CHANNEL_ASSIGN) - 0.002f;
  float mix_parameter[Ui::MIX_PARAMETER_LAST];
  for (int i = 0; i < Ui::MIX_PARAMETER_LAST; ++i) {
    float base = ui.mix_parameter(Ui::MixParameter(i)) * 1.005f;
    float amount = mix_assigned &&
        settings.state().assignable_cv_destination == 3
        ? float(settings.state().assignable_cv_amount[i]) / 256.0f
        : (mix_assigned &&
            settings.state().assignable_cv_destination == i ? 1.0f : 0.0f);
    mix_parameter[i] = base + \
        assign_cv * amount;
    CONSTRAIN(mix_parameter[i], 0.0f, 1.0f);
  }

  const float stereo_width = ModulatedStereoWidth(
      ui.stereo_width(), width_assigned ? assign_cv : 0.0f);

  Processor::MixParameters m;
  m.feedback = mix_parameter[Ui::MIX_PARAMETER_FEEDBACK];
  m.dry_wet = mix_parameter[Ui::MIX_PARAMETER_DRY_WET];
  m.reverb_amount = mix_parameter[Ui::MIX_PARAMETER_REVERB];
  m.output_gate = settings.state().gate_output_mode;

  SeedButtonState seed_button_state = ui.seed_button_locked()
      ? SEED_BUTTON_STATE_LOCKED
      : (ui.seed_button_state() ? SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF);
  
  if (seed_latch_enabled && seed_input &&
      !normalization_detector.normalized(NORMALIZATION_DETECTOR_CHANNEL_SEED) &&
      seed_button_state == SEED_BUTTON_STATE_LOCKED) {
    if (seed_latch_mode == 1) {
      seed_input = NULL;
      ui.set_seed_led_state(false);
    } else {
      seed_button_state = SeedButtonState(
          SEED_BUTTON_STATE_IGNORE + seed_latch_mode - 1);
    }
  }

  if (ui.trim_flag()) {
    if (!processor.spectral()) {
      processor.Trim();
    }
    ui.ClearTrimFlag();
  }
  // Fade to silence before changing delay topology in the shared buffer,
  // and before main swaps the engine.
  const bool requested_style = ui.clouds_style();
  const bool engine_change = ui.spectral() != processor.spectral();
  reverb_style_previous_gain = reverb_style_gain;
  if (requested_style != clouds_style || engine_change || codec_fade) {
    reverb_style_gain = max(0.0f, reverb_style_gain - kReverbStyleFadeStep);
    if (reverb_style_previous_gain == 0.0f) {
      if (requested_style != clouds_style) {
        clouds_style = requested_style;
        processor.SetCloudsStyle(clouds_style);
        synth.SetCloudsStyle(clouds_style, false);
        plaits_synth.SetCloudsStyle(clouds_style, false);
      }
      Codec::Frame silence;
      silence.l = 0;
      silence.r = 0;
      fill(&out[0], &out[size], silence);
      return;
    }
  } else if (reverb_style_gain < 1.0f) {
    reverb_style_gain = min(1.0f, reverb_style_gain + kReverbStyleFadeStep);
  }
#ifdef DWT_CALIBRATION
  if (dwt_calibration::stats.synthetic) {
    dwt_calibration::PrepareSynthetic(
        &processor, delay_buffer_1, sizeof(delay_buffer_1),
        delay_buffer_2, sizeof(delay_buffer_2), reverb_buffer,
        sizeof(reverb_buffer) / sizeof(reverb_buffer[0]),
        clouds_style, &synth_mode, &p, &d, &m, &seed_button_state,
        &seed_input, seed, freeze, float_in, size, &calibration_callback);
  }
#endif  // DWT_CALIBRATION
  processor.set_output_limiter_mode(ui.output_limiter_mode());
  synth.set_output_limiter_mode(ui.output_limiter_mode());
  if (synth_mode && ui.plaits_synth()) {
    plaits_synth.set_output_limiter_mode(ui.output_limiter_mode());
    plaits_synth.set_pitch_scale(ui.pitch_scale());
    plaits_synth.set_stereo_width(stereo_width, ui.stereo_width() != 0);
#ifdef DWT_CALIBRATION
    const uint32_t calibration_start = dwt_calibration::Read();
#endif  // DWT_CALIBRATION
    plaits_synth.Render(
        seed_button_state,
        seed_input,
        freeze,
        Synth::Resolution(settings.state().media),
        d, p, m,
        float_out, size);
#ifdef DWT_CALIBRATION
    dwt_calibration::Record(dwt_calibration::BRACKET_INNER,
        dwt_calibration::Read() - calibration_start);
#endif  // DWT_CALIBRATION
  } else if (synth_mode) {
    synth.set_pitch_scale(ui.pitch_scale());
    synth.set_random_intervals(ui.random_intervals());
    synth.set_stereo_width(stereo_width, ui.stereo_width() != 0);
#ifdef DWT_CALIBRATION
    const uint32_t calibration_start = dwt_calibration::Read();
#endif  // DWT_CALIBRATION
    synth.Render(
        seed_button_state,
        seed_input,
        freeze,
        Synth::Resolution(settings.state().media),
        d, p, m,
        float_out, size);
#ifdef DWT_CALIBRATION
    dwt_calibration::Record(dwt_calibration::BRACKET_INNER,
        dwt_calibration::Read() - calibration_start);
#endif  // DWT_CALIBRATION
  } else {
    processor.set_pitch_scale(ui.pitch_scale());
    processor.set_random_intervals(ui.random_intervals());
    processor.set_stereo_width(stereo_width, ui.stereo_width() != 0);
    processor.set_delay_mode(ui.delay_mode());
    processor.set_evolving_feedback(ui.evolving_feedback());
    float diffusion = settings.diffusion_set()
        ? settings.diffusion()
        : (clouds_style ? kCloudsDiffusion : 0.0f);
    if (diffusion_assigned) {
      diffusion += assign_cv;
      CONSTRAIN(diffusion, 0.0f, 1.0f);
    }
    processor.set_diffusion(diffusion);
#ifdef DWT_CALIBRATION
    const uint32_t calibration_start = dwt_calibration::Read();
#endif  // DWT_CALIBRATION
    processor.Process(
        seed_button_state,
        seed_input,
        freeze,
        d, p, m,
        float_in,
        float_out, size);
#ifdef DWT_CALIBRATION
    dwt_calibration::Record(dwt_calibration::BRACKET_INNER,
        dwt_calibration::Read() - calibration_start);
#endif  // DWT_CALIBRATION
  }
  
  if (reverb_style_previous_gain != 1.0f || reverb_style_gain != 1.0f) {
    ParameterInterpolator gain(
        &reverb_style_previous_gain, reverb_style_gain, size);
    for (size_t i = 0; i < size; ++i) {
      float_out[i] = float_out[i] * gain.Next();
    }
  }

  // Conversion of output samples into integers.
  for (size_t i = 0; i < size; ++i) {
    float l = float_out[i].l;
    float r = float_out[i].r;
    CONSTRAIN(l, -1.0f, 0.999999f);
    CONSTRAIN(r, -1.0f, 0.999999f);
    if (fabsf(l) > 0.01f || fabsf(r) > 0.01f) {
      synth_mode_start_time = 0;
    }
    out[i].l = static_cast<int32_t>(l * 2147483648.0f);
    out[i].r = static_cast<int32_t>(r * 2147483648.0f);
  }
}

void ConfigureAndStartCodec(bool new_synth_mode, bool reset_gain_control) {
  int sr = new_synth_mode ? 48000 : int(processor.sample_rate());
  synth_mode = new_synth_mode;
  synth_mode_start_time = 0;
  if (synth_mode) {
    plaits_synth.InitVoices(delay_buffer_1);
  }

  gain_control.Init(
      float(sr),
      processor.media() == Processor::MEDIA_DIRTY_TAPE
          ? GainControl::SOFT_CLIPPING_MODE_ON
          : (processor.media() == Processor::MEDIA_CLEAN_TAPE
                ? GainControl::SOFT_CLIPPING_MODE_DYNAMIC
                : GainControl::SOFT_CLIPPING_MODE_OFF),
      reset_gain_control);
  normalization_detector.Init(sr == 48000 ? 2 : (sr == 32000 ? 3 : 4));
  event_delay.Init();

  codec_transition_frames = kBlockRate / 4;
  codec_fade = false;
  if (codec_running && sr == codec_sample_rate) {
    // Same clocks: the converter played silence while paused (#53).
    codec.Resume(&Process);
    return;
  }
  if (codec_sample_rate) {
    codec.Stop();
  }
  codec_running = true;
  codec_sample_rate = sr;
  codec.Start(sr, sr / kBlockRate, &Process);
}

void Init() {
  System sys;
  sys.Init(true);
#ifdef DWT_CALIBRATION
  dwt_calibration::Init();
#endif  // DWT_CALIBRATION

  Watchdog::Start();
  freeze_input.Init();
  seed_input.Init();
  codec.Init();

  cv_reader.Init(&settings);
  calibrator.Init(&settings, &cv_reader, &normalization_detector);
  
  uint8_t* delay_buffers[3];
  delay_buffers[0] = delay_buffer_1;
  delay_buffers[1] = delay_buffer_1 + (256 * 1024);
  delay_buffers[2] = delay_buffer_2;
  
  processor.Init(delay_buffers, reverb_buffer);
  synth.Init(reverb_buffer);
  plaits_synth.Init(reverb_buffer);
  clouds_style = false;
  reverb_style_gain = 1.0f;
  reverb_style_previous_gain = 1.0f;
  codec_fade = false;
  codec_running = false;
  codec_sample_rate = 0;
  background_buffer_backup.Init(&processor, &settings);

  ui.Init(&settings, &gain_control, &calibrator, &background_buffer_backup);
  
  factory_test.Init(
      &cv_reader,
      &normalization_detector,
      &calibrator,
      &ui.switches());
  
  gain_control.Init(48000.0f, GainControl::SOFT_CLIPPING_MODE_OFF, true);

  const bool recovery_mode = ui.pressed_immediate(SWITCH_SEED);
  const bool freshly_baked = !settings.Init(recovery_mode);
  if (settings.repair() != STORAGE_REPAIR_NONE) {
    background_buffer_backup.EraseDamagedSectors();
  }
  ui.LoadOptions();
  if (freshly_baked && !skip_factory_test) {
    factory_test.Start();
    ui.set_factory_test(true);
    normalization_detector.Init(2);
    codec_sample_rate = 48000;
    codec.Start(48000, 48000 / kBlockRate, &FactoryTest::ProcessFn);
  } else {
#ifdef PROFILE_INTERRUPT
    DebugPin::Init();
#endif  // PROFILE_INTERRUPT
    ui.set_factory_test(false);
    ui.ShowStorageStatus();
    synth_mode_start_time = 0;
    synth_mode = true;
  }
  ready = true;
  
  // A frozen buffer saved by the granular engine stays in flash.
  processor.set_spectral(ui.spectral());
  bool was_frozen = !processor.spectral() && background_buffer_backup.Load();
  if (was_frozen) {
    ui.set_freeze_button_state(was_frozen);
    ConfigureAndStartCodec(false, true);
  }
}

// Codec restarts fade the output out first (#53): true once it is silent.
bool FadedOut() {
  // Before the first start there is no audio to fade.
  codec_fade = codec_running;
  return !codec_running || reverb_style_gain == 0.0f;
}

void MonitorStateChange() {
  seed_latch_mode = settings.FeatureValue(FEATURE_SEED_LATCH);
  seed_latch_enabled = seed_latch_mode != 0;
  gain_control.set_attenuation(settings.FeatureValue(FEATURE_ATTENUATION));
  processor.set_time_scan(settings.FeatureValue(FEATURE_TIME_SCAN));
  ui.set_delay_active(processor.delay_active());
  if (settings.FeatureValue(FEATURE_PITCH_NOTCH) ||
      settings.FeatureValue(FEATURE_LIVE_ASSIGN)) {
    ui.set_modulation_indication(cv_reader.pitch_on_notch(),
        cv_reader.cv(CV_ADC_CHANNEL_ASSIGN));
  }
  ui.set_no_input(normalization_detector.no_input());
  if (ui.should_ignore_state_change() || codec_transition_frames) {
    synth_mode_start_time = 0;
    ui.ClearSelfPlayRequest();
    codec_fade = false;
    return;
  }

  // Once faded out, swap the engine. This clears the recording; a frozen
  // buffer saved to flash returns with the granular engine.
  if (ui.spectral() != processor.spectral() && reverb_style_gain == 0.0f) {
    codec.Pause();
    processor.set_spectral(ui.spectral());
    if (processor.spectral()) {
      background_buffer_backup.Init(&processor, &settings);
    }
    processor.Configure(Processor::Media(settings.state().media),
        normalization_detector.mono());
    if (synth_mode && settings.FeatureValue(FEATURE_SYNTH_TAIL)) {
      processor.ClearReverb();
    }
    if (!processor.spectral() && background_buffer_backup.Load()) {
      // The restored recording keeps the quality it was saved with.
      settings.mutable_state()->media = processor.media();
      ui.set_freeze_button_state(true);
    }
    ConfigureAndStartCodec(false, synth_mode);
    return;
  }
  
  // The wavetable synth does not start in spectral mode.
  if (normalization_detector.no_input() && !processor.spectral()) {
    if (settings.self_play_on_request()) {
      // Self-play starts and stops only on a MEDIA hold.
      if (ui.self_play_requested()) {
        if (!FadedOut()) {
          return;
        }
        ui.ClearSelfPlayRequest();
        codec.Pause();
        if (synth_mode) {
          processor.Configure(Processor::Media(settings.state().media),
              normalization_detector.mono());
          if (settings.FeatureValue(FEATURE_SYNTH_TAIL)) {
            processor.ClearReverb();
          }
        }
        ConfigureAndStartCodec(!synth_mode, true);
      }
    } else if (!synth_mode) {
      if (!synth_mode_start_time) {
        synth_mode_start_time = system_clock.milliseconds();
        if (!synth_mode_start_time) synth_mode_start_time = 1;
      } else {
        if ((system_clock.milliseconds() - synth_mode_start_time) >= 10000) {
          // If 10 seconds have elapsed with nothing patched in the audio
          // inputs, enable the synth mode.
          if (!FadedOut()) {
            return;
          }
          codec.Pause();
          ConfigureAndStartCodec(true, true);
        }
      }
    }
  } else {
    // Awful block of code to troubleshoot glitches related to changes of
    // sample rate and more general codec settings.
    //
    // static uint32_t elapsed = 0;
    // bool autoreset = (system_clock.milliseconds() - elapsed) >= 400;
    // if (autoreset) {
    //   settings.mutable_state()->media = 
    //       (settings.mutable_state()->media + 1) % 4;
    // }

    Processor::Media media = Processor::Media(settings.state().media);
    bool mono = normalization_detector.mono();
    bool media_changed = media != processor.media();
    bool mono_changed = mono != processor.mono();
    
    if (synth_mode || mono_changed || media_changed) {
      if (!FadedOut()) {
        return;
      }
      // elapsed = system_clock.milliseconds();
      codec.Pause();
      processor.Configure(media, mono);
      if (synth_mode && settings.FeatureValue(FEATURE_SYNTH_TAIL)) {
        processor.ClearReverb();
      }
      ConfigureAndStartCodec(false, mono_changed || synth_mode);
    }
    synth_mode_start_time = 0;
    ui.ClearSelfPlayRequest();
  }
  // Restarted, or nothing to restart after all: fade back in.
  codec_fade = false;
}

int main(void) {
  Init();

  while (1) {
    ui.DoEvents();
    MonitorStateChange();
    if (!synth_mode && !processor.spectral()) {
      background_buffer_backup.Process(ui.freeze_led_state());
    }
    processor.Buffer();
    processor.WasteCPUCycles(16);
  }
}

