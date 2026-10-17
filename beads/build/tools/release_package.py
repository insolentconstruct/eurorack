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

"""Reproducible release archives and checksums."""

import hashlib
import subprocess
import zipfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_zip(path, names):
    # Reproducible zip.
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as z:
        for name in sorted(names):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.create_system = 0
            info.external_attr = 0
            info.internal_attr = 0
            info.extra = b""
            z.writestr(info, (path.parent / name).read_bytes())
            # writestr supplies Unix permissions; reset before close.
            info.external_attr = 0


def package(out, here, wav, wav_zip, combo, combo_zip):
    assets = [wav_zip, combo_zip, "NOTICE.txt"]
    guides = ["OPTIONS.md", "options-guide.svg", "options-guide-mobile.svg"]
    for name in ["README.txt"] + guides:
        (out / name).write_bytes((here / name).read_bytes())
    write_zip(out / wav_zip, [wav, "README.txt", "NOTICE.txt"] + guides)
    # Combo image, named after the release.
    (out / combo).write_bytes((out / "combo.bin").read_bytes())
    write_zip(out / combo_zip, [combo, "README.txt", "NOTICE.txt"] + guides)
    (out / "SHA256SUMS").write_text(
        "".join(sha(out / name) + "  " + name + "\n" for name in assets)
    )
    subprocess.run(["sha256sum", "-c", "SHA256SUMS"], cwd=out, check=True)
