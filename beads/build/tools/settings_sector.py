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

Each input is a dump of the settings sector (0x081E0000, 128 KB) from the same module, as the
README's backup command makes. OUT holds the calibration record of the first and the latest settings
record of the second, laid out as stmlib's ChunkStorage writes them after erasing the sector. OUT is
only those two records (160 bytes): the README's restore command erases the whole sector and programs
just them, so the firmware's later saves land on erased flash, as after its own rewrite.
"""

import argparse
import struct
import sys

SECTOR_SIZE = 0x20000
CALI = 0x494C4143
STAT = 0x54415453
CALI_SIZE = 120  # PersistentData: 6 calibration pairs, 72 bytes padding.
STATE_SIZE = 24  # State.
CALI_CHUNK = 128  # Header + data, rounded up to the 32-byte flash word.
STATE_CHUNK = 32


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


def parse(sector):
    """Returns the calibration and the latest settings, as the firmware loads them."""
    if len(sector) != SECTOR_SIZE:
        raise ValueError("a dump is %d bytes, not %d" % (len(sector), SECTOR_SIZE))
    calibration = read_chunk(sector, 0, CALI, CALI_SIZE)
    if calibration is None:
        raise ValueError("no valid calibration record: the firmware would not read this dump")
    state = None
    address = CALI_CHUNK
    while address + STATE_CHUNK <= SECTOR_SIZE:
        record = read_chunk(sector, address, STAT, STATE_SIZE)
        if record is None:
            break
        state = record
        address += STATE_CHUNK
    if state is None:
        raise ValueError("no valid settings record: the firmware would not read this dump")
    return calibration, state


def build(calibration, state):
    return chunk(CALI, calibration) + chunk(STAT, state)


def erased(image):
    """The sector after the restore command: the image, the rest erased."""
    return image + b"\xff" * (SECTOR_SIZE - len(image))


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--calibration", required=True, help="dump to take the calibration from")
    parser.add_argument("--settings", required=True, help="dump to take the settings from")
    parser.add_argument("out", help="image to write (must not exist)")
    args = parser.parse_args(argv)
    with open(args.calibration, "rb") as f:
        calibration, _ = parse(f.read())
    with open(args.settings, "rb") as f:
        _, state = parse(f.read())
    image = build(calibration, state)
    if parse(erased(image)) != (calibration, state):
        raise ValueError("the image does not read back")
    with open(args.out, "xb") as f:
        f.write(image)
    pitch_scale, pitch_offset = struct.unpack_from("<ff", calibration, 2 * 8)  # CV_ADC_CHANNEL_PITCH
    print("calibration from %s (PITCH scale %.4f, offset %.4f)" % (
            args.calibration, pitch_scale, pitch_offset))
    print("settings from %s" % args.settings)
    print("wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except (OSError, ValueError) as error:
        print("settings_sector.py: %s" % error, file=sys.stderr)
        sys.exit(1)
