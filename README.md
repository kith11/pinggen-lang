# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Current features:

- `import std::{ io }`
- typed top-level functions with `func name(arg: type) -> type`
- `let` and `let mut`
- integers and strings
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

func greet(name: string) {
    io::println(name);
}

func main() {
    let total = add(20, 22);
    greet("pinggen online");
    io::println(total);
}
```
