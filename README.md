# nathra

A typed systems language in valid Python syntax that compiles to portable C. It preserves enough source structure for aggressive compile-time rewrites that conventional C compilers cannot recover from handwritten C.

```python
import glut

struct Particle:
    x: float
    y: float
    vx: float
    vy: float

def update(particles: ptr[Particle], n: int, dt: float) -> void:
    for i in range(n):
        particles[i].x += particles[i].vx * dt
        particles[i].y += particles[i].vy * dt

def main() -> int:
    glutInitDisplayMode(GLUT_DOUBLE + GLUT_RGB + GLUT_DEPTH)
    glutCreateWindow("nathra")
    glutMainLoop()
    return 0
```

## Quick start

```sh
make                                          # build the native compiler (~2 sec)
python3 cli/nathra.py program.nth             # compile + link
python3 cli/nathra.py program.nth --run       # compile, link, run
python3 cli/nathra.py program.nth --emit-c    # emit C only
python3 cli/nathra.py program.nth --safe      # enable runtime safety checks
python3 cli/nathra.py program.nth --shared    # compile to shared library
python3 cli/nathra.py build.nth               # run a project build script
python3 cli/snekc.py                          # interactive REPL
```

## What makes it different

**Source-aware optimizations.** The compiler sees the full program AST and applies rewrites that a C compiler cannot recover from flat C: `restrict` inference on non-aliasing pointers, `@soa` struct-of-arrays transformation, hot/cold code splitting, constant specialization, alloca substitution for small allocations, and stack variable lifetime narrowing. These are the transforms that justify writing nathra instead of C.

**Python syntax, C semantics.** Valid Python syntax means any editor, linter, or LLM can read and write it without learning a new grammar. But the semantics are C-level: no garbage collector, no runtime, no object model. Structs are value types. Pointers are explicit. You control the memory layout.

**Portable C output.** The compiler emits readable, auditable C. You can inspect it, diff it, feed it to any C compiler on any platform. No LLVM dependency, no custom backend.

**Automatic memory management (opt-out, not opt-in).** The default mode is auto-defer: the compiler's escape analysis detects local-only allocations and inserts cleanup automatically. When you need more control, escalate to `defer`, `own[T]`, scoped arenas, or raw `alloc`/`free`.

**Safety checks.** `--safe` enables division-by-zero, bounds, overflow, and null pointer checks — all gated behind a single `#define`, zero overhead in release builds. Static null analysis catches provably-null dereferences as compile errors with no flag needed.

**C library integration.** `import glut` maps to C headers via the build system. The compiler runs `gcc -E` at compile time to extract every function signature and `#define` constant. No manual extern declarations.

## Status

| Tier | Features |
|------|----------|
| **Stable** | Types, structs, enums, functions, modules, control flow, C emission, lists, dicts, strings, f-strings, defer, auto-defer, error handling (`Result[T]`), testing framework, build system, native bootstrap compiler |
| **Solid** | Safety checks (`--safe`), `@soa`, `@hot`/`@cold`, serialization (`@serializable`), SIMD, concurrency (threads, mutexes, channels), hot-reload, REPL, codegen hooks, `c_import` |
| **New** | `own[T]` ownership tracking, scoped arenas (`with scope`), heap assertions, `c_modules` build integration |

## Non-goals

- **Not Python-compatible.** No Python runtime, no Python object model, no Python import semantics. This is a C-level systems language that borrows Python's syntax.
- **Not memory-safe by default.** Raw pointer access is allowed. `--safe` adds runtime checks; `own[T]` adds compile-time ownership enforcement. Neither is mandatory.
- **Not aiming for C++ abstraction complexity.** No templates, no RAII, no move constructors, no exceptions. Nathra is closer to "typed C with Python syntax" than to Rust or C++.
- **Not hiding C-level costs.** Every operation has a predictable cost. There are no hidden allocations, no implicit copies, no virtual dispatch.

## Memory model

The default is **auto-defer**: local-only `str`, `list[T]`, and `dict` variables are freed automatically when the function returns. You write allocations; the compiler handles cleanup.

When auto-defer isn't enough, escalate:

```python
# 1. Explicit defer — you control the cleanup point
buf: ptr[byte] = alloc(4096)
defer(free(buf))

# 2. Ownership transfer — compile-time enforcement
def process(data: own[list[int]]) -> void:
    defer(list_free(data))
    # data must be freed or moved before return

# 3. Scoped arenas — batch allocation, single free
with scope(arena, 65536):
    s: str = arena_str_new(arena, "temp")
    # freed when scope exits

# 4. Raw control — escape hatch
p: ptr[int] = alloc(8)
deref(p, 42)
free(p)
```

## Bootstrap performance

The native compiler — written in nathra — compiles its own ~4,000 lines of source in 5 milliseconds (405x faster than the Python implementation). Total build time is dominated by `gcc`, not the compiler.

```sh
python3 scripts/benchmark.py    # reproduce the numbers
```

See [docs/benchmarks.md](docs/benchmarks.md) for the full benchmark table.

## Project structure

```
nathra/
  cli/
    nathra.py                         CLI entry point
    snekc.py                          Interactive REPL shell
    nathra_stubs.py                   IDE stubs
  compiler/                           Python compiler (stage 0)
    compiler.py                       Front-end: parse, analyze, emit
    codegen_stmts.py                  Statement code generation
    codegen_exprs.py                  Expression code generation
    type_map.py                       Type annotation → C type mapping
    ast_serial.py                     Binary AST serializer
  runtime/                            C headers shipped with the project
    nathra_rt.h                       Full runtime
    nathra_types.h                    Forward declarations
    nathra_test.h                     Test runner infrastructure
  native/                             Bootstrap native compiler (405x faster)
    src/                              .nth source for the native compiler
    generated/                        Pre-generated .c/.h — just run make
  lib/
    build.py                          Build script interpreter
  tests/                              Test suite (48 tests)
  bench/                              Benchmarks
  examples/                           Example programs
  docs/                               Detailed documentation
```

```sh
make                    # build native compiler from pre-generated C (~2 sec)
make regenerate         # regenerate C from .nth sources (needs Python compiler)
make test               # run the test suite
make clean              # remove build artifacts
```

## Documentation

- [Language Reference](docs/language.md) — types, structs, functions, memory, modules, concurrency, serialization
- [Compiler Optimizations & Safety](docs/optimizations.md) — restrict inference, SoA, hot/cold, safety checks
- [Benchmarks & Project Structure](docs/benchmarks.md) — performance numbers, bootstrap details
