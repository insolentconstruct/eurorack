# Code style

Code changes should read like the existing Mutable Instruments code. When in doubt, copy the nearest existing file.

## Layout

- 2-space indent, no tabs in C/C++. `public:` and `private:` get one space. Braces on the same line; empty bodies are `{ }`. Aim for 80 columns.
- Put firmware code in `namespace beads` and close it with `}  // namespace beads`. Host test functions may be global, as in MI tests.
- New headers use `BEADS_<PATH>_H_` guards, e.g. `BEADS_DSP_LIMITER_H_`.
- Includes, usually: the file's own header, then `<system>`, `stmlib/...`, `beads/...`.

## Names

- Types and methods: `PascalCase` (`Process`, `Init`). Accessors, locals and parameters: `lower_case`.
- Private members end in `_` (`peak_lp_`); public struct fields usually don't.
- Constants: `kPascalCase`. Named enum values and macros: `ALL_CAPS`, enum values with a common prefix (`MEDIA_DIGITAL`).

## Classes

- Most stateful classes have empty constructors, set up in `Init()`, and end with `DISALLOW_COPY_AND_ASSIGN(Name);`.
- Small DSP classes usually live in the header. Follow how nearby files are split.
- Templates are used for fixed DSP variants (sample resolution, buffer size).

## Code

- C++98-era syntax: no `auto`, `nullptr`, lambdas or `constexpr`.
- Prefer `const` for locals that don't change, and `bool` for flags.
- Loops: `while (size--)` with a pointer, or an indexed `for`; indices are usually `size_t` or `int`.
- Use the stmlib helpers: `CONSTRAIN`, `ONE_POLE`, `SLOPE`, `ParameterInterpolator`, the `Interpolate*` functions and lookup tables.
- Numeric casts: `float(x)` or `static_cast<T>(x)`.
- New float math uses `f` (`0.5f`). The existing code isn't strict about this, so leave existing literals alone.
- Name a constant when it has meaning; small obvious numbers can stay inline.

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
- Use the options.h MIT header and a `// ---` separator with a brief description.
- Use one Beads-style makefile per program; run `make -f beads/test/<name>.mk test` from the root. Options also runs the advanced build.
- Make recipes use tabs, as in upstream test makefiles.
