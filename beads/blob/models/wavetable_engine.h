// Copyright 2016 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
//
// -----------------------------------------------------------------------------
//
// 8x8x3 wave terrain, as a plain class for a blob: Plaits' WavetableEngine
// (wavetable_engine.cc), its code unchanged, reading the waves from the
// Plaits wavetables library instead of the app.

#ifndef BEADS_BLOB_MODELS_WAVETABLE_ENGINE_H_
#define BEADS_BLOB_MODELS_WAVETABLE_ENGINE_H_

#include "stmlib/utils/buffer_allocator.h"

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/wavetable_oscillator.h"

namespace plaits {

class Wavetable {
 public:
  Wavetable() { }
  ~Wavetable() { }

  // waves is the library's copy of wav_integrated_waves.
  void Init(const int16_t* waves, stmlib::BufferAllocator* allocator);
  void Reset();
  void LoadUserData(const uint8_t* user_data);
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float ReadWave(int x, int y, int z, int phase_i, float phase_f);

  const int16_t* waves_;

  float phase_;

  float x_pre_lp_;
  float y_pre_lp_;
  float z_pre_lp_;

  float x_lp_;
  float y_lp_;
  float z_lp_;

  float previous_x_;
  float previous_y_;
  float previous_z_;
  float previous_f0_;

  // Maps a (bank, X, Y) coordinate to a waveform index.
  const int16_t** wave_map_;

  Differentiator diff_out_;

  DISALLOW_COPY_AND_ASSIGN(Wavetable);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_WAVETABLE_ENGINE_H_
