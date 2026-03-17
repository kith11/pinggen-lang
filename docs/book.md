# Puff Language Book

A beginner-friendly guide to writing small programs with Puff.

## Preface

This book is meant to be easy to follow.

You do not need to know Puff already. If you have written a little code in another language, that is enough. The book explains what the main parts of Puff are, how a project is laid out, and how to read and write simple programs without assuming deep compiler knowledge.

Puff is the name of the CLI you use day to day. `pinggen` is still the compiler and repository name you will see in source paths and internal docs. In normal use, you create, build, and run programs with `puff`.

Everything in this book is based on features that exist in the repository now. It does not describe planned features as if they already work.

## Who This Book Is For

This book is for:

- beginners who want a gentle first pass through the language
- programmers coming from another language who want to learn Puff quickly
- anyone who wants one document that explains both the language and the usual project workflow

This book is not a formal language specification. It is a practical guide.

## How To Use This Book

If you are brand new to Puff, read the chapters in order.

If you already know the basics, you can jump straight to:

- modules and dependencies
- vectors with `Vec<T>`
- the standard library
- the reference appendix

## 1. What Puff Is

Puff is a small compiled language for practical command-line programs.

You write source files with the `.pg` extension. Puff reads those files, checks them, lowers them through LLVM IR, and builds a native executable.

Puff is good at:

- small tools
- file-processing programs
- code generators
- small utility apps
- language experiments

Puff tries to stay simple:

- explicit project files
- explicit imports
- explicit types
- small standard library
- small number of core concepts

That narrow design is a feature. Puff is easier to understand because it does not try to do everything at once.

## 2. Installing and Running `puff`

From the repository root, build the compiler with CMake:

```powershell
cmake -S . -B build
cmake --build build
```

You can use the local launcher from the repository root:

```powershell
.\puff help
.\puff doctor
```

To install `puff` into your user environment on Windows:

```powershell
.\install-puff.ps1
```

After that, a new terminal can usually run:

```powershell
puff help
```

The most important commands are:

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

This creates a small project with:

- `pinggen.toml`
- `src/main.pg`
- one helper module under `src/`

The manifest looks like this:

```toml
[package]
name = "my_app"
version = "0.1.0"

[build]
name = "my_app"
entry = "src/main.pg"
output = "build/my_app"
```

Read it like this:

- `[package]` gives the project a name and version
- `[build]` defines the default program that will be built
- `entry` points to the first source file
- `output` decides where the executable will go

Run the project:

```powershell
puff run .\my_app
```

If you are already inside `my_app`, use:

```powershell
puff run
```

That difference matters because Puff expects the given path to be the project folder that contains `pinggen.toml`.

## 4. Functions, Variables, Primitive Types, and Expressions

### Functions

A function is a named block of code that can take inputs and return a value.

```text
func add(a: int, b: int) -> int {
    return a + b;
}
```

Read this as:

- `func` starts a function
- `add` is the function name
- `a` and `b` are parameters
- `int` is the parameter type
- `-> int` means the function returns an `int`

The entrypoint of a program is `main`:

```text
import std::{ io }

func main() {
    io::println("hello");
}
```

### Variables

Use `let` to create a local value:

```text
let count = 1;
let name: string = "puff";
```

Use `let mut` when you want to change it later:

```text
let mut total = 0;
total = total + 1;
```

### Primitive types

Puff has these built-in primitive types:

- `int`
- `bool`
- `string`
- `void`

### Expressions

Expressions are pieces of code that produce a value.

Examples:

- `1 + 2`
- `count == 3`
- `"a" + "b"`
- `name != "done"`

Common operators:

- arithmetic: `+`, `-`, `*`, `/`, `%`
- comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- logic: `&&`, `||`, `!`

Simple example:

```text
func label(name: string, count: int) -> string {
    if count == 1 {
        return name + " item";
    }
    return name + " items";
}
```

Puff prefers explicit code. It does not try to silently convert the wrong type into the right one.

## 5. Control Flow

Control flow means deciding what code runs and how often it runs.

### `if`

```text
if ready {
    io::println("ready");
} else {
    io::println("not ready");
}
```

The condition must be a `bool`.

### `while`

Use `while` when you want to repeat code until a condition becomes false.

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

This runs with `i = 0`, `1`, and `2`.

### `break` and `continue`

- `break` exits the loop
- `continue` jumps to the next iteration

## 6. Structs and Methods

A struct is a custom type with named fields.

```text
struct SharedLabel {
    count: int,
}
```

Create a value with a struct literal:

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

Call the method like this:

```text
let size = label.width();
```

This exact style appears in the path dependency example:

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

That makes mutation visible in the function signature instead of hidden.

## 7. Enums and Exhaustive `match`

An enum is a value that can be one of several named cases.

Simple enum:

```text
enum State {
    Ready,
    Done,
}
```

Create a value with a qualified variant:

```text
let state = State::Ready;
```

Enums can also carry one payload value:

```text
enum Result {
    Ok(int),
    Err(string),
    Pending,
}
```

Create payload variants like this:

```text
let ok = Result::Ok(9);
let err = Result::Err("bad");
let pending = Result::Pending;
```

Use `match` to handle every possible case:

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

This is called exhaustive matching. Puff requires you to cover all enum variants, which helps prevent missing-case bugs.

The `hello` example uses exactly this style:

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

A tuple is a fixed group of values.

```text
let pair = (1, "two");
```

Take it apart with destructuring:

```text
let (left, right) = pair();
```

Tuples are useful when one expression naturally returns more than one value.

### Arrays

An array is a fixed-size list of values of the same type.

```text
let values = [1, 2, 3];
```

Read by index:

```text
let first = values[0];
```

Change an element if the array is mutable:

```text
let mut values = [1, 2, 3];
values[1] = 9;
```

Arrays are bounds checked.

### Vectors with `Vec<T>`

If you need a growable collection, use `Vec<T>`.

```text
let mut values = vec[1, 2, 3];
values.push(4);
values[1] = 9;
io::println(values.len());
```

An empty vector needs an explicit element type:

```text
let mut names: Vec<string> = vec<string>[];
```

This appears directly in the `cli_workspace` example:

```text
let mut names: Vec<string> = vec<string>[];
names.push("note.txt");
names.push("note.log");
names.push("todo.txt");
io::println(count_selected(names));
```

One important detail: `Vec<T>` uses shared-handle semantics. If you assign a vector to another variable, both names refer to the same underlying collection.

The vector example shows that:

```text
let mut values = vec[1, 2, 3];
let alias_values = values;
values.push(4);
values[1] = 9;
```

After those changes, `alias_values` sees the updated data too.

## 9. Modules, Hierarchical Imports, and Dependencies

As projects grow, you split code into modules.

Flat import:

```text
import greet;
```

Hierarchical import:

```text
import util::path;
```

That maps to nested files under `src/`, for example:

- `import util::path;` means `src/util/path.pg`
- `import util::math::ops;` means `src/util/math/ops.pg`

The hierarchical modules example uses this style:

```text
import util::path;
import util::math::ops;
```

Dependencies use the same import idea across projects.

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

Puff also supports exact-version registry dependencies, but the core idea stays simple: dependency names become import roots.

## 10. Standard Library

Puff's standard library is small and explicit.

### `io`

Import:

```text
import std::{ io }
```

Current function:

- `io::println(int|string)`

### `str`

Import:

```text
import std::{ str }
```

Current functions:

- `str::len(value)`
- `str::eq(a, b)`
- `str::starts_with(value, prefix)`
- `str::ends_with(value, suffix)`

The `cli_workspace` helper module uses them like this:

```text
func is_selected(name: string) -> bool {
    if str::eq(name, "todo.txt") {
        return true;
    }
    return str::starts_with(name, "note") && str::ends_with(name, ".txt");
}
```

### `fs`

Import:

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

Filesystem functions use built-in result enums:

```text
enum FsResult { Ok(string), Err(string) }
enum FsWriteResult { Ok, Err(string) }
```

The `file_process` example shows a basic read:

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

Import:

```text
import std::{ env }
```

Current function:

- `env::get(name)`

Its built-in result enum is:

```text
enum EnvResult { Ok(string), Missing }
```

The stdlib pattern is consistent:

- imports are explicit
- each namespace is small
- fallible operations return enum-style results

## 11. Strict Concurrency with `con`

Puff has a concurrency feature, but it is intentionally strict.

You use `con` with approved calls and get a tuple of results back:

```text
let (left, right) = con { number_a(), job.label_len() };
```

The current rules are:

- each item must be a direct function call or safe non-mutating method call
- callees must be marked safe for `con`
- builtin functions are not allowed inside `con`
- nested `con` is rejected
- the parent waits for all child tasks before continuing

Mark a function as safe like this:

```text
safe func square(x: int) -> int {
    return x * x;
}
```

This keeps the feature useful without pretending Puff already has a full ownership or effect system.

## 12. Building Larger CLI Apps in Puff

The `cli_workspace` example is a good model for a larger Puff program.

It combines:

- hierarchical modules
- `Vec<string>`
- string helpers
- filesystem helpers
- explicit `match`-based error handling

Its file layout is:

- `src/main.pg`
- `src/util/names.pg`
- `src/util/workspace.pg`

The main module creates a workspace directory:

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

Then it works with a vector of names:

```text
let mut names: Vec<string> = vec<string>[];
names.push("note.txt");
names.push("note.log");
names.push("todo.txt");
io::println(count_selected(names));
```

This is the current Puff style for bigger programs:

- keep modules small
- keep names explicit
- use structs and enums for clear program state
- use `Vec<T>` for growable data
- use `match` for fallible stdlib operations

## 13. Current Limits and the Right Mental Model

Puff is intentionally small.

Important current limits:

- no general generics
- no ownership or borrow checker
- no formatter or LSP
- no large collections library beyond `Vec<T>`
- no GUI or networking standard library
- `con` remains strict
- build configuration is declarative, not programmable
- package management is still intentionally narrow

The best mental model is:

- Puff is a small compiled language
- Puff is already useful for CLI tools
- Puff is still growing
- Puff values clarity over feature count

If you expect a compact, explicit language, Puff feels consistent and approachable.

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
