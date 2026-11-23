// Copyright 2026 Combust.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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
//
// BubbleTime: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_BUBBLETIME_ENGINE_H_
#define BEADS_BLOB_MODELS_BUBBLETIME_ENGINE_H_


#include "plaits/dsp/engine/engine.h"
#include "beads/blob/models/palette_chord_bank.h"

namespace plaits {

class BubbleTime {
 public:
  BubbleTime() { }
  ~BubbleTime() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void RestartPlayback();
  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);

 private:
  static const int kLoopGaps = 64;

  void Reseed();
  void RefillUniforms();
  void RegenerateGaps(int zone, float timbre);
  void StartIncrementalRegen(int zone, float timbre);
  void UpdateLoopNorm();
  float NextGap(int zone, float timbre, float mean_gap_samples);

  // Stored uniform randoms (the realization's identity) and the gap ring
  uint32_t seed_;
  float uniforms_[kLoopGaps];
  float gap_ring_[kLoopGaps];
  int gap_pos_;
  int loop_length_;
  int normed_loop_length_;
  float loop_norm_;      // scales the first L gaps to sum exactly L
  float fray_;           // probability a gap is replaced by an ensemble draw
  uint32_t freerun_state_;

  // Procedural zone state (zones 0-2).
  float sd_accumulator_;
  float rotation_phase_;
  float rotation_countdown_;

  // Event scheduling.
  float event_countdown_;
  float cumulative_position_;    // running sum of gaps, mod m (ring 1)
  float cumulative_position_b_;  // the same sum, mod m' (ring 2, booleans)
  float samples_since_event_;
  float last_lattice_u_;

  // The gap melody's chord tones come from the firmware PaletteChordBank; the
  PaletteChordBank chords_;

  // Ping resonators (complex one-pole): A = gap melody, B = base tone.
  float a_re_, a_im_, a_c_, a_s_;
  float b_re_, b_im_, b_c_, b_s_;
  float a_right_re_, a_right_im_;
  float b_right_re_, b_right_im_;
  float resonator_r_;
  // Strike excitation ramp: injection spread over ~1 ms so OUT carries a
  int excite_remaining_;
  float excite_amp_;
  float excite_left_;
  float excite_right_;

  // Gates.
  int gate_remaining_;
  float gate_level_;
  float dc_aux_;

  // Hysteresis, regen, reseed-gesture state.
  int zone_;
  int modulus_index_;
  int base_interval_;
  int cached_zone_;
  float cached_timbre_;
  bool ring_valid_;
  float regen_k_;
  int regen_pending_;
  int regen_write_;
  int hold_blocks_;
  bool reseed_armed_;

  DISALLOW_COPY_AND_ASSIGN(BubbleTime);
};

class BubbleTimeEngine : public Engine {
 public:
  BubbleTimeEngine() { }
  ~BubbleTimeEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    bubbletime_.Init(allocator);
  }

  virtual void Reset() {
    bubbletime_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    bubbletime_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  BubbleTime bubbletime_;

  DISALLOW_COPY_AND_ASSIGN(BubbleTimeEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_BUBBLETIME_ENGINE_H_
