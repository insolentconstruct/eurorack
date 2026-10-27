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
// Load a settings sector image with stmlib's ChunkStorage, as the firmware
// does at boot, and print whether it loaded, the calibration record and the
// settings record in hex.

#include <sys/mman.h>

#include <cstdio>

#include "beads/settings.h"

bool test_down[4];
unsigned test_writes;

using namespace beads;

namespace {

void Print(const char* name, const void* data, size_t size) {
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  printf("%s ", name);
  for (size_t i = 0; i < size; ++i) {
    printf("%02x", bytes[i]);
  }
  printf("\n");
}

}  // namespace

int main(int argc, char** argv) {
  void* const sector = reinterpret_cast<void*>(0x081e0000);
  if (argc != 2 || mmap(sector, 0x20000, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != sector) {
    return 2;
  }
  FILE* f = fopen(argv[1], "rb");
  if (!f || fread(sector, 1, 0x20000, f) != 0x20000) {
    return 2;
  }
  fclose(f);
  PersistentData persistent_data = { };
  State state = { };
  stmlib::ChunkStorage<15, PersistentData, State> storage;
  const bool loaded = storage.Init(&persistent_data, &state);
  printf("loaded %d\n", loaded);
  Print("calibration", &persistent_data, sizeof(persistent_data));
  Print("settings", &state, sizeof(state));
  return 0;
}
