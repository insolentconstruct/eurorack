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

"""One Cortex-M7 under Unicorn with a firmware image loaded.

Memory is the STM32H743's: flash at 0x08000000 (2 MiB, erased 0xFF), ITCM,
DTCM, AXI SRAM, SRAM1-3 and SRAM4. Peripherals (0x40000000..0x5FFFFFFF) and
the system region (0xE0000000..) are a recording stub: a read returns the last
word written there (0 before), and every access is counted by address, so a
run can prove it touched no hardware.

A call puts the arguments in r0-r3 and on the stack (AAPCS), sets FPSCR by
the guest, and runs until the return sentinel. An unmapped access, a fault or
an exhausted instruction budget raises MachineError with the PC.
"""

from collections import Counter
import struct

from repro.emu import qualify as Q

FLASH, FLASH_SIZE = 0x08000000, 0x200000
RAM_WINDOWS = (  # (base, size, name): the H743 windows
    (0x00000000, 0x10000, "itcm"),
    (0x20000000, 0x20000, "dtcm"),
    (0x24000000, 0x80000, "axi"),
    (0x30000000, 0x48000, "sram1-3"),
    (0x38000000, 0x10000, "sram4"),
    (0x38800000, 0x1000, "backup"),
)
MMIO_WINDOWS = ((0x40000000, 0x20000000, "periph"),
                (0xE0000000, 0x100000, "system"))
SCRATCH, SCRATCH_SIZE = 0x0FF00000, 0x10000  # driver buffers, outside the device
SNIPPET = SCRATCH + SCRATCH_SIZE - 0x100      # guest FPSCR set/read
STOP = SCRATCH + SCRATCH_SIZE - 0x10          # return sentinel (lr = STOP | 1)
CALL_STACK_GAP = 0x100  # calls run just below the initial stack pointer
FZ, DN = Q.FZ, Q.DN


class MachineError(Exception):
    pass


class Machine:
    """A booted image. `image` is the raw flash bytes loaded at `base`;
    `patches` are (address, bytes) written into flash; `itcm` bytes are
    written at address 0."""

    def __init__(self, name, image, base, patches=(), itcm=b""):
        self.name = name
        self.unicorn, _, self.UA = Q.libs()
        self.uc = Q.new_uc(self.unicorn, self.UA)
        uc = self.uc
        uc.mem_map(FLASH, FLASH_SIZE)
        uc.mem_write(FLASH, b"\xff" * FLASH_SIZE)
        if base < FLASH or base + len(image) > FLASH + FLASH_SIZE:
            raise MachineError(f"{name}: image does not fit in flash at {base:#x}")
        uc.mem_write(base, bytes(image))
        self.base = base
        # The initial stack pointer is the first word of the vector table.
        self.stack_top = struct.unpack_from("<I", bytes(image), 0)[0]
        self.call_sp = (self.stack_top - CALL_STACK_GAP) & ~7
        for addr, data in patches:
            uc.mem_write(addr, bytes(data))
        for b, s, _ in RAM_WINDOWS:
            uc.mem_map(b, s)
        if itcm:
            uc.mem_write(0, bytes(itcm))
        uc.mem_map(SCRATCH, SCRATCH_SIZE)
        self.mmio = {}
        self.mmio_log = Counter()
        for b, s, _ in MMIO_WINDOWS:
            uc.mmio_map(b, s, self._mmio_read, b, self._mmio_write, b)
        self.fpscr = None

    # ------------------------------------------------------------ MMIO stub
    def _mmio_read(self, uc, offset, size, base):
        a = base + offset
        self.mmio_log[("r", a)] += 1
        word = self.mmio.get(a & ~3, 0)
        return (word >> (8 * (a & 3))) & ((1 << (8 * size)) - 1)

    def _mmio_write(self, uc, offset, size, value, base):
        a = base + offset
        self.mmio_log[("w", a)] += 1
        mask = ((1 << (8 * size)) - 1) << (8 * (a & 3))
        old = self.mmio.get(a & ~3, 0)
        self.mmio[a & ~3] = (old & ~mask) | ((value << (8 * (a & 3))) & mask)

    def remap_mmio(self, read, write):
        """Replace the peripheral stub's callbacks (read(uc, off, size, base),
        write(uc, off, size, value, base))."""
        for b, s, _ in MMIO_WINDOWS:
            self.uc.mem_unmap(b, s)
            self.uc.mmio_map(b, s, read, b, write, b)

    # ------------------------------------------------------------ memory
    def read(self, addr, n):
        return bytes(self.uc.mem_read(addr, n))

    def write(self, addr, data):
        self.uc.mem_write(addr, bytes(data))

    def u32(self, addr):
        return struct.unpack("<I", self.read(addr, 4))[0]

    def w32(self, addr, v):
        self.write(addr, struct.pack("<I", v & 0xFFFFFFFF))

    # ------------------------------------------------------------ execution
    def set_fpscr(self, value):
        """Written by the guest and read back by the guest."""
        UA = self.UA
        code = Q.ENC["vmsr_fpscr_r0"] + Q.ENC["vmrs_r1_fpscr"]
        self.uc.mem_write(SNIPPET, code)
        self.uc.reg_write(UA.UC_ARM_REG_R0, value)
        self.uc.emu_start(SNIPPET | 1, SNIPPET + len(code))
        got = self.uc.reg_read(UA.UC_ARM_REG_R1)
        if (got ^ value) & 0xF7C0009F:
            raise MachineError(f"{self.name}: guest FPSCR reads {got:#010x} "
                               f"after writing {value:#010x}")
        self.fpscr = value

    def run(self, entry, until, sp=None, budget=0):
        UA = self.UA
        if sp is not None:
            self.uc.reg_write(UA.UC_ARM_REG_SP, sp)
        try:
            self.uc.emu_start(entry | 1, until, count=budget)
        except self.unicorn.UcError as e:
            pc = self.uc.reg_read(UA.UC_ARM_REG_PC)
            raise MachineError(f"{self.name}: {e} at pc {pc:#010x} "
                               f"(entry {entry:#010x})") from None
        pc = self.uc.reg_read(UA.UC_ARM_REG_PC)
        if pc != until:
            raise MachineError(f"{self.name}: stopped at pc {pc:#010x}, not "
                               f"{until:#010x} (entry {entry:#010x}; budget "
                               f"{budget} exhausted?)")

    def call(self, entry, args=(), fpscr=None, budget=50_000_000):
        """AAPCS call of the Thumb function at `entry`; returns r0."""
        UA = self.UA
        if fpscr is not None and fpscr != self.fpscr:
            self.set_fpscr(fpscr)
        sp = (self.call_sp - 4 * max(0, len(args) - 4)) & ~7
        if len(args) > 4:
            self.write(sp, struct.pack(f"<{len(args) - 4}I",
                                       *[a & 0xFFFFFFFF for a in args[4:]]))
        for i, a in enumerate(args[:4]):
            self.uc.reg_write(UA.UC_ARM_REG_R0 + i, a & 0xFFFFFFFF)
        self.uc.reg_write(UA.UC_ARM_REG_LR, STOP | 1)
        self.run(entry, STOP, sp=sp, budget=budget)
        return self.uc.reg_read(UA.UC_ARM_REG_R0)

    def boot(self, main, budget=5_000_000):
        """Run the vector table's Reset_Handler (.data copy, .bss clear,
        constructors) up to `main`, at the reset FPSCR (0)."""
        sp0, reset = struct.unpack("<2I", self.read(self.base, 8))
        self.set_fpscr(0)
        self.run(reset & ~1, main, sp=sp0, budget=budget)
        return sp0, reset & ~1
