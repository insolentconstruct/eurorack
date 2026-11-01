# Code style

Follow upstream Mutable Instruments master: Beads (2021) first, then Plaits, Stages and Marbles; Clouds for the diffuser, reverb and phase vocoder. Rules marked (Drizzlet) are ours. Leave unrelated existing code alone.

## Layout

- Indent C and C++ with two spaces and no tabs. `public:` and `private:` get one space. Put braces on the same line; empty bodies are `{ }`. Aim for 80 columns.
- Bodies of `if`, `for` and `while` are braced on their own lines, one statement per line, and each data member has its own line. Beads has a few one-line `if`s (e.g. `drivers/codec.cc:221`, `beads.cc:361`, `dsp/mu_law.h:61`); don't add more.
- Put module code in `namespace beads` and close it with `}  // namespace beads`. Entry points (`beads.cc`, the bootloader) and host test functions stay global; specializations of stmlib templates go in `namespace stmlib` (`dsp/sample_rate_conversion_filters.h`).
- New headers use `BEADS_<PATH>_H_` guards, closed by `#endif  // BEADS_<PATH>_H_`.
- Usually include the file's own header first, then `<system>`, `stmlib/...` and `beads/...`, with blank lines between groups. Upstream grouping varies (`stmlib/stmlib.h` alone, HAL headers last in drivers); follow the nearby or ported file. `using namespace std;` / `stmlib` go inside the module namespace in module `.cc` files; entry points and host tests put them at global scope.

## File and folder structure

- Follow the Beads tree on upstream master: entry point (`beads.cc`), UI, settings and coordination at the top level; hardware in `drivers/`, audio in `dsp/`, effects in `dsp/fx/`, generators and inputs in `resources/`, host tests in `test/`, boot code in `bootloader/`, board files in `hardware_design/`, standalone generators in `tools/`. Makefiles stay at the module root, `bootloader/makefile` and `test/makefile`. Other modules keep their own folders; don't impose Beads' tree on them.
- Use `lower_snake_case.h` / `.cc` named after the main class (`DelayClock` in `dsp/delay_clock.*`). Declarations pair with out-of-line definitions; small inline DSP helpers and templates stay header-only (`dsp/delay_line.h`, `pot_controller.h`). Don't add a `.cc` for symmetry.
- Usually keep one main class per file; group small related types with their owner (`Processor::Media` in `dsp/processor.h`). Shared data stays in the owning header (`settings.h`, `dsp/frame.h`), not one file per enum.
- Keep generated `resources.cc` / `.h` at the module root. Edit `resources/resources.py` or its inputs and regenerate with `make -f beads/makefile resources` (add `-B` after changing a non-Python input such as `waves.bin`); never hand-edit the output.
- Order headers as follows: licence and author, separator, brief description, guard, includes, namespace, constants and related types, then the class. The public constructor and destructor, `Init`, methods and accessors come before private helpers and members; usually finish with `DISALLOW_COPY_AND_ASSIGN`.
- Mutable Instruments tests are flat: one `<module>_test.cc` and `makefile`, with helper headers such as `test/fixtures.h`.
- File splits and source order are build changes. `PACKAGES` in the module makefile selects folders; `stmlib/makefile.inc` expands their `.cc` files into ordered objects (host makefiles list sources explicitly). A new `.cc` in a selected folder adds a translation unit and changes the link; a new folder needs a package entry. Preserve order and unique object basenames when matching firmware.

### Drizzlet test exceptions

- (Drizzlet) Keep several focused test programs with their own `<name>.mk` files so sanitizer builds, UI variants and hardware replacements can use separate flags, sources and object directories. Share small helpers in headers such as `test_checker.h`.
- (Drizzlet) Keep `test/*_stubs/` for host tests only; programs may share them (the granular and processor tests use the options stubs). Their nested paths mirror firmware includes so an earlier `-I` can replace hardware headers without changing firmware code.
- (Drizzlet) Keep `test/mutation_check.py` beside the tests; it checks that deliberate faults fail in disposable copies. This tooling and the build kit in `build/` are Drizzlet additions, not Mutable Instruments folder conventions.

## Names

- Use `PascalCase` for types and methods (`Process`, `Init`). Use `lower_case` for accessors, locals and parameters. Ported helpers keep their names (`fast_p2r` in the phase vocoder). Common accessors are `inline T name() const`, setters `set_name(...)`, and mutable storage access `mutable_name()`; some getters omit `const`.
- Private members end in `_` (`peak_lp_`); public struct fields usually don't.
- Named scalar constants use `kPascalCase`, at namespace scope or as class-local `static const`. Local constants, tables and class-static arrays are often `lower_case` (`switch_definitions`, `long_press_duration`); follow the owning file. Enum values and macros use `ALL_CAPS`; enum values share a prefix (`MEDIA_DIGITAL`), and counted enums commonly end in `*_LAST`. Anonymous enums for compile-time traits use `lower_case` (`bundle_size` in `dsp/delay_line.h`, `length` in `dsp/fx/fx_engine.h`).

## Classes

- Most stateful classes have empty constructors, set up in `Init()`, and end with `DISALLOW_COPY_AND_ASSIGN(Name);`.
- Small DSP classes usually live in the header. Follow how nearby files are split.
- Templates configure fixed DSP variants (sample resolution, buffer size, channel count) at compile time, rather than providing compiler hints.
- Plain settings/frame structs commonly have public fields, no constructor and no `DISALLOW_COPY_AND_ASSIGN`; persistent records use `enum { tag = 0x... };`.
- Out-of-line static member definitions commonly have `/* static */` on the preceding line.
- Firmware uses global or class-static buffers, with section placement where needed, passed to `Init()`; no heap in firmware. Host tests may allocate dynamically.

## Code

- (Drizzlet) Pass new arguments explicitly rather than adding default arguments.
- Use `uint32_t`, `int32_t` and `size_t`, not `unsigned`, in new code (Beads has none outside a comment in `dsp/mu_law.h`).
- (Drizzlet) Keep `RandomizedParameter::value()` calls where they are in an argument list: a call may draw a random number, so moving one can change the sound.
- Use the C++98-era subset: no `auto`, `nullptr`, lambdas or `constexpr`; no exceptions or RTTI; no STL containers or virtual dispatch in per-sample code (`std::min`/`std::max` as in `limiter.h` and `std::fill` as in `gain_control.cc` are fine).
- (Drizzlet) Prefer `const` for new locals that don't change; upstream is not consistent. Use `bool` for flags.
- Use `while (size--)` with a pointer, or an indexed `for` loop. Indices are usually `size_t` or `int`.
- Use the stmlib helpers: `CONSTRAIN`, `ONE_POLE`, `SLOPE`, `ParameterInterpolator`, the `Interpolate*` functions and lookup tables.
- Numeric casts commonly use `T(x)` or `static_cast<T>(x)`. Drivers use C casts for register and DMA addresses, and the phase vocoder for reinterpreting its spectral storage.
- (Drizzlet) New float math uses `f` (`0.5f`). The existing code isn't strict about this, so leave existing literals alone.
- Set runtime frames, controls and settings field by field, as in Beads codec frames. Constant tables use aggregate initialization (`switch_definitions`, `Ui::media_palette_`).
- Name a constant when it has meaning; small obvious numbers can stay inline.

## Performance

Observed upstream DSP conventions and design principles:

- Use float on FPU parts; use lookup tables where measurement shows they are faster.
- Specialize configuration with template parameters and measure; specialization can be slower.
- Process audio in blocks, do control work once per block, and keep loop state in locals so it can stay in registers.
- Keep interrupt `Poll()` work constant-time; put deferred work in `DoEvents()` or the main loop.
- Prefer a data-format fix (for example fixed-point delay addresses) over micro-tuning.
- Spend spare CPU on audio-rate quality; measure the worst-case block cost and leave deadline headroom.
- Keep DSP portable and test it on the desktop.
- (Drizzlet) An audio-path change needs a measured cycle count and the sound check in [OPTIMIZATIONS.md](OPTIMIZATIONS.md) (bit-identical, or a flagged difference below -90 dBFS).

## Comments

- Brief `//` comments explain intent; trailing comments usually have two spaces before `//`. Short labels and units are fine. Measured-cost comments, honest hack labels, owned `TODO(...)` notes and retained commented profiling switches are useful upstream idioms, not required boilerplate.
- (Drizzlet) New files use the existing header format (`//` in C/C++, `#` in Python, shell and makefiles) with Insolent Construct attribution: `// Copyright YEAR Insolent Construct.`, `// Author: Insolent Construct`, then the MIT notice. Code copied or adapted from Mutable Instruments files keeps its original header.

## Project policy

- Don't edit the copyright headers of existing files.
- (Drizzlet) Keep stock behaviour unchanged unless the change is the point. New options default to stock.
- Mutable Instruments puts `packed`, `aligned` and section attributes directly on the type or declaration (`drivers/codec.h`, the buffers in `beads.cc`); stmlib's `IN_RAM` places code in RAM (Tides 2), and its interpolation helpers put `always_inline` on a separate declaration (`stmlib/utils/dsp.h`). Mutable Instruments module code never uses `__builtin_expect`, `noinline` or `flatten`; keep that restriction.
- (Drizzlet) Behaviour changes get a line in CHANGELOG.md.

## Tests

- Mutable Instruments host programs render WAVs or print values for listening/plotting; Beads comments out its test calls in `main()`.
- (Drizzlet) CI programs use `void TestXxx()` functions, print `ERROR: <test>: <condition>`, count failures and return non-zero; this adapts the printed-error checker idiom.
- (Drizzlet) Keep compile-time layout checks and sanitizer coverage.
- Preserve the file's licence. New host files use the Beads/Clouds MIT header
  and separator; (Drizzlet) add a brief description. Upstream Plaits, Marbles
  and Stages tests instead retain GPL headers without a separator; do not
  replace them.
- (Drizzlet) Use one makefile per program, following the host `beads/test/makefile` structure (`PACKAGES`, `VPATH`, explicit `CC_FILES`, `-DTEST -Wall -Werror`); run `make -f beads/test/<name>.mk test` from the root.
- (Drizzlet) `beads/test/ci-jobs.json` lists the CI jobs; a job may run several programs. Register a new program there. Shared fragments such as `plaits_sources.mk` are not programs. `mutation_check.py` runs weekly.
- Make recipes use tabs, as in upstream test makefiles.
- Firmware makefiles commonly keep the MIT header, aligned assignments, `PACKAGES` and `include stmlib/makefile.inc`; Beads gives an overridable `TOOLCHAIN_PATH ?=` default with compiler comparison comments. Host makefiles list sources explicitly and need not have the firmware header, toolchain default or include.

## Python

- Python follows MI's own, tools included (`beads/resources/wavetables.py`, `beads/tools/generate_src_filters.py`, `stmlib/tools/resources_compiler.py`, `stm_audio_bootloader/qpsk/encoder.py`): two-space indent, `UPPER_CASE` constants, `import numpy` (not `np`), one statement per line. Module-level functions usually use `snake_case`; methods follow their owning class, including `PascalCase` in stmlib's resource compiler (`ResourceEntry.Declare`, `DeclareAlias`, `ResourceTable.DeclareEntries`). (Drizzlet) The four-space scripts in `build/tools/` predate this rule; reindent one only when rewriting it.
