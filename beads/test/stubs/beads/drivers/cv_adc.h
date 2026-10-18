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
// ADC channels used by settings.

#ifndef BEADS_TEST_HARDWARE_STUBS_BEADS_DRIVERS_CV_ADC_H_
#define BEADS_TEST_HARDWARE_STUBS_BEADS_DRIVERS_CV_ADC_H_

namespace beads {

enum CvAdcChannel {
  CV_ADC_CHANNEL_ASSIGN, CV_ADC_CHANNEL_DENSITY, CV_ADC_CHANNEL_PITCH,
  CV_ADC_CHANNEL_TIME, CV_ADC_CHANNEL_SIZE, CV_ADC_CHANNEL_SHAPE,
  CV_ADC_CHANNEL_LAST
};

}  // namespace beads

#endif  // BEADS_TEST_HARDWARE_STUBS_BEADS_DRIVERS_CV_ADC_H_
