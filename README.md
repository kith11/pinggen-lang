![pinggen logo](./assets/pinggen-logo.svg)

# pinggen

`pinggen` is a small compiled programming language built in C++ with an LLVM IR backend.

Source files use the `.pg` extension.

This repository is now targeting a small usable `1.0` surface. The current language, stdlib, manifest v2 targets, and CLI should be treated as frozen for this release pass.

## Quick Start

Build the compiler:

```powershell
cmake -S . -B build
cmake --build build
```

Create and run a new project from the repo root:

```powershell
.\puff init .\my_app
.\puff run .\my_app
```

Check your toolchain and current project:

```powershell
.\puff doctor
```

Run the interactive setup flow:

```powershell
.\puff setup
```

Install `puff` to your user bin directory:

```powershell
.\install-puff.ps1
```

That script now installs `puff`, adds its user-local bin directory to your `PATH`, and tells you when to open a new terminal.

Run the included starter example:

```powershell
.\puff run .\examples\starter
```

Build a named target:

```powershell
.\puff build .\examples\multi_target --target tool
```

Reference docs:

- [1.0 Reference](./docs/reference.md)
- [Release Checklist](./docs/release-checklist.md)

Editor support:

- [VS Code syntax highlighting](./editors/vscode/pinggen/README.md)

Launchers:

- [repo-root launcher](./puff.cmd)
- [Windows installer script](./install-puff.ps1)

## Project Layout

`puff` projects use `pinggen.toml` plus flat modules under `src/`.

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

## Stable 1.0 Surface

### Core language

- typed top-level functions
- flat project-local modules with `import name;`
- `struct`, `enum`, payload enums, and exhaustive `match`
- tuples and tuple destructuring
- fixed-size arrays
- `if`, `else if`, `else`, `while`, `for`, `break`, `continue`
- methods with `impl`, `self`, and `mut self`
- strict task-parallel `con { ... }` with `safe func`

### Standard library

- `import std::{ io }`
  - `io::println(...)`
- `import std::{ str }`
  - `str::len(value)`
- `import std::{ fs }`
  - `fs::read_to_string(path)`
  - `fs::write_string(path, contents)`
  - `fs::exists(path)`
- `import std::{ env }`
  - `env::get(name)`

### CLI

- `puff new`
- `puff init`
- `puff check`
- `puff build`
- `puff run`
- `puff targets`
- `puff doctor`
- `puff install`
- `puff setup`

## Examples

- [starter](./examples/starter): small multi-file starter-grade project
- [multi_target](./examples/multi_target): manifest v2 named targets
- [file_process](./examples/file_process): practical `fs` + `match` example
- [hello](./examples/hello): advanced multi-feature demo

For the compact syntax and stdlib reference, use [docs/reference.md](./docs/reference.md).

## Editor Support

A minimal VS Code extension for `.pg` syntax highlighting lives at [editors/vscode/pinggen](./editors/vscode/pinggen).

## Current 1.0 Limits

- arrays are fixed-size only
- `con` is intentionally strict and only allows approved safe calls
- build configuration is declarative; there is no programmable build scripting
- no generics, borrow checker, package manager, formatter, or LSP in this milestone
- post-1.0 work is documented in [docs/release-checklist.md](./docs/release-checklist.md)
