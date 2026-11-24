// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
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
// A checked blob as a Plaits engine: its state lives in the voice's RAM.

#ifndef BEADS_BLOB_PLAITS_BLOB_ENGINE_H_
#define BEADS_BLOB_PLAITS_BLOB_ENGINE_H_

#include <cstring>

#include "stmlib/stmlib.h"
#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "beads/blob/plaits_libraries.h"

namespace beads {

// Fade the first two blocks of OUT after a switch to mask the transient from
// rebuilding blob state (see ABI.md).
const size_t kPlaitsBlobFadeSize = 48;
const float kPlaitsBlobFadeStep = 1.0f / float(kPlaitsBlobFadeSize);
// (position * kPlaitsBlobFadeStep) squared, in single precision.
extern const float kPlaitsBlobFadeGain[kPlaitsBlobFadeSize];

class PlaitsBlobEngine : public plaits::Engine {
 public:
  PlaitsBlobEngine() { }
  ~PlaitsBlobEngine() { }

  // Bind a checked blob in place on each switch, then call Init.
  // Resolve dependencies from the initialized library registry (see ABI.md).
  void Bind(
      const uint8_t* blob,
      const PlaitsBlobServices* services,
      uint32_t model,
      const PlaitsLibraries* libraries) {
    const PlaitsBlobHeader* h = reinterpret_cast<const PlaitsBlobHeader*>(
        blob);
    const uintptr_t base = reinterpret_cast<uintptr_t>(blob);
    abi_ = h->abi;
    if (abi_ == kPlaitsBlobAbi) {
      SetEntries(base, h->init_offset, h->reset_offset, h->render_offset);
      state_size_ = h->state_size;
      arena_size_ = h->arena_size;
      argument_ = 0;
      SetPostProcessing(h->flags, h->out_gain, h->aux_gain);
    } else {
      const PlaitsBlobHeader2* h2 =
          reinterpret_cast<const PlaitsBlobHeader2*>(blob);
      SetEntries(base, h2->init_offset, h2->reset_offset, h2->render_offset);
      state_size_ = h2->state_size;
      arena_size_ = h2->arena_size;
      argument_ = model - h2->first_id;
      SetPostProcessing(h2->flags, h2->out_gain, h2->aux_gain);
      if (h2->num_dependencies || h2->flags & PLAITS_BLOB_FLAG_TERRAINS) {
        libraries->Resolve(blob, libraries_);
      }
    }
    services_ = services;
    object_ = NULL;
  }

  // The state starts zeroed, as Plaits' .bss does. The arena keeps what the
  // last engine left, as the compiled-in engines' shared buffer does.
  virtual void Init(stmlib::BufferAllocator* allocator) {
    uint8_t* arena = arena_size_
        ? reinterpret_cast<uint8_t*>(
            allocator->Allocate<float>((arena_size_ + 3) / 4))
        : NULL;
    // newlib's memset clears by words; a byte loop cost 1% of a block.
    memset(state_, 0, state_size_);
    object_ = abi_ == kPlaitsBlobAbi
        ? (*reinterpret_cast<PlaitsBlobInitFn>(init_))(
            state_, services_, arena, arena_size_)
        : (*reinterpret_cast<PlaitsBlobInitFn2>(init_))(
            state_, services_, arena, arena_size_, libraries_);
    fade_ = 0;
  }

  virtual void Reset() {
    if (abi_ == kPlaitsBlobAbi) {
      (*reinterpret_cast<PlaitsBlobResetFn>(reset_))(object_, services_);
    } else {
      (*reinterpret_cast<PlaitsBlobResetFn2>(reset_))(
          object_, services_, argument_);
    }
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  // The slot, for tests.
  inline const uint8_t* state() const { return state_; }
  // The records init gets, for tests.
  inline const PlaitsBlobLibrary* const* libraries() const {
    return libraries_;
  }

  virtual void Render(
      const plaits::EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    (*render_)(
        object_, services_, &parameters, out, aux, size, already_enveloped);
    if (fade_ < kPlaitsBlobFadeSize) {
      fade_ = FadeIn(out, size, fade_);
    }
  }

  // Scales a block from sample position of the fade-in, a squared ramp from
  // 0; returns the position after it.
  static size_t FadeIn(float* out, size_t size, size_t position) {
    size_t i = 0;
    while (i < size && position < kPlaitsBlobFadeSize) {
      out[i] *= kPlaitsBlobFadeGain[position];
      ++i;
      ++position;
    }
    return position;
  }

 private:
  void SetEntries(
      uintptr_t base,
      uint32_t init,
      uint32_t reset,
      uint32_t render) {
    init_ = base + init;
    reset_ = base + reset;
    render_ = reinterpret_cast<PlaitsBlobRenderFn>(base + render);
  }

  void SetPostProcessing(uint32_t flags, float out_gain, float aux_gain) {
    post_processing_settings.already_enveloped =
        flags & PLAITS_BLOB_FLAG_ENVELOPED;
    post_processing_settings.out_gain = out_gain;
    post_processing_settings.aux_gain = aux_gain;
  }

  uint32_t abi_;
  uintptr_t init_;
  uintptr_t reset_;
  PlaitsBlobRenderFn render_;
  const PlaitsBlobServices* services_;
  size_t state_size_;
  size_t arena_size_;
  uint32_t argument_;
  size_t fade_;
  const PlaitsBlobLibrary* libraries_[kMaxPlaitsBlobRecords];
  void* object_;
  // Bytes, so that the blob's object is nested in this one.
  uint8_t state_[kPlaitsBlobMaxStateSize2] __attribute__((aligned(8)));

  DISALLOW_COPY_AND_ASSIGN(PlaitsBlobEngine);
};

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_BLOB_ENGINE_H_
