# Code style

Follow upstream Mutable Instruments master: Beads (2021) first, then Plaits,
Stages and Marbles. Use Clouds effects as the diffuser/reverb idiom. These
rules come after those sources; leave unrelated existing code alone.

## Layout

- 2-space indent, no tabs in C/C++. `public:` and `private:` get one space. Braces on the same line; empty bodies are `{ }`. Aim for 80 columns.
- Put firmware code in `namespace beads` and close it with `}  // namespace beads`. Host test functions may be global, as in MI tests.
- New headers use `BEADS_<PATH>_H_` guards, e.g. `BEADS_DSP_LIMITER_H_`.
- Includes, usually: the file's own header, then `<system>`, `stmlib/...`, `beads/...`.

## File and folder structure

- Follow the [released Beads source](https://github.com/pichenettes/eurorack/tree/d8f4739/beads) first. Keep the entry point (`beads.cc`), UI, settings and module coordination at the top level; hardware access in `drivers/`, audio processing in `dsp/`, effects in `dsp/fx/`.
- Keep resource generators and inputs in `resources/`, host tests in `test/`, boot code in `bootloader/`, board and panel files in `hardware_design/`, and standalone generators in `tools/`. Keep makefiles at the module root, `bootloader/makefile` and `test/makefile`.
- Follow the module's existing folders. Plaits has `dsp/engine/`, `dsp/oscillator/` and other DSP groups; Stages keeps processing classes at the top level, Marbles uses `random/` and `ramp/`, and Tides 2 uses `ramp/`. Don't impose Beads' tree on them.
- Use `lower_snake_case.h` / `.cc` matching the main class (`DelayClock` in `dsp/delay_clock.*`). Pair declarations with out-of-line definitions; small inline DSP helpers and templates stay header-only (`dsp/delay_line.h`), as do module helpers (`pot_controller.h`). Don't add a `.cc` just for symmetry.
- Usually keep one main class per file. Group small related types with their owner (`Processor::Media` and `MixParameters` in `dsp/processor.h`; `RandomWalkModulation` and `RandomizedParameter` in `dsp/randomized_parameter.h`). Keep shared data in the owning header (`settings.h`, `dsp/frame.h`); use a shared parameters header when needed, as in older Clouds/Warps `dsp/parameters.h`, rather than one file per enum.
- Keep generated `resources.cc` / `.h` at the module root. Change `resources/resources.py`, its generators or inputs and regenerate with `make -f beads/makefile resources` (add `-B` after changing a non-Python input such as `waves.bin`); don't hand-edit the output.
- Follow the usual Beads header order: licence and author, separator, brief description, guard, includes, namespace, constants and related types, class. Put public constructor/destructor, `Init`, methods and accessors before private helpers and members; usually finish with `DISALLOW_COPY_AND_ASSIGN`. Nearby files may interleave methods and accessors or nest types.
- MI tests are normally flat: one `<module>_test.cc` and `makefile`, with helper headers such as Stages/Marbles `test/fixtures.h` and Marbles `test/ramp_checker.h`. Tides 2 calls its program `tides_test.cc`; Beads also has `test/plot_distributions.py`.
- Treat file splits and source order as build changes. The module makefile's `PACKAGES` selects folders; `stmlib/makefile.inc` expands their `.cc` files into `CC_FILES` and ordered objects (host makefiles list sources explicitly). Adding a `.cc` to a selected folder adds a translation unit and changes the link; moving one to a new folder needs a package entry. Preserve order and unique object basenames when matching firmware.

### Drizzlet test exceptions

- Keep several focused test programs with their own `<name>.mk` files so sanitizer builds, UI variants and hardware replacements can use separate flags, sources and object directories. Share small helpers in headers such as `test_checker.h`.
- Keep `test/*_stubs/` for host tests only; programs may share them (the granular and processor tests use the options stubs). Their nested paths mirror firmware includes so an earlier `-I` can replace hardware headers without changing firmware code.
- Keep `test/mutation_check.py` beside the tests; it checks that deliberate faults fail in disposable copies. This tooling and the build kit in `build/` are Drizzlet additions, not MI folder conventions.

## Names

- Types and methods: `PascalCase` (`Process`, `Init`). Accessors, locals and parameters: `lower_case`.
- Private members end in `_` (`peak_lp_`); public struct fields usually don't.
- Put new named constants at namespace scope, using `kPascalCase`. Named enum values and macros: `ALL_CAPS`, enum values with a common prefix (`MEDIA_DIGITAL`).

## Classes

- Most stateful classes have empty constructors, set up in `Init()`, and end with `DISALLOW_COPY_AND_ASSIGN(Name);`.
- Small DSP classes usually live in the header. Follow how nearby files are split.
- Templates are used for fixed DSP variants (sample resolution, buffer size, channel count).

## Code

- Pass new arguments explicitly rather than adding default arguments.
- C++98-era syntax: no `auto`, `nullptr`, lambdas or `constexpr`.
- Prefer `const` for locals that don't change, and `bool` for flags.
- Loops: `while (size--)` with a pointer, or an indexed `for`; indices are usually `size_t` or `int`.
- Use the stmlib helpers: `CONSTRAIN`, `ONE_POLE`, `SLOPE`, `ParameterInterpolator`, the `Interpolate*` functions and lookup tables.
- Numeric casts: `float(x)` or `static_cast<T>(x)`.
- New float math uses `f` (`0.5f`). The existing code isn't strict about this, so leave existing literals alone.
- Initialize struct fields one by one, as in Beads codec frames.
- Name a constant when it has meaning; small obvious numbers can stay inline.

## Performance

From Mutable Instruments ([STM32 miscellania](https://pichenettes.github.io/mutable-instruments-documentation/tech_notes/stm32_miscellania/) and their code):

- Specialize hot DSP on configuration values with template parameters (Clouds' grain renderer, stmlib's filter), and measure, because it can be slower.
- Do control work once per block and audio work per sample (`ParameterInterpolator`, lookup tables).
- Keep loop state in locals so it stays in registers.
- Prefer a data-format fix (for example fixed-point delay addresses) over micro-tuning.
- Design for 80 to 95% of the CPU, with 99% of the block as the hard ceiling.

Drizzlet adds: an audio-path change needs a measured cycle count and the sound check in [OPTIMIZATIONS.md](OPTIMIZATIONS.md) (bit-identical, or a flagged difference below -90 dBFS).

## Comments

- Brief `//` comments that explain intent. Short labels and units are fine.
- New files use the existing header format with your own name: `// Copyright YEAR Your Name.`, `// Author: Your Name (email)`, then the MIT notice. Code copied or adapted from Mutable Instruments files keeps its original header.

## Project policy

- Don't edit the copyright headers of existing files.
- Keep stock behaviour unchanged unless the change is the point. New options default to stock.
- No compiler hints such as `__builtin_expect`, `noinline` or `flatten`.
- Behaviour changes get a line in CHANGELOG.md.

## Tests

- Use `void TestXxx()` functions called from `main()`.
- Print `ERROR: <test>: <condition>` and count failures; return non-zero on failure.
- Keep compile-time layout checks and sanitizer coverage.
- Preserve the file's licence. For new host files, use the Beads/Clouds MIT
  header, separator and brief description. Upstream Plaits, Marbles and Stages
  tests instead retain GPL headers without a separator; do not replace them.
- Use one Beads-style makefile per program; run `make -f beads/test/<name>.mk test` from the root. Options also runs the advanced build.
- CI runs every `beads/test/*.mk` as its own job, and `mutation_check.py` weekly; a new program needs no workflow change.
- Make recipes use tabs, as in upstream test makefiles.
