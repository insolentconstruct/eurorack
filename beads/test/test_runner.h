// Copyright 2026 Insolent Construct.
// Author: Insolent Construct
#ifndef BEADS_TEST_TEST_RUNNER_H_
#define BEADS_TEST_TEST_RUNNER_H_

#include <cstdio>
#include <cstring>
#include <chrono>

struct HostTest {
  const char* name;
  void (*run)();
};

// No arguments runs everything; otherwise each argument is an exact test name.
inline int RunHostTests(int argc, char** argv, const HostTest* tests, int count) {
  if (argc == 2 && !strcmp(argv[1], "--list")) {
    for (int i = 0; i < count; ++i) puts(tests[i].name);
    return 0;
  }
  for (int arg = 1; arg < argc; ++arg) {
    bool found = false;
    for (int i = 0; i < count; ++i) {
      if (!strcmp(argv[arg], tests[i].name)) found = true;
    }
    if (!found) {
      fprintf(stderr, "Unknown test: %s\n", argv[arg]);
      return 2;
    }
  }
  for (int i = 0; i < count; ++i) {
    bool selected = argc == 1;
    for (int arg = 1; arg < argc; ++arg) {
      if (!strcmp(argv[arg], tests[i].name)) selected = true;
    }
    if (!selected) continue;
    const auto start = std::chrono::steady_clock::now();
    tests[i].run();
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    printf("TEST %s %.3f s\n", tests[i].name, seconds);
  }
  return 0;
}

#endif  // BEADS_TEST_TEST_RUNNER_H_
