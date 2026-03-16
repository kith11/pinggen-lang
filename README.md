![pinggen logo](./assets/pinggen-logo.svg)

# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Source files use the `.pg` extension.

Current features:

- `import std::{ io }`
- typed top-level functions with `func name(arg: type) -> type`
- top-level `enum` declarations with qualified variants like `State::Ready`
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

enum State {
    Idle,
    Ready,
    Done,
}

struct Job {
    state: State,
    label: string,
}

func is_ready(state: State) -> bool {
    return state == State::Ready;
}

func main() {
    let job = Job { state: State::Ready, label: "pinggen" };
    let states: [State; 3] = [State::Idle, State::Ready, State::Done];

    if is_ready(job.state) {
        io::println(job.label);
    } else {
        io::println("not ready");
    }

    if states[2] == State::Done {
        io::println("done");
    }
}
```
