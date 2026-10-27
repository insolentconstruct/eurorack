// Copyright 2026 Insolent Construct.
//
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
// Host hardware interface.

#ifndef BEADS_TEST_FIRMWARE_STUBS_BEADS_DRIVERS_CODEC_H_
#define BEADS_TEST_FIRMWARE_STUBS_BEADS_DRIVERS_CODEC_H_

#include <stddef.h>
#include <stdint.h>

namespace beads {

const size_t kMaxCodecBlockSize = 24;
class Codec {
 public:
  struct Frame { int32_t l; int32_t r; };
  enum Event { EVENT_NONE, EVENT_SEED, EVENT_FREEZE };
  typedef void (*FillBufferCallback)(
      const Event*, const Frame*, Frame*, size_t);
  bool Init() {
    callback_ = NULL;
    starts_ = stops_ = idle_stops_ = 0;
    running_ = false;
    return true;
  }
  bool Start(int32_t rate, size_t size, FillBufferCallback callback) {
    sample_rate_ = rate;
    block_size_ = size;
    callback_ = callback;
    running_ = true;
    ++starts_;
    return true;
  }
  // The HAL faults when the SAI is stopped before its first start.
  void Stop() {
    idle_stops_ += !running_;
    running_ = false;
    ++stops_;
  }
  void Pause() { callback_ = NULL; }
  void Resume(FillBufferCallback callback) { callback_ = callback; }
  int sample_rate_;
  size_t block_size_;
  FillBufferCallback callback_;
  int starts_;
  int stops_;
  int idle_stops_;
  bool running_;
};

}  // namespace beads

#endif  // BEADS_TEST_FIRMWARE_STUBS_BEADS_DRIVERS_CODEC_H_
