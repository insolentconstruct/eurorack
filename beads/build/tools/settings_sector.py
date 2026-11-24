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
"""Build a settings sector image from the calibration of one dump and the settings of another.

    settings_sector.py --calibration CALIBRATION.bin --settings SETTINGS.bin OUT.bin
    settings_sector.py ... [--options OPTIONS.bin] --now NOW.bin --mirror-out MIRROR.bin OUT.bin

Each settings input is a dump of the settings sector (0x081E0000, 128 KB) from the same module, as
RECOVERY.md's backup command makes. OUT holds the calibration record of the first and the latest
settings record of the second. Each dump needs only the requested record. The output uses
stmlib's ChunkStorage layout after erasing the
sector. OUT is only those two records (160 bytes): the restore command erases the whole sector and
programs just them, so the firmware's later saves land on erased flash, as after its own rewrite.

With --mirror-out, MIRROR is the image for the calibration copy sector (0x081C0000): the same
calibration, then the latest options record of OPTIONS, a dump of that sector (none: the options
start from their defaults). NOW, the settings sector as it is, decides which image goes first, so a
usable calibration stays in one of the two sectors at every step.
"""

import argparse
import os
import struct
import sys

SECTOR_SIZE = 0x20000
CALI = 0x494C4143
STAT = 0x54415453
CALI_SIZE = 120  # PersistentData: 6 calibration pairs, 72 bytes padding.
STATE_SIZE = 24  # State.
CALI_CHUNK = 128  # Header + data, rounded up to the 32-byte flash word.
STATE_CHUNK = 32
DRZL = 0x4C5A5244
OPTIONS_SIZE = 56  # OptionsRecord.
OPTIONS_CHUNK = 64
# Settings::Init's factory calibration: (scale, offset) per CV channel.
FACTORY = [(-2.0, 1.0), (-2.0, 1.0), (-120.0, 60.0), (-3.2, 1.6), (-3.2, 1.6), (-3.2, 1.6)]


def checksum(data):
    return (sum(data) & 0xffff) ^ 0xffff


def chunk(tag, data):
    return struct.pack("<IHH", tag, len(data), checksum(data)) + data


def read_chunk(sector, address, tag, size):
    header_tag, header_size, header_checksum = struct.unpack_from("<IHH", sector, address)
    data = sector[address + 8:address + 8 + size]
    if header_tag != tag or header_size != size or checksum(data) != header_checksum:
        return None
    return data


def check_sector_size(sector):
    if len(sector) != SECTOR_SIZE:
        raise ValueError("a dump is %d bytes, not %d" % (len(sector), SECTOR_SIZE))


def read_calibration(sector):
    check_sector_size(sector)
    calibration = read_chunk(sector, 0, CALI, CALI_SIZE)
    if calibration is None:
        raise ValueError("no valid calibration record")
    return calibration


def read_state(sector):
    check_sector_size(sector)
    state = None
    address = CALI_CHUNK
    while address + STATE_CHUNK <= SECTOR_SIZE:
        record = read_chunk(sector, address, STAT, STATE_SIZE)
        if record is None:
            break
        state = record
        address += STATE_CHUNK
    if state is None:
        raise ValueError("no valid settings record")
    return state


def parse(sector):
    """Returns the calibration and the latest settings, as the firmware loads them."""
    return read_calibration(sector), read_state(sector)


def usable(calibration):
    """Within 25% of the factory values, as Settings::CalibrationPlausible checks; NaN fails."""
    for i, (scale, offset) in enumerate(FACTORY):
        s, o = struct.unpack_from("<ff", calibration, 8 * i)
        if not (abs(s / scale - 1.0) <= 0.25 and abs(o / offset - 1.0) <= 0.25):
            return False
    return True


def options(sector):
    """The latest options record of a calibration copy sector dump, as the firmware reads it."""
    if len(sector) != SECTOR_SIZE:
        raise ValueError("a dump is %d bytes, not %d" % (len(sector), SECTOR_SIZE))
    if read_chunk(sector, 0, CALI, CALI_SIZE) is None:
        return None
    latest = None
    address = CALI_CHUNK
    while address + OPTIONS_CHUNK <= SECTOR_SIZE:
        record = read_chunk(sector, address, DRZL, OPTIONS_SIZE)
        if record is None:
            break
        latest = record
        address += OPTIONS_CHUNK
    return latest


def build_mirror(calibration, record):
    return chunk(CALI, calibration) + (chunk(DRZL, record) if record else b"")


def build(calibration, state):
    return chunk(CALI, calibration) + chunk(STAT, state)


def erased(image):
    """The sector after the restore command: the image, the rest erased."""
    return image + b"\xff" * (SECTOR_SIZE - len(image))


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--calibration", required=True, help="dump to take the calibration from")
    parser.add_argument("--settings", required=True, help="dump to take the settings from")
    parser.add_argument("--options", help="calibration copy sector dump to take the options from")
    parser.add_argument("--now", help="the settings sector as it is now")
    parser.add_argument("--mirror-out", help="calibration copy sector image to write (must not exist)")
    parser.add_argument("out", help="image to write (must not exist)")
    args = parser.parse_args(argv)
    if (args.now is None) != (args.mirror_out is None) or (args.options and not args.mirror_out):
        raise ValueError("--mirror-out needs --now, and --options needs --mirror-out")
    for name in (args.out, args.mirror_out):
        if name and os.path.exists(name):
            raise ValueError("%s exists" % name)
    with open(args.calibration, "rb") as f:
        calibration = read_calibration(f.read())
    with open(args.settings, "rb") as f:
        state = read_state(f.read())
    if not usable(calibration):
        raise ValueError("the calibration is more than 25% from the factory values")
    image = build(calibration, state)
    if parse(erased(image)) != (calibration, state):
        raise ValueError("the image does not read back")
    record = None
    if args.mirror_out:
        if args.options:
            with open(args.options, "rb") as f:
                record = options(f.read())
        with open(args.now, "rb") as f:
            now = f.read()
        if len(now) != SECTOR_SIZE:
            raise ValueError("a dump is %d bytes, not %d" % (len(now), SECTOR_SIZE))
        current = read_chunk(now, 0, CALI, CALI_SIZE)
        mirror = build_mirror(calibration, record)
        if read_chunk(erased(mirror), 0, CALI, CALI_SIZE) != calibration or \
                options(erased(mirror)) != record:
            raise ValueError("the copy image does not read back")
        with open(args.mirror_out, "xb") as f:
            f.write(mirror)
    with open(args.out, "xb") as f:
        f.write(image)
    pitch_scale, pitch_offset = struct.unpack_from("<ff", calibration, 2 * 8)  # CV_ADC_CHANNEL_PITCH
    print("calibration from %s (PITCH scale %.4f, offset %.4f)" % (
            args.calibration, pitch_scale, pitch_offset))
    print("settings from %s" % args.settings)
    print("wrote %s" % args.out)
    if args.mirror_out:
        print("options from %s" % (args.options if record else "none: the profile's defaults"))
        print("wrote %s" % args.mirror_out)
        # Erase a sector only while the other holds a usable calibration.
        if current is not None and usable(current):
            order = [(args.mirror_out, "0x081C0000"), (args.out, "0x081E0000")]
        else:
            order = [(args.out, "0x081E0000"), (args.mirror_out, "0x081C0000")]
        for step, (name, address) in enumerate(order, 1):
            print("write %d: %s at %s" % (step, name, address))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except (OSError, ValueError) as error:
        print("settings_sector.py: %s" % error, file=sys.stderr)
        sys.exit(1)
