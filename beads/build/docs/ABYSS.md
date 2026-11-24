<!--
Copyright 2026 Insolent Construct.

Author: Insolent Construct

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

-->

# Abyss

Select Abyss in the Processor option with the FEEDBACK knob in its top
quarter. The indication pulses red. Entering Abyss borrows the recording memory.
Returning to Granular restores its saved frozen recording when available.

| Control | Effect |
| --- | --- |
| TIME | Scales the eight reverb delay lengths. |
| SIZE | Sets the decay time. |
| SHAPE | Increases modulation depth and rate. |
| DENSITY | Blends in two input all-passes per channel. |
| FEEDBACK | Counterclockwise blooms after each onset. Noon gives a conventional decay. Clockwise swells towards a cutoff. |
| PITCH | Shifts part of the recirculating signal up or down. The centre notch disables shimmer. |
| REVERB | Tilts the feedback damping from dark to bright. |
| DRY/WET | Crossfades input and reverb. |
| FREEZE | Holds the tail at unity feedback with fixed taps. It releases back to the current controls. |

Abyss is an eight-line feedback delay network with an orthonormal Hadamard
matrix, separately diffused stereo injection and a different modulation
rate per line. Delays and control time constants scale with the codec rate.
For moving taps the loop damps each line according to its round-trip time
and a two-band tilt. The shimmer branch crossfades two shifting reads
from one line back into that line's feedback path before the matrix.
Gravity shapes the wet output with a retriggerable onset envelope.
During FREEZE the loop uses integer reads and bypasses damping and
shimmer. The dry signal remains available through DRY/WET.

Design references include [Jot and Chaigne, Digital Delay Networks for
Designing Artificial Reverberators](https://aes2.org/publications/elibrary-page/?id=5663)
and [Dattorro, Effect Design, Part 1](https://aes.org/publications/elibrary/elibrary-page/?id=10160).
