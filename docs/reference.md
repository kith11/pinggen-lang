# Pinggen 1.0 Reference

`pinggen` 1.0 is a small compiled language with a fixed, intentionally limited surface.

## Project Model

- Source files use the `.pg` extension.
- Projects use `pinggen.toml`.
- Modules live under `src/` and are imported with `import name;` or `import util::path;`.
- From the repo root, use `.\puff ...`.
- For a user-local install on Windows, run `.\install-puff.ps1`.

### Manifest v2

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

- `[build]` defines the default executable target.
- `[[target]]` defines additional named executable targets.
- `[[dependency]]` defines either a local path dependency or an exact-version registry dependency imported by its `name`.
- `[registry].index` is required when any dependency uses `version = "..."`
- Registry dependency requirements support exact versions like `1.2.0` and narrow caret ranges like `^1.2.0`.
- `puff build` and `puff run` use the default target.
- `puff build <project> --target <name>` and `puff run <project> --target <name>` select a named target.
- `puff targets <project>` prints the available targets.
- `puff fmt [project] [--check]` formats `.pg` source files under `src/`.
- `puff.lock` is generated automatically for registry-backed projects and reused on later builds.

## Editor Support

- VS Code syntax highlighting is available in [../editors/vscode/pinggen](../editors/vscode/pinggen).
- It currently provides syntax highlighting, `#` comments, bracket/quote auto-closing, diagnostics, completions, go-to-definition, and hover.
- `puff lsp` is the stdio language server used by the VS Code extension.

## Syntax Summary

### Functions and modules

- `func name(arg: type) -> type { ... }`
- `import name;`
- `import util::path;`
- `import shared::util::text;`
- `import http::client;`
- `import std::{ io, str, fs, env }`

### Data types

- primitives: `int`, `bool`, `string`, `void`
- tuples: `(int, string)`, `(1, 2)`
- arrays: `[int; 3]`, `[1, 2, 3]`
- dynamic vectors: `Vec<int>`, `vec[1, 2, 3]`, `vec<int>[]`
- structs: `struct Name { field: type }`
- enums: `enum State { Ready, Done }`
- payload enums: `enum Result { Ok(int), Err(string) }`

### Collections

- fixed arrays support indexing and indexed assignment: `values[0]`, `values[1] = 9`
- vectors support:
  - `values.len()`
  - `values.push(value)` on mutable local vectors
  - `values[index]`
  - `values[index] = value`
- `Vec<T>` uses shared-handle semantics in 1.0:
  - assignment, params, and returns copy the handle
  - mutating one alias updates the others

### Control flow

- `if`, `else if`, `else`
- `while condition { ... }`
- `for name in start..end { ... }`
- `break`, `continue`
- exhaustive enum `match`

### Methods and concurrency

- `impl Type { func method(self) -> int { ... } }`
- `mut self` for mutating methods
- `safe func` for `con`-eligible calls
- `con { call_a(), call_b() }`

## Standard Library

### `io`

- `io::println(int|string)`

### `str`

- `str::len(string) -> int`

### `fs`

- `fs::read_to_string(path: string) -> FsResult`
- `fs::write_string(path: string, contents: string) -> FsWriteResult`
- `fs::exists(path: string) -> bool`

### `env`

- `env::get(name: string) -> EnvResult`

## CLI

- `puff help`
- `puff new <path>`
- `puff init <path>`
- `puff check [path]`
- `puff fmt [path] [--check]`
- `puff lsp`
- `puff build [path] [--target <name>]`
- `puff run [path] [--target <name>]`
- `puff add <name>[@version] [path]`
- `puff update [name] [path]`
- `puff deps [path]`
- `puff targets [path]`
- `puff doctor [path]`
- `puff install [--bin-dir <path>]`
- `puff setup [project-path] [--bin-dir <path>]`

## Intentional 1.0 Limits

- fixed arrays plus built-in `Vec<T>` only; no general generics
- declarative build configuration only
- package management uses one registry per project with exact or caret requirements only; no full solver or publishing flow
- strict `con` restrictions
- no generics
- `puff fmt` currently formats `.pg` source files only and does not support source comments yet
- no rename, references, code actions, semantic tokens, or formatting-through-LSP
