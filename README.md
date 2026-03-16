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
- struct methods with `impl Type { ... }` and `value.method(...)`
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

struct Counter {
    value: int,
}

impl Counter {
    func bumped(self, amount: int) -> Counter {
        self.value = self.value + amount;
        return self;
    }
    func total(self) -> int {
        return self.value;
    }
}

func show(counter: Counter) -> int {
    return counter.total();
}

func main() {
    let counter = Counter { value: 10 };
    let next = counter.bumped(5);
    io::println(show(next));
    io::println(counter.total());
}
```
