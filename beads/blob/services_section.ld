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

	The firmware side of services_v2.ld, for a SECTIONS command: the services
	block and its exports at their fixed addresses. stmlib::Random's state
	is placed by the firmware's own script (PLAITS_BLOB_RANDOM_STATE).
*/

.blob_services PLAITS_BLOB_SERVICES :
{
	KEEP(*(.blob_services))
	. = PLAITS_BLOB_LUT_SINE - PLAITS_BLOB_SERVICES;
} >FLASH =0xff
.blob_lut_sine PLAITS_BLOB_LUT_SINE :
{
	KEEP(*(.rodata._ZN6plaits8lut_sineE))
} >FLASH
.blob_lut_pitch_ratio_high PLAITS_BLOB_LUT_PITCH_RATIO_HIGH :
{
	KEEP(*(.rodata._ZN6stmlib20lut_pitch_ratio_highE))
} >FLASH
.blob_lut_pitch_ratio_low PLAITS_BLOB_LUT_PITCH_RATIO_LOW :
{
	KEEP(*(.rodata._ZN6stmlib19lut_pitch_ratio_lowE))
} >FLASH
.blob_lut_fold PLAITS_BLOB_LUT_FOLD :
{
	KEEP(*(.rodata._ZN6plaits8lut_foldE))
} >FLASH
.blob_lut_fm_frequency_quantizer PLAITS_BLOB_LUT_FM_FREQUENCY_QUANTIZER :
{
	KEEP(*(.rodata._ZN6plaits26lut_fm_frequency_quantizerE))
} >FLASH
.blob_lut_4x_downsampler_fir PLAITS_BLOB_LUT_4X_DOWNSAMPLER_FIR :
{
	KEEP(*(.rodata._ZN6plaits22lut_4x_downsampler_firE))
} >FLASH
.blob_lut_ws_inverse_tan PLAITS_BLOB_LUT_WS_INVERSE_TAN :
{
	KEEP(*(.rodata._ZN6plaits18lut_ws_inverse_tanE))
} >FLASH
.blob_lut_ws_inverse_sin PLAITS_BLOB_LUT_WS_INVERSE_SIN :
{
	KEEP(*(.rodata._ZN6plaits18lut_ws_inverse_sinE))
} >FLASH
.blob_lut_ws_linear PLAITS_BLOB_LUT_WS_LINEAR :
{
	KEEP(*(.rodata._ZN6plaits13lut_ws_linearE))
} >FLASH
.blob_lut_ws_bump PLAITS_BLOB_LUT_WS_BUMP :
{
	KEEP(*(.rodata._ZN6plaits11lut_ws_bumpE))
} >FLASH

ASSERT(_ZN6plaits8lut_sineE == PLAITS_BLOB_LUT_SINE, "lut_sine moved")
ASSERT(_ZN6stmlib20lut_pitch_ratio_highE == PLAITS_BLOB_LUT_PITCH_RATIO_HIGH,
    "lut_pitch_ratio_high moved")
ASSERT(_ZN6stmlib19lut_pitch_ratio_lowE == PLAITS_BLOB_LUT_PITCH_RATIO_LOW,
    "lut_pitch_ratio_low moved")
ASSERT(SIZEOF(.blob_lut_sine) == PLAITS_BLOB_LUT_SINE_SIZE,
    "lut_sine size changed")
ASSERT(SIZEOF(.blob_lut_pitch_ratio_high) ==
    PLAITS_BLOB_LUT_PITCH_RATIO_HIGH_SIZE, "lut_pitch_ratio_high size changed")
ASSERT(SIZEOF(.blob_lut_pitch_ratio_low) ==
    PLAITS_BLOB_LUT_PITCH_RATIO_LOW_SIZE, "lut_pitch_ratio_low size changed")
ASSERT(ALIGNOF(.blob_lut_sine) == PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_pitch_ratio_high) == PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_pitch_ratio_low) == PLAITS_BLOB_EXPORT_ALIGNMENT,
    "an export's alignment changed")
ASSERT(ADDR(.blob_lut_pitch_ratio_low) + SIZEOF(.blob_lut_pitch_ratio_low) ==
    PLAITS_BLOB_SERVICES_END, "the services block changed size")

ASSERT(_ZN6plaits8lut_foldE == PLAITS_BLOB_LUT_FOLD, "lut_fold moved")
ASSERT(_ZN6plaits26lut_fm_frequency_quantizerE ==
    PLAITS_BLOB_LUT_FM_FREQUENCY_QUANTIZER, "lut_fm_frequency_quantizer moved")
ASSERT(_ZN6plaits22lut_4x_downsampler_firE ==
    PLAITS_BLOB_LUT_4X_DOWNSAMPLER_FIR, "lut_4x_downsampler_fir moved")
ASSERT(_ZN6plaits18lut_ws_inverse_tanE == PLAITS_BLOB_LUT_WS_INVERSE_TAN,
    "lut_ws_inverse_tan moved")
ASSERT(_ZN6plaits18lut_ws_inverse_sinE == PLAITS_BLOB_LUT_WS_INVERSE_SIN,
    "lut_ws_inverse_sin moved")
ASSERT(_ZN6plaits13lut_ws_linearE == PLAITS_BLOB_LUT_WS_LINEAR,
    "lut_ws_linear moved")
ASSERT(_ZN6plaits11lut_ws_bumpE == PLAITS_BLOB_LUT_WS_BUMP,
    "lut_ws_bump moved")
ASSERT(SIZEOF(.blob_lut_fold) == PLAITS_BLOB_LUT_FOLD_SIZE,
    "lut_fold size changed")
ASSERT(SIZEOF(.blob_lut_fm_frequency_quantizer) ==
    PLAITS_BLOB_LUT_FM_FREQUENCY_QUANTIZER_SIZE,
    "lut_fm_frequency_quantizer size changed")
ASSERT(SIZEOF(.blob_lut_4x_downsampler_fir) ==
    PLAITS_BLOB_LUT_4X_DOWNSAMPLER_FIR_SIZE,
    "lut_4x_downsampler_fir size changed")
ASSERT(SIZEOF(.blob_lut_ws_inverse_tan) == PLAITS_BLOB_LUT_WS_SIZE &&
    SIZEOF(.blob_lut_ws_inverse_sin) == PLAITS_BLOB_LUT_WS_SIZE &&
    SIZEOF(.blob_lut_ws_linear) == PLAITS_BLOB_LUT_WS_SIZE &&
    SIZEOF(.blob_lut_ws_bump) == PLAITS_BLOB_LUT_WS_SIZE,
    "a waveshaper table's size changed")
ASSERT(ALIGNOF(.blob_lut_fold) == PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_fm_frequency_quantizer) ==
        PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_4x_downsampler_fir) == PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_ws_inverse_tan) == PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_ws_inverse_sin) == PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_ws_linear) == PLAITS_BLOB_EXPORT_ALIGNMENT &&
    ALIGNOF(.blob_lut_ws_bump) == PLAITS_BLOB_EXPORT_ALIGNMENT,
    "a version 2 export's alignment changed")
ASSERT(ADDR(.blob_lut_ws_bump) + SIZEOF(.blob_lut_ws_bump) ==
    PLAITS_BLOB_SERVICES_V2_END, "the version 2 block changed size")
