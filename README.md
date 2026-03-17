![pinggen logo](./assets/pinggen-logo.svg)

# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Source files use the `.pg` extension.

Current features:

- `import std::{ io, str, fs }`
- project-local flat modules with `import name;`
- `safe func` and `con { ... }` for strict task-parallel calls
- typed top-level functions with `func name(arg: type) -> type`
- top-level `enum` declarations with qualified variants like `State::Ready`
- single-payload enum variants like `Result::Ok(1)`
- payload enum binding in match arms like `Result::Ok(value) => { ... }`
- plain `struct` declarations with named fields
- named-field struct literals and `value.field` access
- tuple types and values like `(int, string)` and `(1, 2)`
- tuple destructuring like `let (a, b) = pair;`
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
- `fs::read_to_string(path)` returning `FsResult`
- `fs::write_string(path, contents)` returning `FsWriteResult`
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
// src/main.pg
import std::{ io, fs }
import model;
import logic;

func main() {
    let job = Job { result: finish(0), label: "pinggen" };
    let results: [Result; 3] = [Result::Pending, Result::Err("bad"), Result::Ok(9)];
    let (left, right) = con { number_a(), job.label_len() };

    match job.result {
        Result::Ok(code) => {
            io::println(job.label);
            io::println(code);
        }
        Result::Err(message) => {
            io::println(message);
        }
        Result::Pending => {
            io::println("pending");
        }
    }

    let written = "hello from file\n";
    io::println(left);
    io::println(right);
    match fs::write_string("message.txt", written) {
        FsWriteResult::Ok => {
            match fs::read_to_string("message.txt") {
                FsResult::Ok(text) => {
                    io::println(text);
                }
                FsResult::Err(message) => {
                    io::println(message);
                }
            }
        }
        FsWriteResult::Err(message) => {
            io::println(message);
        }
    }
    io::println(describe(results[2]));
    io::println(describe(results[0]));
}
```

```pinggen
// src/model.pg
import std::{ str }

enum Result {
    Ok(int),
    Err(string),
    Pending,
}

struct Job {
    result: Result,
    label: string,
}

impl Job {
    safe func label_len(self) -> int {
        return str::len(self.label);
    }
}
```

```pinggen
// src/logic.pg
import model;

safe func number_a() -> int {
    return 11;
}

func finish(code: int) -> Result {
    if code == 0 {
        return Result::Ok(7);
    }
    return Result::Err("bad");
}

func describe(result: Result) -> string {
    match result {
        Result::Ok(value) => {
            if value > 5 {
                return "ok";
            }
            return "small";
        }
        Result::Err(message) => {
            return message;
        }
        Result::Pending => {
            return "pending";
        }
    }
}
```
