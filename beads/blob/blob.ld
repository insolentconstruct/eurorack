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
	else in an output section other than .blob (and a library's
	.blob_exports) fails the gate.
*/

INCLUDE beads/blob/services_v2.ld

_ZN6plaits8lut_sineE = PLAITS_BLOB_LUT_SINE;
_ZN6stmlib20lut_pitch_ratio_highE = PLAITS_BLOB_LUT_PITCH_RATIO_HIGH;
_ZN6stmlib19lut_pitch_ratio_lowE = PLAITS_BLOB_LUT_PITCH_RATIO_LOW;
_ZN6stmlib6Random10rng_state_E = PLAITS_BLOB_RANDOM_STATE;
_ZN6plaits8lut_foldE = PLAITS_BLOB_LUT_FOLD;
_ZN6plaits26lut_fm_frequency_quantizerE =
    PLAITS_BLOB_LUT_FM_FREQUENCY_QUANTIZER;
_ZN6plaits22lut_4x_downsampler_firE = PLAITS_BLOB_LUT_4X_DOWNSAMPLER_FIR;
_ZN6plaits18lut_ws_inverse_tanE = PLAITS_BLOB_LUT_WS_INVERSE_TAN;
_ZN6plaits18lut_ws_inverse_sinE = PLAITS_BLOB_LUT_WS_INVERSE_SIN;
_ZN6plaits13lut_ws_linearE = PLAITS_BLOB_LUT_WS_LINEAR;
_ZN6plaits11lut_ws_bumpE = PLAITS_BLOB_LUT_WS_BUMP;

/* No entry symbol: a library has no render. The header, entries and export
   table are kept by name. */

SECTIONS
{
	. = blob_base;
	.blob :
	{
		/* Writable input stays out, in its own section, which the gate refuses. */
		KEEP(INPUT_SECTION_FLAGS(!SHF_WRITE) *(.blob_header))
		KEEP(INPUT_SECTION_FLAGS(!SHF_WRITE) *(.text.plaits_blob_init))
		KEEP(INPUT_SECTION_FLAGS(!SHF_WRITE) *(.text.plaits_blob_reset))
		KEEP(INPUT_SECTION_FLAGS(!SHF_WRITE) *(.text.plaits_blob_render))
		INPUT_SECTION_FLAGS(!SHF_WRITE) *(.text .text.*)
		INPUT_SECTION_FLAGS(!SHF_WRITE) *(.rodata .rodata.*)
		. = ALIGN(4);
		plaits_blob_got_plt = .;
		*(.got.plt)
		plaits_blob_got = .;
		*(.got)
		plaits_blob_got_end = .;
	}
	/* A library's export table: absolute words the packer turns into offsets. */
	.blob_exports : { KEEP(*(.blob_exports)) }
	/DISCARD/ : { *(.comment) }
}
