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
- `if condition { ... } else { ... }`
- `while condition { ... }`
- `break` and `continue` inside `while`
- `!`, `&&`, and `||` for boolean logic
- `<`, `<=`, `>`, and `>=` for integer comparisons
- `%` for integer modulo
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

func is_even(value: int) -> bool {
    return value % 2 == 0;
}

func main() {
    let mut count = 6;
    while count > 0 {
        if is_even(count) {
            io::println(count);
        }
        if count <= 2 {
            break;
        }
        count = count - 1;
    }
}
```
