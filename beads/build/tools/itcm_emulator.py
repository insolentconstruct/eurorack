# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)
# MIT; see beads/build/LICENSE.

"""Qualify the ITCM copy even though the oracle stubs System::Init."""


def qualify_itcm(side):
  import capstone
  import machine as M
  import sides as SD

  if "_sdsp" not in side.symbols:
    return b""

  def value(name):
    return SD.defined_symbol(side.symbols, name, "NOTYPE").value

  lo, hi, src = (value(n) for n in ("_sdsp", "_edsp", "_sidsp"))
  if hi == lo:
    return b""
  assert lo == 0 and 0 < hi <= 65536
  sym = SD.defined_symbol(side.symbols, "_ZN5beads6System4InitEb", "FUNC")
  entry = sym.value & ~1
  code = side.image[entry - side.base:entry - side.base + sym.size]
  md = capstone.Cs(capstone.CS_ARCH_ARM,
      capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)
  instructions = list(md.disasm(code, entry))
  barrier = next(i for i in instructions if i.mnemonic == "isb")
  machine = M.Machine("itcm-startup", side.image, side.base)
  machine.uc.reg_write(machine.UA.UC_ARM_REG_R1, 1)
  machine.run(entry, barrier.address + barrier.size,
      sp=M.CALL_SP, budget=1000000)
  expected = side.image[src - side.base:src - side.base + hi - lo]
  assert machine.read(lo, hi - lo) == expected, "C startup failed to initialize ITCM"
  return expected
