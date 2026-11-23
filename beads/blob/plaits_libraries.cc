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
// Library blobs, built at synth entry.

#include "beads/blob/plaits_libraries.h"

#include <algorithm>
#include <cstring>

namespace beads {

using namespace std;

void PlaitsLibraries::Init(
    const PlaitsRegion& region,
    const PlaitsBlobServices* services,
    uint8_t* ram,
    size_t ram_size,
    uint8_t* scratch,
    size_t scratch_size) {
  num_libraries_ = 0;
  terrains_.grids = region.terrains();
  terrains_.count = region.num_terrains();
  memset(&terrain_record_, 0, sizeof(terrain_record_));
  terrain_record_.context = &terrains_;
  records_ = reinterpret_cast<PlaitsBlobLibrary*>(ram);
  size_t used = PlaitsLibraryRamAlign(
      region.num_libraries() * sizeof(PlaitsBlobLibrary));
  // Library state starts zeroed, as a .bss would.
  fill(&ram[0], &ram[min(used, ram_size)], 0);
  for (size_t i = 0; i < region.num_libraries(); ++i) {
    const uint8_t* blob = region.library(i);
    PlaitsBlobInfo info;
    PlaitsBlob::Info(blob, &info);
    const size_t size = PlaitsLibraryRamAlign(info.state_size);
    if (size > ram_size - used || info.arena_size > scratch_size) {
      // Not reached: the region refused what the synth cannot give.
      return;
    }
    PlaitsBlobLibrary* library = &records_[i];
    for (uint32_t j = 0; j < info.num_exports; ++j) {
      uint32_t offset;
      memcpy(&offset, blob + info.exports_offset + 4 * j, sizeof(offset));
      library->exports[j] = blob + offset;
    }
    const PlaitsBlobLibrary* dependencies[kMaxPlaitsBlobRecords];
    Resolve(blob, dependencies);
    uint8_t* library_ram = ram + used;
    fill(&library_ram[0], &library_ram[size], 0);
    used += size;
    library->context = info.init_offset
        ? (*reinterpret_cast<PlaitsBlobLibraryInitFn>(
            blob + info.init_offset))(
                library_ram, info.state_size, scratch, scratch_size,
                services, dependencies)
        : NULL;
    ids_[i] = region.library_id(i);
    num_libraries_ = i + 1;
  }
}

const PlaitsBlobLibrary* PlaitsLibraries::Find(uint32_t id) const {
  for (size_t i = 0; i < num_libraries_; ++i) {
    if (ids_[i] == id) {
      return &records_[i];
    }
  }
  return NULL;
}

void PlaitsLibraries::Resolve(
    const uint8_t* blob,
    const PlaitsBlobLibrary** libraries) const {
  const PlaitsBlobHeader2* h = reinterpret_cast<const PlaitsBlobHeader2*>(
      blob);
  for (uint32_t i = 0; i < h->num_dependencies; ++i) {
    const PlaitsBlobDependency* d =
        reinterpret_cast<const PlaitsBlobDependency*>(
            blob + h->dependencies_offset) + i;
    libraries[i] = Find(d->id);
  }
  if (h->flags & PLAITS_BLOB_FLAG_TERRAINS) {
    libraries[h->num_dependencies] = &terrain_record_;
  }
}

}  // namespace beads
