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

#ifndef BEADS_UI_H_
#define BEADS_UI_H_

#include "stmlib/ui/event_queue.h"

#include "beads/gesture_controller.h"
#include "beads/options.h"
#include "beads/pot_controller.h"
#include "beads/profile.h"
#include "beads/drivers/leds.h"
#include "beads/drivers/switches.h"
#include "beads/dsp/plaits_synth.h"

namespace beads {

class BackgroundBufferBackup;
class Calibrator;
class GainControl;
class Settings;

class Ui {
 public:
  Ui() { }
  ~Ui() { }
  
  enum Mode {
    MODE_NORMAL,
    MODE_CALIBRATION_1,
    MODE_CALIBRATION_2,
    MODE_CALIBRATION_3,
    MODE_CALIBRATION_4,
    MODE_DISPLAY_AUTOSAVE,
    MODE_FACTORY_TEST,
    MODE_PANIC,
    MODE_STORAGE_WARNING,
    MODE_REPAIR_CALIBRATION_RESET,
    MODE_REPAIR_FAILED
  };
  
  enum MixParameter {
    MIX_PARAMETER_FEEDBACK,
    MIX_PARAMETER_DRY_WET,
    MIX_PARAMETER_REVERB,
    MIX_PARAMETER_LAST = beads::MIX_PARAMETER_LAST,
  };
  
  void Init(
      Settings* settings,
      GainControl* gain_control,
      Calibrator* calibrator,
      BackgroundBufferBackup* background_buffer_backup);
  void LoadOptions();
  void Poll();
  void DoEvents();
  void FlushEvents();
  void Panic() {
    mode_ = MODE_PANIC;
  }
  // At power-up: damaged storage, or the result of its repair.
  void ShowStorageStatus();
  
  inline bool should_ignore_state_change() const {
    bool frozen = freeze_led_state_;
    bool calibrating = mode_ >= MODE_CALIBRATION_1 && \
        mode_ <= MODE_CALIBRATION_4;
    bool testing = mode_ == MODE_FACTORY_TEST;
    
    return frozen || calibrating || testing;
  }

  // As above, without FREEZE: the Rings synth reconfigures while sustained.
  inline bool calibrating_or_testing() const {
    return (mode_ >= MODE_CALIBRATION_1 && mode_ <= MODE_CALIBRATION_4) || \
        mode_ == MODE_FACTORY_TEST;
  }
  
  inline bool pressed_immediate(Switch s) const {
    return switches_.pressed_immediate(s);
  }
  
  void set_delay_active(bool active);
  void set_modulation_indication(bool notch, float assign) {
    pitch_on_notch_ = notch;
    assign_modulation_ = assign;
  }

  inline bool freeze_led_state() const {
    return freeze_led_state_;
  }
  
  inline bool freeze_button_state() const {
    return freeze_button_state_;
  }
  
  inline bool seed_button_state() const {
    return seed_button_state_;
  }

  inline bool seed_button_locked() const {
    return seed_button_locked_;
  }
  
  inline void set_freeze_button_state(bool freeze_button_state) {
    freeze_button_state_ = freeze_button_state;
    freeze_led_state_ = freeze_button_state;
  }
  
  inline float mix_parameter(MixParameter parameter) {
    return assignable_cv_pot_[parameter].main_parameter();
  }
  
  inline void set_freeze_led_state(bool freeze_led_state) {
    freeze_led_state_ = freeze_led_state;
  }

  inline void set_seed_led_state(bool seed_led_state) {
    seed_led_state_ = seed_led_state;
  }
  
  inline void set_overwrite_reserve(float overwrite_reserve) {
    overwrite_reserve_ = overwrite_reserve;
  }
  
  OutputLimiterMode output_limiter_mode() const;
  inline uint8_t reverb_character() const { return reverb_character_; }
  inline bool clouds_style() const { return reverb_character() == 1; }
  inline bool evolving_feedback() const { return evolving_feedback_; }
  // The selected synth, previewed while its page is open; a SynthEngine.
  inline uint8_t synth() const { return synth_; }
  inline AttackMode attack_mode() const { return attack_mode_; }
  inline float attack_threshold() const { return attack_threshold_; }
  inline bool manual_delay_requested() const {
    return gestures_.manual_delay_requested();
  }
  inline DelayMode delay_mode() const { return delay_mode_; }
  inline StereoWidth stereo_width() const { return stereo_width_; }
  inline PitchScale pitch_scale() const { return pitch_scale_; }
  inline bool random_intervals() const { return random_intervals_; }
  inline RandomIntervals intervals_mode() const { return intervals_mode_; }
  inline uint8_t gain_ceiling() const { return gain_ceiling_; }
  inline uint8_t resonator() const { return resonator_; }
  inline uint8_t resonator_material() const { return resonator_material_; }
  inline uint8_t resonator_ring() const { return resonator_ring_; }
  // The assignable CV moves the resonator's damping; not saved yet.
  inline bool resonator_assigned() const { return resonator_assigned_; }
  inline uint8_t seed_latch() const { return seed_latch_; }
  inline bool time_scan() const { return time_scan_; }

  // The saved engine, applied once the options page has closed.
  inline ProcessorMode processor_mode() const { return processor_mode_; }
  inline bool spectral() const { return processor_mode_ == PROCESSOR_SPECTRAL; }

  void ProcessPot(MixParameter parameter, float value) {
    mix_pot_value_[parameter] = value;
    assignable_cv_pot_[parameter].ProcessControlRate(value);
  }
  
  inline const Switches& switches() const {
    return switches_;
  }
  
  inline void set_factory_test(bool factory_test) {
    mode_ = factory_test ? MODE_FACTORY_TEST : MODE_NORMAL;
  }
  
  inline void set_no_input(bool no_input) { no_input_ = no_input; }
  inline void set_synth_mode(bool synth_mode) { synth_mode_ = synth_mode; }
  // The synths Synth offers, as SynthMask bits.
  inline void set_playable_synths(uint8_t playable) {
    playable_synths_ = playable;
    options_.set_playable_synths(playable);
  }
  // The SynthEngine playing, fallbacks included: [A]+FREEZE follows it.
  inline void set_playing_synth(uint8_t synth) { playing_synth_ = synth; }
  // No playable synth: synth mode is silent.
  inline void set_synth_fault(bool fault) { synth_fault_ = fault; }
  inline bool self_play_requested() const {
    return gestures_.self_play_requested();
  }
  inline void ClearSelfPlayRequest() { gestures_.ClearSelfPlayRequest(); }

  inline bool trim_flag() const {
    return gestures_.trim_flag();
  }
  
  inline void ClearTrimFlag() {
    gestures_.ClearTrimFlag();
  }
  
 private:
  inline bool DoAction(uint8_t slot) { return gestures_.DoAction(slot); }

  friend class GestureController;
  void FlashOptionsPage();

  void OnSwitchPressed(const stmlib::Event& e);
  void OnSwitchReleased(const stmlib::Event& e);

  void UpdateLeds();
  void NextCalibrationStep();

  inline bool chord_modifier(Switch s) const {
    return switches_.pressed(s) && !suppressed_press_[s];
  }

  inline bool showing_storage_status() const {
    return mode_ >= MODE_STORAGE_WARNING;
  }

  // [A] turns a knob into its profile amount; FEEDBACK is the gain by default.
  inline bool media_amount(int i) const {
    return gestures_.media_amount(i);
  }

  stmlib::EventQueue<16> queue_;

  Leds leds_;
  Switches switches_;
  uint32_t press_time_[SWITCH_LAST];
  static const uint32_t long_press_duration[SWITCH_LAST];
  bool ignore_release_[SWITCH_LAST];
  bool suppressed_press_[SWITCH_LAST];
  Mode mode_;
  
  bool no_input_;
  bool synth_mode_;
  uint8_t playable_synths_;
  uint8_t playing_synth_;
  bool synth_fault_;
  bool freeze_button_state_;
  bool freeze_led_state_;
  
  bool seed_button_state_;
  bool seed_button_locked_;
  bool delay_active_;
  bool pitch_on_notch_;
  float assign_modulation_;
  uint32_t delay_cue_time_;
  uint32_t boot_cue_time_;
  bool seed_led_state_;
  
  float overwrite_reserve_;
  
  GainControl* gain_control_;
  Settings* settings_;
  Calibrator* calibrator_;
  BackgroundBufferBackup* background_buffer_backup_;
  
  PotController assignable_cv_pot_[MIX_PARAMETER_LAST];
  float mix_pot_value_[MIX_PARAMETER_LAST];
  Options options_;
  volatile uint32_t options_flash_time_;
  volatile int options_flash_page_;
  // Entry-only request: main discards events queued before options entry.
  volatile bool flush_options_events_;
  volatile OutputLimiterMode output_limiter_mode_;
  volatile uint8_t reverb_character_;
  volatile bool evolving_feedback_;
  volatile uint8_t synth_;
  volatile AttackMode attack_mode_;
  float attack_threshold_;
  volatile ProcessorMode processor_mode_;
  volatile PitchScale pitch_scale_;
  volatile bool random_intervals_;
  volatile RandomIntervals intervals_mode_;
  volatile StereoWidth stereo_width_;
  volatile DelayMode delay_mode_;
  uint32_t storage_status_time_;

  static const Leds::Color media_palette_[4];
  volatile uint8_t resonator_;
  volatile uint8_t resonator_material_;
  volatile uint8_t resonator_ring_;
  volatile bool resonator_assigned_;
  volatile uint8_t seed_latch_;
  volatile bool time_scan_;
  volatile uint8_t gain_ceiling_;

  GestureController gestures_;

  volatile uint8_t processor_mode_;

  DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace beads

#endif  // BEADS_UI_H_
