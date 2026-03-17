# Pinggen 1.0 Release Checklist

## Supported in 1.0

- core language: functions, modules, structs, enums, payload `match`, tuples, fixed-size arrays, loops, methods, and strict `con`
- std modules: `io`, `str`, `fs`, `env`
- manifest v2 targets in `pinggen.toml`
- CLI: `new`, `check`, `build`, `run`

## Intentionally Deferred

- programmable build scripting
- generics
- richer collections
- borrow checker / ownership system
- package manager
- formatter / LSP
- broader `con` effect analysis

## Release Examples

- `examples/starter`
  - small starter-grade multi-file project
- `examples/multi_target`
  - default target plus named target flow
- `examples/file_process`
  - `fs` + `match` workflow
- `examples/hello`
  - advanced multi-feature demo

## Release Verification

Build the compiler:

```powershell
cmake -S . -B build
cmake --build build
```

Run the release-facing checks:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Key acceptance coverage:

- starter project builds and runs
- `pinggen new` generates a runnable minimal starter
- named target build/run works
- file-processing example runs
- advanced demo still builds and runs
- diagnostics checks still cover the main failure classes
