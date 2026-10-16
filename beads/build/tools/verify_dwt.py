#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)

"""Offline artifact identity check; does not verify timing or firmware behavior."""
import argparse
import hashlib
from pathlib import Path
import dwt


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("release", type=Path)
    p.add_argument("calibration", type=Path)
    a = p.parse_args()
    pins = Path(__file__).resolve().parents[1] / "expected-hashes.sha256"
    release_sha = next(line.split()[0] for line in pins.read_text().splitlines()
                       if line.split()[-1] == "app.bin")
    for path, expected in ((a.release, release_sha), (a.calibration, dwt.BUILD_SHA)):
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError(str(path) + ": SHA256 mismatch")
        print(actual, path)


if __name__ == "__main__":
    main()
