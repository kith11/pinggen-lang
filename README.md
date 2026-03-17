![pinggen logo](./assets/pinggen-logo.svg)

# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Source files use the `.pg` extension.

Current features:

- `import std::{ io, str }`
- typed top-level functions with `func name(arg: type) -> type`
- top-level `enum` declarations with qualified variants like `State::Ready`
- single-payload enum variants like `Result::Ok(1)`
- plain `struct` declarations with named fields
- named-field struct literals and `value.field` access
- `bool`, `true`, `false`
- `if`, `else if`, and `else`
- exhaustive enum `match` statements by variant tag
- `while condition { ... }`
- `for name in start..end { ... }`
- `break` and `continue` inside loops
- `!`, `&&`, and `||` for boolean logic
- `<`, `<=`, `>`, and `>=` for integer comparisons
- `%` for integer modulo
- `+` for string concatenation
- `str::len(value)` for byte-count string length
- fixed-size arrays with `[T; N]`, `[a, b, c]`, and `array[index]`
- struct methods with `impl Type { ... }` and `value.method(...)`
- mutating struct methods with `mut self`
- `let` and `let mut`
- integers and strings
- `==` and `!=` for `int`, `bool`, and tag-only enums
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
import std::{ io, str }

enum Result {
    Ok(int),
    Err(string),
    Pending,
}

struct Job {
    result: Result,
    label: string,
}

func finish(code: int) -> Result {
    if code == 0 {
        return Result::Ok(7);
    }
    return Result::Err("bad");
}

func describe(result: Result) -> string {
    match result {
        Result::Ok => {
            return "ok";
        }
        Result::Err => {
            return "err";
        }
        Result::Pending => {
            return "pending";
        }
    }
}

func main() {
    let job = Job { result: finish(0), label: "pinggen" };
    let results: [Result; 3] = [Result::Pending, Result::Err("bad"), Result::Ok(9)];
    let label_size = str::len(job.label);

    match job.result {
        Result::Ok => {
            io::println(job.label);
        }
        Result::Err => {
            io::println("err");
        }
        Result::Pending => {
            io::println("pending");
        }
    }

    io::println(label_size);
    io::println(describe(results[2]));
    io::println(describe(results[0]));
}
```
