# Blob math sources

The math copies use the pinned toolchain's algorithms, with the existing
`math_errf_no_errno.c` disabling errno writes. No approximation is substituted.

- Newlib 3.3.0: `libm/common/sinf.c`, `sincosf.h`, `sincosf_data.c`,
  `sf_expm1.c`, and `libm/math/sf_tanh.c`, copied unchanged.
  [Source](https://sourceware.org/git/?p=newlib-cygwin.git;a=tree;f=newlib/libm;hb=newlib-3.3.0).
- GCC 9.3.0: `libgcc/config/arm/lib1funcs.S`, `ieee754-df.S`,
  `ieee754-sf.S`, and `bpabi.S`, copied unchanged. `libgcc/convert.S`
  selects the double add/subtract and conversion group containing
  `__aeabi_l2d`, which `sinf` needs for large-argument reduction.
  [Source](https://gcc.gnu.org/git/?p=gcc.git;a=tree;f=libgcc/config/arm;hb=releases/gcc-9.3.0).

The conversion group's 888 code bytes equal the pinned toolchain's
`_arm_addsubdf3.o` (`3808cedaf2278b9cdf99b3c47879a8321d56b4ba89eeee01d026d4fb7578b9f6`).
It has no data, BSS or allocated relocations. Keeping the source in the blob
requires no archive exception in the gate.

Original notices remain in every copied file: Arm BSD-3-Clause for sine,
Sun's permissive notice for tanh/expm1, GPLv3 with GCC Runtime Library
Exception 3.1 for the conversion helper. Both GCC licence texts accompany it.
The Runtime Library Exception covers eligible compiled target code; these
source copies retain their original terms.

`beads/test/libm_blob_emu` gates a probe at three link bases, rejects its errno
variant, and compares it with the toolchain's libm under Cortex-M7 emulation.
