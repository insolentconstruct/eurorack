# Copyright 2026 Insolent Construct.
#
# Author: Insolent Construct (insolentconstruct@gmail.com)
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



"""Pitch scales and the grain schedule of the beta scenarios. Plain data, so
tools that must not need numpy can import it."""

SCALES = (
    ('chromatic', tuple(range(12))), ('fifths', (0,7)),
    ('major', (0,2.0004,3.9996,5.0004,6.9996,9,11.0004)),
    ('minor', (0,2.0004,3,5.0004,6.9996,8.0004,9.9996)),
    ('major-pentatonic', (0,2.0004,3.9996,6.9996,9)),
    ('minor-pentatonic', (0,3,5.0004,6.9996,9.9996)),
    ('pelog', (0,1.53,7.0596,8.4804)),
    ('bhairav', (0,.9024,3.8628,4.98,7.02,7.9212,10.8828)),
    ('shri', (0,2.0388,3.156,4.98,7.02,9.0588,10.1748)),
)


# One isolated grain per second: the trigger times of the random-side scenarios.
TRIGGERS = tuple(3.+i for i in range(24))
