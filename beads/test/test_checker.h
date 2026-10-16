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
// Host test errors.

#ifndef BEADS_TEST_TEST_CHECKER_H_
#define BEADS_TEST_TEST_CHECKER_H_

#include <cstdio>

#include "stmlib/stmlib.h"

namespace beads {

class TestChecker {
 public:
  TestChecker() : test_("main"), failures_(0) { }

  void Start(const char* test) { test_ = test; }
  bool Check(bool condition, const char* message) {
    if (!condition) {
      printf("ERROR: %s: %s\n", test_, message);
      ++failures_;
    }
    return condition;
  }
  int failures() const { return failures_; }

 private:
  const char* test_;
  int failures_;

  DISALLOW_COPY_AND_ASSIGN(TestChecker);
};

}  // namespace beads

static beads::TestChecker test_checker;

#define CHECK(condition) test_checker.Check((condition), #condition)

#endif  // BEADS_TEST_TEST_CHECKER_H_
