#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# See http://creativecommons.org/licenses/MIT/ for more information.
#
# The blob gate's controls, in the build image from the Drizzlet root: every
# beads/test/blob_gate/pos_*.cc must pass; every neg_*.cc must fail to link or
# be refused, with the diagnostic named on its "// Expect:" line. A
# "// Library: NAME" line builds against that control's library blob; a
# "// Source: PATH" line adds a source.
#   beads/build/tools/blob_gate_test.sh OUT
set -euo pipefail
out=${1:?usage: blob_gate_test.sh OUT}
mkdir -p "$out"
failures=0
libraries() {
  sed -n 's|^// Library: \(.*\)$|--library '"$out"'/\1.bin|p' "$1"
}
# A "// Source: PATH" line adds a source to the control's blob.
sources_of() {
  echo "$1"
  sed -n 's|^// Source: ||p' "$1"
}
# The library first: other controls build against it.
sources=(beads/test/blob_gate/pos_library.cc)
for source in beads/test/blob_gate/pos_*.cc; do
  [ "$source" = "${sources[0]}" ] || sources+=("$source")
done
for source in "${sources[@]}"; do
  name=$(basename "$source" .cc)
  # shellcheck disable=SC2046
  if bash beads/build/tools/build_blob.sh "$out" "$name" \
      $(libraries "$source") $(sources_of "$source") > "$out/$name.log" 2>&1; then
    echo "$name: accepted: $(tail -n1 "$out/$name.log")"
  else
    echo "ERROR: $name: the gate refused it"
    cat "$out/$name.log"
    failures=$((failures + 1))
  fi
done
for source in beads/test/blob_gate/neg_*.cc; do
  name=$(basename "$source" .cc)
  expect=$(sed -n 's|^// Expect: ||p' "$source")
  # shellcheck disable=SC2046
  if bash beads/build/tools/build_blob.sh "$out" "$name" \
      $(libraries "$source") $(sources_of "$source") > "$out/$name.log" 2>&1; then
    echo "ERROR: $name: the gate accepted it"
    failures=$((failures + 1))
  elif [ -z "$expect" ] || ! grep -qE "$expect" "$out/$name.log"; then
    echo "ERROR: $name: refused, but not for the expected reason"
    failures=$((failures + 1))
  else
    echo "$name: refused: $(grep -m1 -E "$expect" "$out/$name.log")"
  fi
done
exit $((failures > 0))
