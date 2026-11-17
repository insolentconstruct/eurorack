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

	The firmware side of services_v1.ld, for a SECTIONS command: the services
	block and its exports at their fixed addresses.
*/

.blob_services PLAITS_BLOB_SERVICES :
{
	KEEP(*(.blob_services))
	. = PLAITS_BLOB_LUT_SINE - PLAITS_BLOB_SERVICES;
	KEEP(*(.rodata._ZN6plaits8lut_sineE))
	. = PLAITS_BLOB_LUT_PITCH_RATIO_HIGH - PLAITS_BLOB_SERVICES;
	KEEP(*(.rodata._ZN6stmlib20lut_pitch_ratio_highE))
	. = PLAITS_BLOB_LUT_PITCH_RATIO_LOW - PLAITS_BLOB_SERVICES;
	KEEP(*(.rodata._ZN6stmlib19lut_pitch_ratio_lowE))
	. = PLAITS_BLOB_SERVICES_END - PLAITS_BLOB_SERVICES;
} >FLASH =0xff

ASSERT(_ZN6plaits8lut_sineE == PLAITS_BLOB_LUT_SINE, "lut_sine moved")
ASSERT(_ZN6stmlib20lut_pitch_ratio_highE == PLAITS_BLOB_LUT_PITCH_RATIO_HIGH,
    "lut_pitch_ratio_high moved")
ASSERT(_ZN6stmlib19lut_pitch_ratio_lowE == PLAITS_BLOB_LUT_PITCH_RATIO_LOW,
    "lut_pitch_ratio_low moved")
