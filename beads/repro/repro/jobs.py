# Copyright 2026 Insolent Construct.
#
# Author: Insolent Construct (insolentconstruct@gmail.com)
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


"""How many worker processes to use."""

import os
from pathlib import Path

DEFAULT_CAP = 4


def cgroup_cpus(root="/sys/fs/cgroup"):
    """CPU limit from the cgroup (v2 cpu.max, then v1 cfs quota), or None."""
    root = Path(root)
    try:
        quota, period = (root / "cpu.max").read_text().split()
        if quota != "max":
            return max(1, int(quota) // int(period))
    except (OSError, ValueError):
        pass
    try:
        quota = int((root / "cpu" / "cpu.cfs_quota_us").read_text())
        period = int((root / "cpu" / "cpu.cfs_period_us").read_text())
        if quota > 0:
            return max(1, quota // period)
    except (OSError, ValueError):
        pass
    return None


def usable_cores(cgroup_root="/sys/fs/cgroup"):
    try:
        cores = len(os.sched_getaffinity(0))
    except AttributeError:
        cores = os.cpu_count() or 1
    limit = cgroup_cpus(cgroup_root)
    return max(1, min(cores, limit) if limit else cores)


def resolve(cli_value=None, environ=None, cgroup_root="/sys/fs/cgroup"):
    """--jobs beats BEADS_JOBS beats min(4, usable cores)."""
    environ = os.environ if environ is None else environ
    for source, value in (("--jobs", cli_value),
                          ("BEADS_JOBS", environ.get("BEADS_JOBS"))):
        if value not in (None, ""):
            try:
                jobs = int(value)
            except ValueError:
                raise SystemExit(f"{source} must be a whole number, got {value!r}")
            if jobs < 1:
                raise SystemExit(f"{source} must be at least 1, got {jobs}")
            return jobs
    return min(DEFAULT_CAP, usable_cores(cgroup_root))
