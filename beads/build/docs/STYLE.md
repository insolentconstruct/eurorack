# Code style

**TL;DR:** follow upstream Mutable Instruments master: Beads (2021) first, then Plaits, Stages and Marbles; Clouds effects for diffuser/reverb. Rules marked (Drizzlet) are ours. Leave unrelated existing code alone. This branch shares Drizzlet's style guide.

## Layout

- 2-space indent, no tabs in C/C++. `public:` and `private:` get one space. Braces on the same line; empty bodies are `{ }`. Aim for 80 columns.
- Put firmware code in `namespace beads` and close it with `}  // namespace beads`. Host test functions may be global, as in MI tests.
- New headers use `BEADS_<PATH>_H_` guards, closed by `#endif  // BEADS_<PATH>_H_`.
- Includes: the file's own header, then `<system>`, `stmlib/...`, `beads/...`, with blank lines between groups. `using namespace std;` / `stmlib` go inside the module namespace, usually in `.cc` files.

## File and folder structure

- Follow the Beads tree on upstream master: entry point (`beads.cc`), UI, settings and coordination at the top level; hardware in `drivers/`, audio in `dsp/`, effects in `dsp/fx/`, generators and inputs in `resources/`, host tests in `test/`, boot code in `bootloader/`, board files in `hardware_design/`, standalone generators in `tools/`. Makefiles stay at the module root, `bootloader/makefile` and `test/makefile`. Other modules keep their own folders; don't impose Beads' tree on them.
- Use `lower_snake_case.h` / `.cc` named after the main class (`DelayClock` in `dsp/delay_clock.*`). Declarations pair with out-of-line definitions; small inline DSP helpers and templates stay header-only (`dsp/delay_line.h`, `pot_controller.h`). Don't add a `.cc` for symmetry.
- Usually one main class per file; group small related types with their owner (`Processor::Media` in `dsp/processor.h`). Shared data stays in the owning header (`settings.h`, `dsp/frame.h`), not one file per enum.
- Keep generated `resources.cc` / `.h` at the module root. Edit `resources/resources.py` or its inputs and regenerate with `make -f beads/makefile resources` (add `-B` after changing a non-Python input such as `waves.bin`); never hand-edit the output.
- Header order: licence and author, separator, brief description, guard, includes, namespace, constants and related types, class. Public constructor/destructor, `Init`, methods and accessors come before private helpers and members; usually finish with `DISALLOW_COPY_AND_ASSIGN`.
- MI tests are flat: one `<module>_test.cc` and `makefile`, with helper headers such as `test/fixtures.h`.
- File splits and source order are build changes. `PACKAGES` in the module makefile selects folders; `stmlib/makefile.inc` expands their `.cc` files into ordered objects (host makefiles list sources explicitly). A new `.cc` in a selected folder adds a translation unit and changes the link; a new folder needs a package entry. Preserve order and unique object basenames when matching firmware.

### Drizzlet test exceptions

- (Drizzlet) Keep several focused test programs with their own `<name>.mk` files so sanitizer builds, UI variants and hardware replacements can use separate flags, sources and object directories. Share small helpers in headers such as `test_checker.h`.
- (Drizzlet) Keep `test/*_stubs/` for host tests only; programs may share them. Their nested paths mirror firmware includes so an earlier `-I` can replace hardware headers without changing firmware code.
- (Drizzlet) Keep `test/mutation_check.py` beside the tests; it checks that deliberate faults fail in disposable copies. This tooling and the build kit in `build/` are additions, not MI folder conventions.

## Names

- Types and methods: `PascalCase` (`Process`, `Init`). Accessors, locals and parameters: `lower_case`. Common accessors are `inline T name() const`, setters `set_name(...)`, and mutable storage access `mutable_name()`; some getters omit `const`.
- Private members end in `_` (`peak_lp_`); public struct fields usually don't.
- Named constants use `kPascalCase`, at namespace scope or as class-local `static const`. Enum values and macros use `ALL_CAPS`; enum values share a prefix (`MEDIA_DIGITAL`), and counted enums commonly end in `*_LAST`.

## Classes

- Most stateful classes have empty constructors, set up in `Init()`, and end with `DISALLOW_COPY_AND_ASSIGN(Name);`.
- Small DSP classes usually live in the header. Follow how nearby files are split.
- Templates configure fixed DSP variants (sample resolution, buffer size, channel count) at compile time, rather than providing compiler hints.
- Plain settings/frame structs commonly have public fields, no constructor and no `DISALLOW_COPY_AND_ASSIGN`; persistent records use `enum { tag = 0x... };`.
- Out-of-line static member definitions commonly have `/* static */` on the preceding line.
- Firmware uses global or class-static buffers, with section placement where needed, passed to `Init()`; no heap in firmware. Host tests may allocate dynamically.

## Code

- (Drizzlet) Pass new arguments explicitly rather than adding default arguments.
- C++98-era subset: no `auto`, `nullptr`, lambdas or `constexpr`; no exceptions or RTTI; no STL containers or virtual dispatch in per-sample code (`std::min`/`std::max` as in `limiter.h` and `std::fill` as in `gain_control.cc` are fine).
- Prefer `const` for locals that don't change, and `bool` for flags.
- Loops: `while (size--)` with a pointer, or an indexed `for`; indices are usually `size_t` or `int`.
- Use the stmlib helpers: `CONSTRAIN`, `ONE_POLE`, `SLOPE`, `ParameterInterpolator`, the `Interpolate*` functions and lookup tables.
- Numeric casts commonly use `T(x)` or `static_cast<T>(x)`. Drivers also use C casts for register and DMA addresses.
- (Drizzlet) New float math uses `f` (`0.5f`). The existing code isn't strict about this, so leave existing literals alone.
- Initialize struct fields one by one, as in Beads codec frames.
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
- (Drizzlet) An audio-path change keeps the audio reference byte-identical, or changes it on purpose with `UPDATE=1` and says why; see the [host test README](../../test/README.md).

## Comments

- Brief `//` comments explain intent; trailing comments usually have two spaces before `//`. Short labels and units are fine. Measured-cost comments, honest hack labels, owned `TODO(...)` notes and retained commented profiling switches are useful upstream idioms, not required boilerplate.
- (Drizzlet) New files use the existing header format with Insolent Construct attribution: `// Copyright YEAR Insolent Construct.`, `// Author: Insolent Construct`, then the MIT notice. Code copied or adapted from Mutable Instruments files keeps its original header.

## Project policy

- Don't edit the copyright headers of existing files.
- (Drizzlet) Keep stock behaviour unchanged unless the change is the point. New options default to stock.
- MI uses `packed`, `aligned`, section placement (`IN_RAM`, `.ramd*`, `.dma`) and `always_inline` on a separate declaration. MI module code never uses `__builtin_expect`, `noinline` or `flatten`; keep that restriction.
- (Drizzlet) Behaviour changes get a line in CHANGELOG.md.

## Tests

- MI host programs render WAVs or print values for listening/plotting; Beads comments out its test calls in `main()`.
- (Drizzlet) CI programs use `void TestXxx()` functions, print `ERROR: <test>: <condition>`, count failures and return non-zero; this adapts the printed-error checker idiom.
- (Drizzlet) Keep compile-time layout checks and sanitizer coverage.
- Preserve the file's licence. For new host files, use the Beads/Clouds MIT
  header, separator and brief description. Upstream Plaits, Marbles and Stages
  tests instead retain GPL headers without a separator; do not replace them.
- (Drizzlet) Add a test program by including `beads/test/suite.mk` from its own `<name>.mk`; see the [host test README](../../test/README.md).
- (Drizzlet) CI runs the host tests in one job, next to two parallel release builds that must match.
- Make recipes use tabs, as in upstream test makefiles.
- Firmware makefiles commonly keep the MIT header, aligned assignments, `PACKAGES` and `include stmlib/makefile.inc`; Beads gives an overridable `TOOLCHAIN_PATH ?=` default with compiler comparison comments. Host makefiles list sources explicitly and need not have the firmware header, toolchain default or include.
