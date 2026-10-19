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


"""Tier adapters. A tier turns a Stimulus into a render; rules and verdicts
are shared. Tiers: desktop (host build), emulator (the firmware ELF in a
Cortex-M7 emulator). Seed3 and module arrive later."""


class TierUnavailable(RuntimeError):
    pass


def get(name, **options):
    """A tier object. `options` are the tier's own settings (the emulator
    takes `elf` and `window`)."""
    if name == "desktop":
        from repro.tiers import desktop
        return desktop.DesktopTier()
    if name == "emulator":
        from repro.tiers import emulator
        return emulator.EmulatorTier(**options)
    raise TierUnavailable(
        f"tier {name!r} is not implemented yet (desktop and emulator are "
        "the tiers so far)")
