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

	Emulator harness: the services block at its fixed address, code after it,
	RAM in AXI SRAM. The region is written by render_compare.py.
*/

ENTRY(harness_setup)
EXTERN(harness_reset harness_render)

MEMORY
{
	FLASH (rx) : ORIGIN = 0x08021400, LENGTH = 640K
	RAM (rwx) : ORIGIN = 0x24000000, LENGTH = 256K
}

INCLUDE beads/blob/services_v1.ld

SECTIONS
{
	INCLUDE beads/blob/services_section.ld
	.text : { *(.text*) *(.rodata*) . = ALIGN(4); } >FLASH
	.data : { *(.data*) . = ALIGN(4); } >RAM AT>FLASH
	.bss (NOLOAD) : { *(.bss*) *(COMMON) . = ALIGN(4); } >RAM
	/DISCARD/ : { *(.init_array*) *(.fini_array*) *(.ARM.exidx*)
	              *(.ARM.extab*) *(.init) *(.fini) *(.comment) }
}
