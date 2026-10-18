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
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

sys.path.insert(0, str(Path("beads/build/tools").resolve()))
from release_package import package
from options_guide import SHORT, indication, generate

def body(source, signature, opening):
    start = source.index(opening, source.index(signature)) + len(opening)
    depth = 1
    for end in range(start, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if not depth:
            return source[start:end]
    raise AssertionError("Unclosed loop body")


def check_main_loop(firmware, harness):
    import re
    compact = lambda text: re.sub(r"\s+", "", text)
    actual = body(firmware, "int main(void)", "while (1) {")
    expected = body(harness, "void MainLoopStep() {", "{")
    assert compact(actual) == compact(
        expected + "processor.WasteCPUCycles(16);"
    ), "TestMainLoopMatchesHarness: main loop differs from host harness"


firmware = Path("beads/beads.cc").read_text()
harness = Path("beads/test/firmware_test.cc").read_text()
check_main_loop(firmware, harness)
for old, new in (
    ("processor.WasteCPUCycles(16);", "processor.WasteCPUCycles(15);"),
    ("ui.DoEvents();", "ui.DoEvents(); ui.DoEvents();"),
    ("background_buffer_backup.Process(ui.freeze_led_state());", ""),
):
    assert firmware.count(old) == 1
    try:
        check_main_loop(firmware.replace(old, new), harness)
    except AssertionError:
        pass
    else:
        raise AssertionError("Main loop drift survived: " + old)
print("Main loop matches harness; three drift controls detected: PASS")

export = subprocess.check_output(["./build/options_tables"], text=True)
actual = export.split("\nconstant\tname\tvalue\n")[0]
readme = Path("beads/build/README.txt").read_text()
section = readme.split("Options page\n------------\n", 1)[1].split(
    "Saved data\n", 1
)[0]
expected = section.split("Options reference (TSV)\n", 1)[1].split(
    "End options reference", 1
)[0]
assert (
    actual == expected
), "README.txt Options page differs from firmware tables"

# Also check the short map above the detailed reference, where readers start.
rows = actual.splitlines()[1:]
pages = {}
for row in rows:
    page, led, knob, setting, *_ = row.split("\t")
    pages.setdefault((page, led), {})[knob] = setting
for (page, led), knobs in pages.items():
    summary = [f"{page} / {led}"] + [
        knobs[k] for k in ("FEEDBACK", "DRY/WET", "REVERB")
    ]
    assert any(
        " ".join(line.split()) == " ".join(summary)
        for line in section.splitlines()
    ), summary

markdown_readme = Path("beads/build/README.md").read_text()
plain_options = section.split("Options reference (TSV)", 1)[0]
rich_options = markdown_readme.split("## Options page\n", 1)[1]
rich_options = rich_options.split("\n## ", 1)[0]
# Markdown keeps width and delay in their own sections instead of repeating
# them under Options. Compare the relocated width prose and shared delay facts.
width_start = plain_options.index("On page 1 (REVERB LED), DRY/WET selects Stock")
delay_start = plain_options.index("Delay mode: Auto (Stock)", width_start)
limiter_start = plain_options.index("Output limiter:", delay_start)
rich_width = markdown_readme.split("## Stereo width\n", 1)[1].split("\n## ", 1)[0]
assert " ".join(plain_options[width_start:delay_start].split()) == " ".join(
    rich_width.split()
), "README.md and README.txt Stereo width prose differ"
plain_delay = plain_options[delay_start:limiter_start]
rich_delay = markdown_readme.split("## Delay mode\n", 1)[1].split("\n## ", 1)[0]
for fact in ("Auto (Stock)", "Never", "Wide detent", "40%", "60%", "SIZE CV",
             "green / yellow / red", "does not affect synth mode"):
    assert fact in " ".join(plain_delay.split()), fact
    assert fact in " ".join(rich_delay.split()), fact
plain_options = plain_options[:width_start] + plain_options[limiter_start:]
plain_reference = "See below for every value."
rich_reference = "See [OPTIONS.md](OPTIONS.md) for every value."
assert plain_reference in plain_options
assert rich_reference in rich_options
plain_options = plain_options.replace(plain_reference, rich_reference)
assert " ".join(plain_options.split()) == " ".join(
    rich_options.split()
), "README.md and README.txt Options prose differ"

# Validate the exported indication vocabulary as well as matching the prose.
values = {}
for row in rows:
    _, _, _, setting, value, color, pulse = row.split("\t")
    if setting == "Not used yet":
        assert color == "unchanged" and pulse == "steady"
        continue
    assert color in {"green", "yellow", "red", "meter"}
    assert pulse in {
        "steady",
        "slow (0.98 Hz)",
        "fast (3.91 Hz)",
        "2 yellow pulses / 2.048 s",
    }
    seen = values.setdefault(setting, set())
    if not seen:
        assert (color, pulse) == ("green", "steady"), setting
    assert (color, pulse) not in seen, (setting, value)
    seen.add((color, pulse))
assert len(values["Pitch scale"]) == 10
assert ("meter", "slow (0.98 Hz)") in values["Stereo width"]

print("Options page map and TSV match firmware tables: PASS")

# Regeneration must reproduce all committed guides from the live TSV.
with tempfile.TemporaryDirectory(prefix="options-guide-") as directory:
    subprocess.run(
        [
            "python3",
            "beads/build/tools/options_guide.py",
            "--output-dir",
            directory,
        ],
        input=export,
        text=True,
        check=True,
    )
    for name in (
        "OPTIONS.md",
        "options-guide.svg",
        "options-guide-mobile.svg",
    ):
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
    # Buyer-facing tables retain every live value/indication, with technical
    # numbers confined to Details. Pitch's eight scales form a separate list.
    public, details = markdown.split("## Details", 1)
    assert public.count("| Knob | Setting |") == 3
    assert (
        sum(
            line[:2] in {"1.", "2.", "3.", "4."}
            for line in public.splitlines()
        )
        == 4
    )
    for row in rows:
        page, _, knob, setting, value, color, pulse = row.split("\t")
        if setting == "Not used yet":
            assert f"| {knob} | Not used yet | — | — |" in public
            continue
        label = value
        shown = indication(dict(colour=color, pulse=pulse))
        assert label in public and shown in public, row
    for page in ("1", "2", "3"):
        page_text = public.split(f"## Page {page} —", 1)[1].split(
            "## Page", 1
        )[0]
        for knob in ("FEEDBACK", "DRY/WET", "REVERB"):
            matching = [
                row.split("\t")
                for row in rows
                if row.split("\t")[0] == page and row.split("\t")[2] == knob
            ]
            setting = matching[0][3]
            if setting == "Not used yet":
                continue
            listed = matching[:2] if setting == "Pitch scale" else matching
            names = [row[4] for row in listed]
            indications = [
                indication(dict(colour=row[5], pulse=row[6])) for row in listed
            ]
            if setting == "Pitch scale":
                names.append("Scale")
                indications.append("see scale list")
            assert (
                f"| {knob} | {setting} | {' / '.join(names)} | "
                f"{' / '.join(indications)} |"
            ) in page_text
    for phrase in (
        "13%",
        "7%",
        "3%",
        "0.98 Hz",
        "3.91 Hz",
        "2.048 s",
        "100 ms",
        "1.5 s",
        "40–60%",
    ):
        assert phrase in details and phrase not in public, phrase
    for name in ("options-guide.svg", "options-guide-mobile.svg"):
        root = ET.parse(Path(directory, name)).getroot()
        texts = list(root.iter("{http://www.w3.org/2000/svg}text"))
        labels = [node.text for node in texts]
        assert labels.count("pulsing") == 3
        assert all(labels.count(label) == 3 for label in ("F", "D/W", "R"))
        assert labels.count("READ THE MEDIA LED") == 1
        assert labels.count("REVERB: Not used yet") == 2
        assert labels.count("FEEDBACK: Not used yet") == 1
        assert "Reserved" not in labels
        assert "Scale (tap SEED to step through):" in labels
        assert all(float(node.attrib["font-size"]) >= 15 for node in texts)
        flat = " ".join(labels)
        for phrase in ("qualified", "dead band", "2.048", "0.98", "3.91"):
            assert phrase not in flat, phrase
        assert "MEDIA LED" in labels and "ASSIGN" in labels
        for row in rows:
            _, _, _, setting, value, color, pulse = row.split("\t")
            if setting != "Not used yet":
                assert value in labels, value
                assert SHORT.get(value, value) in labels, value
    # Check the non-TSV zone descriptions against the shipped reference too.
    for phrase in (
        "Three zones",
        "Lower half selects",
        "Above this zone",
        "13%",
        "7%",
        "three seconds",
        "one second",
    ):
        assert phrase in section, phrase
print("Generated SVG/Markdown match live tables and README: PASS")

# Mutate a temporary TSV, independently checking every exported boundary.
with tempfile.TemporaryDirectory(prefix="options-boundary-") as directory:
    tsv = Path(directory, "options.tsv")
    original = generate(export)
    for boundary in (
        "kWidthStockThreshold", "kWidthEnabledThreshold",
        "kOptionsHalfLow", "kOptionsHalfHigh", "kOptionsZoneLow",
        "kOptionsZoneHigh", "kOptionsZoneBand",
    ):
        lines = export.splitlines()
        for i, line in enumerate(lines):
            fields = line.split("\t")
            if fields[:2] == ["constant", boundary]:
                fields[2] = str(float(fields[2]) + 0.01)
                lines[i] = "\t".join(fields)
                break
        else:
            raise AssertionError(boundary)
        tsv.write_text("\n".join(lines) + "\n")
        changed = generate(tsv.read_text())
        for name in ("options-guide.svg", "options-guide-mobile.svg"):
            assert changed[name] != original[name], (boundary, name)
print("Exported boundary mutations change both SVGs: PASS")

# Exercise the real ZIP packager with tiny stand-ins, without firmware builds
# or importing its audio decoder dependencies.
with tempfile.TemporaryDirectory(prefix="options-package-") as directory:
    out = Path(directory)
    for name in ("test.wav", "combo.bin", "NOTICE.txt"):
        (out / name).write_bytes(b"fixture")
    package(
        out,
        Path("beads/build"),
        "test.wav",
        "test-wav.zip",
        "test-combo.bin",
        "test-combo.zip",
    )
    for name in ("test-wav.zip", "test-combo.zip"):
        with zipfile.ZipFile(out / name) as archive:
            for guide in (
                "OPTIONS.md",
                "options-guide.svg",
                "options-guide-mobile.svg",
            ):
                assert (
                    archive.read(guide)
                    == Path("beads/build", guide).read_bytes()
                )
print("Release ZIPs include the generated guides: PASS")
