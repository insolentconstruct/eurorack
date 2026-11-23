// Copyright 2012 Emilie Gillet.
// Copyright 2018 Tom Burns.
// Copyright 2026 Lyle Mills.
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
// WavetableChord: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_WAVETABLE_CHORD_ENGINE_H_
#define BEADS_BLOB_MODELS_WAVETABLE_CHORD_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "beads/blob/libraries/scale_voices_library.h"
#include "beads/blob/libraries/wavetable_scale_voices_library.h"

#ifndef PLAITS_BLOB
#include "plaits/resources.h"
#endif

namespace plaits {

class WavetableChord {
 public:
  WavetableChord() { }
  ~WavetableChord() { }

  void Init(
      const beads::PlaitsBlobLibrary* scales,
      const beads::PlaitsBlobLibrary* wavetable,
      stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  const beads::PlaitsBlobLibrary* scales_;
  const beads::PlaitsBlobLibrary* wavetable_;
  int num_scales_;
  ScaleVoiceBank voices_;

  DISALLOW_COPY_AND_ASSIGN(WavetableChord);
};

#ifndef PLAITS_BLOB
class WavetableChordEngine : public Engine {
 public:
  WavetableChordEngine() { }
  ~WavetableChordEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    scales_.exports[beads::SCALE_VOICES_NUM_SCALES] = &kScaleVoicesNumScales;
    wavetable_.context = const_cast<int16_t*>(wav_integrated_waves);
    core_.Init(&scales_, &wavetable_, allocator);
  }

  virtual void Reset() {
    core_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    core_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  beads::PlaitsBlobLibrary scales_;
  beads::PlaitsBlobLibrary wavetable_;
  WavetableChord core_;

  DISALLOW_COPY_AND_ASSIGN(WavetableChordEngine);
};
#endif

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_WAVETABLE_CHORD_ENGINE_H_
