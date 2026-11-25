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
#include "beads/dsp/rings_synth.h"
#ifdef PLAITS_BLOBS
#include "beads/blob/region_loader.h"
#include "stmlib/utils/random.h"
#endif  // PLAITS_BLOBS
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/blob/synth_choice.h"
#include "beads/dwt_calibration.h"
#include "beads/engine_transition.h"
#include "beads/factory_test.h"
#include "beads/gain_control.h"
#include "beads/normalization_detector.h"
#include "beads/settings.h"
#include "beads/ui.h"

using namespace std;
using namespace beads;
using namespace stmlib;

const bool skip_factory_test = false;

uint8_t delay_buffer_1[512 * 1024] __attribute__ ((section (".ramd1")));
uint8_t delay_buffer_2[256 * 1024] __attribute__ ((section (".ramd2")));
uint16_t reverb_buffer[32768] __attribute__ ((section (".ramd3")));

// Plaits borrows bank 0; Configure clears it before other users return.
// Exclude its last byte: 12-bit media never clear it.
STATIC_ASSERT(kPlaitsSynthRamSize < 256 * 1024, plaits_voices_in_bank_0);
const size_t kBank0Size = 256 * 1024 - 1;
// Feature 42's values.
const int kRingsPolyphonies[3] = { 1, 2, 4 };

BackgroundBufferBackup background_buffer_backup;
Calibrator calibrator;
CvReader cv_reader;
Codec codec;
FactoryTest factory_test;
FreezeInput freeze_input;
GainControl gain_control;
NormalizationDetector normalization_detector;
PlaitsSynth plaits_synth;
RingsSynth rings_synth;
#ifdef PLAITS_BLOBS
// Read once at boot; its identity record is readable over SWD.
PlaitsRegion plaits_region;
#endif  // PLAITS_BLOBS
// What can play, what plays, and which synth's RAM is in bank 0.
uint8_t playable_synths;
EngineTransition transition;
Processor processor;
SeedInput seed_input;
Settings settings;
Synth synth;
Ui ui;

volatile bool ready = false;
uint8_t resonator_placement;

// The page's curated value, or the exact one Hygrostat fixed.
float ResonatorValue(ProfileFeature feature, float curated, float scale) {
  const ProfileEntry& e = settings.profile().entry(feature);
  return e.state == PROFILE_FIXED ? float(e.value) * scale : curated;
}

void UpdateResonator() {
  if (!ui.resonator()) {
    return;
  }
  const ResonatorMaterial& material = kResonatorMaterials[
      ui.resonator_material()];
  ResonatorControls c;
  c.structure = ResonatorValue(
      FEATURE_RESONATOR_STRUCTURE, material.structure, 1.0f / 256.0f);
  c.brightness = ResonatorValue(
      FEATURE_RESONATOR_BRIGHTNESS, material.brightness, 1.0f / 256.0f);
  c.damping = ResonatorValue(FEATURE_RESONATOR_DAMPING,
      kResonatorRingTimes[ui.resonator_ring()], 1.0f / 256.0f);
  // The CV also reaches FEEDBACK (destination 0): the resonator holds the knob.
  c.feedback = ui.mix_parameter(Ui::MIX_PARAMETER_FEEDBACK) * 1.005f;
  CONSTRAIN(c.feedback, 0.0f, 1.0f);
  c.hold_feedback = ui.resonator_assigned();
  if (ui.resonator_assigned()) {
    c.damping += cv_reader.cv(CV_ADC_CHANNEL_ASSIGN) - 0.002f;
    // Below the table's last entry, as Rings holds STRUCTURE.
    CONSTRAIN(c.damping, 0.0f, 0.9995f);
  }
  c.position = ResonatorValue(
      FEATURE_RESONATOR_POSITION, kResonatorPosition, 1.0f / 256.0f);
  c.amount = ResonatorValue(FEATURE_RESONATOR_AMOUNT, 1.0f, 1.0f / 128.0f);
  // An exact structure is Rings' curve, even under a Drizzlet object.
  c.modes = settings.profile().entry(FEATURE_RESONATOR_STRUCTURE).state ==
      PROFILE_FIXED ? NULL : material.modes;
  c.chord_gain = material.chord_gain;
  processor.set_resonator_controls(c);
}

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


bool synth_mode;
bool synth_fade;
int synth_mode_start_time;

GateFlags previous_seed;
uint8_t seed_latch_mode;
bool seed_latch_enabled;
stmlib::DelayLine<Codec::Event, 256> event_delay;

template<bool chord>
void ProcessAudio(
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
    processor.SetAttacks(attack_mode, ui.attack_threshold());
  }
  // Snap and trigger keeps listening while frozen, only to start grains.
  const bool attack_trigger_mode = attack_mode == ATTACK_MODE_SNAP_AND_TRIGGER;
  bool attack_onset = false;
  if (attack_mode && !transition.blanking() &&
      (!freeze_block || attack_trigger_mode) &&
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
    if (!freeze_block) {
      processor.SetAttackOnset(attack_onset);
    }
  }
  // Hits add grains apart from SEED, in granular playback only.
  const bool attack_trigger = attack_onset && attack_trigger_mode;
  processor.set_attack_trigger(attack_trigger);
  bool seed_block = attack_trigger;
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
  } else {
    previous_seed = GATE_FLAG_LOW;
  }
  ui.set_seed_led_state(seed_block && !transition.blanking());
  ui.set_overwrite_reserve(synth_mode || processor.spectral()
      ? 1.0f : processor.overwrite_reserve());
  
  // Conversion of input samples into floats, (with automatic gain control).
  gain_control.Process(
      !transition.blanking(),
      settings.state().gain,
      normalization_detector.normalized(
          NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT),
      normalization_detector.normalized(
          NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT),
      in,
      float_in,
      size);
  if (transition.Blanking(out, size)) {
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
  
  // Only a patched cable gives a SEED input.
  if (seed_latch_enabled && seed_input &&
      seed_button_state == SEED_BUTTON_STATE_LOCKED) {
    if (seed_latch_mode == 1) {
      seed_input = NULL;
      ui.set_seed_led_state(attack_trigger);
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
  // and before main swaps the engine, which waits for FREEZE to be released.
  const bool requested_style = ui.clouds_style();
  const bool engine_change = ui.spectral() != processor.spectral() &&
      !ui.should_ignore_state_change();
  if (transition.Hold(requested_style, engine_change, &processor, &synth,
      &plaits_synth, &rings_synth, out, size)) {
    return;
  }
#ifdef DWT_CALIBRATION
  if (dwt_calibration::stats.synthetic) {
    dwt_calibration::PrepareSynthetic(
        &processor, delay_buffer_1, sizeof(delay_buffer_1),
        delay_buffer_2, sizeof(delay_buffer_2), reverb_buffer,
        sizeof(reverb_buffer) / sizeof(reverb_buffer[0]),
        transition.clouds_style(), &synth_mode, &p, &d, &m, &seed_button_state,
        &seed_input, seed, freeze, float_in, size, &calibration_callback);
  }
#endif  // DWT_CALIBRATION
  processor.set_output_limiter_mode(ui.output_limiter_mode());
  synth.set_output_limiter_mode(ui.output_limiter_mode());
  if (synth_mode && transition.playing_synth() == SYNTH_SILENT) {
    FloatFrame silence;
    silence = 0.0f;
    fill(&float_out[0], &float_out[size], silence);
  } else if (synth_mode && transition.playing_synth() == SYNTH_RINGS) {
    rings_synth.set_output_limiter_mode(ui.output_limiter_mode());
    rings_synth.set_pitch_scale(ui.pitch_scale());
    rings_synth.set_stereo_width(stereo_width, ui.stereo_width() != 0);
#ifdef DWT_CALIBRATION
    const uint32_t calibration_start = dwt_calibration::Read();
#endif  // DWT_CALIBRATION
    rings_synth.Render(
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
  } else if (synth_mode && transition.playing_synth() == SYNTH_PLAITS) {
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
        : (transition.clouds_style() ? kCloudsDiffusion : 0.0f);
    if (diffusion_assigned) {
      diffusion += assign_cv;
      CONSTRAIN(diffusion, 0.0f, 1.0f);
    }
    processor.set_diffusion(diffusion);
#ifdef DWT_CALIBRATION
    const uint32_t calibration_start = dwt_calibration::Read();
    const bool render_chord = chord && processor.chord_enabled();
#else
    const bool render_chord = chord;
#endif  // DWT_CALIBRATION
    if (render_chord) {
      processor.ProcessChordBlock(seed_button_state, seed_input, freeze,
          d, p, m, float_in, float_out, size);
    } else {
      processor.Process(seed_button_state, seed_input, freeze,
          d, p, m, float_in, float_out, size);
    }
#ifdef DWT_CALIBRATION
    dwt_calibration::Record(dwt_calibration::BRACKET_INNER,
        dwt_calibration::Read() - calibration_start);
#endif  // DWT_CALIBRATION
  }
  
  transition.Apply(float_out, size);

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

#ifdef TEST
void Process(const Codec::Event* events, const Codec::Frame* in,
    Codec::Frame* out, size_t size) {
  ProcessAudio<false>(events, in, out, size);
}
const Codec::FillBufferCallback kLegacyCallback = &Process;
#else
const Codec::FillBufferCallback kLegacyCallback = &ProcessAudio<false>;
#endif  // TEST

// Selected only for synth profiles with an output trim.
void ProcessSynthLevel(
    const Codec::Event* events,
    const Codec::Frame* in,
    Codec::Frame* out,
    size_t size) {
  ProcessAudio<false>(events, in, out, size);
  const int32_t divisor = 1 << settings.FeatureValue(FEATURE_SYNTH_LEVEL);
  if (settings.state().gate_output_mode) {
    for (size_t i = 0; i < size; ++i) {
      out[i].l /= divisor;
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      out[i].l /= divisor;
      out[i].r /= divisor;
    }
  }
}

Codec::FillBufferCallback AudioCallback(bool for_synth) {
  if (for_synth) {
    return settings.FeatureValue(FEATURE_SYNTH_LEVEL)
        ? &ProcessSynthLevel : kLegacyCallback;
  }
  return resonator_placement == RESONATOR_CHORD
      ? &ProcessAudio<true> : kLegacyCallback;
}

void ConfigureAndStartCodec(bool new_synth_mode, bool reset_gain_control) {
  int sr = new_synth_mode ? 48000 : int(processor.sample_rate());
  if (new_synth_mode && background_buffer_backup.busy()) {
    background_buffer_backup.Process(false);
  }
  synth_mode = new_synth_mode;
  synth_fade = false;
  synth_mode_start_time = 0;
  if (synth_mode) {
    // Rings when it plays, else Plaits, so Beads <-> Plaits stays instant.
    transition.TakeBank(transition.playing_synth() == SYNTH_RINGS
        ? SYNTH_RINGS : SYNTH_PLAITS);
  } else {
    // The granular processor's Configure clears the bank.
    transition.set_bank_owner(SYNTH_SILENT);
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

  transition.Start(sr, AudioCallback(new_synth_mode), true);
}

void SwapPaused(SynthEngine owner, SynthEngine playing) {
  transition.SwapPaused(owner, playing, AudioCallback(synth_mode));
}

void Init() {
#ifdef PLAITS_BLOBS
  // Its state is not loaded: seed it with stmlib's initial value.
  Random::Seed(0x21);
#endif  // PLAITS_BLOBS
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
  rings_synth.Init(reverb_buffer);
  playable_synths = SynthMask(true, true, false);
  transition.Init(&codec, &plaits_synth, &rings_synth, &ui,
      delay_buffer_1, kBank0Size);
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
#ifdef PLAITS_BLOBS
  RegionLoader region_loader;
  const Profile& profile = settings.profile();
  playable_synths = region_loader.Load(profile, kBank0Size, &plaits_region);
  integrated_waves = plaits_region.waves();
  rings_synth.set_blob(playable_synths & (1 << SYNTH_RINGS)
      ? plaits_region.synth_blob() : NULL);
  plaits_synth.set_bank_table(profile.plaits_banks());
  plaits_synth.set_region(&plaits_region);
#endif  // PLAITS_BLOBS
  ui.set_playable_synths(playable_synths);
  transition.set_playing_synth(EffectiveSynth(ui.synth(), playable_synths));
  ui.set_playing_synth(transition.playing_synth());
  cv_reader.set_smooth_pitch(settings.FeatureValue(FEATURE_PITCH_LAW));
  processor.set_gentle_mix(settings.FeatureValue(FEATURE_MIX_LAW) == 1);
  processor.set_send_mix(settings.FeatureValue(FEATURE_MIX_LAW) == 2);
  processor.set_output_division(
      1 << settings.FeatureValue(FEATURE_OUTPUT_DIVISION));
  processor.set_reverb_on_grains(settings.FeatureValue(FEATURE_REVERB_INPUT));
  // 0, the default, is the 2021/2024 grain stealing; 1 is the 2026 source's.
  const bool legacy_stealing = !settings.FeatureValue(FEATURE_GRAIN_STEALING);
  processor.set_legacy_stealing(legacy_stealing);
  synth.set_legacy_stealing(legacy_stealing);
  processor.set_unity_feedback(settings.FeatureValue(FEATURE_UNITY_FEEDBACK));
  processor.set_delay_crossfade(
      settings.FeatureValue(FEATURE_DELAY_CROSSFADE));
  resonator_placement = settings.FeatureValue(FEATURE_RESONATOR);
  processor.set_resonator(resonator_placement);
  if (freshly_baked && !skip_factory_test) {
    factory_test.Start();
    ui.set_factory_test(true);
    normalization_detector.Init(2);
    transition.Start(48000, &FactoryTest::ProcessFn, false);
  } else {
#ifdef PROFILE_INTERRUPT
    DebugPin::Init();
#endif  // PROFILE_INTERRUPT
    ui.set_factory_test(false);
    ui.ShowStorageStatus();
    synth_mode_start_time = 0;
    synth_fade = false;
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

void MonitorStateChange() {
  if (transition.RetryCodec()) {
    return;
  }
  UpdateResonator();
  seed_latch_mode = ui.seed_latch();
  seed_latch_enabled = seed_latch_mode != 0;
  gain_control.set_attenuation(settings.FeatureValue(FEATURE_ATTENUATION));
  gain_control.set_ceiling(ui.gain_ceiling());
  processor.set_time_scan(ui.time_scan());
  processor.set_seed_latch_fade(
      settings.FeatureValue(FEATURE_SEED_LATCH_FADE));
  // The page moves the resonator; it restarts from silence.
  const uint8_t placement = ui.resonator();
  if (placement != resonator_placement) {
    const bool was_chord = resonator_placement == RESONATOR_CHORD;
    const bool chord = placement == RESONATOR_CHORD;
#ifndef TEST
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
#endif  // TEST
    resonator_placement = placement;
    processor.set_resonator(resonator_placement);
    if (!synth_mode && chord != was_chord) {
      // Keep the converter running and leave paused callbacks detached.
      codec.ReplaceCallback(
          was_chord ? &ProcessAudio<true> : kLegacyCallback,
          chord ? &ProcessAudio<true> : kLegacyCallback);
    }
#ifndef TEST
    __set_PRIMASK(primask);
#endif  // TEST
  }
  ui.set_delay_active(processor.delay_active());
  if (settings.FeatureValue(FEATURE_PITCH_NOTCH) ||
      settings.FeatureValue(FEATURE_LIVE_ASSIGN)) {
    ui.set_modulation_indication(cv_reader.pitch_on_notch(),
        cv_reader.cv(CV_ADC_CHANNEL_ASSIGN));
  }
  // The wavetable synth adds the PITCH CV to the knob: no recent notes there.
  RememberNote(ui.intervals_mode(), !synth_mode &&
      !normalization_detector.normalized(
          NORMALIZATION_DETECTOR_CHANNEL_PITCH_CV),
      cv_reader.cv(CV_ADC_CHANNEL_PITCH),
      -cv_reader.pot(POTS_ADC_CHANNEL_PITCH_AMOUNT),
      system_clock.milliseconds());
  ui.set_no_input(normalization_detector.no_input());
  ui.set_synth_mode(synth_mode);
  plaits_synth.set_bank(settings.FeatureValue(FEATURE_PLAITS_BANK));
  rings_synth.set_polyphony(
      kRingsPolyphonies[settings.FeatureValue(FEATURE_RINGS_POLYPHONY)]);
  const SynthEngine wanted = EffectiveSynth(ui.synth(), playable_synths);
  ui.set_synth_fault(wanted == SYNTH_SILENT);
  // A settled zone or polyphony change reconfigures Rings off the interrupt,
  // also while FREEZE sustains it.
  if (synth_mode && transition.playing_synth() == SYNTH_RINGS &&
      wanted == transition.playing_synth() &&
      rings_synth.configure_pending() && !transition.blanking() &&
      !ui.calibrating_or_testing()) {
    // FREEZE still drops what the state-change guard below drops.
    if (ui.should_ignore_state_change()) {
      synth_mode_start_time = 0;
      ui.ClearSelfPlayRequest();
    }
    if (transition.Fade()) {
      SwapPaused(SYNTH_SILENT, transition.playing_synth());
    }
    return;
  }
  if (ui.should_ignore_state_change() || transition.blanking()) {
    synth_fade = false;
    synth_mode_start_time = 0;
    ui.ClearSelfPlayRequest();
    transition.Cancel();
    return;
  }

  // Plaits and Rings share bank 0: changing owner fades out, rebuilds the
  // new owner from zero, then fades back in. Beads' synth needs no bank.
  if (wanted != transition.playing_synth()) {
    const SynthEngine owner = wanted == SYNTH_RINGS || wanted == SYNTH_PLAITS
        ? wanted : transition.bank_owner();
    if (synth_mode && owner != transition.bank_owner()) {
      if (!transition.Fade()) {
        return;
      }
      SwapPaused(owner, wanted);
      return;
    }
    transition.set_playing_synth(wanted);
    ui.set_playing_synth(wanted);
  }

  // Once faded out, swap the engine. This clears the recording; a frozen
  // buffer saved to flash returns with the granular engine.
  if (ui.spectral() != processor.spectral() && transition.faded_out()) {
    transition.Pause();
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
      synth_fade = false;
      // Self-play starts and stops only on a MEDIA hold.
      if (ui.self_play_requested()) {
        if (!transition.Fade()) {
          return;
        }
        ui.ClearSelfPlayRequest();
        transition.Pause();
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
      const uint32_t now = system_clock.milliseconds();
      const int start = synth_mode_start_time;
      if (synth_fade || (start && now - start >= 10000)) {
        // Finish the fade even if the callback resets the idle timer.
        synth_fade = true;
        if (!transition.Fade()) {
          return;
        }
        transition.Pause();
        ConfigureAndStartCodec(true, true);
      } else if (!start) {
        synth_mode_start_time = now ? now : 1;
      }
    }
  } else {
    synth_fade = false;
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
      if (!transition.Fade()) {
        return;
      }
      // elapsed = system_clock.milliseconds();
      transition.Pause();
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
  transition.Cancel();
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

