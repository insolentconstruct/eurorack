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

import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

sys.path.insert(0, str(Path("beads/build/tools").resolve()))
import release_package
from release_package import package
from options_guide import SHORT, WIDTH_ROWS, indication, generate

# Words and units that belong in FEATURES.md, not in text a musician reads.
STYLE_BANNED = ("%", " Hz", " ms", "dead band", "threshold", "qualified")


def body(source, signature, opening):
    start = source.index(opening, source.index(signature)) + len(opening)
    depth = 1
    for end in range(start, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if not depth:
            return source[start:end]
    raise AssertionError("Unclosed loop body")


def check_main_loop(firmware, harness):
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
# Markdown keeps delay in its own section instead of repeating it under
# Options. Width is set with [A]+DRY/WET, outside the pages, in both READMEs.
plain_width = re.split(
    r"\n\n[^\n]+\n-+\n", readme.split("Stereo width\n------------\n", 1)[1], 1
)[0]
delay_start = plain_options.index("Delay mode: Auto (Stock)")
limiter_start = plain_options.index("Output limiter:", delay_start)
user_sections = [
    markdown_readme.split(f"## {name}\n", 1)[1].split("\n## ", 1)[0]
    for name in ("Delay mode", "Stereo width", "Monophonic grains",
                 "Buffer trim", "Frozen-buffer autosave", "Options page")
]
user_sections.append(
    readme.split("Buffer trim\n-----------\n", 1)[1].split(
        "Options reference (TSV)", 1
    )[0]
)
# Brightness percentages are the owner-selected public level names.
for text in user_sections:
    text = re.sub(r"(?<![0-9])(100|66|33)%", "", text)
    for banned in STYLE_BANNED:
        assert banned not in text, banned
rich_width = markdown_readme.split("## Stereo width\n", 1)[1].split("\n## ", 1)[0]
assert " ".join(plain_width.split()) == " ".join(
    rich_width.split()
), "README.md and README.txt Stereo width prose differ"
for text in (plain_width, rich_width):
    flat = " ".join(text.split())
    assert flat.startswith("Hold [A] and turn DRY/WET"), flat[:40]
    assert "At the bottom of the knob it is Stock" in flat
    assert "page" not in flat.lower(), "Stereo width is not on a page"
assert "Stereo width" not in plain_options + rich_options
plain_delay = plain_options[delay_start:limiter_start]
rich_delay = markdown_readme.split("## Delay mode\n", 1)[1].split("\n## ", 1)[0]
for fact in ("Auto (Stock)", "Disabled", "Wide/smooth filter", "middle", "SIZE CV",
             "green / yellow / red", "does not affect wavetable synth mode"):
    assert fact in " ".join(plain_delay.split()), fact
    assert fact in " ".join(rich_delay.split()), fact
plain_options = plain_options[:delay_start] + plain_options[limiter_start:]
plain_reference = "See below for every value."
rich_reference = "See [OPTIONS.md](docs/OPTIONS.md) for every value."
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
    assert color in {"green", "yellow", "red"}
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
for setting in ("Delay mode", "Attacks", "Limiter"):
    assert len(values[setting]) == 3, setting
for setting in ("Reverb character", "Frozen feedback", "Processor", "Synth",
                "Synth start", "Random intervals"):
    assert len(values[setting]) == 2, setting
assert len(values) == 10 and "Stereo width" not in values
# [A]+DRY/WET width keeps its meter on the guides.
assert [indication(row) for row in WIDTH_ROWS] == [
    "green; steady", "green → yellow → red"
]
page_numbers = sorted({row.split("\t")[0] for row in rows})
assert page_numbers == ["1", "2", "3", "4"]

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
        assert generated == Path("beads/build/docs", name).read_bytes(), name
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
    assert public.count("| Knob | Setting |") == len(page_numbers)
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
    for page in page_numbers:
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
    # User-facing prose carries no exact numbers; those live in FEATURES.md.
    for banned in STYLE_BANNED:
        assert banned not in markdown, ("OPTIONS.md", banned)
    for phrase in ("Wide/smooth filter", "Raag Shri", "about a second and a half",
                   "each stored value in turn", "about two seconds",
                   "hold [A] and turn DRY/WET"):
        assert phrase in details, phrase
    assert "each stored value in turn" in public
    warning = ("Don't hold SEED while powering on: it resets all settings "
               "to defaults (calibration is kept).")
    assert "## Other gestures" in markdown and warning in markdown
    for phrase in ("Monophonic grains", "Buffer trim", "Manual gain",
                   "Wavetable synth (Manual)", "FREEZE [B]",
                   "Switching clears the recording."):
        assert phrase in markdown.split("## Details")[0], phrase
    assert "erase" not in markdown.lower()
    for name in ("options-guide.svg", "options-guide-mobile.svg"):
        root = ET.parse(Path(directory, name)).getroot()
        texts = list(root.iter("{http://www.w3.org/2000/svg}text"))
        labels = [node.text for node in texts]
        markers = len(page_numbers)
        assert labels.count("pulsing") == markers
        assert all(labels.count(label) == markers for label in ("D/W", "R"))
        assert labels.count("F") == markers + 1  # page markers + panel knob F
        printable = name == "options-guide.svg"
        # The printable shows the [A] gestures off the pages on page 1.
        assert labels.count("Off the pages: [A] + DRY/WET" if printable
                            else "[A] + DRY/WET: Stereo width") == 1
        assert labels.count("Off the pages: Manual gain" if printable
                            else "Manual gain") == 1
        assert labels.count("Processor: switching clears the recording."
                            if printable
                            else "Switching clears the recording.") == 1
        assert ("hold [A] 1.5 s" if printable
                else "about a second and a half") in " ".join(labels)
        assert labels.count("READ THE [A] LED") == 1
        assert labels.count("OTHER GESTURES") == 1
        for title in ("Monophonic grains", "Buffer trim",
                      "Wavetable synth (Manual)"):
            assert title in labels, title
        assert warning in " ".join(labels), name
        for knob in ("FEEDBACK", "DRY/WET", "REVERB"):
            reserved = sum(1 for row in rows
                           if row.split("\t")[2] == knob
                           and row.split("\t")[3] == "Not used yet")
            assert labels.count(knob + ": Not used yet") == reserved, knob
        assert "Reserved" not in labels
        assert "Scale (tap SEED to step through):" in labels
        assert all(float(node.attrib["font-size"]) >= 15 for node in texts)
        flat = " ".join(labels)
        for phrase in STYLE_BANNED:
            assert phrase not in flat, (name, phrase)
        assert {"[A]", "CV assign", "[M]"} <= set(labels)
        for row in rows:
            _, _, _, setting, value, color, pulse = row.split("\t")
            if setting != "Not used yet":
                assert value in labels, value
                assert SHORT.get(value, value) in labels, value
    # Check the non-TSV zone descriptions against the shipped reference too.
    for phrase in (
        "Three zones",
        "Lower half selects",
        "each stored value in turn",
        "about two seconds",
        "three seconds",
        "a second",
    ):
        assert phrase in section, phrase
    # The exact constants belong in FEATURES.md, built from the same export.
    constants = {}
    for line in export.split("\nconstant\tname\tvalue\n")[1].splitlines():
        _, name, number = line.split("\t")
        constants[name] = float(number)
    pct = lambda name: format(round(constants[name] * 100, 6), "g")
    seconds = lambda name: format(constants[name] / 1000, "g")
    band = pct("kOptionsZoneBand")
    features = Path("beads/build/docs/FEATURES.md").read_text()
    features = features.split("## Exact option values", 1)[1]
    flat_features = " ".join(features.split())
    for phrase in (
        f"hold [M] {seconds('kOptionsHoldTime')} s",
        f"more than {pct('kOptionsPickupThreshold')}% of its travel",
        f"{band}% dead band",
        f"1/3 ± {band}% and 2/3 ± {band}%",
        f"below {pct('kOptionsHalfLow')}% and above {pct('kOptionsHalfHigh')}%",
        f"Stock below {pct('kWidthStockThreshold')}%",
        f"above {pct('kWidthEnabledThreshold')}%",
        f"returns below {pct('kWidthStockThreshold')}%",
        f"zero spread at {pct('kWidthEnabledThreshold')}%",
        f"after {seconds('kOptionsSettleTime')} second",
        f"{constants['kOptionsFlashTime']:g} ms",
        f"{1000 / constants['kOptionsSlowPeriod']:.2f} Hz",
        f"{1000 / constants['kOptionsFastPeriod']:.2f} Hz",
        f"{seconds('kOptionsPulsePeriod')} s group",
        f"from {pct('kDelayDetentLow')}% to {pct('kDelayDetentHigh')}%",
        f"{seconds('kSelfPlayHoldTime')} s",
        f"{seconds('kOptionsPreviewTime')} s each",
    ):
        assert phrase in flat_features, phrase
    # Wording assumptions: thirds, halves, "a second", "a second and a half".
    assert abs(constants["kOptionsZoneLow"] - 1 / 3) < 0.005
    assert abs(constants["kOptionsZoneHigh"] - 2 / 3) < 0.005
    assert abs(constants["kOptionsHalfLow"] + constants["kOptionsHalfHigh"]
               - 1) < 1e-6 and abs(constants["kOptionsHalfLow"] - 0.5) < 0.05
    assert constants["kOptionsSettleTime"] == 1000
    assert constants["kSelfPlayHoldTime"] == 1500
    assert constants["kOptionsHoldTime"] == 3000
    assert abs(constants["kOptionsPreviewTime"] - 2000) < 100
    assert constants["kWidthRange"] == 1 - constants["kWidthEnabledThreshold"]
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



def hex_image(path):
    # Stand-in for objcopy where the ARM toolchain is absent: lowest address
    # and contents, gaps zero-filled.
    image = {}
    upper = 0
    for line in path.read_text().splitlines():
        record = bytes.fromhex(line[1:])
        assert line[0] == ":" and len(record) == record[0] + 5 and not sum(record) & 0xFF
        if record[3] == 4:
            upper = int.from_bytes(record[4:6], "big") << 16
        elif record[3] == 0:
            address = upper + (record[1] << 8 | record[2])
            for i, byte in enumerate(record[4:-1]):
                image[address + i] = byte
    start = min(image)
    data = bytes(image.get(a, 0) for a in range(start, max(image) + 1))
    return "%08x" % start, data


if "TOOLCHAIN_PATH" not in os.environ:
    release_package.hex_image = hex_image

# Exercise the real ZIP packager with tiny stand-ins, without firmware builds
# or importing its audio decoder dependencies.
COMBO_HEX = ":020000040800F2\n:0700000066697874757265F2\n:00000001FF\n"
WRONG_CONTENT = ":020000040800F2\n:0700000066697874757266F1\n:00000001FF\n"
WRONG_ADDRESS = ":020000040802F0\n:0700000066697874757265F2\n:00000001FF\n"
with tempfile.TemporaryDirectory(prefix="options-package-") as directory:
    out = Path(directory)
    for name in ("test.wav", "combo.bin", "NOTICE.txt"):
        (out / name).write_bytes(b"fixture")
    (out / "objects/beads").mkdir(parents=True)
    combo_hex = out / "objects/beads/beads_bootloader_combo.hex"
    for wrong in (WRONG_CONTENT, WRONG_ADDRESS):
        combo_hex.write_text(wrong)
        try:
            package(out, Path("beads/build"), "test.wav", "test-wav.zip",
                    "test-combo.bin", "test-combo.hex", "test-combo.zip")
        except ValueError:
            continue
        raise AssertionError("wrong combo HEX was packaged")
    combo_hex.write_text(COMBO_HEX)
    package(
        out,
        Path("beads/build"),
        "test.wav",
        "test-wav.zip",
        "test-combo.bin",
        "test-combo.hex",
        "test-combo.zip",
    )
    with zipfile.ZipFile(out / "test-combo.zip") as archive:
        assert archive.read("test-combo.hex") == COMBO_HEX.encode()
        assert archive.read("test-combo.bin") == b"fixture"
    assert "test-combo.hex" in (out / "SHA256SUMS").read_text()
    for name in ("test-wav.zip", "test-combo.zip"):
        with zipfile.ZipFile(out / name) as archive:
            for guide in (
                "OPTIONS.md",
                "options-guide.svg",
                "options-guide-mobile.svg",
            ):
                assert (
                    archive.read(guide)
                    == Path("beads/build/docs", guide).read_bytes()
                )
    listed = [line.split()[1] for line in (out / "SHA256SUMS").read_text().splitlines()]
    for guide in ("OPTIONS.md", "options-guide.svg", "options-guide-mobile.svg"):
        assert guide in listed, guide
        assert (out / guide).read_bytes() == Path("beads/build/docs", guide).read_bytes()
print("Release ZIPs include the generated guides and the combo HEX; wrong HEX rejected: PASS")


# The profile page consumes permanent IDs and placements, never source enums.
# This file is the page's whole catalogue; the page copies it verbatim.
catalog = json.loads(Path("beads/build/hygrostat/static/profile-features.json").read_text())
exported = subprocess.check_output(["build/options_tables", "--profile"],
                                  text=True).splitlines()
assert len(catalog["features"]) == len(exported) == 23
assert next(f for f in catalog["features"] if f["id"] == 18)["values"] == [
    "100%", "66%", "33%"]
for feature, row in zip(catalog["features"], exported):
    identity, name, count, field, page, amount, action, stateless = (
        int(x) if x.isdigit() else x for x in row.split("\t"))
    assert feature["id"] == identity
    assert len(feature["values"] or []) == count
    assert feature["field"] == (None if field == 255 else field)
    assert feature["page_slots"] == (list(range(1, 13)) if page else [])
    assert bool(feature["amount_slots"]) == bool(amount)
    assert bool(feature["actions"]) == bool(action)
    assert feature.get("stateless", False) == bool(stateless)
    assert ("Adjustable" in feature["states"]) == bool(page or amount or action)
    if identity >= 15:
        assert feature["default_state"] == "Off" and feature["default"] == 0
print("Profile page catalogue: PASS")
