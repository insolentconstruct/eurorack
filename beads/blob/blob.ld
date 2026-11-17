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

	One blob: header, code and constants, then the imports, at blob_base
	(--defsym). Imports resolve only to the services' fixed exports; anything
	else in an output section other than .blob fails the gate.
*/

INCLUDE beads/blob/services_v1.ld

_ZN6plaits8lut_sineE = PLAITS_BLOB_LUT_SINE;
_ZN6stmlib20lut_pitch_ratio_highE = PLAITS_BLOB_LUT_PITCH_RATIO_HIGH;
_ZN6stmlib19lut_pitch_ratio_lowE = PLAITS_BLOB_LUT_PITCH_RATIO_LOW;

ENTRY(plaits_blob_render)

SECTIONS
{
	. = blob_base;
	.blob :
	{
		KEEP(*(.blob_header))
		KEEP(*(.text.plaits_blob_init))
		KEEP(*(.text.plaits_blob_reset))
		KEEP(*(.text.plaits_blob_render))
		*(.text .text.*)
		*(.rodata .rodata.*)
		. = ALIGN(4);
		plaits_blob_got_plt = .;
		*(.got.plt)
		plaits_blob_got = .;
		*(.got)
		plaits_blob_got_end = .;
	}
	/DISCARD/ : { *(.comment) }
}
