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
"""FillBuffer on a booted system: the firmware's own boot, driven by blocks.

The boot is the firmware's: Reset_Handler up to `main`, then `main` itself
until its loop head (the instruction after `bl Init`), so `Init()` builds every
object (processor, synth, AGC, UI, settings, the ADC smoother, the
normalization detector) as on the module. Settings are read from a synthetic
flash bank by the real `Settings::Init`. The hardware is the recording stub
plus these seams:

  System::Init            clocks, MPU, caches: polls PLL/voltage flags
  HAL_ADCEx_Calibration_Start   spins on ADCAL
  HAL_GetTick             returns a counter that advances per call, so every
                          HAL timeout completes
  Codec::Start            runs, and its arguments are recorded: the sample
                          rate, the block size and the callback. The harness
                          then calls THAT callback with THAT block size.
  USART1 ISR              enabled and idle
  GPIO input registers    all ones except the scripted switches
  GPIOE BSRR/ODR          followed for PE15, the normalization probe's drive

Per codec block the harness writes the GPIO words, the ADC DMA words (CV and
pots, at the symbols' addresses), the gate bytes and the input frames, calls
the callback as the DMA interrupt does, calls SysTick_Handler every fourth
block (1 ms), and resumes `main`'s loop body every fourth block.
"""

import struct

from repro.emu import inputs as E
from repro.emu import machine as M
from repro.emu.elf import ElfError

SCR = M.SCRATCH
GATE, RX, TX = SCR + 0x1000, SCR + 0x1100, SCR + 0x1200
# Peripheral addresses are the chip's, not the firmware's.
USART1_ISR, USART1_RDR = 0x4001101C, 0x40011024
USART1_IDLE = (1 << 22) | (1 << 21) | (1 << 7) | (1 << 6)
USART1_RXNE = 1 << 5

FUNCTIONS = {
    "main": "main",
    "init": "_Z4Initv",
    "boot_init": "_ZN5beads6System4InitEb",
    "adc_cal": "HAL_ADCEx_Calibration_Start",
    "get_tick": "HAL_GetTick",
    "systick": "SysTick_Handler",
    "codec_start":
    "_ZN5beads5Codec5StartEljPFvPKNS0_5EventEPKNS0_5FrameEPS4_jE",
}
PROCESS_PREFIX = "_Z7Process"  # the audio callback (a free function)
OBJECTS = {
    "processor": "processor",
    "settings": "settings",
    "ui": "ui",
    "synth": "synth",
    "gain_control": "gain_control",
    "synth_mode": "synth_mode"
}
CV_VALUES = "_ZN5beads5CvAdc7values_E"
POT_VALUES = "_ZN5beads7PotsAdc7values_E"
RNG_STATE = "_ZN6stmlib6Random10rng_state_E"

# The synthetic flash bank: stmlib's chunk storage in sector 15 of the
# second bank, a CALI chunk then a STAT chunk. Settings::Init reading these
# back is checked at boot.
BANK2, BANK2_SIZE, SECTOR15 = 0x08100000, 0x100000, 0xE0000
CALI_TAG, STAT_TAG = 0x494C4143, 0x54415453
# Synthetic calibration of the golden fixtures: CvAdcChannel name ->
# (scale, offset).
DEFAULT_CAL = {
    "ASSIGN": (-2.02, 1.01),
    "DENSITY": (-1.98, 0.99),
    "PITCH": (-119.7, 59.85),
    "TIME": (-3.19, 1.595),
    "SIZE": (-3.21, 1.605),
    "SHAPE": (-3.18, 1.59)
}
DEFAULT_STATE = dict(media=3,
                     assignable_cv_destination=1,
                     assignable_cv_amount=(255, 64, 128),
                     gain=64,
                     gate_output_mode=0,
                     frozen=0,
                     disable_autosave=0,
                     monophonic_mode=0)


def chunk(tag, payload):
  return struct.pack("<IHH", tag, len(payload),
                     (~sum(payload)) & 0xFFFF) + payload


def state_bytes(layout, state):
  """The State struct with `state` field values at their DWARF offsets."""
  out = bytearray(layout.size("State"))
  members = layout.members("State")
  for name, value in state.items():
    if name not in members:
      raise ElfError(f"State has no field {name}")
    vals = value if isinstance(value, (tuple, list)) else (value, )
    for i, v in enumerate(vals):
      out[members[name] + i] = v
  return bytes(out)


def settings_bank(layout, state, cal=DEFAULT_CAL):
  """A 1 MiB bank 2 image holding valid CALI and STAT chunks. `cal` maps a
    CvAdcChannel name to (scale, offset)."""
  from repro.emu.panel import calibration_list
  cal = calibration_list(layout, cal)
  persistent = bytearray(layout.size("PersistentData"))
  base = layout.offset("PersistentData", "adc_calibration")
  fields = layout.members("AdcCalibrationData")
  stride = layout.size("AdcCalibrationData")
  for i, (scale, offset) in enumerate(cal):
    struct.pack_into("<f", persistent, base + i * stride + fields["scale"],
                     scale)
    struct.pack_into("<f", persistent, base + i * stride + fields["offset"],
                     offset)
  image = bytearray(b"\xff" * BANK2_SIZE)
  cali = chunk(CALI_TAG, bytes(persistent))
  stat = chunk(STAT_TAG, state_bytes(layout, state))
  image[SECTOR15:SECTOR15 + len(cali)] = cali
  off = SECTOR15 + 128
  image[off:off + len(stat)] = stat
  return bytes(image)


def call_site(image, base, caller, callee, span=0x400):
  """The return address of the first `bl callee` in `caller`."""
  import capstone
  md = capstone.Cs(capstone.CS_ARCH_ARM,
                   capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)
  code = image[caller - base:caller - base + span]
  for ins in md.disasm(code, caller):
    if ins.mnemonic == "bl" and int(ins.op_str.lstrip("#"), 16) == callee:
      return ins.address + 4
  raise ElfError(f"no bl {callee:#x} in {caller:#x}")


class Firmware:

  def __init__(self,
               elf,
               state=None,
               cal=DEFAULT_CAL,
               itcm=b"",
               init_fpscr=M.FZ,
               name="drizzlet",
               autostart=False):
    self.elf = elf
    layout = elf.layout()
    self.layout = layout
    self.state = dict(DEFAULT_STATE, **(state or {}))
    self.cal = cal
    base, image = elf.flash_image()
    self.fn = {k: elf.func(v) for k, v in FUNCTIONS.items()}
    self.fn["process"] = elf.func_with_prefix(PROCESS_PREFIX)
    self.obj = {k: elf.obj(v) for k, v in OBJECTS.items()}
    self.cv_values = elf.obj(CV_VALUES, 12)
    self.pot_values = elf.obj(POT_VALUES, 24)
    self.rng_state = elf.obj(RNG_STATE, 4)
    bank = settings_bank(layout, self.state, cal)
    self.m = m = M.Machine(name, image, base, [(BANK2, bank)], itcm)
    uc, UA = m.uc, m.UA
    for a in E.GPIO_IDR:
      m.mmio[a] = 0xFFFF
    m.mmio[USART1_ISR] = USART1_IDLE
    self.tick = 0
    self.pin = 0  # PE15, the probe's drive (ODR resets to 0)
    orig_write, orig_read = m._mmio_write, m._mmio_read

    def receive(uc, offset, size, base):
      value = orig_read(uc, offset, size, base)
      if base + offset == USART1_RDR:
        m.mmio[USART1_ISR] = m.mmio.get(USART1_ISR, USART1_IDLE) \
            & ~USART1_RXNE
      return value

    def write(uc, offset, size, value, base):
      self.pin = E.pin_level(self.pin, base + offset, size, value)
      orig_write(uc, offset, size, value, base)

    m.remap_mmio(receive, write)
    self.stack_top, _ = m.boot(self.fn["main"])

    def ret(uc, value):
      uc.reg_write(UA.UC_ARM_REG_R0, value)
      uc.reg_write(UA.UC_ARM_REG_PC, uc.reg_read(UA.UC_ARM_REG_LR))

    def tick(uc, addr, size, _):
      self.tick += 1
      ret(uc, self.tick)

    for role, hook in (("get_tick", tick), ("boot_init",
                                            lambda uc, a, s, _: ret(uc, 0)),
                       ("adc_cal", lambda uc, a, s, _: ret(uc, 0))):
      uc.hook_add(m.unicorn.UC_HOOK_CODE,
                  hook,
                  begin=self.fn[role],
                  end=self.fn[role])
    self.block_size, self.callback = None, None
    self.codec = []
    self.block_index = -1

    def start(uc, addr, size, _):
      rate, n, cb = (uc.reg_read(UA.UC_ARM_REG_R1 + i) for i in range(3))
      self.codec.append((self.block_index, rate, n, cb & ~1))
      self.block_size, self.callback = n, cb & ~1

    uc.hook_add(m.unicorn.UC_HOOK_CODE,
                start,
                begin=self.fn["codec_start"],
                end=self.fn["codec_start"])
    self.loop_head = call_site(image, base, self.fn["main"], self.fn["init"])
    armed = [False]

    def head(uc, addr, size, _):
      if armed[0]:
        uc.emu_stop()
      armed[0] = True

    self._armed = armed
    uc.hook_add(m.unicorn.UC_HOOK_CODE,
                head,
                begin=self.loop_head,
                end=self.loop_head)
    m.set_fpscr(init_fpscr)
    uc.reg_write(UA.UC_ARM_REG_LR, M.STOP | 1)
    self._run_main(self.fn["main"], sp=self.stack_top)
    self.init_fpscr = init_fpscr
    self.ctx = uc.context_save()
    self.passes = 0
    self.check_settings()
    if autostart:
      # The first pass of main's loop starts the codec for the stored
      # media, as the module does a moment after power-on.
      for _ in range(16):
        if self.codec:
          break
        self.main_pass()
      if self.callback != self.fn["process"]:
        raise ElfError("the codec did not start with the audio callback")

  # ------------------------------------------------------------ settings
  def check_settings(self):
    """Settings::Init must have read the fixture back, not defaults."""
    got = self.read_state()
    want = state_bytes(self.layout, self.state)
    if got != want:
      raise ElfError(f"Settings::Init did not load the fixture: "
                     f"{got.hex()} != {want.hex()}")

  def state_addr(self, field=None):
    a = self.obj["settings"] + self.layout.offset("Settings", "state_")
    return a if field is None else a + self.layout.offset("State", field)

  def read_state(self):
    return self.m.read(self.state_addr(), self.layout.size("State"))

  def write_state_field(self, field, value):
    self.m.write(self.state_addr(field), bytes([value]))

  def processor_field(self, name, size=4):
    a = self.obj["processor"] + self.layout.offset("Processor", name)
    return self.m.read(a, size)

  def in_delay_mode(self):
    """The processor's delay-mode flag: the transition is above zero."""
    return struct.unpack(
        "<f", self.processor_field("delay_mode_transition_"))[0] > 0.0

  def seed_rng(self, seed):
    self.m.w32(self.rng_state, seed)

  # ------------------------------------------------------------ running
  def _run_main(self, start, sp=None):
    m, UA = self.m, self.m.UA
    self._armed[0] = start != self.loop_head
    try:
      if sp is not None:
        m.uc.reg_write(UA.UC_ARM_REG_SP, sp)
      m.uc.emu_start(start | 1, M.STOP, count=200_000_000)
    except m.unicorn.UcError as e:
      raise M.MachineError(
          f"{e} at pc {m.uc.reg_read(UA.UC_ARM_REG_PC):#010x} in main"
      ) from None
    pc = m.uc.reg_read(UA.UC_ARM_REG_PC)
    if pc != self.loop_head:
      raise M.MachineError(f"main stopped at {pc:#010x}, not its loop "
                           f"head {self.loop_head:#010x}")

  def main_pass(self):
    """One resumed pass of main's loop body."""
    m = self.m
    m.uc.context_restore(self.ctx)
    m.set_fpscr(self.init_fpscr)
    m.uc.context_restore(self.ctx)  # set_fpscr ran a snippet
    self._run_main(self.loop_head)
    self.ctx = m.uc.context_save()
    self.passes += 1

  def set_inputs(self, idr):
    for a, w in idr.items():
      self.m.mmio[a] = w

  def block(self, rx, gates, cv, pots, audio_fpscr=M.FZ, systick=False):
    """One DMA interrupt at the current block size, into the current
        callback. `rx` is packed (l, r) int32 pairs, `gates` one byte per
        frame, `cv` six and `pots` twelve ADC words. Returns the TX words."""
    m, n = self.m, self.block_size
    if n is None:
      raise M.MachineError("the codec has not been started")
    m.write(self.cv_values, struct.pack("<6H", *cv))
    m.write(self.pot_values, struct.pack("<12H", *pots))
    m.write(GATE, gates)
    m.write(RX, rx)
    m.write(TX, b"\xa5" * (8 * n))
    m.call(self.callback, [GATE, RX, TX, n], fpscr=audio_fpscr)
    if systick:
      m.call(self.fn["systick"], [], fpscr=audio_fpscr)
    return m.read(TX, 8 * n)


def pack_rx(frames):
  return b"".join(struct.pack("<ii", *v) for v in frames)
