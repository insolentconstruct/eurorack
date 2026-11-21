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

	Development builds with Plaits blobs (PLAITS_BLOBS=1): the services
	block and its exports right after the profile, stmlib::Random's state at
	its export address, and the region from the next 4 KiB after the app.
*/

INCLUDE beads/blob/services_v2.ld

SECTIONS
{
	INCLUDE beads/blob/services_section.ld
}
INSERT AFTER .profile;

SECTIONS
{
	/* Not loaded: the app seeds the generator before anything draws. */
	.blob_random PLAITS_BLOB_RANDOM_STATE (NOLOAD) :
	{
		KEEP(*(.data._ZN6stmlib6Random10rng_state_E))
	} >DTCM
}
INSERT BEFORE .data;

plaits_region_begin = ALIGN(LOADADDR(.itcm) + SIZEOF(.itcm), 4096);

ASSERT(ADDR(.blob_services) == ADDR(.profile) + SIZEOF(.profile),
    "the services block is not right after the profile")
ASSERT(_ZN6stmlib6Random10rng_state_E == PLAITS_BLOB_RANDOM_STATE,
    "stmlib::Random's state moved")
ASSERT(plaits_region_begin < 0x08100000, "no room for the region")
