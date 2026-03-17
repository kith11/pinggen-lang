# Pinggen 1.0 Reference

`pinggen` 1.0 is a small compiled language with a fixed, intentionally limited surface.

## Project Model

- Source files use the `.pg` extension.
- Projects use `pinggen.toml`.
- Modules are flat files under `src/` and are imported with `import name;`.
- From the repo root, use `.\puff ...`.
- For a user-local install on Windows, run `.\install-puff.ps1`.

### Manifest v2

```toml
[package]
name = "my_app"
version = "0.1.0"

[build]
name = "my_app"
entry = "src/main.pg"
output = "build/my_app"

[[target]]
name = "tool"
entry = "src/tool.pg"
output = "build/tool"
```

- `[build]` defines the default executable target.
- `[[target]]` defines additional named executable targets.
- `puff build` and `puff run` use the default target.
- `puff build <project> --target <name>` and `puff run <project> --target <name>` select a named target.
- `puff targets <project>` prints the available targets.

## Editor Support

- VS Code syntax highlighting is available in [../editors/vscode/pinggen](../editors/vscode/pinggen).
- It currently provides `.pg` syntax highlighting, `#` comments, and bracket/quote auto-closing.

## Syntax Summary

### Functions and modules

- `func name(arg: type) -> type { ... }`
- `import name;`
- `import std::{ io, str, fs, env }`

### Data types

- primitives: `int`, `bool`, `string`, `void`
- tuples: `(int, string)`, `(1, 2)`
- arrays: `[int; 3]`, `[1, 2, 3]`
- structs: `struct Name { field: type }`
- enums: `enum State { Ready, Done }`
- payload enums: `enum Result { Ok(int), Err(string) }`

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
- `puff build [path] [--target <name>]`
- `puff run [path] [--target <name>]`
- `puff targets [path]`
- `puff doctor [path]`
- `puff install [--bin-dir <path>]`
- `puff setup [project-path] [--bin-dir <path>]`

## Intentional 1.0 Limits

- fixed-size arrays only
- declarative build configuration only
- strict `con` restrictions
- no generics
- no package manager
- no formatter or LSP
