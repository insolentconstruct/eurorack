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
"""Instrument checks for the emulator.

Unicorn 2.1.4 in Thumb mode with the Cortex-M7 model read back (the M-class
mode silently gives an M33 and is refused). FPSCR is written by the guest
(vmsr) and read back by the guest (vmrs), because a host register write does
not reach it. `qualify()` runs a table of guest floating-point operations
with exact result bits and flags (rounding modes, flush-to-zero, default NaN,
conversions, compares, single and double), a PRIMASK round trip, and a fused
multiply-add check.

Unicorn does not model CPACR, so this qualifies FP instruction execution
only.
"""

import hashlib
from pathlib import Path
import struct

PINNED = {"unicorn": "2.1.4", "capstone": "5.0.7"}
PAGE = 0x1000
PROBE = 0x0FFE0000  # scratch page for the guest state probes


class Refused(RuntimeError):
  pass


def w16(*halves):
  return b"".join(struct.pack("<H", x) for x in halves)


# Probe instructions (Thumb-2 encodings).
ENC = {
    "msr_primask_r0": w16(0xF380, 0x8810),
    "mrs_r1_primask": w16(0xF3EF, 0x8110),
    "vmrs_r1_fpscr": w16(0xEEF1, 0x1A10),
    "vmsr_fpscr_r3": w16(0xEEE1, 0x3A10),
    "vmsr_fpscr_r4": w16(0xEEE1, 0x4A10),
    "vmsr_fpscr_r0": w16(0xEEE1, 0x0A10),
    "vmov_s0_r0": w16(0xEE00, 0x0A10),
    "vmov_s1_r1": w16(0xEE00, 0x1A90),
    "vmov_r2_s0": w16(0xEE10, 0x2A10),
    "vmov_d0_r0_r1": w16(0xEC41, 0x0B10),
    "vmov_d1_r2_r3": w16(0xEC43, 0x2B11),
    "vmov_r2_r3_d0": w16(0xEC53, 0x2B10),
    "vdiv_s": w16(0xEE80, 0x0A20),
    "vadd_s": w16(0xEE30, 0x0A20),
    "vmul_s": w16(0xEE20, 0x0A20),
    "vsqrt_s": w16(0xEEB1, 0x0AC0),
    "vcvtr_s32": w16(0xEEBD, 0x0A40),
    "vcvt_s32": w16(0xEEBD, 0x0AC0),
    "vcmp_s": w16(0xEEB4, 0x0A60),
    "vcmpe_s": w16(0xEEB4, 0x0AE0),
    "vdiv_d": w16(0xEE80, 0x0B01),
}
VMOV_S2_R2 = w16(0xEE01, 0x2A10)
VFMA_S0_S1_S2 = w16(0xEEA0, 0x0A81)  # fused: one rounding

RM = {"nearest": 0, "plus": 1 << 22, "minus": 2 << 22, "zero": 3 << 22}
DN, FZ = 1 << 25, 1 << 24
IOC, DZC, OFC, UFC, IXC, IDC = 0x01, 0x02, 0x04, 0x08, 0x10, 0x80
# (name, width, op, a, b, fpscr in, result bits or None, fpscr out): the ARM
# (ARMv7-M VFPv4 / FPv5, IEEE 754) results, derived by hand.
FP_CASES = [
    ("1/3 nearest", 32, "vdiv_s", 0x3F800000, 0x40400000, RM["nearest"],
     0x3EAAAAAB, IXC),
    ("1/3 toward +inf", 32, "vdiv_s", 0x3F800000, 0x40400000, RM["plus"],
     0x3EAAAAAB, RM["plus"] | IXC),
    ("1/3 toward -inf", 32, "vdiv_s", 0x3F800000, 0x40400000, RM["minus"],
     0x3EAAAAAA, RM["minus"] | IXC),
    ("1/3 toward zero", 32, "vdiv_s", 0x3F800000, 0x40400000, RM["zero"],
     0x3EAAAAAA, RM["zero"] | IXC),
    ("1/0", 32, "vdiv_s", 0x3F800000, 0x00000000, 0, 0x7F800000, DZC),
    ("0/0 gives the default NaN", 32, "vdiv_s", 0x00000000, 0x00000000, 0,
     0x7FC00000, IOC),
    ("sqrt(-1)", 32, "vsqrt_s", 0xBF800000, 0, 0, 0x7FC00000, IOC),
    ("sqrt(2)", 32, "vsqrt_s", 0x40000000, 0, 0, 0x3FB504F3, IXC),
    ("qNaN payload kept, DN=0", 32, "vadd_s", 0x7FC00001, 0x3F800000, 0,
     0x7FC00001, 0),
    ("qNaN replaced, DN=1", 32, "vadd_s", 0x7FC00001, 0x3F800000, DN,
     0x7FC00000, DN),
    ("sNaN quietened, DN=0", 32, "vadd_s", 0x7F800001, 0x3F800000, 0,
     0x7FC00001, IOC),
    ("exact denormal result, FZ=0", 32, "vmul_s", 0x0D800000, 0x30800000, 0,
     0x00080000, 0),
    ("denormal result flushed, FZ=1", 32, "vmul_s", 0x0D800000, 0x30800000, FZ,
     0x00000000, FZ | UFC),
    ("denormal input, FZ=0", 32, "vmul_s", 0x00080000, 0x4E800000, 0,
     0x0D800000, 0),
    ("denormal input as zero, FZ=1", 32, "vmul_s", 0x00080000, 0x4E800000, FZ,
     0x00000000, FZ | IDC),
    ("overflow", 32, "vmul_s", 0x7F000000, 0x40000000, 0, 0x7F800000,
     OFC | IXC),
    ("inexact underflow", 32, "vmul_s", 0x0D800000, 0x0D800000, 0, 0x00000000,
     UFC | IXC),
    ("vcvtr 2.5 nearest", 32, "vcvtr_s32", 0x40200000, 0, RM["nearest"], 2,
     IXC),
    ("vcvtr 2.5 toward +inf", 32, "vcvtr_s32", 0x40200000, 0, RM["plus"], 3,
     RM["plus"] | IXC),
    ("vcvtr 2.5 toward -inf", 32, "vcvtr_s32", 0x40200000, 0, RM["minus"], 2,
     RM["minus"] | IXC),
    ("vcvtr 2.5 toward zero", 32, "vcvtr_s32", 0x40200000, 0, RM["zero"], 2,
     RM["zero"] | IXC),
    ("vcvtr -2.5 nearest", 32, "vcvtr_s32", 0xC0200000, 0, RM["nearest"],
     0xFFFFFFFE, IXC),
    ("vcvtr -2.5 toward -inf", 32, "vcvtr_s32", 0xC0200000, 0, RM["minus"],
     0xFFFFFFFD, RM["minus"] | IXC),
    ("vcvt 2.5 truncates, nearest", 32, "vcvt_s32", 0x40200000, 0,
     RM["nearest"], 2, IXC),
    ("vcvt 2.5 truncates, toward +inf", 32, "vcvt_s32", 0x40200000, 0,
     RM["plus"], 2, RM["plus"] | IXC),
    ("vcvt 2.5 truncates, toward -inf", 32, "vcvt_s32", 0x40200000, 0,
     RM["minus"], 2, RM["minus"] | IXC),
    ("vcvt 2.5 truncates, toward zero", 32, "vcvt_s32", 0x40200000, 0,
     RM["zero"], 2, RM["zero"] | IXC),
    ("vcvt -2.5 truncates, toward -inf", 32, "vcvt_s32", 0xC0200000, 0,
     RM["minus"], 0xFFFFFFFE, RM["minus"] | IXC),
    ("vcvtr 6e9 saturates", 32, "vcvtr_s32", 0x4FB2D05E, 0, 0, 0x7FFFFFFF, IOC),
    ("vcmp 1 < 2", 32, "vcmp_s", 0x3F800000, 0x40000000, 0, None, 0x80000000),
    ("vcmp 1 == 1", 32, "vcmp_s", 0x3F800000, 0x3F800000, 0, None, 0x60000000),
    ("vcmp 2 > 1", 32, "vcmp_s", 0x40000000, 0x3F800000, 0, None, 0x20000000),
    ("vcmp qNaN is unordered, quiet", 32, "vcmp_s", 0x7FC00000, 0x3F800000, 0,
     None, 0x30000000),
    ("vcmpe qNaN signals", 32, "vcmpe_s", 0x7FC00000, 0x3F800000, 0, None,
     0x30000000 | IOC),
    ("d 1/3 nearest", 64, "vdiv_d", 0x3FF0000000000000, 0x4008000000000000,
     RM["nearest"], 0x3FD5555555555555, IXC),
    ("d 1/3 toward +inf", 64, "vdiv_d", 0x3FF0000000000000, 0x4008000000000000,
     RM["plus"], 0x3FD5555555555556, RM["plus"] | IXC),
    ("d 1/0", 64, "vdiv_d", 0x3FF0000000000000, 0, 0, 0x7FF0000000000000, DZC),
    ("d 0/0", 64, "vdiv_d", 0, 0, 0, 0x7FF8000000000000, IOC),
]


def libs():
  """(unicorn, capstone, arm_const), refusing any other version."""
  import capstone
  import unicorn
  from unicorn import arm_const
  for name, mod in (("unicorn", unicorn), ("capstone", capstone)):
    if mod.__version__ != PINNED[name]:
      raise Refused(f"{name} {mod.__version__} is not the pinned "
                    f"{PINNED[name]}")
  return unicorn, capstone, arm_const


def new_uc(unicorn, UA):
  """A Cortex-M7 instance, or a refusal."""
  uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)
  uc.ctl_set_cpu_model(UA.UC_CPU_ARM_CORTEX_M7)
  if uc.ctl_get_cpu_model() != UA.UC_CPU_ARM_CORTEX_M7:
    raise Refused("unicorn did not select the Cortex-M7 model")
  return uc


def _run_snippet(unicorn, UA, code, regs):
  uc = new_uc(unicorn, UA)
  uc.mem_map(PROBE, PAGE)
  uc.mem_write(PROBE, code)
  for r, v in regs.items():
    uc.reg_write(getattr(UA, f"UC_ARM_REG_{r.upper()}"), v)
  uc.emu_start(PROBE | 1, PROBE + len(code))
  return uc


def fp_case(unicorn, UA, case):
  """(result bits or None, guest fpscr) of one FP_CASES row."""
  _, width, op, a, b, f, _, _ = case
  if width == 32:
    code = (ENC["vmov_s0_r0"] + ENC["vmov_s1_r1"] + ENC["vmsr_fpscr_r3"] +
            ENC[op] + ENC["vmrs_r1_fpscr"] + ENC["vmov_r2_s0"])
    uc = _run_snippet(unicorn, UA, code, dict(r0=a, r1=b, r3=f))
    return uc.reg_read(UA.UC_ARM_REG_R2), uc.reg_read(UA.UC_ARM_REG_R1)
  code = (ENC["vmov_d0_r0_r1"] + ENC["vmov_d1_r2_r3"] + ENC["vmsr_fpscr_r4"] +
          ENC[op] + ENC["vmrs_r1_fpscr"] + ENC["vmov_r2_r3_d0"])
  uc = _run_snippet(
      unicorn, UA, code,
      dict(r0=a & 0xFFFFFFFF, r1=a >> 32, r2=b & 0xFFFFFFFF, r3=b >> 32, r4=f))
  return ((uc.reg_read(UA.UC_ARM_REG_R3) << 32)
          | uc.reg_read(UA.UC_ARM_REG_R2)), uc.reg_read(UA.UC_ARM_REG_R1)


def qualify():
  """Run the checks in throwaway instances; refuse on any mismatch."""
  unicorn, capstone, UA = libs()
  model = new_uc(unicorn, UA).ctl_get_cpu_model()
  for want in (1, 0):
    uc = _run_snippet(unicorn, UA,
                      ENC["msr_primask_r0"] + ENC["mrs_r1_primask"],
                      dict(r0=want, r1=0xFFFFFFFF))
    if uc.reg_read(UA.UC_ARM_REG_R1) != want:
      raise Refused("PRIMASK round trip failed: not ARMv7-M behaviour")
  bad = []
  for case in FP_CASES:
    got, fpscr = fp_case(unicorn, UA, case)
    if (case[6] is not None and got != case[6]) or fpscr != case[7]:
      bad.append(f"{case[0]}: got {got:#x} fpscr {fpscr:#010x}")
  if bad:
    raise Refused("FP qualification failed: " + "; ".join(bad[:3]))
  # (1 + 2^-23) * (1 - 2^-23) - 1 is -2^-46 exactly when fused.
  code = (ENC["vmov_s0_r0"] + ENC["vmov_s1_r1"] + VMOV_S2_R2 + VFMA_S0_S1_S2 +
          ENC["vmov_r2_s0"])
  uc = _run_snippet(unicorn, UA, code,
                    dict(r0=0xBF800000, r1=0x3F800001, r2=0x3F7FFFFE))
  if uc.reg_read(UA.UC_ARM_REG_R2) != 0xA8800000:
    raise Refused("vfma.f32 is not fused under this unicorn")
  lib = sorted((Path(unicorn.__file__).parent / "lib").glob("libunicorn.so*"))
  return dict(unicorn=unicorn.__version__,
              capstone=capstone.__version__,
              libunicorn_sha256=(hashlib.sha256(
                  lib[0].read_bytes()).hexdigest() if lib else None),
              cpu="cortex-m7",
              cpu_model=model,
              fp_cases=len(FP_CASES),
              vfma="fused",
              cpacr="not modelled")
