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

KNOBS = ("FEEDBACK", "DRY/WET", "REVERB")
COLOURS = {"green": "#247443", "yellow": "#a87900", "red": "#bd3030",
           "meter": "#777", "unchanged": "#777"}
STEPS = [
    "Open: hold ASSIGN 3 s, release.",
    "Change page: tap ASSIGN. The pulsing mix LED marks the page (F / D/W / R).",
    "Change a setting: turn its knob; it saves when you let go (the page LED flashes).",
    "Close: hold ASSIGN 3 s, or press any other button.",
]
NOTES = ["Turn a knob a little before it responds.",
         "Wait a second before powering off."]
SHORT = {"Clouds-style": "Clouds", "Auto (Stock)": "Auto",
         "Wide detent": "Wide", "Automatic (Stock)": "Automatic",
         "Fast release": "Fast", "Enabled (1..255)": "wider →"}


def read_tsv(data):
    rows = list(csv.DictReader(io.StringIO(data), delimiter="\t"))
    assert rows and list(rows[0]) == ["page", "LED", "knob", "setting", "value", "colour", "pulse"]
    pages = {}
    for row in rows:
        pages.setdefault(row["page"], {}).setdefault(row["knob"], []).append(row)
    assert list(pages) == ["1", "2", "3"]
    assert all(set(page) == set(KNOBS) for page in pages.values())
    return rows, pages


def zones(rows):
    if rows[0]["setting"] == "Pitch scale":
        values = ["Off", "Chromatic", "Scale"]
    else:
        values = [SHORT.get(r["value"], r["value"]) for r in rows]
    if rows[0]["setting"] == "Stereo width":
        return [(0, .1, "Stock"), (.1, 1, "wider →")]
    return [(i / len(values), (i + 1) / len(values), value)
            for i, value in enumerate(values)]


def indication(row, exact=False):
    if row["colour"] == "unchanged":
        return "—"
    colour = "green → yellow → red" if row["colour"] == "meter" else row["colour"]
    pulse = row["pulse"]
    if not exact:
        pulse = {"slow (0.98 Hz)": "slow blink", "fast (3.91 Hz)": "fast blink",
                 "2 yellow pulses / 2.048 s": "two pulses"}.get(pulse, pulse)
    return colour + "; " + pulse


DETAILS = [
    "Hold ASSIGN alone for 3 seconds with the mix knobs still, then release. "
    "Pages cycle 1 → 2 → 3 → 1; reopening remembers the page until power-off.",
    "Move more than 10% of knob travel to begin editing. Zone boundaries have "
    "a 3% dead band: thirds switch at 1/3 ± 3% and 2/3 ± 3%; halves "
    "switch below 47% and above 53%. Pitch and Delay mode use thirds; Reverb character and "
    "Self-play use halves; Limiter uses thirds.",
    "Width: Stock below 10%; leave Stock above 13%, return below 7%. "
    "Enabled width goes from zero spread at 13% to full spread at 100%, "
    "saved as 1..255 (0 = Stock). Changes affect new grains; zero spread "
    "preserves stereo position, with mono centred. When enabled, ASSIGN "
    "adds WIDTH after the three mix destinations; all three LEDs pulse. "
    "ASSIGN CV adds to saved width, clamped to zero and full spread.",
    "Changes save after 1 second without knob movement that changes the setting "
    "or passes the movement threshold. Pending changes save after leaving too. "
    "The page LED flashes for 100 ms on entry, exit and saving. All three mix "
    "LEDs pulse on entry; one then pulses to mark the page. There is no idle exit or cancel.",
    "Slow blink is 0.98 Hz; fast blink is 3.91 Hz. Two yellow pulses repeat "
    "as a 2.048 s group. MEDIA shows the first available setting on entry, "
    "then the last knob moved past its threshold. Unused knobs leave it unchanged.",
    "Scale: short SEED taps cycle the eight scales instead of closing options; "
    "holds do not cycle; taps do not play grains. Pitch snaps new grains, including synth "
    "grains; playing grains, the continuous synth oscillator and delay pitch "
    "keep their pitch. The root is unity (0 semitones); +1 V is one octave. "
    "Pelog and the raags are microtonal.",
    "Delay Auto switches at SIZE fully clockwise; Never stays granular. Wide "
    "keeps the switch and adds an unfiltered 40–60% zone to SIZE "
    "attenurandomizer plus SIZE CV, with no resonant boost. Synth is unaffected.",
    "Clouds uses Clouds-style reverb. Switching briefly fades and clears the tail.",
    "Self-play On request needs both inputs unpatched and FREEZE off: hold "
    "MEDIA 1.5 s to start or stop. Patching an input stops it once FREEZE is off. "
    "Limiter Off allows loud peaks to hard-clip.",
]


def markdown(rows, pages):
    out = ["<!-- Author: Insolent Construct. Generated by options_guide.py. -->",
           "# Options page", "", "**TL;DR:** Open options with ASSIGN, choose a page, then turn a mix knob.", "",
           "[Printable guide](options-guide.svg) · [Phone guide](options-guide-mobile.svg)",
           "", "## Steps", ""]
    out += [f"{i}. {step}" for i, step in enumerate(STEPS, 1)]
    out += ["", *NOTES, "", "MEDIA shows the setting value: steady, slow blink, fast blink or two pulses; "
            "colours are named below."]
    for page, knobs in pages.items():
        led = next(iter(knobs.values()))[0]["LED"]
        out += ["", f"## Page {page} — {led} LED pulsing", "",
                "| Knob | Setting | Values left to right | MEDIA LED per value |", "|---|---|---|---|"]
        for knob, values in knobs.items():
            setting = values[0]["setting"]
            shown = values[:2] if setting == "Pitch scale" else values
            names = [SHORT.get(r["value"], r["value"]) for r in shown]
            leds = [indication(r) for r in shown]
            if setting == "Reserved":
                names, leds = ["—"], ["—"]
                setting = "not used yet"
            if setting == "Pitch scale":
                names += ["Scale"]
                leds += ["see scale list"]
            out.append(f"| {knob} | {setting} | {' / '.join(names)} | {' / '.join(leds)} |")
        if page == "1":
            out += ["", "Scale (tap SEED to step through):", ""]
            out += [f"- {r['value']}: {indication(r)}" for r in knobs["FEEDBACK"][2:]]
    out += ["", "## Details", ""]
    out += [paragraph + "\n" for paragraph in DETAILS]
    return "\n".join(out)


class Drawing:
    def __init__(self):
        self.items = []

    def text(self, x, y, value, size=15, weight="normal", colour="#222", anchor="start"):
        self.items.append(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{colour}" text-anchor="{anchor}">{escape(value)}</text>')

    def lines(self, x, y, value, columns):
        for line in textwrap.wrap(value, columns):
            self.text(x, y, line)
            y += 21
        return y

    def circle(self, x, y, r, colour="white", pattern=""):
        dash = ' stroke-dasharray="6 4"' if pattern.startswith("slow") else ' stroke-dasharray="1 3"' if pattern.startswith("fast") else ""
        self.items.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{colour}" stroke="#333" stroke-width="2"{dash}/>')
        if pattern.startswith("2 "):
            self.circle(x-4,y,1,"#222")
            self.circle(x+4,y,1,"#222")

    def arc(self, x, y, a, b):
        def point(t):
            angle = math.radians(135 + 270*t)
            return x+37*math.cos(angle), y+37*math.sin(angle)
        x1,y1=point(a+.012); x2,y2=point(b-.012)
        self.items.append(f'<path d="M {x1:.3f} {y1:.3f} A 37 37 0 {int(b-a>.6667)} 1 {x2:.3f} {y2:.3f}" fill="none" stroke="#555" stroke-width="3"/>')

    def card(self, x, y, width, rows, mobile_card=False):
        if rows[0]["setting"] == "Reserved":
            self.text(x, y, rows[0]["knob"] + ": not used yet", colour="#737373")
            return 26
        self.text(x, y, rows[0]["knob"], 16, "bold")
        self.text(x, y+23, rows[0]["setting"], 18, "bold")
        cx, cy = x+width/2, y+95
        self.circle(cx, cy, 22)
        self.items.append(f'<path d="M {cx} {cy} v -16" stroke="#222" stroke-width="2"/>')
        for a,b,value in zones(rows):
            self.arc(cx,cy,a,b)
            angle=math.radians(135+270*((a+b)/2))
            lx, ly = cx+53*math.cos(angle), cy+53*math.sin(angle)
            anchor = "end" if lx < cx-15 else "start" if lx > cx+15 else "middle"
            self.text(round(lx,2),round(ly+5,2),value,15,anchor=anchor)
        base = y+145
        pitch = rows[0]["setting"] == "Pitch scale"
        for index, row in enumerate(rows):
            if pitch and index == 2:
                self.text(x,base,"Scale (tap SEED to step through):",15,"bold")
                base += 26
            self.circle(x+9,base-5,8,COLOURS[row["colour"]],row["pulse"])
            label = row["value"] if pitch else SHORT.get(row["value"],row["value"])
            if mobile_card:
                self.text(x+25,base,label,15)
                self.text(x+25,base+19,indication(row),15,colour="#555")
            else:
                self.text(x+25,base,label,15)
                self.text(x+width-3,base,indication(row).replace(" blink", ""),15,colour="#555",anchor="end")
            base += 44 if mobile_card else 22
        return base-y

    def marker(self, x, y, led):
        for i, knob in enumerate(KNOBS):
            xx=x+i*57
            self.text(xx,y,"F" if i==0 else "D/W" if i==1 else "R",15,anchor="middle")
            self.circle(xx,y+17,7,"#222" if led==knob else "white")
            if led==knob:
                self.text(xx,y+43,"pulsing",15,anchor="middle")

    def panel(self, x, y):
        self.text(x,y,"FIND THE CONTROLS",17,"bold")
        # Original partial panel sketch; positions follow the board control layout.
        self.items.append(f'<rect x="{x}" y="{y+14}" width="310" height="240" rx="12" fill="#f6f5f0" stroke="#aaa"/>')
        for dx,dy,name in ((42,45,"FREEZE"),(265,45,"SEED")):
            self.circle(x+dx,y+dy,10)
            self.text(x+dx,y+dy+29,name,15,anchor="middle")
        self.circle(x+131,y+105,8,COLOURS["green"])
        self.text(x+131,y+88,"MEDIA LED",15,anchor="middle")
        for dx,dy,name in ((42,155,"FEEDBACK"),(265,155,"DRY/WET"),(265,215,"REVERB")):
            self.circle(x+dx,y+dy,14)
            self.circle(x+dx,y+dy-24,5)
            self.text(x+dx+(26 if dx==42 else -26),y+dy+5,name,15,
                      anchor="start" if dx==42 else "end")
        self.circle(x+42,y+215,9)
        self.text(x+68,y+220,"ASSIGN",15)

    def legend(self, x, y):
        self.text(x,y,"READ THE MEDIA LED",17,"bold")
        for i,(label,pattern) in enumerate((("steady","steady"),("slow blink","slow"),
                                           ("fast blink","fast"),("two pulses","2 "))):
            yy=y+35+i*37
            self.circle(x+13,yy-5,12,"white",pattern)
            self.text(x+36,yy,label,16)
        self.text(x,y+188,"Colours: green / yellow / red",15)


def svg(pages, mobile=False):
    d=Drawing()
    width=390 if mobile else 1060
    margin=22 if mobile else 35
    d.text(margin,35,"YOUR OPTIONS",24,"bold")
    y=70
    for i,line in enumerate(STEPS,1):
        y=d.lines(margin,y,f"{i}. {line}",43)+5
    for note in NOTES:
        y=d.lines(margin,y,note,43)
    if mobile:
        d.panel(margin,y+15)
        y+=300
        d.legend(margin,y)
        y+=235
    else:
        d.panel(margin+365,70)
        d.legend(margin+710,70)
        y=max(y+15,350)
    for page,knobs in pages.items():
        d.text(margin,y,f"PAGE {page}",20,"bold")
        d.marker(margin+180,y-12,next(iter(knobs.values()))[0]["LED"])
        y+=50
        if mobile:
            for knob in KNOBS:
                y+=d.card(margin,y,width-2*margin,knobs[knob],True)+20
        else:
            heights=[d.card(margin+i*330,y,315,knobs[k]) for i,k in enumerate(KNOBS)]
            y+=max(heights)+15
    d.text(margin,y+5,"Author: Insolent Construct • Control sketch",15)
    height=y+30
    physical=f'width="190mm" height="{190*height/width:.2f}mm"' if not mobile else f'width="390" height="{height}"'
    assert mobile or height/width < 254/190, f"Exceeds Letter-safe print area: {height}x{width}"
    return f'<svg xmlns="http://www.w3.org/2000/svg" {physical} viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">\n<title id="title">Options page control guide</title>\n<desc id="desc">Three pages of mix knob settings with MEDIA colour and blink indications. Full text version in OPTIONS.md.</desc>\n<metadata>Author: Insolent Construct</metadata>\n<rect width="100%" height="100%" fill="white"/>\n<g font-family="sans-serif" fill="#222">\n'+"\n".join(d.items)+"\n</g>\n</svg>\n"


def generate(data):
    rows,pages=read_tsv(data)
    return {"OPTIONS.md":markdown(rows,pages), "options-guide.svg":svg(pages),
            "options-guide-mobile.svg":svg(pages,True)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parents[1])
    args=parser.parse_args()
    args.output_dir.mkdir(parents=True,exist_ok=True)
    for name,data in generate(sys.stdin.read()).items():
        (args.output_dir/name).write_bytes(data.encode("utf-8"))


if __name__ == "__main__":
    main()
