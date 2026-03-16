![pinggen logo](./assets/pinggen-logo.svg)

# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Source files use the `.pg` extension.

Current features:

- `import std::{ io }`
- typed top-level functions with `func name(arg: type) -> type`
- plain `struct` declarations with named fields
- named-field struct literals and `value.field` access
- `bool`, `true`, `false`
- `if`, `else if`, and `else`
- `while condition { ... }`
- `for name in start..end { ... }`
- `break` and `continue` inside loops
- `!`, `&&`, and `||` for boolean logic
- `<`, `<=`, `>`, and `>=` for integer comparisons
- `%` for integer modulo
- `+` for string concatenation
- fixed-size arrays with `[T; N]`, `[a, b, c]`, and `array[index]`
- struct methods with `impl Type { ... }` and `value.method(...)`
- mutating struct methods with `mut self`
- `let` and `let mut`
- integers and strings
- `==` and `!=` for `int` and `bool`
- integer arithmetic
- assignment to mutable variables
- `io::println(...)`
- `return`
- `pinggen.toml` project files
- `pinggen new`, `check`, `build`, `run`

Build:

```powershell
cmake -S . -B build
cmake --build build
```

Run the example:

```powershell
.\build\pinggen.exe run .\examples\hello
```

Example:

```pinggen
import std::{ io }

func main() {
    let mut sum = 0;
    for i in 0..6 {
        if i == 1 {
            continue;
        }
        if i == 4 {
            break;
        }
        io::println(i);
        sum = sum + i;
    }
    io::println(sum);
}
```
