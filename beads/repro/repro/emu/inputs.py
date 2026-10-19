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

"""The panel's switches and jacks as the firmware sees them.

Switches are GPIO input levels (active low with pull-ups, so a pressed button
reads 0). A jack that is unpatched is normalled to the normalization probe's
drive pin (PE15) and follows it, so the pin level the firmware last wrote
decides what its unpatched jacks read. A patched jack carries the stimulus.
These are board facts: the pin map and the probe amplitude below describe the
module's hardware, not the firmware's data layout.

An event is `BLOCK:VERB[:ARG[:ARG2]]`:

    press:SWITCH   release:SWITCH   tap:SWITCH:MS
    unpatch:JACK   patch:JACK       freeze_gate:off|on|N
"""

GPIOA_IDR, GPIOE_IDR = 0x58020010, 0x58021010
GPIOE_ODR, GPIOE_BSRR = 0x58021014, 0x58021018
GPIO_IDR = [0x58020000 + 0x400 * k + 0x10 for k in range(11)]  # GPIOA..GPIOK
SWITCHES = {"freeze": (GPIOA_IDR, 11), "seed": (GPIOA_IDR, 12),
            "quality": (GPIOE_IDR, 8), "mode": (GPIOE_IDR, 7)}
JACKS = ("seed", "cv1", "cv2", "cv3", "cv4", "cv5", "in_l", "in_r")
PROBE_AMP = 1 << 23  # q31: an unpatched audio input's square, about -48 dBFS
BLOCKS_PER_MS = 4    # 4 kHz blocks at every quality
VERBS = ("press", "release", "tap", "unpatch", "patch", "freeze_gate")


class EventError(ValueError):
    pass


def parse_event(spec):
    """'3000:tap:quality:100' -> (3000, 'tap', 'quality', 100)."""
    parts = spec.split(":")
    if len(parts) < 2:
        raise EventError(f"event {spec!r}: want BLOCK:VERB[:ARG[:ARG2]]")
    ev = (int(parts[0]), parts[1], parts[2] if len(parts) > 2 else None,
          int(parts[3]) if len(parts) > 3 else None)
    check_event(ev)
    return ev


def check_event(ev):
    _, verb, arg, arg2 = ev
    if verb not in VERBS:
        raise EventError(f"unknown verb {verb!r}")
    if verb in ("press", "release", "tap") and arg not in SWITCHES:
        raise EventError(f"{verb}: unknown switch {arg!r}")
    if verb == "tap" and not arg2:
        raise EventError("tap needs a duration in ms")
    if verb in ("patch", "unpatch") and arg not in JACKS:
        raise EventError(f"{verb}: unknown jack {arg!r}")
    if verb == "freeze_gate" and arg not in ("on", "off") \
            and not (arg or "").isdigit():
        raise EventError(f"freeze_gate: on, off or a period, not {arg!r}")


def expand(events):
    """Taps become a press and a later release; sorted by block (stable)."""
    out = []
    for ev in events:
        check_event(ev)
        block, verb, arg, arg2 = ev
        if verb == "tap":
            out += [(block, "press", arg, None),
                    (block + arg2 * BLOCKS_PER_MS, "release", arg, None)]
        else:
            out.append(ev)
    return sorted(out, key=lambda e: e[0])


class Inputs:
    """Pressed switches, unpatched jacks and the FREEZE gate mode."""

    def __init__(self, unpatched=(), events=(), event_bits=(1, 2)):
        """`event_bits` are the codec's SEED and FREEZE event values
        (Codec::Event); the rigs pass the ELF's."""
        self.seed_bit, self.freeze_value = event_bits
        for j in unpatched:
            if j not in JACKS:
                raise EventError(f"unknown jack {j!r}")
        self.pressed, self.unpatched = set(), set(unpatched)
        self.freeze_gate = "off"
        self.events = expand(events)
        self._next = 0

    def add(self, events):
        self.events = sorted(self.events + expand(events), key=lambda e: e[0])

    def due(self, block):
        """Apply every event at or before `block`; True if any applied."""
        changed = False
        while self._next < len(self.events) \
                and self.events[self._next][0] <= block:
            _, verb, arg, _ = self.events[self._next]
            self._next += 1
            changed = True
            if verb == "press":
                self.pressed.add(arg)
            elif verb == "release":
                self.pressed.discard(arg)
            elif verb == "unpatch":
                self.unpatched.add(arg)
            elif verb == "patch":
                self.unpatched.discard(arg)
            else:
                self.freeze_gate = arg
        return changed

    def idr_words(self):
        """{IDR address: word} for every GPIO port: all ones, a pressed
        switch's pin 0."""
        words = {a: 0xFFFF for a in GPIO_IDR}
        for s in self.pressed:
            a, pin = SWITCHES[s]
            words[a] &= ~(1 << pin)
        return words

    def freeze_bit(self, frame):
        g = self.freeze_gate
        if g == "off":
            return 0
        if g == "on":
            return self.freeze_value
        return self.freeze_value if (frame // int(g)) & 1 else 0

    def normalled(self, level, frames, gates, cv, cv_index=None):
        """Each unpatched jack follows the probe pin `level`. `frames` is a
        list of (l, r), `gates` a list of bytes, `cv` the CV ADC words.
        `cv_index` optionally maps a CV jack to (ADC index, {level: word});
        the repro scenarios patch every CV jack, so it is normally absent.
        Returns new copies."""
        if not self.unpatched:
            return frames, gates, cv
        frames, gates, cv = list(frames), list(gates), list(cv)
        amp = PROBE_AMP if level else -PROBE_AMP
        if "in_l" in self.unpatched:
            frames = [(amp, r) for _, r in frames]
        if "in_r" in self.unpatched:
            frames = [(l, amp) for l, _ in frames]
        if "seed" in self.unpatched:
            gates = [(g & ~self.seed_bit) | (self.seed_bit if level else 0)
                     for g in gates]
        for jack, (index, words) in (cv_index or {}).items():
            if jack in self.unpatched:
                cv[index] = words[level]
        return frames, gates, cv


def pin_level(level, addr, size, value):
    """PE15's level after a write of `value` to `addr`."""
    if addr == GPIOE_BSRR and size == 4:
        if value & 0x80000000:
            level = 0
        if value & 0x8000:
            level = 1
    elif addr == GPIOE_BSRR and size == 2:
        if value & 0x8000:
            level = 1
    elif addr == GPIOE_BSRR + 2 and size == 2:
        if value & 0x8000:
            level = 0
    elif addr == GPIOE_ODR and size in (2, 4):
        level = (value >> 15) & 1
    return level
