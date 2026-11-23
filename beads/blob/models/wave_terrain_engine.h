// Copyright 2021 Emilie Gillet.
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
// -----------------------------------------------------------------------------
//
// Plaits' WaveTerrainEngine as a plain class for a blob, its code unchanged
// except terrains 5-7's waves, and 5-8's coordinates clamped to their table.

#ifndef BEADS_BLOB_MODELS_WAVE_TERRAIN_ENGINE_H_
#define BEADS_BLOB_MODELS_WAVE_TERRAIN_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/engine2/wave_terrain_engine.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

class WaveTerrain {
 public:
  WaveTerrain() { }
  ~WaveTerrain() { }

  // waves is the library's copy of wav_integrated_waves.
  void Init(const int16_t* waves, stmlib::BufferAllocator* allocator);
  void Reset();
  // TERR's grids, played after the eight stock terrains; none when 0.
  void LoadUserData(const int8_t* grids, size_t num_grids) {
    grids_ = grids;
    num_grids_ = num_grids;
  }
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  const int16_t* waves_;

  float Terrain(float x, float y, int terrain_index);

  FastSineOscillator path_;
  float offset_;
  float terrain_;

  float* temp_buffer_;
  const int8_t* grids_;
  size_t num_grids_;

  DISALLOW_COPY_AND_ASSIGN(WaveTerrain);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_WAVE_TERRAIN_ENGINE_H_
