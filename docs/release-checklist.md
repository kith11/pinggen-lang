# Pinggen 1.0 Release Checklist

## Supported in 1.0

- core language: functions, modules, structs, enums, payload `match`, tuples, fixed-size arrays, loops, methods, and strict `con`
- std modules: `io`, `str`, `fs`, `env`
- manifest v2 targets in `pinggen.toml`
- package dependencies via local paths or exact-version registry entries with `puff.lock`
- CLI: `puff help`, `puff init`, `puff check`, `puff build`, `puff run`, `puff targets`, `puff doctor`, `puff install`, `puff setup`

## Intentionally Deferred

- programmable build scripting
- generics
- richer collections
- borrow checker / ownership system
- package publishing, semver ranges, multi-registry auth, and lockfile conflict UX
- formatter / LSP
- broader `con` effect analysis

## Release Examples

- `examples/starter`
  - small starter-grade multi-file project
- `examples/multi_target`
  - default target plus named target flow
- `examples/file_process`
  - `fs` + `match` workflow
- `examples/path_dependency_app`
  - local path dependency flow
- `examples/registry_app`
  - registry dependency resolution plus lockfile generation
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
- `puff init` generates a runnable minimal starter
- named target build/run works
- file-processing example runs
- registry dependency example resolves and runs
- advanced demo still builds and runs
- `puff doctor` reports toolchain and project status
- diagnostics checks still cover the main failure classes
