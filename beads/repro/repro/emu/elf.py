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

"""Symbols, flash image and DWARF layouts of a firmware ELF (pyelftools).

Everything the emulator needs to know about the firmware comes from here: no
address, field offset or enumerator value is written down elsewhere.
"""

from collections import namedtuple
import hashlib
import io
from pathlib import Path

from elftools.elf.elffile import ELFFile

FLASH_LO, FLASH_HI = 0x08000000, 0x08200000
RAM_WINDOWS = ((0x20000000, 0x20020000), (0x24000000, 0x24080000),
               (0x30000000, 0x30048000), (0x38000000, 0x38010000))
SHF_WRITE = 1

Sym = namedtuple("Sym", "name value size type bind shndx writable")


class ElfError(RuntimeError):
    pass


def _attr(die, name, default=None):
    a = die.attributes.get(name)
    return a.value if a is not None else default


class Elf:
    def __init__(self, path):
        self.path = Path(path)
        self.raw = self.path.read_bytes()
        self.sha256 = hashlib.sha256(self.raw).hexdigest()
        self._file = ELFFile(io.BytesIO(self.raw))
        self.symbols = {}
        sections = list(self._file.iter_sections())
        symtab = self._file.get_section_by_name(".symtab")
        if symtab is None:
            raise ElfError(f"{path}: no symbol table")
        for s in symtab.iter_symbols():
            if not s.name:
                continue
            idx = s["st_shndx"]
            writable = (isinstance(idx, int) and 0 < idx < len(sections)
                        and bool(sections[idx]["sh_flags"] & SHF_WRITE))
            self.symbols.setdefault(s.name, []).append(Sym(
                s.name, s["st_value"], s["st_size"],
                s["st_info"]["type"], s["st_info"]["bind"], idx, writable))
        self._layout = None

    # ------------------------------------------------------------ symbols
    def defined(self, name, kind):
        """The one defined symbol `name` of type `kind` ("FUNC", "OBJECT" or
        "NOTYPE"); anything else is an error."""
        want = "STT_" + kind
        every = self.symbols.get(name, [])
        hits = [s for s in every if s.type == want and s.shndx != "SHN_UNDEF"]
        if len(hits) != 1:
            seen = ", ".join(f"{s.type}@{s.value:#x}" for s in every) or "none"
            raise ElfError(f"{name}: {len(hits)} defined {kind} symbols, "
                           f"want one (seen: {seen})")
        return hits[0]

    def func(self, name):
        """Entry address (Thumb bit cleared)."""
        return self.defined(name, "FUNC").value & ~1

    def has(self, name):
        return name in self.symbols

    def obj(self, name, size=None):
        """Address of the one defined object `name`, in writable RAM."""
        s = self.defined(name, "OBJECT")
        if size is not None and s.size != size:
            raise ElfError(f"{name}: symbol size {s.size:#x}, want {size:#x}")
        if not s.writable or not any(
                lo <= s.value and s.value + s.size <= hi
                for lo, hi in RAM_WINDOWS):
            raise ElfError(f"{name}: {s.value:#x} is not in writable RAM")
        return s.value

    def func_with_prefix(self, prefix):
        """The one defined function whose name starts with `prefix`."""
        hits = [s for v in self.symbols.values() for s in v
                if s.type == "STT_FUNC" and s.shndx != "SHN_UNDEF"
                and s.name.startswith(prefix)]
        if len(hits) != 1:
            raise ElfError(f"{prefix}*: {len(hits)} functions, want one")
        return hits[0].value & ~1

    # ------------------------------------------------------------ flash
    def flash_image(self):
        """(base, bytes) of every PT_LOAD segment whose physical address is in
        flash, as objcopy -O binary writes them."""
        segs = []
        for seg in self._file.iter_segments():
            h = seg.header
            if (h["p_type"] == "PT_LOAD" and h["p_filesz"]
                    and FLASH_LO <= h["p_paddr"] < FLASH_HI):
                segs.append((h["p_paddr"], seg.data()))
        if not segs:
            raise ElfError("no PT_LOAD segment in flash")
        base = min(a for a, _ in segs)
        out = bytearray(b"\xff" * (max(a + len(d) for a, d in segs) - base))
        for a, d in segs:
            out[a - base:a - base + len(d)] = d
        return base, bytes(out)

    # ------------------------------------------------------------ DWARF
    def layout(self):
        if self._layout is None:
            self._layout = Layout(self._file)
        return self._layout


class Layout:
    """Class sizes, member offsets and enumerators from the DWARF."""

    def __init__(self, elffile):
        dwarf = elffile.get_dwarf_info(relocate_dwarf_sections=False)
        if not dwarf.has_debug_info:
            raise ElfError("the ELF has no DWARF; build with -g")
        self._types = {}
        self.enums = {}
        self.enum_sizes = {}
        for cu in dwarf.iter_CUs():
            for die in cu.iter_DIEs():
                name = _attr(die, "DW_AT_name")
                size = _attr(die, "DW_AT_byte_size")
                if not (name and size) or _attr(die, "DW_AT_declaration"):
                    continue
                if die.tag in ("DW_TAG_class_type", "DW_TAG_structure_type"):
                    self._types.setdefault(name.decode(), die)
                elif die.tag == "DW_TAG_enumeration_type":
                    self.enum_sizes.setdefault(name.decode(), size)
                    self.enums.setdefault(name.decode(), {
                        _attr(c, "DW_AT_name").decode():
                        _attr(c, "DW_AT_const_value")
                        for c in die.iter_children()
                        if c.tag == "DW_TAG_enumerator"})

    def size(self, cls):
        return _attr(self._types[cls], "DW_AT_byte_size")

    def members(self, cls):
        """{member name: byte offset} of the class's own data members."""
        out = {}
        for c in self._types[cls].iter_children():
            if (c.tag == "DW_TAG_member"
                    and "DW_AT_data_member_location" in c.attributes):
                out[_attr(c, "DW_AT_name", b"?").decode()] = \
                    c.attributes["DW_AT_data_member_location"].value
        return out

    def offset(self, cls, member):
        try:
            return self.members(cls)[member]
        except KeyError:
            raise ElfError(f"{cls} has no member {member}") from None

    def enum_size(self, name):
        """Bytes one value of the enumeration takes."""
        self.enum(name)
        return self.enum_sizes[name]

    def enum(self, name):
        try:
            return self.enums[name]
        except KeyError:
            raise ElfError(f"no enumeration {name} in the DWARF") from None
