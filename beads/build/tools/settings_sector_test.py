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
"""Check settings_sector.py against stmlib's ChunkStorage, as the firmware loads the sector."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).parent))
import settings_sector as ss

CHECK = "./build/settings_sector_check"


def calibration(seed):
    pairs = [(-2.0 + seed, 1.0), (-2.0, 1.0 + seed), (-120.0 + seed, 60.0),
             (-3.2, 1.6), (-3.2, 1.6 - seed), (-3.2, 1.6)]
    return b"".join(struct.pack("<ff", s, o) for s, o in pairs) + bytes(72)


def state(seed):
    return bytes((seed * 7 + i * 13) & 0xff for i in range(24))


def dump(cali, states, cali_tag=ss.CALI):
    """A sector as the firmware leaves it: the calibration, then settings saved one after another."""
    data = struct.pack("<IHH", cali_tag, len(cali), ss.checksum(cali)) + cali
    for s in states:
        data += ss.chunk(ss.STAT, s)
    return data + b"\xff" * (ss.SECTOR_SIZE - len(data))


def load(image, directory):
    path = Path(directory) / "load.bin"
    path.write_bytes(image)
    lines = subprocess.check_output([CHECK, str(path)], text=True).split("\n")
    fields = dict(line.split(" ", 1) for line in lines if line)
    return (fields["loaded"] == "1", bytes.fromhex(fields["calibration"]),
            bytes.fromhex(fields["settings"]))


def run(directory, cali_dump, settings_dump):
    paths = []
    for name, data in (("cal.bin", cali_dump), ("set.bin", settings_dump)):
        path = Path(directory) / name
        path.write_bytes(data)
        paths.append(str(path))
    out = Path(directory) / "out.bin"
    if out.exists():
        out.unlink()
    result = subprocess.run(
        [sys.executable, str(Path(__file__).parent / "settings_sector.py"),
         "--calibration", paths[0], "--settings", paths[1], str(out)],
        capture_output=True, text=True)
    return result, out


with tempfile.TemporaryDirectory() as directory:
    backup = dump(calibration(0.5), [state(1), state(2), state(3)])
    now = dump(calibration(0.25), [state(4)] * 300)  # Many saves since.

    # The checker reads real dumps as the firmware does: the latest settings win.
    assert load(backup, directory) == (True, calibration(0.5), state(3))
    assert load(now, directory) == (True, calibration(0.25), state(4))

    # Calibration only, settings only, and both: each loads as chosen.
    for cali_dump, settings_dump, expected in (
            (backup, now, (calibration(0.5), state(4))),
            (now, backup, (calibration(0.25), state(3))),
            (backup, backup, (calibration(0.5), state(3)))):
        result, out = run(directory, cali_dump, settings_dump)
        assert result.returncode == 0, result.stderr
        image = out.read_bytes()
        # Only the two records: the rest of the sector stays erased, never programmed.
        assert len(image) == ss.CALI_CHUNK + ss.STATE_CHUNK
        assert load(ss.erased(image), directory) == (True,) + expected
        assert ss.parse(ss.erased(image)) == expected

    # A full sector: the last settings record wins.
    full = dump(calibration(0.5), [state(i) for i in range(4092)])
    assert ss.parse(full) == (calibration(0.5), state(4091))
    assert load(full, directory) == (True, calibration(0.5), state(4091))

    # Dumps the firmware would not read are refused, and no image is written.
    bad_checksum = bytearray(backup)
    bad_checksum[8] ^= 1
    for broken in (backup[:-1], dump(calibration(0.5), []),
                   dump(calibration(0.5), [state(1)], cali_tag=ss.STAT),
                   bytes(bad_checksum), b"\xff" * ss.SECTOR_SIZE):
        if len(broken) == ss.SECTOR_SIZE:
            assert not load(broken, directory)[0]
        for cali_dump, settings_dump in ((broken, now), (now, broken)):
            result, out = run(directory, cali_dump, settings_dump)
            assert result.returncode == 1 and not out.exists(), result

    # An existing output is never overwritten.
    result, out = run(directory, backup, now)
    assert result.returncode == 0
    second = subprocess.run(
        [sys.executable, str(Path(__file__).parent / "settings_sector.py"),
         "--calibration", str(Path(directory) / "cal.bin"),
         "--settings", str(Path(directory) / "set.bin"), str(out)],
        capture_output=True, text=True)
    assert second.returncode == 1 and "exists" in second.stderr, second

print("settings_sector_test: OK")
