// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// What this build trusts in the region: its release number, the approved
// blobs' digests, and the default layout used without a profile directory
// (plaits_catalog.cc, written by beads/build/tools/blob_catalog.py).

#ifndef BEADS_BLOB_PLAITS_CATALOG_H_
#define BEADS_BLOB_PLAITS_CATALOG_H_

#include "stmlib/stmlib.h"

#include "beads/blob/plaits_region.h"

namespace beads {

extern const uint32_t kPlaitsBlobRelease;
extern const uint8_t kPlaitsApprovedBlobs[][kPlaitsBlobDigestSize];
extern const size_t kNumPlaitsApprovedBlobs;
extern const PlaitsDefaultSection kPlaitsDefaultLayout[];
extern const size_t kNumPlaitsDefaultSections;

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_CATALOG_H_
