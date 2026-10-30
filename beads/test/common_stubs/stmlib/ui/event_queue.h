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
// Queue read interrupt injection for native tests.

#ifndef BEADS_TEST_COMMON_STUBS_STMLIB_UI_EVENT_QUEUE_H_
#define BEADS_TEST_COMMON_STUBS_STMLIB_UI_EVENT_QUEUE_H_

#define EventQueue FirmwareEventQueue
#include_next "stmlib/ui/event_queue.h"
#undef EventQueue

namespace beads {
void TestOptionsDispatchInterrupt() __attribute__((weak));
inline void DispatchInterrupt() {
  if (TestOptionsDispatchInterrupt) {
    TestOptionsDispatchInterrupt();
  }
}
}  // namespace beads

namespace stmlib {
template<uint16_t size = 32>
class EventQueue : public FirmwareEventQueue<size> {
 public:
  Event PullEvent() {
    Event event = FirmwareEventQueue<size>::PullEvent();
    beads::DispatchInterrupt();
    return event;
  }
};
}  // namespace stmlib

#endif  // BEADS_TEST_COMMON_STUBS_STMLIB_UI_EVENT_QUEUE_H_
