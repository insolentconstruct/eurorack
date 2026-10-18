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


"""Run a set of scenarios on one tier, in parallel processes, and merge.

Results never depend on job count or order: seeds come from names, every
scenario is run by the same function, and one serializer writes every file.
Wall-clock data lives in timing.json and is never merged.
"""

from concurrent.futures import ProcessPoolExecutor
import hashlib
import json
import multiprocessing
import os
from pathlib import Path
import platform
import subprocess
import time

import numpy as np

from repro import canon, rules, scenario as scenario_mod, stimulus, tiers

ROOT = Path(__file__).resolve().parents[1]
THREAD_VARS = ("OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS")


def scenario_seed(set_name, scenario_name, base_seed):
    """32-bit seed from names only, so it ignores job order. Never zero."""
    digest = hashlib.sha256(
        f"{set_name}\0{scenario_name}\0{base_seed}".encode()).digest()
    return int.from_bytes(digest[:4], "little") or 1


def _git_head():
    try:
        return subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True,
            text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def _guard_result(scn, render):
    flags = render.delay_mode
    entered = bool(flags.any())
    if scn.guard == "never" and entered:
        return dict(verdict=rules.INVALID, observed_delay_mode=True,
                    error="processor entered delay mode but the scenario "
                          "forbids it (SIZE knob fully clockwise?)")
    if scn.guard == "always" and not (len(flags) and flags[-1]):
        return dict(verdict=rules.INVALID, observed_delay_mode=entered,
                    error="processor never settled in delay mode")
    return dict(verdict=rules.PASS, observed_delay_mode=entered)


def run_one(task):
    """Worker: run one scenario and write its directory. Returns the entry
    that goes into the merged result."""
    started = time.monotonic()
    tier = tiers.get(task["tier"])
    tier.restore(task["tier_state"])
    scn = scenario_mod.load(task["scenario_path"])
    seed = task["seed"]
    stim = stimulus.build(scn, tier.sr, seed)
    stim_hash = stim.sha256()
    render = tier.run(scn, stim, seed)

    guard = _guard_result(scn, render)
    results = rules.evaluate(scn.rules, render.audio.astype(np.float64), render.sr)
    verdict = rules.overall([r["verdict"] for r in results] + [guard["verdict"]])
    peak = float(np.max(np.abs(render.audio))) if render.audio.size else 0.0
    observables = dict(
        frames=int(len(render.audio)), sr=render.sr, peak=peak,
        delay_mode_blocks=int(render.delay_mode.sum()),
        blocks=int(len(render.delay_mode)))
    verdict_doc = dict(scenario=scn.name, verdict=verdict, guard=guard,
                       rules=results)
    provenance = dict(
        scenario=scn.name, scenario_sha256=scn.sha256, seed=seed,
        set=task["set"], base_seed=task["base_seed"], git=task["git"],
        python=platform.python_version(), numpy=np.__version__,
        render_sha256=hashlib.sha256(
            np.ascontiguousarray(render.audio, "<f4").tobytes()).hexdigest(),
        **tier.provenance())

    out = Path(task["out_dir"])
    out.mkdir(parents=True, exist_ok=True)
    (out / "stimulus.sha256").write_text(stim_hash + "\n")
    np.save(out / "render.npy", np.ascontiguousarray(render.audio, "<f4"))
    (out / "observables.json").write_text(canon.dumps(observables))
    (out / "verdict.json").write_text(canon.dumps(verdict_doc))
    (out / "provenance.json").write_text(canon.dumps(provenance))
    (out / "timing.json").write_text(
        json.dumps({"seconds": round(time.monotonic() - started, 3)}) + "\n")
    return dict(scenario=scn.name, verdict=verdict, seed=seed,
                stimulus_sha256=stim_hash, guard=guard, rules=results,
                observables=observables)


def merge(set_name, tier_name, base_seed, entries):
    entries = sorted(entries, key=lambda e: e["scenario"])
    doc = dict(set=set_name, tier=tier_name, base_seed=base_seed,
               verdict=rules.overall([e["verdict"] for e in entries]),
               scenarios={e["scenario"]: e for e in entries})
    lines = ["scenario\trule\tbehaviours\tverdict"]
    for e in entries:
        lines.append(f"{e['scenario']}\t(guard)\t\t{e['guard']['verdict']}")
        for r in e["rules"]:
            lines.append(f"{e['scenario']}\t{r['rule']}\t"
                         f"{','.join(r['behaviours'])}\t{r['verdict']}")
    return canon.dumps(doc), "\n".join(lines) + "\n"


def run_set(set_name, tier_name, out_root, jobs, base_seed=0, only=None,
            root=ROOT):
    """Run, merge, write `merged.json` and `merged.tsv`; return (path, doc)."""
    scns = scenario_mod.load_set(root, set_name)
    scns = [s for s in scns if tier_name in s.tiers]
    if only:
        scns = [s for s in scns if s.name in only]
    if not scns:
        raise SystemExit(f"no scenarios in set {set_name!r} run on {tier_name}")
    for var in THREAD_VARS:
        os.environ[var] = "1"
    tier = tiers.get(tier_name)
    base = Path(out_root) / set_name / tier_name
    state = tier.prepare(Path(out_root) / "_build")
    head = _git_head()
    tasks = [dict(scenario_path=str(s.path), tier=tier_name, tier_state=state,
                  seed=scenario_seed(set_name, s.name, base_seed),
                  set=set_name, base_seed=base_seed, git=head,
                  out_dir=str(base / s.name)) for s in scns]
    if jobs == 1 or len(tasks) == 1:
        entries = [run_one(t) for t in tasks]
    else:
        ctx = multiprocessing.get_context("spawn")
        with ProcessPoolExecutor(min(jobs, len(tasks)), mp_context=ctx) as pool:
            entries = list(pool.map(run_one, tasks))
    text, tsv = merge(set_name, tier_name, base_seed, entries)
    base.mkdir(parents=True, exist_ok=True)
    (base / "merged.json").write_text(text)
    (base / "merged.tsv").write_text(tsv)
    return base, json.loads(text)
