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
// Host test selection and timing.

#ifndef BEADS_TEST_TEST_RUNNER_H_
#define BEADS_TEST_TEST_RUNNER_H_

#include <chrono>
#include <cstdio>
#include <cstring>

struct HostTest {
  const char* name;
  void (*run)();
};

// No arguments runs everything; otherwise each argument is an exact test name.
inline int RunHostTests(
    int argc, char** argv, const HostTest* tests, int count) {
  if (argc == 2 && !strcmp(argv[1], "--list")) {
    for (int i = 0; i < count; ++i) {
      puts(tests[i].name);
    }
    return 0;
  }
  for (int arg = 1; arg < argc; ++arg) {
    bool found = false;
    for (int i = 0; i < count; ++i) {
      if (!strcmp(argv[arg], tests[i].name)) {
        found = true;
      }
    }
    if (!found) {
      fprintf(stderr, "Unknown test: %s\n", argv[arg]);
      return 2;
    }
  }
  for (int i = 0; i < count; ++i) {
    bool selected = argc == 1;
    for (int arg = 1; arg < argc; ++arg) {
      if (!strcmp(argv[arg], tests[i].name)) {
        selected = true;
      }
    }
    if (!selected) {
      continue;
    }
    const std::chrono::steady_clock::time_point start =
        std::chrono::steady_clock::now();
    tests[i].run();
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    printf("TEST %s %.3f s\n", tests[i].name, seconds);
  }
  return 0;
}

#endif  // BEADS_TEST_TEST_RUNNER_H_
