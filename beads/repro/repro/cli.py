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


"""Command line: repro list | repro run SET --tier desktop."""

import argparse
from pathlib import Path
import sys

from repro import jobs as jobs_mod, runner, scenario as scenario_mod


def main(argv=None):
    parser = argparse.ArgumentParser(prog="repro", description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("list", help="list sets and their scenarios")
    run = sub.add_parser("run", help="run a set on one tier and merge")
    run.add_argument("set")
    run.add_argument("--tier", default="desktop")
    run.add_argument("--jobs", type=int, default=None,
                     help="worker processes (default: BEADS_JOBS, else "
                          "min(4, usable cores))")
    run.add_argument("--out", default="repro-out", help="output directory")
    run.add_argument("--seed", type=int, default=0, help="base seed")
    run.add_argument("--scenario", action="append",
                     help="run only this scenario (repeatable)")
    args = parser.parse_args(argv)

    if args.command == "list":
        import tomllib
        sets = tomllib.loads((runner.ROOT / "sets.toml").read_text())["sets"]
        for name, spec in sorted(sets.items()):
            print(name)
            for s in scenario_mod.load_set(runner.ROOT, name):
                print(f"  {s.name}\t{','.join(s.tiers)}")
        return 0

    jobs = jobs_mod.resolve(args.jobs)
    base, doc = runner.run_set(args.set, args.tier, Path(args.out), jobs,
                               args.seed, args.scenario)
    for name, entry in doc["scenarios"].items():
        print(f"{entry['verdict']:8} {name}")
        for r in entry["rules"]:
            print(f"    {r['verdict']:8} {r['rule']}")
        if entry["guard"]["verdict"] != "PASS":
            print(f"    {entry['guard']['verdict']:8} guard: "
                  f"{entry['guard'].get('error', '')}")
    print(f"merged: {base / 'merged.json'}")
    return 1 if doc["verdict"] in ("FAIL", "INVALID") else 0


if __name__ == "__main__":
    sys.exit(main())
