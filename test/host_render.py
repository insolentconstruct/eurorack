#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
#
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
# -----------------------------------------------------------------------------
#
# Host render tests: builds <module>/test/render.cc with the source files of
# the module's desktop test makefile, runs it in an empty directory and checks
# the SHA-256 of every file it writes (and of its standard output) against
# <module>/test/render.sha256.

import argparse
import concurrent.futures
import hashlib
import os
import re
import subprocess
import sys
import tempfile

MODULES = ['braids', 'marbles', 'peaks', 'plaits', 'rings', 'stages', 'tides2',
           'warps']

# Fixed flags, so that a module's tuneup branch renders with the same code
# generation as vanilla. x86-64 does not contract a * b + c without -mfma.
# -w: the vanilla sources are not warning-free on this compiler.
CXXFLAGS = ['-std=gnu++11', '-DTEST', '-O2', '-g', '-msse2', '-I.',
            '-include', 'cstdio', '-include', 'stdint.h', '-w']
SANITIZE = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
# Two fills for uninitialised locals: outputs that differ read them.
STACK_FILLS = [['-ftrivial-auto-var-init=zero'],
               ['-ftrivial-auto-var-init=pattern']]


def makefile_variables(module):
  text = open(os.path.join(module, 'test', 'makefile')).read()
  text = text.replace('\\\n', ' ')
  variables = {}
  for line in text.splitlines():
    match = re.match(r'^(\w+)\s*=\s*(.*)$', line)
    if match:
      variables[match.group(1)] = match.group(2).split()
  return variables


def source_files(module):
  variables = makefile_variables(module)
  sources = []
  for name in variables['CC_FILES']:
    if name.endswith('_test.cc'):
      name = 'render.cc'
      directory = os.path.join(module, 'test')
    else:
      directory = next(
          (p for p in variables['PACKAGES']
           if os.path.exists(os.path.join(p, name))), None)
      if directory is None:
        raise RuntimeError('%s: %s not found in PACKAGES' % (module, name))
    sources.append(os.path.join(directory, name))
  return sources


def build(module, build_dir, extra_flags, jobs):
  flags = CXXFLAGS + extra_flags
  objects = []
  commands = []
  for source in source_files(module):
    target = os.path.join(build_dir, os.path.basename(source)[:-3] + '.o')
    objects.append(target)
    commands.append(['g++', '-c'] + flags + [source, '-o', target])
  with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
    for result in pool.map(
        lambda c: subprocess.run(c, capture_output=True, text=True), commands):
      if result.returncode:
        sys.stderr.write(result.stderr)
        raise RuntimeError('%s: compilation failed' % module)
  binary = os.path.join(build_dir, module + '_render')
  subprocess.run(['g++'] + flags + ['-o', binary] + objects + ['-lm'],
                 check=True)
  return binary


def render(module, out_dir, extra_flags, jobs):
  build_dir = os.path.join(out_dir, 'build')
  run_dir = os.path.join(out_dir, 'run')
  os.makedirs(build_dir)
  os.makedirs(run_dir)
  binary = build(module, build_dir, extra_flags, jobs)
  env = dict(os.environ)
  env['ASAN_OPTIONS'] = 'detect_leaks=1'
  env['UBSAN_OPTIONS'] = 'print_stacktrace=1'
  result = subprocess.run([binary], cwd=run_dir, env=env,
                          capture_output=True)
  with open(os.path.join(run_dir, 'stdout.txt'), 'wb') as f:
    f.write(result.stdout)
  with open(os.path.join(out_dir, 'stderr.txt'), 'wb') as f:
    f.write(result.stderr)
  return result.returncode, result.stderr.decode(errors='replace'), run_dir


def digests(run_dir):
  lines = []
  for name in sorted(os.listdir(run_dir)):
    h = hashlib.sha256()
    with open(os.path.join(run_dir, name), 'rb') as f:
      for chunk in iter(lambda: f.read(1 << 20), b''):
        h.update(chunk)
    lines.append('%s  %s\n' % (h.hexdigest(), name))
  return ''.join(lines)


def check(module, out_dir, update, jobs):
  code, stderr, run_dir = render(module, out_dir, [], jobs)
  if code:
    return False, 'FAIL %s: exit %d\n%s' % (module, code, stderr[-2000:])
  actual = digests(run_dir)
  count = len(actual.splitlines())
  reference = os.path.join(module, 'test', 'render.sha256')
  if update:
    open(reference, 'w').write(actual)
    return True, 'UPDATED %s: %d files' % (module, count)
  expected = open(reference).read() if os.path.exists(reference) else ''
  if actual != expected:
    lines = ['FAIL %s: output differs from %s' % (module, reference)]
    for line in sorted(set(actual.splitlines()) ^ set(expected.splitlines())):
      lines.append('  %s %s' % ('+' if line in actual else '-', line))
    return False, '\n'.join(lines)
  return True, 'PASS %s: %d files' % (module, count)


def sanitize(module, out_dir, jobs):
  code, stderr, _ = render(module, out_dir, SANITIZE, jobs)
  reports = sorted(set(
      l for l in stderr.splitlines()
      if 'runtime error:' in l or 'ERROR: AddressSanitizer' in l
      or 'ERROR: LeakSanitizer' in l))
  outputs = []
  for i, fill in enumerate(STACK_FILLS):
    fill_code, _, run_dir = render(
        module, '%s-fill%d' % (out_dir, i), fill, jobs)
    if fill_code:
      reports.append('%s: exit %d' % (' '.join(fill), fill_code))
    outputs.append(set(digests(run_dir).splitlines()))
  changed = set(line.split()[-1] for line in outputs[0] ^ outputs[1])
  for name in sorted(changed):
    reports.append('uninitialised local changes %s' % name)
  ok = code == 0 and not reports
  lines = ['%s %s (sanitizers): exit %d, %d reports' % (
      'PASS' if ok else 'FAIL', module, code, len(reports))]
  lines += ['  ' + l for l in reports]
  return ok, '\n'.join(lines)


def main():
  parser = argparse.ArgumentParser(
      description='Check the desktop renders against render.sha256.')
  parser.add_argument('modules', nargs='*', default=MODULES)
  parser.add_argument('--update', action='store_true',
                      help='rewrite render.sha256 from this build')
  parser.add_argument('--sanitize', action='store_true',
                      help='run with AddressSanitizer and UBSan, and with '
                      'locals filled two ways, instead')
  parser.add_argument('--out', help='output directory (default: temporary)')
  parser.add_argument('-j', '--jobs', type=int, default=os.cpu_count())
  args = parser.parse_args()
  out = os.path.abspath(args.out or tempfile.mkdtemp(prefix='host-render-'))

  def run(module):
    try:
      if args.sanitize:
        return sanitize(module, os.path.join(out, module + '-sanitize'),
                        args.jobs)
      return check(module, os.path.join(out, module), args.update, args.jobs)
    except (RuntimeError, subprocess.CalledProcessError) as e:
      return False, 'FAIL %s: %s' % (module, e)

  # Modules render concurrently; results print in module order.
  ok = True
  with concurrent.futures.ThreadPoolExecutor(len(args.modules)) as pool:
    for passed, report in pool.map(run, args.modules):
      print(report, flush=True)
      ok = ok and passed
  sys.exit(0 if ok else 1)


if __name__ == '__main__':
  main()
