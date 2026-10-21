#!/usr/bin/python2.5
#
# Copyright 2018 Emilie Gillet.
#
# Author: Emilie Gillet (emilie.o.gillet@gmail.com)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# 
# See http://creativecommons.org/licenses/MIT/ for more information.
#
# -----------------------------------------------------------------------------
#
# Lookup table definitions.

import scipy.signal
import numpy
import pylab

lookup_tables = []
lookup_tables_i16 = []
lookup_tables_i8 = []


"""----------------------------------------------------------------------------
Sine table
----------------------------------------------------------------------------"""

WAVETABLE_SIZE = 1024
t = numpy.arange(0.0, WAVETABLE_SIZE + WAVETABLE_SIZE / 4 + 1) / WAVETABLE_SIZE
x = numpy.sin(2 * numpy.pi * t)
lookup_tables += [('sine', x)]



"""----------------------------------------------------------------------------
Phase vocoder analysis/synthesis window, from Clouds.
----------------------------------------------------------------------------"""

def sum_window(window, steps):
  n = window.shape[0]
  start = 0
  stride = n // steps
  s = 0
  for i in range(steps):
    s = s + window[start:start+stride] ** 2
    start += stride
  return s


window_size = 4096
t = numpy.arange(0.0, window_size) / window_size

# Needs tweaks to provide good reconstruction
power = (1.0 - (2 * t - 1.0) ** 2.0) ** 1.25
compensation = sum_window(power, 2) ** 0.5
compensation = numpy.array(list(compensation) * 2)
power /= compensation

lookup_tables += [('sine_window_4096', power)]
