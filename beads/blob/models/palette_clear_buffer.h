// Copyright 2026 Rubato Audio.
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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Clear an arena without changing its sample representation.

#ifndef BEADS_BLOB_MODELS_PALETTE_CLEAR_BUFFER_H_
#define BEADS_BLOB_MODELS_PALETTE_CLEAR_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

namespace plaits {

// may_alias: the buffer is later read through its own element type.
typedef uint32_t __attribute__((__may_alias__)) ClearWord;

// Clear words through the alias-safe type, then the remaining bytes.
inline void ClearBuffer(void* buffer, size_t bytes) {
  uint8_t* p = static_cast<uint8_t*>(buffer);
  while (bytes && (reinterpret_cast<uintptr_t>(p) & 3)) {
    *p++ = 0;
    --bytes;
  }
  ClearWord* w = reinterpret_cast<ClearWord*>(p);
  size_t words = bytes >> 2;
  while (words >= 4) {
    w[0] = 0;
    w[1] = 0;
    w[2] = 0;
    w[3] = 0;
    w += 4;
    words -= 4;
  }
  while (words--) {
    *w++ = 0;
  }
  p = reinterpret_cast<uint8_t*>(w);
  bytes &= 3;
  while (bytes--) {
    *p++ = 0;
  }
}

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_PALETTE_CLEAR_BUFFER_H_
