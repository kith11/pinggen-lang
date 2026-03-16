# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Current features:

- `import std::{ io }`
- `func main() { ... }`
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
