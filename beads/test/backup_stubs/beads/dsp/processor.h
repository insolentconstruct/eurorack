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
// Audio buffer and state saved by the backup.

#ifndef BEADS_TEST_BACKUP_STUBS_BEADS_DSP_PROCESSOR_H_
#define BEADS_TEST_BACKUP_STUBS_BEADS_DSP_PROCESSOR_H_

#include <cstddef>
#include <stdint.h>

namespace beads {

const size_t kDelayBufferSizeBits = 768 * 1000 * 8;
// The buffer bytes and the state the backup saves, nothing else.
class Processor {
 public:
  enum Media {
    MEDIA_DIRTY_TAPE,
    MEDIA_CLEAN_TAPE,
    MEDIA_VINTAGE_DIGITAL,
    MEDIA_DIGITAL
  };
  void Configure(Media media, bool mono) {
    media_ = media;
    mono_ = mono;
    ++configured_;
  }
  Media media() const { return media_; }
  bool mono() const { return mono_; }
  int* mutable_delay_write_ptr() { return &write_ptr_; }
  uint8_t* delay_buffer_raw_bytes(size_t offset) { return &data_[offset]; }
  uint8_t data_[kDelayBufferSizeBits / 8];
  Media media_;
  bool mono_;
  int write_ptr_;
  int configured_;
};

}  // namespace beads

#endif  // BEADS_TEST_BACKUP_STUBS_BEADS_DSP_PROCESSOR_H_
