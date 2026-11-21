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

#include <algorithm>

#include "stmlib/stmlib.h"
#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"

namespace beads {

class PlaitsBlobEngine : public plaits::Engine {
 public:
  PlaitsBlobEngine() { }
  ~PlaitsBlobEngine() { }

  // Only for a blob that passed PlaitsBlob::Check; Init must follow. model
  // is the selected ID, which version 2 blobs get as reset's argument.
  void Bind(
      const uint8_t* blob,
      const PlaitsBlobServices* services,
      uint32_t model) {
    PlaitsBlobInfo info;
    PlaitsBlob::Info(blob, &info);
    const uintptr_t base = reinterpret_cast<uintptr_t>(blob);
    abi_ = info.abi;
    init_ = base + info.init_offset;
    reset_ = base + info.reset_offset;
    render_ = reinterpret_cast<PlaitsBlobRenderFn>(base + info.render_offset);
    state_size_ = info.state_size;
    arena_size_ = info.arena_size;
    argument_ = model - info.first_id;
    services_ = services;
    object_ = NULL;
    const PlaitsBlobHeader* h = reinterpret_cast<const PlaitsBlobHeader*>(
        blob);
    const PlaitsBlobHeader2* h2 = reinterpret_cast<const PlaitsBlobHeader2*>(
        blob);
    const bool v1 = abi_ == kPlaitsBlobAbi;
    post_processing_settings.already_enveloped =
        (v1 ? h->flags : h2->flags) & PLAITS_BLOB_FLAG_ENVELOPED;
    post_processing_settings.out_gain = v1 ? h->out_gain : h2->out_gain;
    post_processing_settings.aux_gain = v1 ? h->aux_gain : h2->aux_gain;
  }

  // State and arena start zeroed, as Plaits' .bss and the voice's RAM do.
  // Check keeps arena_size within the freed shared buffer; float-aligned.
  virtual void Init(stmlib::BufferAllocator* allocator) {
    uint8_t* arena = arena_size_
        ? reinterpret_cast<uint8_t*>(
            allocator->Allocate<float>((arena_size_ + 3) / 4))
        : NULL;
    std::fill(&state_[0], &state_[state_size_], 0);
    if (arena) {
      std::fill(&arena[0], &arena[arena_size_], 0);
    }
    object_ = abi_ == kPlaitsBlobAbi
        ? (*reinterpret_cast<PlaitsBlobInitFn>(init_))(
            state_, services_, arena, arena_size_)
        : (*reinterpret_cast<PlaitsBlobInitFn2>(init_))(
            state_, services_, arena, arena_size_, NULL);
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

  virtual void Render(
      const plaits::EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    (*render_)(
        object_, services_, &parameters, out, aux, size, already_enveloped);
  }

 private:
  uint32_t abi_;
  uintptr_t init_;
  uintptr_t reset_;
  PlaitsBlobRenderFn render_;
  const PlaitsBlobServices* services_;
  size_t state_size_;
  size_t arena_size_;
  uint32_t argument_;
  void* object_;
  // Bytes, so that the blob's object is nested in this one.
  uint8_t state_[kPlaitsBlobMaxStateSize2] __attribute__((aligned(8)));

  DISALLOW_COPY_AND_ASSIGN(PlaitsBlobEngine);
};

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_BLOB_ENGINE_H_
