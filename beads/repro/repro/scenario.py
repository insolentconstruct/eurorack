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
import math
import re
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
    "media": "digital",
    "mono": False,
    "density": 0.5,
    "size": 0.5,
    "shape": 0.5,
    "time": 0.5,
    "pitch": 0.0,
    "size_att": 1.0,
    "shape_att": 1.0,
    "time_att": 1.0,
    "pitch_att": 1.0,
    "dry_wet": 1.0,
    "feedback": 0.0,
    "reverb": 0.0,
    "seed_button": "off",
}
SEED_BUTTON = ("off", "on", "locked")
# Volts per Bela level are nominal (level 0.2 = 1 V); this maps a level to the
# firmware's CV unit. Pitch is in semitones, so 1 V/oct gives 60 per level.
ANALOG_DEFAULTS = {
    "pitch": 60.0,
    "density": 1.0,
    "size": 1.0,
    "shape": 1.0,
    "assign": 1.0,
    "time": 1.0
}
# Above this knob value the processor enters delay mode (the pot only; CV on
# SIZE cannot get there).
DELAY_MODE_SIZE = 0.99

# Panel settings a scenario may add; a tier that cannot honour one refuses the
# scenario. Absent means the tier's own default.
OPTIONAL_PANEL = {
    "limiter": ("stock", "fast", "off"),
    "pitch_scale":
    ("off", "chromatic", "fifths", "major", "minor", "major-pentatonic",
     "minor-pentatonic", "pelog", "bhairav", "shri"),
    "assign_destination": ("feedback", "dry_wet", "reverb"),
    "inputs_patched": (True, False),
    "self_play": ("automatic", "on_request"),
    "reverb_style": ("stock", "clouds"),
    "delay_mode": ("auto", "never", "wide_detent"),
    "width_assigned": (True, False),
}
# Integer panel settings: name -> (lowest, highest). stereo_width is the saved
# byte: 0 is stock, 1..255 spans no displacement to full.
INT_PANEL = {"stereo_width": (0, 255)}
LAYER_KINDS = ("sine", "steps", "chirp", "cyclic_chirp", "noise")

NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


class ScenarioError(ValueError):
  pass


def series(spec):
  """A list of times, written out or as {start, step, count}."""
  if isinstance(spec, dict):
    try:
      return [spec["start"] + spec["step"] * i for i in range(spec["count"])]
    except (KeyError, TypeError) as exc:
      raise ScenarioError(f"bad time series {spec!r}: {exc!r}") from exc
  return list(spec)


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
  _check(
      isinstance(name, str) and NAME.match(name), path,
      "name must be letters, digits, '.', '_' or '-' (one path component)")
  allowed = {
      "name", "description", "seconds", "tiers", "behaviours", "panel", "guard",
      "stimulus", "analog", "events", "rules"
  }
  _check(not set(data) - allowed, name,
         f"unknown keys {sorted(set(data) - allowed)}")
  seconds = float(data.get("seconds", 0))
  _check(seconds > 0, name, "seconds must be positive")
  tiers = tuple(data.get("tiers", ()))
  _check(tiers and all(t in TIERS for t in tiers), name,
         f"tiers must be a non-empty subset of {TIERS}")

  panel = dict(PANEL_DEFAULTS)
  extra = (set(data.get("panel", {})) - set(PANEL_DEFAULTS) -
           set(OPTIONAL_PANEL) - set(INT_PANEL))
  _check(not extra, name, f"unknown panel keys {sorted(extra)}")
  panel.update(data.get("panel", {}))
  for key, allowed in OPTIONAL_PANEL.items():
    _check(key not in panel or panel[key] in allowed, name,
           f"panel.{key} must be one of {allowed}")
  for key, (low, high) in INT_PANEL.items():
    _check(
        key not in panel or
        (isinstance(panel[key], int) and not isinstance(panel[key], bool) and
         low <= panel[key] <= high), name,
        f"panel.{key} must be an integer in {low}..{high}")
  if panel.get("width_assigned"):
    _check(
        panel.get("stereo_width", 0) > 0, name,
        "panel.width_assigned needs panel.stereo_width above 0")
    _check(
        panel.get("assign_destination", "feedback") == "feedback", name,
        "panel.width_assigned replaces the ASSIGN destination; leave "
        "assign_destination unset")
  _check(panel["media"] in MEDIA, name, f"media must be one of {MEDIA}")
  _check(panel["seed_button"] in SEED_BUTTON, name,
         f"seed_button must be one of {SEED_BUTTON}")

  guard = data.get("guard", {}).get("delay_mode", "never")
  _check(guard in GUARDS, name, f"guard.delay_mode must be one of {GUARDS}")
  if guard == "never":
    _check(
        panel["size"] <= DELAY_MODE_SIZE, name,
        "panel.size is fully clockwise, which is delay mode; lower it "
        "or set guard.delay_mode = \"always\"")
  elif guard == "always":
    _check(
        panel["size"] > DELAY_MODE_SIZE, name,
        "guard.delay_mode = \"always\" needs panel.size above "
        f"{DELAY_MODE_SIZE}")

  generator = dict(data.get("stimulus", {}))
  _check(
      generator.get("generator") in ("sine", "chirps", "silence", "layers"),
      name, "stimulus.generator must be sine, chirps, silence or layers")
  if generator["generator"] == "layers":
    layers = generator.get("layers", ())
    _check(layers, name, "generator \"layers\" needs [[stimulus.layers]]")
    for layer in layers:
      _check(
          layer.get("kind") in LAYER_KINDS, name,
          f"layer kind {layer.get('kind')!r} must be one of {LAYER_KINDS}")
      _check(isinstance(layer.get("amp"), (int, float)), name,
             "a layer needs a numeric amp")
      _check(
          layer.get("channel", "both") in (0, 1, "both"), name,
          "layer channel must be 0, 1 or \"both\"")
      _check(
          not (layer.get("stereo") and layer.get("channel", "both") != "both"),
          name, "a stereo noise layer plays on both channels")
      _check(
          not layer.get("phase_s") or layer.get("kind") != "sine" or
          layer.get("fade_s", 0.01) == 0, name,
          "a sine with phase_s is already running: set fade_s = 0")
      for key in ("seg_s", "dur_s", "every", "period_s"):
        _check(
            key not in layer or (isinstance(layer[key],
                                            (int, float)) and layer[key] > 0),
            name, f"layer {key} must be a positive number")
      _check(layer.get("count", 1) >= 1, name, "layer count must be at least 1")

  events = list(data.get("events", ()))
  for ev in events:
    kind = ev.get("kind")
    _check(kind in EVENT_KINDS, name, f"event kind {kind!r} unknown")
    channel = ev.get("channel")
    valid = CV_CHANNELS if kind == "cv" else GATE_CHANNELS
    _check(channel in valid, name,
           f"{kind} event channel {channel!r} must be one of {valid}")
    times = series(ev.get("times", ())) if kind == "trigger" else [
        ev.get(k) for k in (("t0", "t1") if kind == "hold" else ("t", ))
    ]
    _check(
        times and all(
            isinstance(t, (int,
                           float)) and math.isfinite(t) and 0 <= t <= seconds
            for t in times), name,
        f"{kind} event times must lie within 0..{seconds} s")
    if kind == "hold":
      _check(ev["t0"] <= ev["t1"], name, "hold needs t0 <= t1")
    if kind == "cv":
      _check(isinstance(ev.get("level"), (int, float)), name,
             "cv event needs a numeric level")
      _check(ev.get("ramp", 0) >= 0, name, "cv ramp must not be negative")

  rules = list(data.get("rules", ()))
  _check(rules, name, "at least one rule is required")
  analog = dict(ANALOG_DEFAULTS)
  analog.update(data.get("analog", {}))
  return Scenario(name=name,
                  path=Path(path),
                  description=data.get("description", ""),
                  seconds=seconds,
                  tiers=tiers,
                  behaviours=tuple(data.get("behaviours", ())),
                  panel=panel,
                  guard=guard,
                  generator=generator,
                  events=events,
                  rules=rules,
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
    loaded = load(path)
    if loaded.name in found:
      raise ScenarioError(f"scenario name {loaded.name!r} is used by "
                          f"{found[loaded.name].name} and {path.name}")
    found[loaded.name] = path
  out = []
  for name in sets[set_name]["scenarios"]:
    if name not in found:
      raise ScenarioError(f"set {set_name}: no scenario file for {name}")
    out.append(load(found[name]))
  return out
