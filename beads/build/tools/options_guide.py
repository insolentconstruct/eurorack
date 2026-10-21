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

"""Generate options guides from options_tables TSV; Python 3 stdlib only."""

import argparse
import csv
import io
import math
from pathlib import Path
import sys
import textwrap
from html import escape

KNOBS = ()


def load_panel_geometry():
    """Read panel_geometry.tsv: x/y centre and drawn size (knob cap or real)."""
    rows = []
    path = Path(__file__).with_name("panel_geometry.tsv")
    for line in path.read_text().splitlines():
        if line.startswith("#") or line.startswith("letter\t"):
            continue
        letter, kind, name, x, y, d, cap = line.split("\t")
        if kind == "outline":
            size = tuple(float(v) for v in d.split("x"))
            continue
        rows.append(
            dict(
                letter=letter,
                kind=kind,
                name=name,
                x=float(x),
                y=float(y),
                d=float(cap or d),
                key=f"{letter}:{kind}:{name}",
            )
        )
    return rows, size


PANEL_GEOMETRY, PANEL_SIZE_MM = load_panel_geometry()
PANEL_CROP_MM = (11.0, 89.0)
# Controls the options pages use, as (manual letter, kind).
PANEL_USED = {
    ("A", "button"), ("A", "led"), ("B", "button"), ("C", "button"),
    ("J", "knob"), ("K", "knob"), ("L", "knob"), ("J", "led"),
    ("K", "led"), ("L", "led"), ("M", "button"),
}
# (geometry key, label lines, side): names follow the manual.
PANEL_LABELS = (
    ("B:button:FREEZE", ("FREEZE", "[B]"), "left"),
    ("C:button:SEED", ("SEED", "[C]"), "right"),
    ("A:button:quality selector", ("[A]",), "below"),
    ("J:knob:FEEDBACK", ("FEEDBACK", "[J]"), "left"),
    ("M:button:CV assign", ("CV assign", "[M]"), "left"),
    ("K:knob:DRY/WET", ("DRY/WET", "[K]"), "right"),
    ("L:knob:REVERB", ("REVERB", "[L]"), "right"),
)
CONSTANTS = {}
COLOURS = {
    "green": "#247443",
    "yellow": "#a87900",
    "red": "#bd3030",
    "meter": "#777",
    "unchanged": "#777",
}
ATTRIBUTION = (
    "Panel layout from the Mutable Instruments Beads panel file, "
    "geometry only (CC BY-SA 3.0)."
)
STEPS = [
    "Open: hold the CV-assign button [M] {hold}, release.",
    "Change page: tap [M]. Pages 1-4 pulse FEEDBACK, DRY/WET, REVERB, "
    "then all three.",
    "[A] shows each stored value in turn. Turn a knob to change it; it "
    "saves by itself.",
    "Close: hold [M] {hold} or press another button. With Scale selected, "
    "SEED taps step the scale.",
]
NOTES = [
    "Move a knob a little before it responds. Wait a moment before "
    "powering off.",
]
SHORT = {
    "Clouds-style": "Clouds",
    "Auto (Stock)": "Auto",
    "Wide/smooth filter": "Wide",
    "Granular (Stock)": "Granular",
    "Fast release": "Fast",
    "Width (wider →)": "wider →",
    "Beads (Stock)": "Beads",
    "Automatic (Stock)": "Auto",
}
# Stereo width is set with [A]+DRY/WET, outside the pages.
WIDTH_ROWS = [
    dict(knob="[A] + DRY/WET", setting="Stereo width", value=value,
         colour=colour, pulse="steady")
    for value, colour in (("Stock", "green"), ("Width (wider →)", "meter"))
]


def read_tsv(data):
    global KNOBS, CONSTANTS
    data, constants = data.split("\nconstant\tname\tvalue\n")
    CONSTANTS = {
        name: float(value)
        for _, name, value in csv.reader(
            io.StringIO(constants), delimiter="\t"
        )
    }
    rows = list(csv.DictReader(io.StringIO(data), delimiter="\t"))
    KNOBS = tuple(dict.fromkeys(row["knob"] for row in rows))
    assert rows and list(rows[0]) == [
        "page",
        "LED",
        "knob",
        "setting",
        "value",
        "colour",
        "pulse",
    ]
    pages = {}
    for row in rows:
        pages.setdefault(row["page"], {}).setdefault(row["knob"], []).append(
            row
        )
    assert list(pages) == ["1", "2", "3", "4"]
    assert all(set(page) == set(KNOBS) for page in pages.values())
    return rows, pages


def zones(rows):
    if rows[0]["setting"] == "Pitch scale":
        values = [row["value"] for row in rows[:2]] + ["Scale"]
    else:
        values = [SHORT.get(r["value"], r["value"]) for r in rows]
    bands = zone_bands(rows)
    starts = [0] + [high for low, high in bands]
    ends = [low for low, high in bands] + [1]
    return list(zip(starts, ends, values))


def zone_bands(rows):
    setting = rows[0]["setting"]
    if setting == "Stereo width":
        return [(CONSTANTS["kWidthStockThreshold"],
                 CONSTANTS["kWidthEnabledThreshold"])]
    if len(rows) == 2:
        return [(CONSTANTS["kOptionsHalfLow"],
                 CONSTANTS["kOptionsHalfHigh"])]
    band = CONSTANTS["kOptionsZoneBand"]
    return [(CONSTANTS[name] - band, CONSTANTS[name] + band)
            for name in ("kOptionsZoneLow", "kOptionsZoneHigh")]


def indication(row):
    if row["colour"] == "unchanged":
        return "—"
    if row["colour"] == "meter":
        return "green → yellow → red"
    colour = row["colour"]
    pulse = row["pulse"]
    if pulse.startswith("slow"):
        pulse = "slow blink"
    elif pulse.startswith("fast"):
        pulse = "fast blink"
    elif pulse.startswith("2 "):
        pulse = "two pulses"
    return colour + "; " + pulse


DETAILS = [
    "Hold the CV-assign button [M] for three seconds, with the mix knobs still, then "
    "release. Tap [M] to move between the pages; Beads remembers the last "
    "page until it is powered off. Hold [M] again, or press another "
    "button, to close.",
    "When a page opens, the quality LED [A] shows each stored value in "
    "turn, left to right, for about two seconds each, with that knob's mix "
    "LED lit. Turning a knob, or tapping SEED to step the scale, ends this.",
    "Move a knob far enough to take it over; small bumps before that are "
    "ignored. Pitch scale, Delay mode, Attacks and Limiter use the knob's "
    "three thirds; the other settings use its two halves. Near a boundary, "
    "turn a little past it to switch.",
    "Settings save by themselves about a second after you stop turning. "
    "Closing keeps your changes, and there is no timeout or cancel. The "
    "page LED flashes when you open, close, and when a change is saved.",
    "After the stored values, [A] shows the page's first setting, then "
    "the last knob you took over. The first value of every setting is steady green. Other "
    "values follow in yellow and red, then as slow blinks, then as fast "
    "blinks. Raag Shri is two yellow pulses and a pause. Unused knobs "
    "leave the LED alone.",
    "Scale: on page 1, tap SEED [C] to step through the eight scales "
    "instead of closing; a long press does not step, and the taps do not "
    "play grains. The scale snaps the pitch of new grains, including "
    "wavetable synth grains. Grains already playing, the continuous "
    "wavetable oscillator and delay pitch keep their pitch. The root is "
    "the unshifted pitch, and +1 V is still one octave. Pelog and the "
    "raags are microtonal.",
    "Attacks: Snap starts grains on the hits in the recording; Snap and "
    "trigger also starts a grain on each hit.",
    "Delay mode: Auto (Stock) switches to delay with SIZE fully CW; "
    "Disabled stays granular. Wide/smooth filter keeps the switch and adds a "
    "neutral, unfiltered zone around the middle of the delay filter "
    "(SIZE attenurandomizer plus SIZE CV), with no resonant boost. The "
    "wavetable synth is unaffected.",
    "Clouds-style changes the reverb character and, until a diffusion "
    "amount is set with [A]+REVERB, adds grain diffusion. Switching briefly "
    "fades the sound and clears the tail.",
    "Frozen feedback Evolving runs frozen grains through a slowly moving "
    "all-pass loop; FEEDBACK sets how far they drift.",
    "Processor Spectral: Clouds' spectral processing replaces the grains. "
    "TIME picks the stored spectrum, SIZE warps it, SHAPE quantizes it and "
    "PITCH shifts it (following the pitch scale). DENSITY sets how quickly "
    "the spectrum is renewed, and scrambles its phases towards either end. "
    "Holding SEED glitches it; FREEZE holds it. There is no grain "
    "diffusion, delay mode or wavetable synth. A new processor takes over "
    "when the page closes, clearing the recording; a frozen buffer saved "
    "to flash comes back with Granular.",
    "Synth Plaits plays six Plaits voices instead of the wavetable "
    "synth: FEEDBACK picks one of 24 models, TIME, SIZE and SHAPE set "
    "HARMONICS, TIMBRE and MORPH, DRY/WET sets the decay and FREEZE "
    "sustains. With DENSITY at noon the last voice drones until SEED plays "
    "a note.",
    "Synth start Manual: the synth never starts by itself. It needs both "
    "audio inputs unpatched and FREEZE off: hold the quality button [A] for "
    "about a second and a half to start or stop it. Patching an input "
    "stops it once FREEZE is off.",
    "Limiter Off lets loud peaks clip.",
    "Stereo width is not on the pages: hold [A] and turn DRY/WET, then "
    "release [A] to save. Fully CCW is Stock; turn clockwise to spread the "
    "grains wider, up to a full random spread fully CW. Only new grains "
    "are affected. When width is on, short presses of [M] add WIDTH after "
    "the three mix destinations of the assignable CV input (7); all three "
    "mix LEDs pulse together for WIDTH.",
]


# (title, text, shorter text for the printable guide)
GESTURES = [
    (
        "Monophonic grains",
        "Hold the quality button [A], then press SEED [C] to toggle. Best "
        "with the first two SHAPE settings (square and decay). The [A] LED "
        "is dimmed while on.",
        "Hold [A], press SEED [C]. Suits square and decay shapes.",
    ),
    (
        "Buffer trim",
        "Hold [A], then press FREEZE [B] to start a fresh recording window. "
        "The next freeze keeps only what was recorded since.",
        "Hold [A], press FREEZE [B]: the next freeze keeps only what follows.",
    ),
    (
        "Stereo width",
        "Hold [A] and turn DRY/WET; fully CCW is Stock. The [A] LED shows "
        "the width as a meter; releasing [A] saves it. Once on, [M] can "
        "route CV to it (all three mix LEDs pulsing).",
        "Hold [A], turn DRY/WET; fully CCW is Stock. [M] can route CV to it.",
    ),
    (
        "Grain diffusion",
        "Hold [A] and turn REVERB; fully CCW is none. Once set, [M] can "
        "route CV to it (REVERB LED pulsing).",
        "Hold [A], turn REVERB; fully CCW is none. [M] can route CV to it.",
    ),
    (
        "Wavetable synth (Manual)",
        "With Synth start on Manual, both inputs unplugged and FREEZE off, "
        "hold [A] for about a second and a half to start or stop it.",
        "Synth start Manual: no inputs, FREEZE off, hold [A] to start or stop.",
    ),
]
WARNING = (
    "Don't hold SEED while powering on: it resets all settings to defaults "
    "(calibration is kept)."
)


def instructions(lines):
    hold = CONSTANTS["kOptionsHoldTime"] / 1000
    assert hold == 3, "Update the spoken hold time"
    return [line.format(hold="3 seconds") for line in lines]


def page_leds(knobs):
    led = next(iter(knobs.values()))[0]["LED"]
    return led + (" LEDs" if led == "all three" else " LED")


def markdown(rows, pages):
    out = [
        "<!-- Author: Insolent Construct. Generated by options_guide.py. -->",
        "# Drizzlet hidden options",
        "",
        "**TL;DR:** Open options with the CV-assign button [M], below the FEEDBACK [J] knob, choose a page, then turn "
        "a mix knob.",
        "",
        "[Printable guide](options-guide.svg) · [Phone "
        "guide](options-guide-mobile.svg)",
        "",
        "## Steps",
        "",
    ]
    out += [f"{i}. {step}" for i, step in enumerate(instructions(STEPS), 1)]
    out += [
        "",
        *instructions(NOTES),
        "",
        "The quality LED [A] shows the setting value: steady, slow blink, "
        "fast blink or two pulses; "
        "colours are named below.",
    ]
    for page, knobs in pages.items():
        out += [
            "",
            f"## Page {page} — {page_leds(knobs)} pulsing",
            "",
            "| Knob | Setting | Values left to right | [A] LED per value |",
            "|---|---|---|---|",
        ]
        for knob, values in knobs.items():
            setting = values[0]["setting"]
            shown = values[:2] if setting == "Pitch scale" else values
            names = [r["value"] for r in shown]
            leds = [indication(r) for r in shown]
            if setting == "Not used yet":
                names, leds = ["—"], ["—"]
                setting = "Not used yet"
            if setting == "Pitch scale":
                names += ["Scale"]
                leds += ["see scale list"]
            out.append(
                f"| {knob} | {setting} | {' / '.join(names)} | "
                f"{' / '.join(leds)} |"
            )
        if page == "1":
            out += ["", "Scale (tap SEED to step through):", ""]
            out += [
                f"- {r['value']}: {indication(r)}"
                for r in knobs["FEEDBACK"][2:]
            ]
    out += ["", "## Other gestures", ""]
    out += [f"- **{title}:** {text}" for title, text, _ in GESTURES]
    out += ["", f"**Warning:** {WARNING}"]
    out += ["", "## Details", ""]
    out += [paragraph + "\n" for paragraph in DETAILS]
    return "\n".join(out)


class Drawing:
    def __init__(self):
        self.items = []

    def text(
        self,
        x,
        y,
        value,
        size=15,
        weight="normal",
        colour="#222",
        anchor="start",
    ):
        self.items.append(
            (
                f'<text x="{x}" y="{y}" font-size="{size}" '
                f'font-weight="{weight}" fill="{colour}" '
                f'text-anchor="{anchor}">{escape(value)}</text>'
            )
        )

    def lines(self, x, y, value, columns):
        for line in textwrap.wrap(value, columns):
            self.text(x, y, line)
            y += 21
        return y

    def circle(self, x, y, r, colour="white", pattern=""):
        dash = (
            ' stroke-dasharray="6 4"'
            if pattern.startswith("slow")
            else (
                ' stroke-dasharray="1 3"' if pattern.startswith("fast") else ""
            )
        )
        self.items.append(
            f'<circle cx="{x}" cy="{y}" r="{r}" fill="{colour}" '
            f'stroke="#333" stroke-width="2"{dash}/>'
        )
        if pattern.startswith("2 "):
            self.circle(x - 4, y, 1, "#222")
            self.circle(x + 4, y, 1, "#222")

    def arc(self, x, y, a, b, band=False):
        def point(t):
            angle = math.radians(135 + 270 * t)
            return x + 37 * math.cos(angle), y + 37 * math.sin(angle)

        x1, y1 = point(a if band else a + 0.012)
        x2, y2 = point(b if band else b - 0.012)
        self.items.append(
            (
                f'<path d="M {x1:.3f} {y1:.3f} A 37 37 0 '
                f'{int(b - a > .6667)} 1 {x2:.3f} {y2:.3f}" '
                f'fill="none" stroke="{"#aaa" if band else "#555"}" '
                f'stroke-width="{8 if band else 3}"/>'
            )
        )

    def card(self, x, y, width, rows, mobile_card=False):
        if rows[0]["setting"] == "Not used yet":
            self.text(
                x, y, rows[0]["knob"] + ": Not used yet", colour="#737373"
            )
            return 26
        self.text(x, y, f'{rows[0]["knob"]}: {rows[0]["setting"]}', 17, "bold")
        cx, cy = x + width / 2, y + 72
        self.circle(cx, cy, 22)
        self.items.append(
            f'<path d="M {cx} {cy} v -16" stroke="#222" stroke-width="2"/>'
        )
        labels = []
        for a, b, value in zones(rows):
            self.arc(cx, cy, a, b)
            angle = math.radians(135 + 270 * ((a + b) / 2))
            lx, ly = cx + 53 * math.cos(angle), cy + 53 * math.sin(angle)
            anchor = (
                "end"
                if lx < cx - 15
                else "start" if lx > cx + 15 else "middle"
            )
            self.text(round(lx, 2), round(ly + 5, 2), value, 15, anchor=anchor)
            labels.append(ly + 5)
        for a, b in zone_bands(rows):
            self.arc(cx, cy, a, b, True)
        # Leave room for a zone label low on the dial (width's Stock).
        base = max(y + 117, y + math.ceil(max(labels) - y) + 26)
        pitch = rows[0]["setting"] == "Pitch scale"
        for index, row in enumerate(rows):
            if pitch and index == 2:
                self.text(
                    x, base, "Scale (tap SEED to step through):", 15, "bold"
                )
                base += 26
            self.circle(
                x + 9, base - 5, 8, COLOURS[row["colour"]], row["pulse"]
            )
            label = row["value"]
            if mobile_card:
                self.text(x + 25, base, label, 15)
                self.text(
                    x + 25, base + 19, indication(row), 15, colour="#555"
                )
            else:
                self.text(x + 25, base, label, 15)
                self.text(
                    x + width - 3,
                    base,
                    indication(row).replace(" blink", ""),
                    15,
                    colour="#555",
                    anchor="end",
                )
            base += 44 if mobile_card else 21
        return base - y

    def gestures(self, x, y, columns, short=False, gap=10):
        """Gesture list in one column; the printable uses the short texts
        and leaves stereo width to its card."""
        self.text(x, y, "OTHER GESTURES", 20, "bold")
        y += 30
        for title, text, brief in GESTURES:
            if short and title == "Stereo width":
                continue
            self.text(x, y, title, 16, "bold")
            y = self.lines(x, y + 22, brief if short else text, columns) + gap
        return y

    def warning(self, x, y, width, columns):
        lines = textwrap.wrap(WARNING, columns)
        box = 21 * len(lines) + 12
        self.items.append(
            f'<rect x="{x - 8}" y="{y}" width="{width + 16}" '
            f'height="{box}" rx="6" fill="#fdf0f0" stroke="{COLOURS["red"]}" '
            'stroke-width="3"/>'
        )
        for i, line in enumerate(lines):
            self.text(x, y + 22 + 21 * i, line, 15, "bold", COLOURS["red"])
        return y + box

    def marker(self, x, y, led):
        """Mix LEDs on the title line: filled is pulsing."""
        for knob, label, width in zip(KNOBS, ("F", "D/W", "R"), (10, 31, 11)):
            self.text(x, y, label, 15)
            lit = led in (knob, "all three")
            self.circle(x + width + 11, y - 5, 7, "#222" if lit else "white")
            x += width + 40
            if lit and led != "all three":
                self.text(x - 16, y, "pulsing", 15, colour="#555")
                x += 56
        if led == "all three":
            self.text(x - 16, y, "pulsing", 15, colour="#555")

    def panel(self, x, y, scale):
        """To-scale top of the panel from panel_geometry.tsv; returns height.

        scale is pixels per PostScript point of the panel file (1 mm is 2.83
        points). Controls the options pages use are drawn heavy and labelled
        by name; the others stay light with only their manual letter.
        """
        mm = scale * 72 / 25.4
        x0 = x + 85
        top, bottom = PANEL_CROP_MM
        y0 = y + 24
        width = PANEL_SIZE_MM[0] * mm
        height = (bottom - top) * mm

        def at(item):
            return x0 + item["x"] * mm, y0 + (item["y"] - top) * mm

        self.text(x, y, "FIND THE CONTROLS", 17, "bold")
        self.text(x + 205, y, "to scale", 15, colour="#555")
        self.items.append(
            f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{width:.1f}" '
            f'height="{height:.1f}" fill="#f6f5f0"/>'
        )
        for yy in (y0, y0 + height):
            self.items.append(
                f'<line x1="{x0 - 4:.1f}" y1="{yy:.1f}" '
                f'x2="{x0 + width + 4:.1f}" y2="{yy:.1f}" stroke="#aaa" '
                'stroke-width="1.5" stroke-dasharray="5 4"/>'
            )
        for xx in (x0, x0 + width):
            self.items.append(
                f'<line x1="{xx:.1f}" y1="{y0:.1f}" x2="{xx:.1f}" '
                f'y2="{y0 + height:.1f}" stroke="#aaa" stroke-width="2"/>'
            )
        for item in PANEL_GEOMETRY:
            if not top <= item["y"] <= bottom or item["kind"] in (
                "jack",
                "mount",
            ):
                continue
            cx, cy = at(item)
            r = item["d"] * mm / 2
            used = (item["letter"], item["kind"]) in PANEL_USED
            stroke = "#222" if used else "#aaa"
            weight = 2 if used else 1.5
            if item["kind"] == "button":
                self.items.append(
                    f'<rect x="{cx - r:.1f}" y="{cy - r:.1f}" '
                    f'width="{2 * r:.1f}" height="{2 * r:.1f}" rx="4" '
                    f'fill="white" stroke="{stroke}" stroke-width="{weight}"/>'
                )
            elif item["kind"] == "led":
                self.items.append(
                    f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" '
                    f'fill="{"#222" if used else "none"}" stroke="{stroke}" '
                    f'stroke-width="{weight}"/>'
                )
            else:
                self.items.append(
                    f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" '
                    f'fill="{"white" if used else "none"}" '
                    f'stroke="{stroke}" stroke-width="{weight}"/>'
                )
                if not used:
                    self.text(
                        f"{cx:.1f}", f"{cy + 5:.1f}", item["letter"], 15,
                        colour="#666", anchor="middle",
                    )
        for key, lines, side in PANEL_LABELS:
            item = next(i for i in PANEL_GEOMETRY if i["key"] == key)
            cx, cy = at(item)
            r = item["d"] * mm / 2
            if side == "below":
                for i, line in enumerate(lines):
                    self.text(
                        f"{cx:.1f}", f"{cy + r + 17 + 17 * i:.1f}", line, 15,
                        "bold", anchor="middle",
                    )
                continue
            left = side == "left"
            edge = x0 - 8 if left else x0 + width + 8
            for i, line in enumerate(lines):
                self.text(
                    f"{edge:.1f}",
                    f"{cy - 2 + 17 * i:.1f}",
                    line,
                    15,
                    "normal",
                    anchor="end" if left else "start",
                )
            self.items.append(
                f'<line x1="{edge + (3 if left else -3):.1f}" '
                f'y1="{cy:.1f}" x2="{cx + (-r if left else r):.1f}" '
                f'y2="{cy:.1f}" stroke="#222" stroke-width="1"/>'
            )
        return 24 + height

    def legend(self, x, y):
        self.text(x, y, "READ THE [A] LED", 17, "bold")
        for i, (label, pattern) in enumerate(
            (
                ("steady", "steady"),
                ("slow blink", "slow"),
                ("fast blink", "fast"),
                ("two pulses", "2 "),
            )
        ):
            yy = y + 35 + i * 37
            self.circle(x + 13, yy - 5, 12, "white", pattern)
            self.text(x + 36, yy, label, 16)
        self.text(x, y + 188, "Colours: green / yellow / red", 15)


SVG_HEADER = """<svg xmlns="http://www.w3.org/2000/svg" {physical}
 viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">
<title id="title">Drizzlet hidden options guide</title>
<desc id="desc">Four pages of mix knob settings with quality LED [A] colour and blink
indications. Full text version in OPTIONS.md.</desc>
<metadata>Author: Insolent Construct</metadata>
<rect width="100%" height="100%" fill="white"/>
<g font-family="sans-serif" fill="#222">
"""


def svg(pages, mobile=False):
    d = Drawing()
    width = 390 if mobile else 1100
    margin = 22 if mobile else 35
    columns = 43 if mobile else 46
    d.text(margin, 35, "Drizzlet Hidden Options", 24, "bold")
    y = 70
    for i, line in enumerate(instructions(STEPS), 1):
        y = d.lines(margin, y, f"{i}. {line}", columns) + 3
    for note in instructions(NOTES):
        y = d.lines(margin, y, note, columns)
    if mobile:
        y += d.panel(margin, y + 15, 1.0) + 52
        d.legend(margin, y)
        y += 235
        y = d.gestures(margin, y, 43) + 5
        y += d.card(margin, y + 15, width - 2 * margin, WIDTH_ROWS, True) + 30
        y = d.warning(margin, y, width - 2 * margin, 43) + 35
    else:
        panel_height = d.panel(margin + 370, 70, 1.0)
        d.legend(margin + 745, 70)
        warned = d.warning(margin + 745, 270, 285, 31)
        # Card titles sit 34 below the page title, clear of the warning.
        y = math.ceil(max(y + 15, 70 + panel_height + 16, warned - 14))
    for page, knobs in pages.items():
        d.text(margin, y, f"PAGE {page}", 20, "bold")
        d.marker(margin + 100, y, next(iter(knobs.values()))[0]["LED"])
        y += 34
        if mobile:
            for knob in KNOBS:
                y += (
                    d.card(margin, y, width - 2 * margin, knobs[knob], True)
                    + 20
                )
        else:
            heights = [
                d.card(margin + i * 330, y, 315, knobs[k])
                for i, k in enumerate(KNOBS)
            ]
            if page == "1":
                # Gestures and the width card fill page 1's free space.
                heights[1] = d.gestures(
                    margin + 330, y + heights[1] + 14, 40, True
                ) - y
                heights[2] += 16 + d.card(
                    margin + 660, y + heights[2] + 16, 315, WIDTH_ROWS
                )
            y += max(heights) + 12
    y -= 16
    for line in textwrap.wrap(
        "Author: Insolent Construct • Control sketch • " + ATTRIBUTION,
        41 if mobile else 140,
    ):
        y += 21
        d.text(margin, y, line, 15)
    height = y + 30
    if mobile:
        physical = f'width="390" height="{height}"'
    else:
        print_height = 190 * height / width
        physical = f'width="190mm" height="{print_height:.2f}mm"'
    assert (
        mobile or height / width < 254 / 190
    ), f"Exceeds Letter-safe print area: {height}x{width}"
    return SVG_HEADER.format(physical=physical, width=width, height=height) + (
        "\n".join(d.items) + "\n</g>\n</svg>\n"
    )


def generate(data):
    rows, pages = read_tsv(data)
    return {
        "OPTIONS.md": markdown(rows, pages),
        "options-guide.svg": svg(pages),
        "options-guide-mobile.svg": svg(pages, True),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir", type=Path, default=Path(__file__).resolve().parents[1] / "docs"
    )
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, data in generate(sys.stdin.read()).items():
        (args.output_dir / name).write_bytes(data.encode("utf-8"))


if __name__ == "__main__":
    main()
