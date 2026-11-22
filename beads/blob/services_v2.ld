/*
	Copyright 2026 Insolent Construct.

	Author: Insolent Construct (insolentconstruct@gmail.com)

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
	See http://creativecommons.org/licenses/MIT/ for more information.

	Blob services, version 2: version 1's exports, unchanged, then
	stmlib::Random's state at a fixed RAM address and seven shared tables
	after version 1's block. Frozen as version 1's are.
*/

INCLUDE beads/blob/services_v1.ld

PLAITS_BLOB_RANDOM_STATE = 0x20000000;
PLAITS_BLOB_LUT_FOLD = 0x0802264c;
PLAITS_BLOB_LUT_FM_FREQUENCY_QUANTIZER = 0x08022e5c;
PLAITS_BLOB_LUT_4X_DOWNSAMPLER_FIR = 0x08023064;
PLAITS_BLOB_LUT_WS_INVERSE_TAN = 0x08023074;
PLAITS_BLOB_LUT_WS_INVERSE_SIN = 0x08023278;
PLAITS_BLOB_LUT_WS_LINEAR = 0x0802347c;
PLAITS_BLOB_LUT_WS_BUMP = 0x08023680;
PLAITS_BLOB_SERVICES_V2_END = 0x08023882;
PLAITS_BLOB_LUT_FOLD_SIZE = 2064;
PLAITS_BLOB_LUT_FM_FREQUENCY_QUANTIZER_SIZE = 520;
PLAITS_BLOB_LUT_4X_DOWNSAMPLER_FIR_SIZE = 16;
PLAITS_BLOB_LUT_WS_SIZE = 514;
PLAITS_BLOB_INT16_EXPORT_ALIGNMENT = 2;
