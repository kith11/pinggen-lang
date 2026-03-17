# Puff Language Book

Puff is a small compiled language for practical native command-line programs.

## Preface

This book is for programmers who are new to Puff.

It assumes you already know the basics of programming, but not the shape of this language. The goal is to get you productive quickly, without pretending Puff is larger than it is. Every chapter in this book is based on features that exist in the repository today.

Puff is the public CLI and user-facing tool. `pinggen` is still the compiler and repository name you will see in source, docs, and internal implementation paths. In practice, you create, build, and run programs with `puff`.

## Who Puff Is For

Puff is a good fit when you want:

- a small compiled language with explicit project structure
- straightforward syntax for functions, structs, enums, and modules
- a minimal but useful standard library for CLI-style work
- native executables without a large runtime or framework

Puff is not trying to be everything yet. It is strongest today for:

- command-line utilities
- file-processing tools
- code generators
- small automation programs
- experiments in language and compiler design

## A Short Learning Path

If you want the fastest route through the language, read these chapters in order:

1. Installing and running Puff
2. Your first project
3. Functions, variables, and expressions
4. Control flow
5. Structs, enums, and `match`
6. Tuples, arrays, and `Vec<T>`
7. Modules and dependencies
8. Standard library

After that, use the reference appendix when you need exact syntax.

## 1. What Puff Is and What It Is Good For

Puff is a compiled language built in C++ with a textual LLVM IR backend. Source files use the `.pg` extension. A Puff project is usually a directory with a `pinggen.toml` file and a `src/` folder containing one or more `.pg` modules.

The current language surface includes:

- typed top-level functions
- structs and methods
- enums with optional single payloads
- exhaustive `match`
- tuples
- fixed-size arrays
- built-in dynamic vectors with `Vec<T>`
- explicit modules and dependencies
- a small namespaced standard library
- a strict task-parallel `con { ... }` feature

Puff stays deliberately narrow. You get a compact language and a direct toolchain, not a large framework or a giant ecosystem.

## 2. Installing and Running `puff`

From the repository root, build the compiler with CMake:

```powershell
cmake -S . -B build
cmake --build build
```

You can use the local launcher directly from the repo:

```powershell
.\puff help
.\puff doctor
```

To install `puff` into a user-local bin directory on Windows:

```powershell
.\install-puff.ps1
```

That installer places `puff` on your user `PATH`, so future shells can use:

```powershell
puff help
```

Important CLI commands:

- `puff init <path>`
- `puff check [path]`
- `puff build [path]`
- `puff run [path]`
- `puff targets [path]`
- `puff doctor [path]`
- `puff setup`

## 3. Your First Project

Create a project:

```powershell
puff init .\my_app
```

The generated project is intentionally small. Its manifest looks like this:

```toml
[package]
name = "my_app"
version = "0.1.0"

[build]
name = "my_app"
entry = "src/main.pg"
output = "build/my_app"
```

The default starter has a `src/main.pg` and one helper module under `src/`.

This is the shape Puff expects:

- `pinggen.toml` declares the package and build target
- `src/main.pg` is the default entrypoint
- additional modules live under `src/`

Run the project:

```powershell
puff run .\my_app
```

From inside the project directory, just use:

```powershell
puff run
```

That difference matters because Puff expects the given path to point at the project root containing `pinggen.toml`.

## 4. Functions, Variables, Primitive Types, and Expressions

Functions are top-level and explicitly typed:

```text
func add(a: int, b: int) -> int {
    return a + b;
}
```

The entrypoint is `main`:

```text
import std::{ io }

func main() {
    io::println("hello");
}
```

Local values use `let`:

```text
let count = 1;
let name: string = "puff";
```

Mutable locals use `let mut`:

```text
let mut total = 0;
total = total + 1;
```

Primitive types are:

- `int`
- `bool`
- `string`
- `void`

Expressions are conventional:

- arithmetic on `int`
- boolean logic with `&&`, `||`, and `!`
- string concatenation with `+`
- comparisons using `==`, `!=`, `<`, `<=`, `>`, `>=`

Example:

```text
func label(name: string, count: int) -> string {
    if count == 1 {
        return name + " item";
    }
    return name + " items";
}
```

The important design choice here is that Puff prefers explicitness over magical conversions. If a builtin expects a string, you pass a string. If an operator only supports `int`, the compiler rejects mismatched operands.

## 5. Control Flow

### `if`, `else if`, and `else`

```text
if ready {
    io::println("ready");
} else if waiting {
    io::println("waiting");
} else {
    io::println("done");
}
```

Conditions must be `bool`.

### `while`

```text
let mut i = 0;
while i < 3 {
    io::println(i);
    i = i + 1;
}
```

### `for`

Puff supports integer ranges:

```text
for i in 0..3 {
    io::println(i);
}
```

### `break` and `continue`

They work inside loops only, with the usual meaning.

This gives you a compact control-flow surface: simple boolean branching plus integer iteration, without adding a second family of iterator-specific constructs.

## 6. Structs and Methods

Structs are nominal types with named fields:

```text
struct SharedLabel {
    count: int,
}
```

Construct them with a struct literal:

```text
let label = SharedLabel { count: 5 };
```

Methods live in an `impl` block:

```text
impl SharedLabel {
    func width(self) -> int {
        return self.count;
    }
}
```

This exact style is used in the repository’s path-dependency example:

```text
struct SharedLabel {
    count: int,
}

impl SharedLabel {
    func width(self) -> int {
        return self.count;
    }
}
```

Mutating methods use `mut self`:

```text
impl Counter {
    func bump(mut self) {
        self.value = self.value + 1;
    }
}
```

Puff keeps methods simple:

- methods belong to structs
- `self` is explicit
- `mut self` is explicit
- methods are called with `value.method()`

## 7. Enums and Exhaustive `match`

Plain enums are tagged values:

```text
enum State {
    Ready,
    Done,
}
```

Construction is always qualified:

```text
let state = State::Ready;
```

Payload enums carry at most one value per payload variant:

```text
enum Result {
    Ok(int),
    Err(string),
    Pending,
}
```

Construction:

```text
let a = Result::Ok(9);
let b = Result::Err("bad");
let c = Result::Pending;
```

`match` is statement-only and exhaustive:

```text
match result {
    Result::Ok(code) => {
        io::println(code);
    }
    Result::Err(message) => {
        io::println(message);
    }
    Result::Pending => {
        io::println("pending");
    }
}
```

This is one of Puff’s core strengths. You can represent explicit program states, handle them exhaustively, and bind payloads directly inside the matching arm.

The `hello` example uses this pattern with a `Job` result:

```text
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
```

## 8. Tuples, Arrays, and `Vec<T>`

### Tuples

Tuple literals:

```text
let pair = (1, "two");
```

Tuple destructuring:

```text
let (left, right) = pair();
```

Puff uses tuples heavily for multi-result expressions, including `con`.

### Fixed-size arrays

Array literals:

```text
let values = [1, 2, 3];
```

Indexing:

```text
let first = values[0];
```

Mutable indexed assignment:

```text
let mut values = [1, 2, 3];
values[1] = 9;
```

Arrays are fixed-size and bounds checked.

### Dynamic vectors

Vectors are the growable collection in Puff:

```text
let mut values = vec[1, 2, 3];
values.push(4);
values[1] = 9;
io::println(values.len());
```

An empty vector with explicit type:

```text
let mut names: Vec<string> = vec<string>[];
```

This is used directly in the `cli_workspace` example:

```text
let mut names: Vec<string> = vec<string>[];
names.push("note.txt");
names.push("note.log");
names.push("todo.txt");
io::println(count_selected(names));
```

One important design detail: `Vec<T>` uses shared-handle semantics. If you assign a vector or pass it to a function, you copy the handle, not the contents.

The vector runtime example demonstrates this:

```text
let mut values = vec[1, 2, 3];
let alias_values = values;
values.push(4);
values[1] = 9;
```

After mutation, `alias_values` sees the same underlying vector contents.

## 9. Modules, Hierarchical Imports, and Dependencies

Flat module import:

```text
import greet;
```

Hierarchical module import:

```text
import util::path;
```

This maps to nested files under `src/`, for example:

- `import util::path;` -> `src/util/path.pg`
- `import util::math::ops;` -> `src/util/math/ops.pg`

The hierarchical example project uses this style:

```text
import util::path;
import util::math::ops;
```

Dependencies extend the same model across projects.

Local dependency in `pinggen.toml`:

```toml
[[dependency]]
name = "shared"
path = "../shared_lib"
```

Then import from that dependency:

```text
import shared::util::text;
```

The `path_dependency_app` example does exactly this:

```text
import std::{ io }
import shared::util::text;

func main() {
    let label = shared_label();
    io::println(label.width());
    io::println(shared_value());
}
```

Puff also supports exact-version registry-backed dependencies. That is useful for larger multi-project work, but the mental model stays the same: dependency names become import roots.

## 10. Standard Library: `io`, `str`, `fs`, and `env`

Puff’s standard library is intentionally explicit and small.

### `io`

```text
import std::{ io }
```

Current function:

- `io::println(int|string)`

### `str`

```text
import std::{ str }
```

Current functions:

- `str::len(value)`
- `str::eq(a, b)`
- `str::starts_with(value, prefix)`
- `str::ends_with(value, suffix)`

The `cli_workspace` helper module uses these together:

```text
func is_selected(name: string) -> bool {
    if str::eq(name, "todo.txt") {
        return true;
    }
    return str::starts_with(name, "note") && str::ends_with(name, ".txt");
}
```

### `fs`

```text
import std::{ fs }
```

Current functions:

- `fs::read_to_string(path)`
- `fs::write_string(path, contents)`
- `fs::exists(path)`
- `fs::remove(path)`
- `fs::create_dir(path)`
- `fs::cwd()`

Builtin result types used by filesystem operations:

```text
enum FsResult { Ok(string), Err(string) }
enum FsWriteResult { Ok, Err(string) }
```

The `file_process` example shows the simplest read flow:

```text
match fs::read_to_string("input.txt") {
    FsResult::Ok(text) => {
        io::println(text);
    }
    FsResult::Err(message) => {
        io::println(message);
    }
}
```

### `env`

```text
import std::{ env }
```

Current function:

- `env::get(name)`

Builtin result type:

```text
enum EnvResult { Ok(string), Missing }
```

Across all std modules, the pattern is consistent: imports are explicit, fallible APIs return result-like enums, and nothing is hidden behind implicit ambient state.

## 11. Strict Concurrency with `con`

Puff’s concurrency model is intentionally narrow.

`con` runs a small set of approved calls in parallel and returns a tuple of results:

```text
let (left, right) = con { number_a(), job.label_len() };
```

The current rules are strict:

- each item must be a direct function call or safe non-mutating method call
- callees must be marked safe for `con`
- builtin functions are not allowed inside `con`
- nested `con` is rejected
- the parent waits for all child tasks before continuing

You opt in with `safe func`:

```text
safe func square(x: int) -> int {
    return x * x;
}
```

Puff does this on purpose. The feature is meant to be useful for independent work units without pretending the language already has a full effect or ownership system.

## 12. Building Larger CLI Apps in Puff

The shape of a larger Puff CLI project is already visible in the repository.

The `cli_workspace` example combines:

- hierarchical modules
- the `str` and `fs` std modules
- `Vec<string>`
- explicit helper functions
- filesystem result handling with `match`

Its structure is simple:

- `src/main.pg`
- `src/util/names.pg`
- `src/util/workspace.pg`

The main module coordinates the flow:

```text
match fs::create_dir(workspace_dir()) {
    FsWriteResult::Ok => {
        io::println("dir");
    }
    FsWriteResult::Err(message) => {
        io::println(message);
    }
}
```

Then it uses a vector of names:

```text
let mut names: Vec<string> = vec<string>[];
names.push("note.txt");
names.push("note.log");
names.push("todo.txt");
io::println(count_selected(names));
```

This is the current Puff model for “bigger” programs:

- keep modules small
- keep imports explicit
- model state with structs and enums
- use `Vec<T>` for growable collections
- keep filesystem work explicit with `FsResult` and `FsWriteResult`

It is already enough for real CLI utilities, even though it is not yet a full general-purpose ecosystem language.

## 13. Current Limits and the Right Mental Model

Puff is best understood as a small, serious language rather than a maximal one.

Current important limits:

- no general generics
- no ownership or borrow checker
- no formatter or LSP
- no large built-in collections library beyond `Vec<T>`
- no GUI or networking standard library
- `con` stays intentionally strict
- build configuration is declarative, not programmable
- package management is still intentionally narrow

The right mental model is:

- Puff is for explicit structure
- Puff is for small-to-medium native tools first
- Puff values a coherent core over a broad surface
- Puff is ready for real CLI work, not for every kind of application yet

If you approach it that way, the language feels consistent instead of limited.

## 14. Reference Appendix

### Manifest summary

```toml
[package]
name = "my_app"
version = "0.1.0"

[registry]
index = "file:///path/to/registry/index.toml"

[build]
name = "my_app"
entry = "src/main.pg"
output = "build/my_app"

[[target]]
name = "tool"
entry = "src/tool.pg"
output = "build/tool"

[[dependency]]
name = "shared"
path = "../shared_lib"

[[dependency]]
name = "http"
version = "1.2.0"
```

### Syntax summary

- `func name(arg: type) -> type { ... }`
- `import name;`
- `import util::path;`
- `import shared::util::text;`
- `import std::{ io, str, fs, env }`
- `struct Name { field: type }`
- `impl Name { func method(self) -> int { ... } }`
- `enum State { Ready, Done }`
- `enum Result { Ok(int), Err(string) }`
- `match value { ... }`
- `let value = expr;`
- `let mut value = expr;`
- `(a, b)` tuples and destructuring
- `[1, 2, 3]` arrays
- `vec[1, 2, 3]` and `vec<int>[]`
- `con { call_a(), call_b() }`

### Standard library summary

`io`

- `io::println(int|string)`

`str`

- `str::len(string) -> int`
- `str::eq(string, string) -> bool`
- `str::starts_with(string, string) -> bool`
- `str::ends_with(string, string) -> bool`

`fs`

- `fs::read_to_string(path: string) -> FsResult`
- `fs::write_string(path: string, contents: string) -> FsWriteResult`
- `fs::exists(path: string) -> bool`
- `fs::remove(path: string) -> FsWriteResult`
- `fs::create_dir(path: string) -> FsWriteResult`
- `fs::cwd() -> FsResult`

`env`

- `env::get(name: string) -> EnvResult`

### Builtin result enums

```text
enum FsResult { Ok(string), Err(string) }
enum FsWriteResult { Ok, Err(string) }
enum EnvResult { Ok(string), Missing }
```

### CLI commands

- `puff help`
- `puff init <path>`
- `puff check [path]`
- `puff build [path] [--target <name>]`
- `puff run [path] [--target <name>]`
- `puff targets [path]`
- `puff doctor [path]`
- `puff install [--bin-dir <path>]`
- `puff setup [project-path] [--bin-dir <path>]`

### Example projects

- `examples/starter`
- `examples/file_process`
- `examples/hierarchical_modules`
- `examples/path_dependency_app`
- `examples/runtime_vec_success`
- `examples/hello`
