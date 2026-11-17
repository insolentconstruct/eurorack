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
//
// -----------------------------------------------------------------------------
//
// A checked blob as a Plaits engine: its state lives in the voice's RAM.

#ifndef BEADS_BLOB_PLAITS_BLOB_ENGINE_H_
#define BEADS_BLOB_PLAITS_BLOB_ENGINE_H_

#include "stmlib/stmlib.h"

#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"

namespace beads {

class PlaitsBlobEngine : public plaits::Engine {
 public:
  PlaitsBlobEngine() { }
  ~PlaitsBlobEngine() { }

  // Only for a blob that passed PlaitsBlob::Check; Init must follow.
  void Bind(const uint8_t* blob, const PlaitsBlobServices* services) {
    const PlaitsBlobHeader* h = reinterpret_cast<const PlaitsBlobHeader*>(
        blob);
    const uintptr_t base = reinterpret_cast<uintptr_t>(blob);
    init_ = reinterpret_cast<PlaitsBlobInitFn>(base + h->init_offset);
    reset_ = reinterpret_cast<PlaitsBlobResetFn>(base + h->reset_offset);
    render_ = reinterpret_cast<PlaitsBlobRenderFn>(base + h->render_offset);
    arena_size_ = h->arena_size;
    services_ = services;
    object_ = NULL;
  }

  // Check keeps arena_size within the freed shared buffer; float-aligned.
  // The blob constructs its object in state_ and returns a pointer to it.
  virtual void Init(stmlib::BufferAllocator* allocator) {
    uint8_t* arena = arena_size_
        ? reinterpret_cast<uint8_t*>(
            allocator->Allocate<float>((arena_size_ + 3) / 4))
        : NULL;
    object_ = (*init_)(state_, services_, arena, arena_size_);
  }

  virtual void Reset() {
    (*reset_)(object_, services_);
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

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
  PlaitsBlobInitFn init_;
  PlaitsBlobResetFn reset_;
  PlaitsBlobRenderFn render_;
  const PlaitsBlobServices* services_;
  size_t arena_size_;
  void* object_;
  uint64_t state_[kPlaitsBlobMaxStateSize / sizeof(uint64_t)];

  DISALLOW_COPY_AND_ASSIGN(PlaitsBlobEngine);
};

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_BLOB_ENGINE_H_
