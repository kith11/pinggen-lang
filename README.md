![pinggen logo](./assets/pinggen-logo.svg)

# pinggen

`pinggen` is a small compiled programming language MVP built in C++.

Source files use the `.pg` extension.

Current features:

- `import std::{ io }`
- typed top-level functions with `func name(arg: type) -> type`
- plain `struct` declarations with named fields
- named-field struct literals and `value.field` access
- `bool`, `true`, `false`
- `if condition { ... } else { ... }`
- `while condition { ... }`
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

struct Hero {
    hp: int
    alive: bool
    name: string
}

func heal(hero: Hero) -> Hero {
    return Hero { hp: hero.hp + 10, alive: true, name: hero.name };
}

func main() {
    let mut hero = Hero { hp: 20, alive: false, name: "pinggen" };
    hero.hp = 40;
    let healed = heal(hero);
    if healed.alive {
        io::println(healed.name);
        io::println(healed.hp);
    }
}
```
