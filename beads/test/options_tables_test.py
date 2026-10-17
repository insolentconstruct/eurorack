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

"""Keep the plain-text Options page's reference table tied to firmware."""
from pathlib import Path
import subprocess

actual = subprocess.check_output(["./build/options_tables"], text=True)
readme = Path("beads/build/README.txt").read_text()
section = readme.split("Options page\n------------\n", 1)[1].split("Saved data\n", 1)[0]
expected = section.split("Options reference (TSV)\n", 1)[1].split("End options reference", 1)[0]
assert actual == expected, "README.txt Options page differs from firmware tables"

# Also check the short map above the detailed reference, where readers start.
rows = actual.splitlines()[1:]
pages = {}
for row in rows:
    page, led, knob, setting, *_ = row.split("\t")
    pages.setdefault((page, led), {})[knob] = setting
for (page, led), knobs in pages.items():
    summary = [f"{page} / {led}"] + [knobs[k] for k in ("FEEDBACK", "DRY/WET", "REVERB")]
    assert any(" ".join(line.split()) == " ".join(summary) for line in section.splitlines()), summary

# Validate the exported indication vocabulary as well as matching the prose.
values = {}
for row in rows:
    _, _, _, setting, value, color, pulse = row.split("\t")
    if setting == "Reserved":
        assert color == "unchanged" and pulse == "steady"
        continue
    assert color in {"green", "yellow", "red", "meter"}
    assert pulse in {"steady", "slow (0.98 Hz)", "fast (3.91 Hz)",
                     "2 yellow pulses / 2.048 s"}
    seen = values.setdefault(setting, set())
    if not seen:
        assert (color, pulse) == ("green", "steady"), setting
    assert (color, pulse) not in seen, (setting, value)
    seen.add((color, pulse))
assert len(values["Pitch scale"]) == 10
assert ("meter", "slow (0.98 Hz)") in values["Stereo width"]

print("Options page map and TSV match firmware tables: PASS")

# Keep page terminology and C++ naming aligned with the public API.
options = Path("beads/options.h").read_text()
ui = Path("beads/ui.cc").read_text()
assert "Reverb LED category" not in readme
assert "#include <algorithm>" not in options
assert "const char* color;" in options
assert "Leds::Color colour" not in ui
assert "}\n  void " not in options, "Separate methods with a blank line"

# Regeneration must reproduce all committed guides from the live TSV.
import tempfile
import xml.etree.ElementTree as ET

with tempfile.TemporaryDirectory(prefix="options-guide-") as directory:
    subprocess.run(["python3", "beads/build/tools/options_guide.py",
                    "--output-dir", directory], input=actual, text=True,
                   check=True)
    for name in ("OPTIONS.md", "options-guide.svg", "options-guide-mobile.svg"):
        generated = Path(directory, name).read_bytes()
        assert generated == Path("beads/build", name).read_bytes(), name
        if name.endswith(".svg"):
            root = ET.fromstring(generated)
            assert root.tag == "{http://www.w3.org/2000/svg}svg"
            if name == "options-guide.svg":
                assert root.attrib["width"] == "190mm"
                assert float(root.attrib["height"].removesuffix("mm")) <= 254
            else:
                assert root.attrib["width"] == "390"
    markdown = Path(directory, "OPTIONS.md").read_text()
    for row in rows:
        page, _, knob, _, value, color, pulse = row.split("\t")
        led = "unchanged" if color == "unchanged" else (
            "green → yellow → red meter" if color == "meter" else color
        ) + "; " + pulse
        assert f"| {page} | {knob} | {value} | {led} |" in markdown
    # Check the non-TSV zone descriptions against the shipped reference too.
    for phrase in ("Three equal zones", "Lower half selects", "Above this zone",
                   "13%", "7%", "three seconds", "one second"):
        assert phrase in section, phrase
print("Generated SVG/Markdown match live tables and README: PASS")

# Exercise the real ZIP packager with tiny stand-ins, without firmware builds
# or importing its audio decoder dependencies.
import ast
import hashlib
import zipfile

release = ast.parse(Path("beads/build/tools/release.py").read_text())
functions = [node for node in release.body if isinstance(node, ast.FunctionDef)
             and node.name in {"write_zip", "package", "sha"}]
namespace = {"zipfile": zipfile, "hashlib": hashlib, "subprocess": subprocess,
             "HERE": Path("beads/build").resolve(), "WAV": "test.wav",
             "ZIP": "test-wav.zip", "COMBO": "test-combo.bin",
             "COMBO_ZIP": "test-combo.zip",
             "ASSETS": ["test-wav.zip", "test-combo.zip", "NOTICE.txt"]}
exec(compile(ast.Module(body=functions, type_ignores=[]), "release.py", "exec"), namespace)
with tempfile.TemporaryDirectory(prefix="options-package-") as directory:
    out = Path(directory)
    for name in ("test.wav", "combo.bin", "NOTICE.txt"):
        (out / name).write_bytes(b"fixture")
    namespace["package"](out)
    for name in ("test-wav.zip", "test-combo.zip"):
        with zipfile.ZipFile(out / name) as archive:
            for guide in ("OPTIONS.md", "options-guide.svg", "options-guide-mobile.svg"):
                assert archive.read(guide) == Path("beads/build", guide).read_bytes()
print("Release ZIPs include the generated guides: PASS")
