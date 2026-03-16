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
- `+` for string concatenation
- fixed-size arrays with `[T; N]`, `[a, b, c]`, and `array[index]`
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

func swap_pair(values: [int; 2]) -> [int; 2] {
    let mut result: [int; 2] = values;
    let first = result[0];
    result[0] = result[1];
    result[1] = first;
    return result;
}

func main() {
    let grid = [[1, 2], [3, 4]];
    let mut pair: [int; 2] = [10, 20];
    pair[0] = 99;
    let swapped = swap_pair(pair);
    io::println(swapped[0]);
    io::println(swapped[1]);
    io::println(grid[1][0]);
}
```
