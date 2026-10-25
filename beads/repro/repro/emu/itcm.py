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
"""The application's C ITCM copy, qualified.

`System::Init` copies the hot code into ITCM, and the emulator stubs that
function. This runs the real copy loop up to its first `isb`, checks the
result against the load image in flash, and returns the bytes to preload.
"""

from repro.emu import machine as M
from repro.emu.elf import ElfError

SYSTEM_INIT = "_ZN5beads6System4InitEb"


def itcm_image(elf):
  """The expected ITCM bytes, or b"" when the ELF has no ITCM section."""
  if not elf.has("_sdsp"):
    return b""
  lo, hi, src = (elf.defined(n, "NOTYPE").value
                 for n in ("_sdsp", "_edsp", "_sidsp"))
  if hi == lo:
    return b""
  if not (lo == 0 and 0 < hi <= 65536):
    raise ElfError(f"ITCM window {lo:#x}..{hi:#x} is not at address 0")
  import capstone
  base, image = elf.flash_image()
  sym = elf.defined(SYSTEM_INIT, "FUNC")
  entry = sym.value & ~1
  code = image[entry - base:entry - base + sym.size]
  md = capstone.Cs(capstone.CS_ARCH_ARM,
                   capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)
  barrier = next(i for i in md.disasm(code, entry) if i.mnemonic == "isb")
  m = M.Machine("itcm-startup", image, base)
  m.uc.reg_write(m.UA.UC_ARM_REG_R1, 1)
  m.run(entry, barrier.address + barrier.size, sp=m.call_sp, budget=1000000)
  expected = image[src - base:src - base + hi - lo]
  if m.read(lo, hi - lo) != expected:
    raise ElfError("the C startup did not initialize ITCM as the load "
                   "image says")
  return expected
