#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)

"""Offline DWT decoder and printable OpenOCD commands. Never contacts hardware."""

import argparse
import json
from pathlib import Path
import struct

BASE = 0x30041000
SIZE = 1112
VERSION = 1
HISTOGRAM_BIN_WIDTH = 2000
MAGIC = 0x44575443
BLOCK_BUDGET_CYCLES = 120000
PINS = Path(__file__).resolve().parents[1] / "expected-hashes.sha256"
BUILD_SHA = next(line.split()[0] for line in PINS.read_text().splitlines()
                 if line.split()[-1] == "calibration.bin")


def decode(data):
    if len(data) != SIZE:
        raise ValueError("wrong dump length")
    magic, version, sequence, request, generation, block = struct.unpack_from(
        "<III4sII", data
    )
    if magic != MAGIC or version != VERSION or sequence & 1:
        raise ValueError("wrong firmware or overlapping update; retry")
    result = {
        "sequence": sequence,
        "generation": generation,
        "fixture": bool(request[1]),
        "blocks_elapsed": block,
        "brackets": [],
    }
    for b in range(4):
        maximum, count, total = struct.unpack_from("<IIQ", data, 24 + 272 * b)
        hist = struct.unpack_from("<64I", data, 40 + 272 * b)
        if sum(hist) != count:
            raise ValueError("torn histogram")

        def quantile(numerator, denominator):
            rank = (count * numerator + denominator - 1) // denominator
            accum = 0
            for i, n in enumerate(hist):
                accum += n
                if accum >= rank and count:
                    return [
                        HISTOGRAM_BIN_WIDTH * i,
                        HISTOGRAM_BIN_WIDTH * (i + 1) - 1 if i < 63 else None,
                    ]
            return None

        median = quantile(1, 2)
        tail = quantile(999, 1000)
        percent = lambda bounds: (
            [x / (BLOCK_BUDGET_CYCLES / 100) if x is not None else None for x in bounds]
            if bounds
            else None
        )
        result["brackets"].append(
            dict(
                name=(
                    "callback",
                    "inner",
                    "fixture_preparation",
                    "callback_minus_fixture",
                )[b],
                count=count,
                maximum=maximum,
                maximum_budget_percent=maximum / (BLOCK_BUDGET_CYCLES / 100),
                mean=total / count if count else None,
                mean_budget_percent=total / count / (BLOCK_BUDGET_CYCLES / 100) if count else None,
                median_interval=median,
                p999_interval=tail,
                median_budget_percent_interval=percent(median),
                p999_budget_percent_interval=percent(tail),
            )
        )
    return result


def openocd(action):
    if action == "sai":
        return "mdw 0x4001581c 1\nmdw 0x4001583c 1\n"
    if action == "reset":
        return (
            "# WARNING: writes RAM; calibration firmware must be running.\n"
            "# Use 3 instead of 1 for the synthetic fixture.\n"
            f"mwb 0x{BASE + 12:08x} 1\nmdw 0x{BASE + 16:08x} 1\n"
        )
    template = """# Run after capture has frozen. No halt or reset.
if {[mrw @BASE@] != @MAGIC@ || [mrw @VERSION_ADDRESS@] != @VERSION@} {error "wrong calibration firmware"}
set first [mrw @SEQUENCE@]
if {$first & 1} {error "update in progress; retry"}
# mdw displays all @WORDS@ words (@SIZE@ bytes) in the console.
mdw @BASE@ @WORDS@
# dump_image emits the binary consumed by decode.
dump_image capture.bin @BASE@ @SIZE@
set last [mrw @SEQUENCE@]
if {$first != $last || ($last & 1)} {error "discard capture.bin; retry"}
"""
    values = {
        "BASE": f"0x{BASE:08x}",
        "MAGIC": f"0x{MAGIC:08x}",
        "VERSION_ADDRESS": f"0x{BASE + 4:08x}",
        "VERSION": str(VERSION),
        "SEQUENCE": f"0x{BASE + 8:08x}",
        "WORDS": str(SIZE // 4),
        "SIZE": str(SIZE),
    }
    for name, value in values.items():
        template = template.replace("@" + name + "@", value)
    return template


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="action", required=True)
    offline = sub.add_parser("decode")
    offline.add_argument("dump", type=Path)
    commands = sub.add_parser("openocd", help="print commands; no connection")
    commands.add_argument("operation", choices=["read", "reset", "sai"])
    a = p.parse_args()
    if a.action == "decode":
        print(json.dumps(decode(a.dump.read_bytes()), indent=2))
    else:
        print(openocd(a.operation), end="")


if __name__ == "__main__":
    main()
