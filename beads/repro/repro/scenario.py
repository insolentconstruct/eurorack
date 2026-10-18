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


"""Scenario files: TOML in, a checked Scenario out."""

from dataclasses import dataclass, field
import hashlib
from pathlib import Path
import tomllib

TIERS = ("desktop", "emulator", "seed3", "module")
CV_CHANNELS = ("pitch", "density", "size", "shape", "assign", "time")
GATE_CHANNELS = ("seed", "freeze")
EVENT_KINDS = ("trigger", "hold", "cv")
GUARDS = ("never", "always", "any")
MEDIA = ("dirty_tape", "clean_tape", "vintage_digital", "digital")

# Same names as the desktop driver's keys; values are the P2 bench panel:
# every knob at noon, attenuverters fully clockwise, wet.
PANEL_DEFAULTS = {
    "media": "digital", "mono": False,
    "density": 0.5, "size": 0.5, "shape": 0.5, "time": 0.5, "pitch": 0.0,
    "size_att": 1.0, "shape_att": 1.0, "time_att": 1.0, "pitch_att": 1.0,
    "dry_wet": 1.0, "feedback": 0.0, "reverb": 0.0, "seed_button": "off",
}
SEED_BUTTON = ("off", "on", "locked")
# Volts per Bela level are nominal (level 0.2 = 1 V); this maps a level to the
# firmware's CV unit. Pitch is in semitones, so 1 V/oct gives 60 per level.
ANALOG_DEFAULTS = {"pitch": 60.0, "density": 1.0, "size": 1.0, "shape": 1.0,
                   "assign": 1.0, "time": 1.0}
# Above this knob value the processor enters delay mode (the pot only; CV on
# SIZE cannot get there).
DELAY_MODE_SIZE = 0.99


class ScenarioError(ValueError):
    pass


@dataclass
class Scenario:
    name: str
    path: Path
    description: str
    seconds: float
    tiers: tuple
    behaviours: tuple
    panel: dict
    guard: str
    generator: dict
    events: list
    rules: list
    analog: dict
    sha256: str = field(default="")


def _check(cond, name, msg):
    if not cond:
        raise ScenarioError(f"{name}: {msg}")


def parse(text, path="<memory>"):
    data = tomllib.loads(text)
    name = data.get("name", "")
    _check(isinstance(name, str) and name, path, "name is required")
    allowed = {"name", "description", "seconds", "tiers", "behaviours",
               "panel", "guard", "stimulus", "analog", "events", "rules"}
    _check(not set(data) - allowed, name,
           f"unknown keys {sorted(set(data) - allowed)}")
    seconds = float(data.get("seconds", 0))
    _check(seconds > 0, name, "seconds must be positive")
    tiers = tuple(data.get("tiers", ()))
    _check(tiers and all(t in TIERS for t in tiers), name,
           f"tiers must be a non-empty subset of {TIERS}")

    panel = dict(PANEL_DEFAULTS)
    extra = set(data.get("panel", {})) - set(PANEL_DEFAULTS)
    _check(not extra, name, f"unknown panel keys {sorted(extra)}")
    panel.update(data.get("panel", {}))
    _check(panel["media"] in MEDIA, name, f"media must be one of {MEDIA}")
    _check(panel["seed_button"] in SEED_BUTTON, name,
           f"seed_button must be one of {SEED_BUTTON}")

    guard = data.get("guard", {}).get("delay_mode", "never")
    _check(guard in GUARDS, name, f"guard.delay_mode must be one of {GUARDS}")
    if guard == "never":
        _check(panel["size"] <= DELAY_MODE_SIZE, name,
               "panel.size is fully clockwise, which is delay mode; lower it "
               "or set guard.delay_mode = \"always\"")
    elif guard == "always":
        _check(panel["size"] > DELAY_MODE_SIZE, name,
               "guard.delay_mode = \"always\" needs panel.size above "
               f"{DELAY_MODE_SIZE}")

    generator = dict(data.get("stimulus", {}))
    _check(generator.get("generator") in ("sine", "chirps", "silence"), name,
           "stimulus.generator must be sine, chirps or silence")

    events = list(data.get("events", ()))
    for ev in events:
        kind = ev.get("kind")
        _check(kind in EVENT_KINDS, name, f"event kind {kind!r} unknown")
        channel = ev.get("channel")
        valid = CV_CHANNELS if kind == "cv" else GATE_CHANNELS
        _check(channel in valid, name,
               f"{kind} event channel {channel!r} must be one of {valid}")

    rules = list(data.get("rules", ()))
    _check(rules, name, "at least one rule is required")
    analog = dict(ANALOG_DEFAULTS)
    analog.update(data.get("analog", {}))
    return Scenario(
        name=name, path=Path(path), description=data.get("description", ""),
        seconds=seconds, tiers=tiers,
        behaviours=tuple(data.get("behaviours", ())), panel=panel,
        guard=guard, generator=generator, events=events, rules=rules,
        analog=analog)


def load(path):
    path = Path(path)
    raw = path.read_bytes()
    scenario = parse(raw.decode("utf-8"), path)
    scenario.sha256 = hashlib.sha256(raw).hexdigest()
    return scenario


def load_set(root, set_name):
    """Scenarios of one set from sets.toml, in file order."""
    root = Path(root)
    sets = tomllib.loads((root / "sets.toml").read_text())["sets"]
    if set_name not in sets:
        raise ScenarioError(f"no set {set_name!r}; have {sorted(sets)}")
    found = {}
    for path in sorted((root / "scenarios").rglob("*.toml")):
        found[load(path).name] = path
    out = []
    for name in sets[set_name]["scenarios"]:
        if name not in found:
            raise ScenarioError(f"set {set_name}: no scenario file for {name}")
        out.append(load(found[name]))
    return out
