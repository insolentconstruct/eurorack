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
//
// -----------------------------------------------------------------------------
//
// Library blobs, built at synth entry: their records, RAM and init.

#ifndef BEADS_BLOB_PLAITS_LIBRARIES_H_
#define BEADS_BLOB_PLAITS_LIBRARIES_H_

#include "stmlib/stmlib.h"

#include "beads/blob/plaits_blob.h"
#include "beads/blob/plaits_region.h"

namespace beads {

class PlaitsLibraries {
 public:
  PlaitsLibraries() { }
  ~PlaitsLibraries() { }

  // Outside the audio interrupt: builds the region's libraries in ram, as
  // PlaitsRegion::Read accounted for them, in table order (a library comes
  // after those it needs). scratch is free until the voices are built.
  void Init(
      const PlaitsRegion& region,
      const PlaitsBlobServices* services,
      uint8_t* ram,
      size_t ram_size,
      uint8_t* scratch,
      size_t scratch_size);

  // The built library with this ID, or NULL.
  const PlaitsBlobLibrary* Find(uint32_t id) const;

  // A blob's dependencies, in its listed order, into libraries; then TERR's
  // record if the blob is flagged for it.
  void Resolve(
      const uint8_t* blob,
      const PlaitsBlobLibrary** libraries) const;

 private:
  PlaitsBlobTerrains terrains_;
  PlaitsBlobLibrary terrain_record_;
  PlaitsBlobLibrary* records_;
  uint8_t ids_[kMaxPlaitsLibraries];
  size_t num_libraries_;

  DISALLOW_COPY_AND_ASSIGN(PlaitsLibraries);
};

}  // namespace beads

#endif  // BEADS_BLOB_PLAITS_LIBRARIES_H_
