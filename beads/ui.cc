// Copyright 2018 Emilie Gillet.
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
//
// -----------------------------------------------------------------------------
//
// User interface

#include "beads/ui.h"

#include <algorithm>

#include "stmlib/system/system_clock.h"

#include "beads/background_buffer_backup.h"
#include "beads/blob/synth_choice.h"
#include "beads/calibrator.h"
#include "beads/dsp/fx/reverb.h"
#include "beads/gain_control.h"
#include "beads/settings.h"

namespace beads {

const uint8_t kReleaseBeta = 3;

using namespace std;
using namespace stmlib;

// Keep the width meter below set_meter's wrap at 1.0.
const float kWidthMeterSpan = 0.749f;
const uint32_t kStorageStatusTime = 3000;
const uint32_t kGestureCueTime = 500;

// Marbles' 4:1 slow/fast blink ratio.
static bool PulseLit(PulsePattern pulses, uint32_t elapsed) {
  const uint32_t phase = elapsed % kOptionsPulsePeriod;
  return pulses == PULSE_STEADY ||
      (pulses == PULSE_SLOW &&
          (elapsed % kOptionsSlowPeriod) < kOptionsSlowPeriod / 2) ||
      (pulses == PULSE_FAST &&
          (elapsed % kOptionsFastPeriod) < kOptionsFastPeriod / 2) ||
      (pulses == PULSE_DOUBLE && (phase < kOptionsFastPeriod / 2 ||
          (phase >= kOptionsFastPeriod &&
          phase < kOptionsFastPeriod * 3 / 2)));
}

// The [A] hold acts on release, so a knob or button can still follow it.
/* static */
const uint32_t Ui::long_press_duration[SWITCH_LAST] = {
  4000, 4000, 0x7fffffff, 0x7fffffff
};

void Ui::FlashOptionsPage() {
  options_flash_time_ = system_clock.milliseconds();
  options_flash_page_ = options_.page();
}

OutputLimiterMode Ui::output_limiter_mode() const {
  return output_limiter_mode_;
}

void Ui::Init(
    Settings* settings,
    GainControl* gain_control,
    Calibrator* calibrator,
    BackgroundBufferBackup* background_buffer_backup) {
  leds_.Init();
  switches_.Init();
  
  system_clock.Init();
  
  settings_ = settings;
  gain_control_ = gain_control;
  calibrator_ = calibrator;
  background_buffer_backup_ = background_buffer_backup;
  
  mode_ = MODE_NORMAL;
  storage_status_time_ = 0;
  options_.Init();
  flush_options_events_ = false;
  options_flash_time_ = 0;
  options_flash_page_ = 0;
  fill(&mix_pot_value_[0], &mix_pot_value_[MIX_PARAMETER_LAST], 0.0f);
  
  queue_.Init();
  
  fill(&press_time_[0], &press_time_[SWITCH_LAST], 0);
  fill(&ignore_release_[0], &ignore_release_[SWITCH_LAST], false);
  fill(&suppressed_press_[0], &suppressed_press_[SWITCH_LAST], false);
  
  freeze_button_state_ = false;
  freeze_led_state_ = false;
  delay_active_ = false;
  pitch_on_notch_ = false;
  assign_modulation_ = 0.0f;
  delay_cue_time_ = 0;
  boot_cue_time_ = system_clock.milliseconds();
  
  seed_button_state_ = false;
  seed_button_locked_ = true;
  seed_led_state_ = false;
  resonator_assigned_ = false;
  resonator_available_ = false;
  gestures_.Init(this, settings);
  no_input_ = false;
  synth_mode_ = false;
  playable_synths_ = SynthMask(true, true, false);
  playing_synth_ = SYNTH_BEADS;
  synth_fault_ = false;
  
  overwrite_reserve_ = 1.0f;
  
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    assignable_cv_pot_[i].Init(1.0f, 0.0f);
  }
}

void Ui::ShowStorageStatus() {
  switch (settings_->repair()) {
    case STORAGE_REPAIR_CALIBRATION_RESET:
      mode_ = MODE_REPAIR_CALIBRATION_RESET;
      break;
    case STORAGE_REPAIR_FAILED:
      mode_ = MODE_REPAIR_FAILED;
      break;
    default:
      if (!settings_->storage_damaged()) {
        return;
      }
      mode_ = MODE_STORAGE_WARNING;
      break;
  }
  storage_status_time_ = system_clock.milliseconds();
}

void Ui::LoadOptions() {
  options_.Configure(settings_->profile());
  gestures_.LoadOptions();
  output_limiter_mode_ = settings_->output_limiter_mode();
  reverb_character_ = settings_->reverb_character();
  clouds_style_ = reverb_character_ == REVERB_CLOUDS;
  evolving_feedback_ = settings_->evolving_feedback();
  synth_ = settings_->synth();
  attack_mode_ = settings_->attack_mode();
  attack_threshold_ = kAttackThresholds[
      settings_->FeatureValue(FEATURE_ATTACK_SENSITIVITY)];
  processor_mode_ = settings_->processor_mode();
  spectral_ = processor_mode_ == PROCESSOR_SPECTRAL;
  pitch_scale_ = settings_->pitch_scale();
  intervals_mode_ = settings_->random_intervals();
  random_intervals_ = intervals_mode_ != RANDOM_INTERVALS_OFF;
  stereo_width_ = settings_->stereo_width();
  delay_mode_ = settings_->delay_mode();
  gain_ceiling_ = settings_->FeatureValue(FEATURE_GAIN_CEILING);
  resonator_ = settings_->FeatureValue(FEATURE_RESONATOR);
  resonator_material_ = settings_->FeatureValue(FEATURE_RESONATOR_MATERIAL);
  resonator_ring_ = settings_->FeatureValue(FEATURE_RESONATOR_RING);
  seed_latch_ = settings_->FeatureValue(FEATURE_SEED_LATCH);
  time_scan_ = settings_->FeatureValue(FEATURE_TIME_SCAN);
  // SEED starts latched, or unlatched when the profile asks.
  seed_button_locked_ = !settings_->FeatureValue(FEATURE_SEED_START);
}

void Ui::Poll() {
  system_clock.Tick();
  switches_.Debounce();
  
  const Options::Stage previous_stage = options_.stage();
  const bool was_active = options_.active();
  const bool storage_status = showing_storage_status();
  bool other = false;
  bool all_up = true;
  bool pot_touched = false;
  for (int i = 0; i < SWITCH_LAST; ++i) {
    const Switch s = Switch(i);
    const bool down = switches_.pressed_immediate(s);
    if (storage_status && down) {
      suppressed_press_[i] = true;
    } else if (!down && switches_.up(s)) {
      suppressed_press_[i] = false;
    }
    const bool scale_seed = s == SWITCH_SEED && options_.seed_allowed() &&
        !switches_.pressed_immediate(SWITCH_ASSIGN);
    other = other || (s != SWITCH_ASSIGN && !scale_seed && down);
    all_up = all_up && !down && switches_.up(s);
  }
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    assignable_cv_pot_[i].ProcessUIRate();
    pot_touched = pot_touched ||
        assignable_cv_pot_[i].editing_hidden_parameter();
  }
  options_.Poll(
      system_clock.milliseconds(), chord_modifier(SWITCH_ASSIGN) &&
      switches_.pressed_immediate(SWITCH_ASSIGN),
      other, all_up, switches_.just_pressed(SWITCH_ASSIGN) &&
      !suppressed_press_[SWITCH_ASSIGN],
      mode_ == MODE_NORMAL, pot_touched, mix_pot_value_,
      !switches_.up(SWITCH_SEED) && !switches_.released(SWITCH_SEED),
      switches_.just_pressed(SWITCH_SEED), *settings_);
  if (!was_active && options_.active()) {
    FlashOptionsPage();
    flush_options_events_ = true;
    ignore_release_[SWITCH_ASSIGN] = true;
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Lock();
    }
  }
  if (options_.stage() == Options::STAGE_DRAIN &&
      previous_stage != Options::STAGE_DRAIN) {
    FlashOptionsPage();
    for (int i = 0; i < SWITCH_LAST; ++i) {
      if (switches_.pressed_immediate(Switch(i))) {
        ignore_release_[i] = true;
      }
    }
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Unlock();
    }
  }

  // Ignore switch events while options are open or draining, and buttons
  // held through the storage status shown at power-up.
  const bool accept_switch_events = !was_active && !options_.active() &&
      options_.stage() != Options::STAGE_DRAIN && !storage_status;
  for (int i = 0; i < SWITCH_LAST; ++i) {
    Switch s = Switch(i);

    if (switches_.just_pressed(s)) {
      ignore_release_[i] = !accept_switch_events || suppressed_press_[i];
      if (!ignore_release_[i]) {
        queue_.AddEvent(CONTROL_SWITCH, i, 0);
      }
      press_time_[i] = system_clock.milliseconds();
    }

    if (switches_.pressed(s) && !ignore_release_[i]) {
      uint32_t pressed_time = system_clock.milliseconds() - press_time_[i];
      if (pressed_time >= long_press_duration[i]) {
        if (accept_switch_events) {
          queue_.AddEvent(CONTROL_SWITCH, i, pressed_time);
        }
        ignore_release_[i] = true;
      }
    }

    if (switches_.released(s) && !ignore_release_[i]) {
      if (accept_switch_events) {
        queue_.AddEvent(
            CONTROL_SWITCH,
            i,
            system_clock.milliseconds() - press_time_[i] + 1);
      }
      ignore_release_[i] = true;
    }
  }

  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    PotController* p = &assignable_cv_pot_[i];
    if (!was_active && !options_.active() && p->editing_hidden_parameter()) {
      State* state = settings_->mutable_state();
      if (chord_modifier(SWITCH_MEDIA)) {
        gestures_.ProcessAmount(i, p,
            &press_time_[SWITCH_MEDIA]);
      } else if (chord_modifier(SWITCH_ASSIGN)) {
        settings_->set_width_assigned(false);
        resonator_assigned_ = false;
        state->assignable_cv_destination = 3;
        state->assignable_cv_amount[i] = p->hidden_parameter() * 256.0f;
      }
    }
  }
  
  const bool previewing = options_.stage() != Options::STAGE_INACTIVE;
  reverb_character_ = previewing
      ? options_.Value(FEATURE_REVERB) : settings_->reverb_character();
  if (reverb_character_ <= REVERB_CLOUDS) {
    clouds_style_ = reverb_character_ == REVERB_CLOUDS;
  }
  synth_ = previewing ? options_.Value(FEATURE_SYNTH) : settings_->synth();
  delay_mode_ = previewing
      ? DelayMode(options_.Value(FEATURE_DELAY)) : settings_->delay_mode();
  evolving_feedback_ = previewing
      ? options_.Value(FEATURE_FROZEN) : settings_->evolving_feedback();
  attack_mode_ = previewing
      ? AttackMode(options_.Value(FEATURE_ATTACKS)) : settings_->attack_mode();
  stereo_width_ = settings_->stereo_width();
  pitch_scale_ = previewing
      ? PitchScale(options_.Value(FEATURE_PITCH)) : settings_->pitch_scale();
  intervals_mode_ = previewing
      ? RandomIntervals(options_.Value(FEATURE_INTERVALS))
      : settings_->random_intervals();
  random_intervals_ = intervals_mode_ != RANDOM_INTERVALS_OFF;
  output_limiter_mode_ = previewing
      ? OutputLimiterMode(options_.Value(FEATURE_LIMITER))
      : settings_->output_limiter_mode();
  gain_ceiling_ = previewing
      ? options_.Value(FEATURE_GAIN_CEILING)
      : settings_->FeatureValue(FEATURE_GAIN_CEILING);
  resonator_ = previewing
      ? options_.Value(FEATURE_RESONATOR)
      : settings_->FeatureValue(FEATURE_RESONATOR);
  resonator_material_ = previewing
      ? options_.Value(FEATURE_RESONATOR_MATERIAL)
      : settings_->FeatureValue(FEATURE_RESONATOR_MATERIAL);
  resonator_ring_ = previewing
      ? options_.Value(FEATURE_RESONATOR_RING)
      : settings_->FeatureValue(FEATURE_RESONATOR_RING);
  seed_latch_ = previewing
      ? options_.Value(FEATURE_SEED_LATCH)
      : settings_->FeatureValue(FEATURE_SEED_LATCH);
  time_scan_ = previewing
      ? options_.Value(FEATURE_TIME_SCAN)
      : settings_->FeatureValue(FEATURE_TIME_SCAN);
  // Where the resonator cannot run, the CV goes back to FEEDBACK.
  if (!resonator_available()) {
    resonator_assigned_ = false;
  }
  if (!previewing) {
    processor_mode_ = settings_->processor_mode();
    spectral_ = processor_mode_ == PROCESSOR_SPECTRAL;
  }
  UpdateLeds();
}

/* static */
const Leds::Color Ui::media_palette_[4] = {
  { 128, 0, 48 },
  { 255, 144, 0 },
  { 0, 255, 192 },
  { 192, 192, 208 },
};

void Ui::UpdateLeds() {
  leds_.Clear();
  
  int triangle = (system_clock.milliseconds() & 2047) >> 2;
  triangle = triangle >= 256 ? 511 - triangle : triangle;

  const bool led_features = settings_->led_features();
  bool adjusting_manual_gain = false;
  bool adjusting_width = false;
  bool adjusting_diffusion = false;
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    if (chord_modifier(SWITCH_MEDIA) &&
        assignable_cv_pot_[i].editing_hidden_parameter()) {
      adjusting_manual_gain |= gestures_.amount_feature(i) == FEATURE_GAIN_KNOB;
      adjusting_width |= gestures_.amount_feature(i) == FEATURE_WIDTH;
      adjusting_diffusion |= gestures_.amount_feature(i) == FEATURE_DIFFUSION;
    }
  }
  
  switch (mode_) {
    case MODE_PANIC:
    case MODE_STORAGE_WARNING:
      {
        bool blink = system_clock.milliseconds() & 64;
        for (int i = 0; i < LED_LAST; ++i) {
          leds_.set(Led(i), blink ? 255 : 0);
        }
      }
      break;
      
    case MODE_CALIBRATION_1:
    case MODE_CALIBRATION_2:
    case MODE_CALIBRATION_3:
    case MODE_CALIBRATION_4:
      {
        int channel = (int(mode_) - MODE_CALIBRATION_1) >> 1;
        int step = (int(mode_) - MODE_CALIBRATION_1) & 1;
        leds_.set(channel == 0 ? LED_FREEZE : LED_SEED, 255);
        leds_.set(LED_MEDIA_R, step == 1 ? 255 : 0);
        leds_.set(LED_MEDIA_G, step == 1 ? 255 : 255);
        leds_.set(LED_MEDIA_B, 0);
        leds_.set(LED_ASSIGN_1, triangle >> 2);
        leds_.set(LED_ASSIGN_2, triangle >> 2);
        leds_.set(LED_ASSIGN_3, triangle >> 2);
      }
      break;
    case MODE_DISPLAY_AUTOSAVE:
    case MODE_NORMAL:
      {
        const State& state = settings_->state();
        
        if (adjusting_manual_gain) {
          const bool unity = settings_->FeatureValue(FEATURE_ATTENUATION) &&
              state.gain >= 120 && state.gain <= 136;
          leds_.set_meter(unity
              ? 0.25f : 0.05f + 0.95f * float(state.gain) / 256.0f);
        } else if (adjusting_width) {
          // Avoid set_meter's wrap at 1.0; its red endpoint is 255/0/0.
          const StereoWidth width = settings_->stereo_width();
          leds_.set_meter(width
              ? 0.25f + kWidthMeterSpan * float(width - 1)
                  / float(kStereoWidthMax - 1)
              : 0.05f);
        } else if (adjusting_diffusion) {
          leds_.set_meter(0.05f + 0.94f * settings_->diffusion());
        } else {
          Leds::Color media_color = media_palette_[state.media];
          const float brightness = overwrite_reserve_ * \
              (state.monophonic_mode ? 0.5f : 1.0f);
          leds_.set_media(media_color, brightness);
          if (led_features && settings_->FeatureValue(FEATURE_PITCH_NOTCH) &&
              pitch_on_notch_) {
            const Leds::Color cue = { 0, 0, 255 };
            leds_.set_media(cue);
          }
          if (led_features && settings_->FeatureValue(FEATURE_DELAY_CUE) &&
              delay_cue_time_ &&
              uint32_t(system_clock.milliseconds() - delay_cue_time_) < 150) {
            const Leds::Color cue = { 255, 0, 0 };
            leds_.set_media(cue);
          }
          if (led_features && settings_->FeatureValue(FEATURE_VERSION) &&
              uint32_t(system_clock.milliseconds() - boot_cue_time_) <
                  200u + 300u * kReleaseBeta) {
            // Light blue identifies Drizzlet; pulse count identifies Beta.
            const uint32_t elapsed =
                system_clock.milliseconds() - boot_cue_time_;
            const bool lit = elapsed < 100 || (elapsed >= 200 &&
                ((elapsed - 200) / 150) % 2 == 0);
            const Leds::Color cue = {
              0, uint8_t(lit ? 211 : 0), uint8_t(lit ? 255 : 0)
            };
            leds_.set_media(cue);
          }
          const uint32_t cue_elapsed =
              system_clock.milliseconds() - gestures_.cue_time();
          if (gestures_.cue_time() && cue_elapsed < kGestureCueTime) {
            leds_.set_media(
                gestures_.cue().color,
                PulseLit(gestures_.cue().pulses, cue_elapsed) ? 1.0f : 0.0f);
          }
        }
        
        if (mode_ == MODE_DISPLAY_AUTOSAVE) {
          leds_.set(LED_FREEZE, state.disable_autosave ? 16 : 64);
        } else if (freeze_led_state_) {
          const bool saving = state.disable_autosave
              ? background_buffer_backup_->busy()
              : background_buffer_backup_->saving();
          leds_.set(LED_FREEZE,
              saving && !settings_->FeatureValue(FEATURE_SAVE_FREEZE)
                  ? 128 : 255);
        } else {
          leds_.set(LED_FREEZE, 0);
        }
        
        if (seed_button_locked_) {
          leds_.set(LED_SEED, (triangle * 3 >> 2) + 64);
        } else {
          leds_.set(
              LED_SEED,
              (seed_button_state_ || seed_led_state_) ? 255 : 0);
        }
        
        if (resonator_assigned_) {
          // Page 5's LEDs, the resonator page.
          leds_.set(Led(LED_ASSIGN_1 + MIX_PARAMETER_FEEDBACK), triangle);
          leds_.set(Led(LED_ASSIGN_1 + MIX_PARAMETER_REVERB), triangle);
        } else if (settings_->width_assigned()) {
          for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
            leds_.set(Led(LED_ASSIGN_1 + i), triangle);
          }
        } else if (settings_->diffusion_assigned()) {
          leds_.set(Led(LED_ASSIGN_1 + MIX_PARAMETER_REVERB), triangle);
        } else if (state.assignable_cv_destination == 3) {
          for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
            leds_.set(
                Led(LED_ASSIGN_1 + i),
                state.assignable_cv_amount[i] >> 1);
          }
        } else {
          leds_.set(Led(LED_ASSIGN_1 + state.assignable_cv_destination), 128);
        }
        if (led_features && settings_->FeatureValue(FEATURE_LIVE_ASSIGN)) {
          float modulation = fabsf(assign_modulation_);
          CONSTRAIN(modulation, 0.0f, 1.0f);
          leds_.ScaleAssignments(32 + uint8_t(modulation * 223.0f));
        }
      }
      break;
      
    case MODE_FACTORY_TEST:
      for (int i = 0; i < LED_LAST; ++i) {
        leds_.set(Led(i), triangle);
      }
      break;

    case MODE_REPAIR_CALIBRATION_RESET:
    case MODE_REPAIR_FAILED:
      {
        // As the recovery firmware shows its result.
        const uint32_t elapsed = system_clock.milliseconds() - \
            storage_status_time_;
        const bool blink = (elapsed % 1000) < 500;
        const bool fast_blink = (elapsed % 250) < 125;
        Leds::Color color;
        color.r = 0;
        color.g = 0;
        color.b = 0;
        if (mode_ == MODE_REPAIR_FAILED) {
          color.r = fast_blink ? 255 : 0;
          for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
            leds_.set(Led(LED_ASSIGN_1 + i), fast_blink ? 255 : 0);
          }
        } else {
          color.g = blink ? 255 : 0;
          for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
            leds_.set(Led(LED_ASSIGN_1 + i), 255);
          }
        }
        leds_.set_media(color);
      }
      break;
  }
  
  // No synth can play until a good install: [A] blinks fast, as the
  // recovery firmware shows a failure.
  if (synth_fault_ && synth_mode_ && mode_ == MODE_NORMAL) {
    const bool fast_blink = (system_clock.milliseconds() % 250) < 125;
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      leds_.set(Led(LED_ASSIGN_1 + i), fast_blink ? 255 : 0);
    }
  }

  if (options_.stage() == Options::STAGE_WAIT_RELEASE ||
      options_.stage() == Options::STAGE_EDITING) {
    const uint8_t page_leds = kOptionPages[options_.page()].leds;
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      // While a page opens, the knob whose value [A] shows is lit steady.
      const bool page = options_.stage() == Options::STAGE_WAIT_RELEASE ||
          (page_leds & (1 << i));
      leds_.set(Led(LED_ASSIGN_1 + i), i == options_.preview_knob() ? 255
          : page ? triangle : 0);
    }
    const OptionValue& indication = options_.indication();
    const Leds::Color color = indication.color;
    const uint32_t now = system_clock.milliseconds() -
        options_.indication_time();
    leds_.set_media(color, PulseLit(indication.pulses, now) ? 1.0f : 0.0f);
  }

  if (options_flash_time_ &&
      uint32_t(system_clock.milliseconds() - options_flash_time_) <
          kOptionsFlashTime) {
    const uint8_t page_leds = kOptionPages[options_flash_page_].leds;
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      if (page_leds & (1 << i)) {
        leds_.set(Led(LED_ASSIGN_1 + i), 255);
      }
    }
  }

  if (mode_ != MODE_FACTORY_TEST && !showing_storage_status()) {
    if (gain_control_->adjusting()) {
      leds_.set(LED_VU_METER, (system_clock.milliseconds() & 128) ? 128 : 0);
    } else {
      leds_.set(LED_VU_METER, gain_control_->vu_meter());
    }
  }
  // The chosen level holds until the page's batch is saved.
  const bool previewing = options_.stage() != Options::STAGE_INACTIVE;
  if (led_features || previewing) {
    const uint8_t level = previewing
        ? options_.Value(FEATURE_BRIGHTNESS)
        : settings_->FeatureValue(FEATURE_BRIGHTNESS);
    leds_.Scale(level);
  }
  leds_.Write();
}

void Ui::FlushEvents() {
  queue_.Flush();
}

void Ui::OnSwitchPressed(const Event& e) {
  // Combo
  //
  // SEED + FREEZE = seed lock.
  //
  if (chord_modifier(SWITCH_SEED) && e.control_id == SWITCH_FREEZE) {
    seed_button_locked_ = true;
    seed_button_state_ = false;
    ignore_release_[SWITCH_SEED] = ignore_release_[SWITCH_FREEZE] = true;
    return;
  }
  
  // Combo
  //
  // Assign + SEED = toggle gate output mode.
  //
  if (chord_modifier(SWITCH_ASSIGN) && e.control_id == SWITCH_SEED) {
    State* state = settings_->mutable_state();
    state->gate_output_mode = !state->gate_output_mode;
    settings_->SaveState();
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Unlock();
    }
    ignore_release_[SWITCH_ASSIGN] = ignore_release_[SWITCH_SEED] = true;
    return;
  }

  // Combo
  //
  // Assign + FREEZE = toggle autosave.
  //
  if (chord_modifier(SWITCH_ASSIGN) && e.control_id == SWITCH_FREEZE) {
    State* state = settings_->mutable_state();
    state->disable_autosave = !state->disable_autosave;
    settings_->SaveState();
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Unlock();
    }
    if (mode_ == MODE_CALIBRATION_1) {
      calibrator_->ResumeNormalization();
    }
    mode_ = MODE_DISPLAY_AUTOSAVE;
    ignore_release_[SWITCH_ASSIGN] = ignore_release_[SWITCH_FREEZE] = true;
    return;
  }

  if (chord_modifier(SWITCH_MEDIA) && gestures_.OnPressed(e, switches_)) {
    ignore_release_[SWITCH_MEDIA] = ignore_release_[e.control_id] = true;
    return;
  }

  // Combo
  //
  // Assign + Media = calibration mode.
  //
  if ((e.control_id == SWITCH_ASSIGN && chord_modifier(SWITCH_MEDIA)) || \
      (e.control_id == SWITCH_MEDIA && chord_modifier(SWITCH_ASSIGN))) {
    mode_ = MODE_CALIBRATION_1;
    calibrator_->Start();
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      assignable_cv_pot_[i].Unlock();
    }
    ignore_release_[SWITCH_MEDIA] = ignore_release_[SWITCH_ASSIGN] = true;
    return;
  }
  
  switch (e.control_id) {
    case SWITCH_FREEZE:
      freeze_button_state_ = !freeze_button_state_;
      break;

    case SWITCH_SEED:
      if (seed_button_locked_) {
        seed_button_locked_ = false;
      }
      seed_button_state_ = true;
      break;
      
    case SWITCH_MEDIA:
      for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
        if (media_amount(i)) {
          assignable_cv_pot_[i].Lock();
        }
      }
      break;
      
    case SWITCH_ASSIGN:
      for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
        assignable_cv_pot_[i].Lock();
      }
      break;
    
    default:
      break;
  }
}

void Ui::NextCalibrationStep() {
  switch (mode_) {
    case MODE_CALIBRATION_1:
      calibrator_->CalibrateOffsets();
      calibrator_->Calibrate(CV_ADC_CHANNEL_PITCH, 0, 12.0f);
      mode_ = MODE_CALIBRATION_2;
      break;

    case MODE_CALIBRATION_2:
      mode_ = calibrator_->Calibrate(CV_ADC_CHANNEL_PITCH, 1, 36.0f)
          ? MODE_CALIBRATION_3
          : MODE_PANIC;
      break;

    case MODE_CALIBRATION_3:
      calibrator_->Calibrate(CV_ADC_CHANNEL_DENSITY, 0, 0.2f);
      mode_ = MODE_CALIBRATION_4;
      break;
    
    case MODE_CALIBRATION_4:
      mode_ = calibrator_->Calibrate(CV_ADC_CHANNEL_DENSITY, 1, 0.6f)
          ? MODE_NORMAL
          : MODE_PANIC;
      // A calibration refused by the settings: the same rejection blink.
      if (mode_ == MODE_NORMAL && !calibrator_->Save()) {
        mode_ = MODE_PANIC;
        queue_.Touch();  // A full second of it after the save's erase.
      }
      break;
      
    default:
      break;
  }
}

void Ui::OnSwitchReleased(const Event& e) {
  switch (e.control_id) {
    case SWITCH_SEED:
      {
        if (uint32_t(e.data) >= long_press_duration[SWITCH_SEED]) {
          seed_button_locked_ = true;
        }
        seed_button_state_ = false;
      }
      break;

    case SWITCH_MEDIA:
      {
        if (!assignable_cv_pot_[MIX_PARAMETER_FEEDBACK].
                editing_hidden_parameter() &&
            !assignable_cv_pot_[MIX_PARAMETER_DRY_WET].
                editing_hidden_parameter() &&
            !assignable_cv_pot_[MIX_PARAMETER_REVERB].
                editing_hidden_parameter()) {
          if (mode_ >= MODE_CALIBRATION_1 && mode_ <= MODE_CALIBRATION_4) {
            NextCalibrationStep();
          } else {
            if (!gestures_.OnReleased(e)) {
              State* state = settings_->mutable_state();
              state->media = (state->media + 3) % 4;
              settings_->SaveState();
            }
          }
        } else {
          // Save the manual gain, width or diffusion settings.
          settings_->SaveState();
        }
        for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
          if (media_amount(i)) {
            assignable_cv_pot_[i].Unlock();
          }
        }
      }
      break;

    case SWITCH_ASSIGN:
      {
        bool pot_touched = false;
        for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
          pot_touched = pot_touched || \
              assignable_cv_pot_[i].editing_hidden_parameter();
          assignable_cv_pot_[i].Unlock();
        }
        if (!pot_touched && !options_.any_page() &&
            uint32_t(e.data) >= kOptionsHoldTime) {
          break;
        }
        if (!pot_touched) {
          gestures_.StepAssignment();
        }
        settings_->SaveState();
      }
      break;

    default:
      break;
  }
}

void Ui::set_delay_active(bool active) {
  if (active != delay_active_) {
    delay_active_ = active;
    delay_cue_time_ = system_clock.milliseconds();
  }
}

void Ui::DoEvents() {
  if (options_.commit_requested()) {
    bool changed = false;
    for (int i = 0; i < kNumPageFeatures; ++i) {
      const ProfileFeature f = kPageFeatures[i];
      // Pages commit their settings; a gesture saves the Plaits bank.
      if (options_.edited(f) && settings_->profile().adjustable(f) &&
          options_.Value(f) != settings_->FeatureValue(f)) {
        settings_->SetFeatureValue(f, options_.Value(f));
        changed = true;
      }
    }
    if (changed) {
      settings_->SaveOptions();
      FlashOptionsPage();
    }
    options_.ClearCommit();
  }
  // Clear before Flush so a request arriving during Flush stays pending.
  while (true) {
    if (flush_options_events_) {
      flush_options_events_ = false;
      queue_.Flush();
    }
    if (!queue_.available()) {
      break;
    }
    Event e = queue_.PullEvent();
    // Drop events queued before options entry.
    if (!options_.active() && !flush_options_events_ &&
        e.control_type == CONTROL_SWITCH) {
      if (e.data == 0) {
        OnSwitchPressed(e);
      } else {
        OnSwitchReleased(e);
      }
    }
  }
  
  if (queue_.idle_time() > 1000 && (mode_ == MODE_PANIC || mode_ == MODE_DISPLAY_AUTOSAVE)) {
    mode_ = MODE_NORMAL;
  }

  if (showing_storage_status() &&
      system_clock.milliseconds() - storage_status_time_ >=
          kStorageStatusTime) {
    mode_ = MODE_NORMAL;
  }

  if (queue_.idle_time() > 3000) {
    queue_.Touch();
  }
}

}  // namespace beads
