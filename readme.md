# luau-zig

Idiomatic [Zig](https://ziglang.org) bindings to [Luau](https://luau.org),
Roblox's typed Lua dialect. Built against **Luau 0.726** with **Zig 0.16**.

The public API is pure, idiomatic Zig — the raw C layer is internal and never
exposed. `zig build coverage` reports two things honestly:

```
A. binding gate (C API + our shims)   1255/1255 wrapped — gated, build fails if any slip
B. Luau C++ surface                   1048 entry points across every module
```

The **C API is complete and enforced.** The **C++ libraries** (parser, compiler,
type checker, codegen) are bound broadly via `extern "C"` shims — a **curated
subset, not parity**: the common embedding and tooling paths are covered, while
deeper corners (e.g. source-level *type*-annotation reflection in the AST) are
thinner. `zig build gaps` prints the per-module gap map; full accounting is
in **[coverage.md](coverage.md)**.

## Highlights

- **Zero-cost state** — `*Lua` *is* a `lua_State*`; the wrapper adds no overhead.
- **Allocator-backed** — `Lua.init(gpa)` routes every VM allocation through a Zig
  allocator; `deinit()` is RAII. `setAllocator(gpa)` routes the C++ tooling's
  allocations (parser/checker/…) through one too.
- **Errors, not codes** — protected calls return Zig error unions.
- **Comptime marshalling** — push/pull any Zig type: ints, floats, bools, enums,
  optionals, slices, structs ↔ tables, tuples ↔ multiple returns.
- **Real function signatures** — expose `fn(a: f64, b: f64) f64` directly; args
  are pulled from the stack, results pushed, a returned `error` raised as a Luau
  error.
- **Sol-style usertypes** — `registerType(T)` binds a struct's constructor,
  methods, fields, operators, and destructor.
- **Full language tooling** — parse to an AST, type-check, lint, autocomplete,
  hand-emit bytecode, and JIT to native code with disassembly.

## Install

```sh
zig fetch --save git+https://github.com/zigsel/luau
```

```zig
// build.zig
const luau = b.dependency("luau", .{ .target = target, .optimize = optimize });
exe.root_module.addImport("luau", luau.module("luau"));
```

Options: `-Dvector_size=3|4` (native vector lanes), `-Dcodegen=true|false` (JIT).

## A taste

```zig
const std = @import("std");
const luau = @import("luau");

fn add(a: f64, b: f64) f64 {
    return a + b;
}

pub fn main() !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();

    var vm = try luau.Lua.init(gpa.allocator());
    defer vm.deinit();
    vm.openLibs();

    // expose a Zig function, call it from Luau
    vm.setFn("add", add);
    try vm.doString("=demo", "print(add(40, 2))"); // 42

    // run a script and read a global back, typed
    try vm.doString("=cfg", "port = 8080");
    const port = try vm.get(u16, "port");
    std.debug.print("port = {d}\n", .{port});
}
```

## Two tiers of API

**Embedding the VM** — the Lua C API, made idiomatic:

| | |
|---|---|
| `luau.Lua` | the state: stack, call/pcall, globals, libraries, GC, refs, coroutines, debug |
| `luau.Table` / `luau.Ref` / `luau.Buffer` | ergonomic value handles |
| `luau.compile` / `luau.compiler` | source → owned bytecode |
| `luau.codegen` | native JIT: compile, stats, disassembly, native toggles |

**Tooling over Luau source** — Luau's own additions, via `extern "C"` shims:

| | |
|---|---|
| `luau.ast` | parse → syntax errors, hot comments, full AST node walking |
| `luau.analysis` | type-check, lint, autocomplete |
| `luau.config` | parse `.luaurc` (mode, globals, lint settings) |
| `luau.require` | configurable require-by-string (`Resolver` vtable; sandboxed `FsResolver` included) |
| `luau.bytecode` | hand-emit bytecode (`BytecodeBuilder`) |

## Examples

Run any with `zig build <name>`:

| command | file | shows |
|---|---|---|
| `embedding` | [examples/embedding.zig](examples/embedding.zig) | run a script, read globals typed |
| `zig-functions` | [examples/zig_functions.zig](examples/zig_functions.zig) | expose Zig functions to Luau |
| `usertypes` | [examples/usertypes.zig](examples/usertypes.zig) | register a Zig struct (methods, fields, operators) |
| `modules` | [examples/modules.zig](examples/modules.zig) | bind a Zig namespace as a library |
| `tables` | [examples/tables.zig](examples/tables.zig) | the `Table` handle and a typed `Ref` |
| `coroutines` | [examples/coroutines.zig](examples/coroutines.zig) | drive a Luau coroutine across its yields |
| `compiling` | [examples/compiling.zig](examples/compiling.zig) | compile to bytecode, then JIT it |
| `bytecode` | [examples/bytecode.zig](examples/bytecode.zig) | hand-emit bytecode, then load and run it |
| `parsing` | [examples/parsing.zig](examples/parsing.zig) | parse and walk the AST |
| `ast-types` | [examples/ast_types.zig](examples/ast_types.zig) | reflect type annotations (aliases, unions, params) |
| `analysis` | [examples/analysis.zig](examples/analysis.zig) | type-check, lint, autocomplete |
| `config` | [examples/config.zig](examples/config.zig) | parse `.luaurc`, resolve path aliases |
| `require` | [examples/require.zig](examples/require.zig) | install `require` via a `Resolver` over a virtual module tree |

## Architecture

```
your Zig code
    │  import "luau"
public API (src/root.zig)         pure idiomatic Zig — the only thing you touch
    ├─ src/lua/    embedding the VM (the Lua C API, idiomatic)
    └─ src/luau/   Luau tooling (ast, analysis, config, require, bytecode)
        │
internal `bindings` module        translate-c over the Luau headers + shims
        │
libluau.a                         Luau C++ compiled by Zig (extern "C" + longjmp)
        ├─ shim/                  extern "C" wrappers over the C++ APIs
        └─ Luau 0.726 sources     fetched & built by build.zig
```

The C++ tooling (parser, type checker, …) has no C entry points, so `shim/`
provides hand-written `extern "C"` wrappers that are compiled into `libluau` and
fed through the same `translate-c` pipeline as the C headers.

## Build commands

| command | what |
|---|---|
| `zig build test` | run the test suite (`tests/`) |
| `zig build coverage` | verify every Luau symbol is wrapped (fails if not) |
| `zig build api` | inventory Luau's real public C++ surface (clang AST) |
| `zig build gaps` | per-module shim coverage gap map (`-- <Module>` to drill in) |
| `zig build <example>` | build and run an example |

## Scope

**The C API** (VM, compiler, codegen, require) is wrapped and **gated at
1255/1255** by `zig build coverage` — the stable layer, won't change under you.

**The C++ libraries** (Ast, Compiler, Bytecode, CodeGen, Config, Analysis) are
bound broadly via `extern "C"` shims — **1048 entry points** spanning the common
embedding & tooling paths: parse, build & compile an AST, typed AST node walking,
pretty-print/lex, bytecode emit, native JIT **and the full x64/a64 assemblers**,
type-check, lint, autocomplete, the **type graph**, type relations & transforms,
multi-module/require, host type definitions, go-to-definition, and type
visualization.

This is a **curated subset, not parity** — `zig build gaps` maps what each
module does and doesn't reach. What's left untouched is now overwhelmingly
live-state-only: the constraint solver / type-function runtime (Analysis) and the
JIT lowering pipeline (CodeGen). The everyday standalone surface — including
source-level type-annotation reflection, structured diagnostics, `.luaurc`
aliases, and signature docs — is bound.

These libraries are internal and unstable, so this layer is **pinned to the
vendored Luau version**; `tools/stability.sh` is the upgrade map. Beyond the
curated subset, the parts that are *provably* un-callable from outside the solver
(e.g. `occursCheck` asserts; `Normalizer::unionType` is private) and the deep
constraint-solver
internals that require live solver state. Full accounting in
**[coverage.md](coverage.md)**.
