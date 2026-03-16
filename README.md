![pinggen logo](./assets/pinggen-logo.svg)

# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Source files use the `.pg` extension.

Current features:

- `import std::{ io }`
- typed top-level functions with `func name(arg: type) -> type`
- `bool`, `true`, `false`
- `if condition { ... } else { ... }`
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

func add(a: int, b: int) -> int {
    return a + b;
}

func is_answer(value: int) -> bool {
    return value == 42;
}

func main() {
    let total = add(20, 22);
    if is_answer(total) {
        io::println("pinggen online");
    } else {
        io::println("wrong answer");
    }
}
```
