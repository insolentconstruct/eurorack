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


"""Desktop tier: the host build of the processor, fed block by block.

It runs x86 floats, so a render never matches firmware bit for bit; only
observables are compared across tiers.
"""

from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import numpy as np

from repro import jobs
from repro.scenario import OPTIONAL_PANEL
from repro.stimulus import BLOCK

SAMPLE_RATE = 48000
REPO = Path(__file__).resolve().parents[4]
MAKEFILE = "beads/repro/desktop/repro.mk"
BINARY = "scenario_host"
MEDIA = {"dirty_tape": 0, "clean_tape": 1, "vintage_digital": 2, "digital": 3}
SEED_BUTTON = {"off": 0, "on": 1, "locked": 2}


@dataclass
class Render:
    audio: np.ndarray       # (n, 2) float32 at sr
    sr: int
    delay_mode: np.ndarray  # (blocks,) uint8, 1 while in delay mode
    info: dict


class DesktopTier:
    name = "desktop"
    sr = SAMPLE_RATE

    def __init__(self):
        self.binary = None
        self.toolchain = ""

    def prepare(self, build_dir):
        """Build the driver once, before any worker starts. Returns the state
        a worker needs to rebuild this tier object."""
        build_dir = Path(build_dir).resolve()
        build_dir.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ)
        subprocess.run(
            ["make", "-s", f"-j{jobs.usable_cores()}", "-f", MAKEFILE,
             f"BUILD_ROOT={build_dir}/obj/", BINARY],
            cwd=REPO, check=True, env=env)
        built = REPO / BINARY
        target = build_dir / BINARY
        shutil.move(str(built), str(target))
        self.binary = target
        self.toolchain = subprocess.run(
            ["g++", "--version"], capture_output=True, text=True,
            check=True).stdout.splitlines()[0]
        return {"binary": str(target), "toolchain": self.toolchain}

    def restore(self, state):
        self.binary = Path(state["binary"])
        self.toolchain = state["toolchain"]

    def provenance(self):
        return {"tier": self.name, "sr": self.sr, "block": BLOCK,
                "toolchain": self.toolchain}

    def run(self, scenario, stimulus, seed):
        if any(e["kind"] == "cv" and e["channel"] == "assign"
               for e in scenario.events):
            raise RuntimeError(
                f"{scenario.name}: the desktop driver does not route the "
                "ASSIGN CV; use another tier for it")
        unsupported = sorted(set(scenario.panel) & set(OPTIONAL_PANEL))
        if unsupported:
            raise RuntimeError(
                f"{scenario.name}: the desktop driver has no panel setting "
                f"for {', '.join(unsupported)}; use another tier")
        panel = dict(scenario.panel)
        args = [f"{k}={v}" for k, v in (
            ("media", MEDIA[panel.pop("media")]),
            ("mono", int(panel.pop("mono"))),
            ("seed_button", SEED_BUTTON[panel.pop("seed_button")]),
            ("seed", seed)) + tuple(panel.items())]
        with tempfile.TemporaryDirectory(prefix="repro-") as tmp:
            stim = Path(tmp) / "stimulus.bin"
            stim.write_bytes(stimulus.pack())
            prefix = Path(tmp) / "out"
            done = subprocess.run(
                [str(self.binary), str(stim), str(prefix)] + args,
                capture_output=True, text=True,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
            if done.returncode != 0:
                raise RuntimeError(
                    f"{scenario.name}: driver exited {done.returncode}: "
                    f"{done.stderr.strip()[:2000]}")
            audio = np.fromfile(f"{prefix}.f32", "<f4").reshape(-1, 2)
            delay = np.fromfile(f"{prefix}.delay", np.uint8)
        return Render(audio, self.sr, delay, {"blocks": int(len(delay))})
