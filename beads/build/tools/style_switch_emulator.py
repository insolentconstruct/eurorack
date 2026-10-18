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

"""Reverb transition regression using the qualified ARM oracle harness.

Run inside the emulator container: python style_switch_emulator.py APP.elf OUT.
The oracle modules (machine, manual_gap, sides) must be on PYTHONPATH. Every
object and member address is read from the tested ELF's symbols and DWARF.
Compare the resulting JSON audio hashes between RC4 and the candidate.
"""

import hashlib
import json
from pathlib import Path
import struct
import sys

import machine as M
import manual_gap as G
import fill as F
import sides as SD
from itcm_emulator import qualify_itcm
from official_layout import OfficialLayout

elf = Path(sys.argv[1])
out = Path(sys.argv[2])
G.DRIZZLET_ELF = hashlib.sha256(elf.read_bytes()).hexdigest()
G.DRIZZLET_PAYLOAD = hashlib.sha256(
    elf.with_suffix(".bin").read_bytes()
).hexdigest()
M.qualify()
table = SD.symbol_table(SD.elf32.Elf(elf.read_bytes()))
F.CV_VALUES = SD.ram_object(table, "_ZN5beads5CvAdc7values_E", 12)
F.POT_VALUES = SD.ram_object(table, "_ZN5beads7PotsAdc7values_E", 24)
side = G.prepare(str(elf), "drizzlet")
itcm = qualify_itcm(side)
layout = OfficialLayout(str(elf))
cursor_offset = (
    layout.members("Synth")[1]["reverb_"]
    + layout.members("Reverb")[1]["engine_"]
    + layout.members("FxEngine<32768, (beads::Format)1>")[1]["write_ptr_"]
)
layout.close()


def function(name):
    return SD.defined_symbol(side.symbols, name, "FUNC").value & ~1


start = function("_Z22ConfigureAndStartCodecbb")
configure = function("_ZN5beads9Processor9ConfigureENS0_5MediaEb")
synth_address = SD.defined_symbol(side.symbols, "synth", "OBJECT").value
monitor = function("_Z18MonitorStateChangev")
synth_mode_address = SD.defined_symbol(
    side.symbols, "synth_mode", "OBJECT"
).value
style_address = SD.defined_symbol(side.symbols, "clouds_style", "OBJECT").value
transition = SD.defined_symbol(
    side.symbols, "codec_transition_frames", "OBJECT"
).value
rows = []
for quality in range(4):
    for mode in ("normal-then-selfplay", "synth"):
        panel = G.Panel(side, quality, ("in_l", "in_r"))
        x = panel.x
        if itcm:
            x.m.write(0, itcm)
        x.main_pass = lambda: None
        panel.pots[0:3] = [50000, 65535, 45000]
        panel.pots[7:11] = [2048, 56000, 39000, 36000]
        x.m.call(configure, [x.obj["processor"], quality, 0], fpscr=M.FZ)

        def set_mode(synth):
            x.m.call(start, [int(synth), 0], fpscr=M.FZ)
            x.m.write(transition, struct.pack("<I", 0))

        def cursor():
            return x.m.u32(synth_address + cursor_offset)

        # Leave the synth cursor off the 32-sample LFO boundary before the
        # processor takes over their shared buffer. This exposes Clear().
        set_mode(True)
        panel.step(2001)
        for _ in range(32):
            if cursor() % 32:
                break
            panel.step(1)
        assert cursor() % 32, "Fixture needs an unaligned synth cursor"
        if mode == "normal-then-selfplay":
            set_mode(False)
            panel.step(301)
        state = x.obj["settings"] + side.fields["Settings"]["state_"]
        audio = bytearray()
        cursors = []
        for requested in (True, False):
            before = cursor()
            x.m.write(state + 13, bytes([0xa2 if requested else 0]))
            for _ in range(256):
                audio += panel.step(1)
                if x.m.read(style_address, 1) == bytes([int(requested)]):
                    break
            else:
                raise AssertionError("Style switch never completed")
            if mode == "normal-then-selfplay":
                assert cursor() == before, "Style switch cleared synth cursor"
            cursors.append(cursor())
            audio += panel.step(513)
        if mode == "normal-then-selfplay":
            # Request self-play through the real main-loop mode monitor.
            x.m.write(state + 11, bytes([0xa1]))
            request = x.obj["ui"] + side.fields["Ui"]["self_play_requested_"]
            x.m.write(request, bytes([1]))
            x.m.call(monitor, [], fpscr=M.FZ)
            assert x.m.read(synth_mode_address, 1) == bytes([1])
            assert x.m.read(request, 1) == bytes([0])
            audio += panel.step(2000)
        assert any(audio), "Silent transition capture"
        row = dict(q=quality, mode=mode, cursors=cursors,
                   sha256=hashlib.sha256(audio).hexdigest(), bytes=len(audio))
        rows.append(row)
        print(json.dumps(row), flush=True)
out.write_text(json.dumps(rows, indent=2) + "\n")
