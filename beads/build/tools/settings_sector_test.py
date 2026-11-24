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
    """A module's own calibration, within 25% of the factory values."""
    seed *= 0.4
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


def mirror_dump(cali, records):
    """A calibration copy sector: the calibration, then options records one after another."""
    data = ss.chunk(ss.CALI, cali)
    for r in records:
        data += ss.chunk(ss.DRZL, r)
    return data + b"\xff" * (ss.SECTOR_SIZE - len(data))


def record(seed):
    """An options record of layout version 1, its values within their counts."""
    return bytes([1, 0, 255, 255]) + struct.pack("<I", 0) + \
        bytes((seed + i) & 1 for i in range(16)) + b"\xff" * 32


def load_both(settings_image, mirror_image, directory):
    paths = []
    for name, data in (("s15.bin", settings_image), ("s14.bin", mirror_image)):
        path = Path(directory) / name
        path.write_bytes(data)
        paths.append(str(path))
    lines = subprocess.check_output([CHECK] + paths, text=True).split("\n")
    fields = dict(line.split(" ", 1) for line in lines if line)
    return (fields["locked"] == "0", fields["record"] == "1",
            bytes.fromhex(fields["calibration"]), bytes.fromhex(fields["settings"]),
            bytes.fromhex(fields["options"]))


def run_mirror(directory, cali_dump, settings_dump, options_dump, now_dump):
    paths = {}
    for name, data in (("cal", cali_dump), ("set", settings_dump),
                       ("opt", options_dump), ("now", now_dump)):
        if data is not None:
            path = Path(directory) / (name + ".bin")
            path.write_bytes(data)
            paths[name] = str(path)
    out = Path(directory) / "out.bin"
    mirror = Path(directory) / "mirror.bin"
    for path in (out, mirror):
        if path.exists():
            path.unlink()
    command = [sys.executable, str(Path(__file__).parent / "settings_sector.py"),
               "--calibration", paths["cal"], "--settings", paths["set"]]
    if "opt" in paths:
        command += ["--options", paths["opt"]]
    if "now" in paths:
        command += ["--now", paths["now"], "--mirror-out", str(mirror)]
    result = subprocess.run(command + [str(out)], capture_output=True, text=True)
    return result, out, mirror


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

    # Each input needs only the requested record.
    cali_only = dump(calibration(0.5), [])
    state_only = dump(calibration(0.5), [state(6)], cali_tag=ss.STAT)
    result, out = run(directory, cali_only, state_only)
    assert result.returncode == 0, result.stderr
    assert load(ss.erased(out.read_bytes()), directory) == (
        True, calibration(0.5), state(6))

    # A full sector: the last settings record wins.
    full = dump(calibration(0.5), [state(i) for i in range(4092)])
    assert ss.parse(full) == (calibration(0.5), state(4091))
    assert load(full, directory) == (True, calibration(0.5), state(4091))

    # Malformed sector sizes and damage to requested records are refused.
    bad_checksum = bytearray(backup)
    bad_checksum[8] ^= 1
    blank = b"\xff" * ss.SECTOR_SIZE
    for broken in (backup[:-1], blank, cali_only, state_only, bytes(bad_checksum)):
        if len(broken) == ss.SECTOR_SIZE:
            assert not load(broken, directory)[0]
    for broken in (backup[:-1], blank, state_only, bytes(bad_checksum)):
        result, out = run(directory, broken, now)
        assert result.returncode == 1 and not out.exists(), result
    for broken in (backup[:-1], blank, cali_only):
        result, out = run(directory, now, broken)
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

    # The calibration copy sector: the same calibration, then the latest options record, which
    # this firmware loads with both images; with no options dump, none (the profile's defaults).
    options_backup = mirror_dump(calibration(0.5), [record(1), record(2)])
    assert ss.options(options_backup) == record(2)
    assert ss.options(mirror_dump(calibration(0.5), [])) is None
    torn = bytearray(mirror_dump(calibration(0.5), [record(1), record(2)]))
    torn[ss.CALI_CHUNK + ss.OPTIONS_CHUNK + 12] ^= 1
    assert ss.options(bytes(torn)) == record(1)
    no_cali = bytearray(options_backup)
    no_cali[8] ^= 1
    assert ss.options(bytes(no_cali)) is None
    for opt, expected in ((options_backup, record(2)), (None, None)):
        result, out, mirror = run_mirror(directory, backup, backup, opt, now)
        assert result.returncode == 0, result.stderr
        assert len(mirror.read_bytes()) == ss.CALI_CHUNK + (ss.OPTIONS_CHUNK if expected else 0)
        unlocked, found, cali, settings, options = load_both(
            ss.erased(out.read_bytes()), ss.erased(mirror.read_bytes()), directory)
        # Stock bytes 2-9 as saved (media and the CV destination are clamped), zero after.
        assert unlocked and cali == calibration(0.5)
        assert settings[2:10] == state(3)[2:10] and settings[10:] == bytes(14)
        assert found == (expected is not None)
        assert expected is None or options == expected
        # A usable calibration in the settings sector now: the copy sector goes first.
        assert result.stdout.index("write 1: " + str(mirror)) < result.stdout.index("write 2: ")
    # No usable calibration in the settings sector now: it goes first, the copy untouched meanwhile.
    for damaged in (b"\xff" * ss.SECTOR_SIZE, dump(bytes(120), [])):
        result, out, mirror = run_mirror(directory, backup, backup, options_backup, damaged)
        assert result.returncode == 0, result.stderr
        assert "write 1: " + str(out) in result.stdout, result.stdout
    # Exact factory values cannot protect an older measured mirror.
    factory = b"".join(struct.pack("<ff", *pair) for pair in ss.FACTORY) + bytes(72)
    factory_now = dump(factory, [state(4)])
    result, out, mirror = run_mirror(directory, now, backup, options_backup, factory_now)
    assert result.returncode == 0, result.stderr
    assert "write 1: " + str(out) in result.stdout, result.stdout

    # A settings-only restore uses the live measured mirror, not old options.
    live_mirror = Path(directory) / "live-mirror.bin"
    live_mirror.write_bytes(mirror_dump(calibration(0.25), []))
    result, out, mirror = run_mirror(directory, factory_now, backup,
                                     options_backup, factory_now)
    assert result.returncode == 1 and not out.exists() and not mirror.exists()
    command = [sys.executable, str(Path(__file__).parent / "settings_sector.py"),
               "--calibration", str(Path(directory) / "cal.bin"),
               "--calibration-copy", str(live_mirror),
               "--settings", str(Path(directory) / "set.bin"),
               "--options", str(Path(directory) / "opt.bin"),
               "--now", str(Path(directory) / "now.bin"),
               "--mirror-out", str(mirror), str(out)]
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert "write 1: " + str(out) in result.stdout, result.stdout
    assert load_both(ss.erased(out.read_bytes()), ss.erased(mirror.read_bytes()),
                     directory)[2] == calibration(0.25)
    assert ss.options(ss.erased(mirror.read_bytes())) == record(2)

    # A calibration more than 25% from the factory values, an options dump without the copy image,
    # or a copy image without the sector as it is now: refused, nothing written.
    wild = dump(calibration(0.5)[:16] + struct.pack("<ff", -200.0, 60.0) + calibration(0.5)[24:],
                [state(1)])
    for args in ((wild, backup, options_backup, now), (backup, backup, options_backup, None)):
        result, out, mirror = run_mirror(directory, *args)
        assert result.returncode == 1 and not out.exists() and not mirror.exists(), result

print("settings_sector_test: OK")
