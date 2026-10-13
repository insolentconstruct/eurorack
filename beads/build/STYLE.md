# Code style

Drizzlet changes should read like Émilie's code. When in doubt, copy the nearest existing file.

## Layout

- 2-space indent, no tabs. `public:` and `private:` get one space. Lines up to 80 columns.
- Braces on the same line. Empty bodies are written `{ }`.
- Header guards: `BEADS_<PATH>_H_`, e.g. `BEADS_DSP_LIMITER_H_`. Close namespaces with `}  // namespace beads`.
- Includes: in a `.cc`, its own header first, then `<system>`, `stmlib/...`, `beads/...`, as groups separated by blank lines.

## Names

- Classes and methods: `PascalCase` (`Process`, `Init`). Simple accessors: `lower_case()`.
- Members end in `_` (`peak_lp_`). Locals and parameters: `lower_case`.
- Constants: `kPascalCase`. Enum values: `ALL_CAPS` with a common prefix (`MEDIA_DIGITAL`).

## Classes

- Constructor and destructor are empty: `Limiter() { }`. Real setup goes in `Init()`.
- End each class with `DISALLOW_COPY_AND_ASSIGN(Name);`.
- Small DSP classes live entirely in the header. Hardware and UI code is split into `.h` and `.cc`.
- Use templates for variants (sample resolution, buffer size), not runtime flags.

## Code

- Plain C++98: no `auto`, `nullptr`, lambdas or `constexpr`.
- Use `const` for locals that don't change, and `bool` for flags.
- Loops: `while (size--)` with a pointer for sample buffers; `for (size_t i = 0; ...; ++i)` or `int i` elsewhere. Not `uint8_t`.
- Use the stmlib helpers: `CONSTRAIN`, `ONE_POLE`, `SLOPE`, `ParameterInterpolator`, the `Interpolate*` functions and lookup tables.
- Casts: `float(x)` or `static_cast<T>(x)`.
- Float literals: new float math uses `f` (`0.5f`). The existing code isn't strict about this, so leave existing literals alone.
- Name a constant when it has meaning; small obvious numbers can stay inline.
- No `__builtin_expect`, `noinline`, `flatten` or other compiler hints.

## Comments

- Few comments. Explain what the DSP or UI does and why, not what each line does.
- `//` comments, full sentences.

## Changes

- Don't edit the copyright headers of existing files. New files use the same MIT header.
- Keep the stock behaviour bit-exact unless the change is the point. Put new options behind a setting that defaults to stock.
- New behaviour gets a line in CHANGELOG.md.
