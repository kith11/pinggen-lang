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

func classify(value: int) -> string {
    if value % 2 == 0 {
        return "even";
    } else if value > 3 {
        return "large";
    } else {
        return "small";
    }
}

func main() {
    let mut count = 6;
    while count > 0 {
        io::println(classify(count));
        if count <= 2 {
            break;
        }
        count = count - 1;
    }
}
```
