# Coverage

What's bound, what isn't, and how to check. Two guarantees of very different
strength — don't conflate them.

Run it yourself:
- `zig build coverage` — gates the binding surface; reports C++ entry points.
- `zig build api` — the real public C++ surface (clang AST), bindable vs internal.
- `zig build gaps` — per-module shim coverage gap map (`-- <Module>` to drill in).
- `tools/stability.sh` — per-header churn (the upgrade map for a Luau bump).

## A. The C API — complete & enforced

Every exported symbol of the C-callable headers (`lua.h`, `lualib.h`, `luaconf.h`,
`luacode.h`, `luacodegen.h`, `Luau/Require.h`, `Luau/Bytecode.h`) is wrapped. The
build **fails** if any is unbound:

```
A. binding gate (C API + our shims)   1255/1255 wrapped (1 allowlisted)
```

(The 1 allowlisted is `lua_pushvfstring` — a C `va_list` variadic with no Zig
equivalent; superseded by `Lua.pushFString`/`raiseError`.)

This is the stable, supported layer. It will not change under you.

## B. The C++ API — broad, best-effort, version-pinned

The Luau C++ libraries have no C entry points; we reach them through hand- and
agent-written `extern "C"` shims (`shim/<module>/`) fed through the same
translate-c pipeline. **1,048** C++ entry points are bound across every module.
These libraries are internal and unstable, so this layer is pinned to the
vendored Luau version (see `tools/stability.sh` for what moves on an upgrade).

The real public C++ surface (`zig build api`, clang-parsed) is **3,343** methods
(2,770 "bindable", 573 internal, plus 1,384 auto-constructors). Our 1,048
entry points are *handle-based, higher-level* wrappers — they don't map 1:1 onto
those methods (one entry point often subsumes several overloads; many trivial
getters are folded into a handle). **This is a curated subset — not parity.**

### Where the gaps are (`zig build gaps`)

Coverage is **broad, not total**, and the gaps cluster in whole sub-areas the binding
never reached rather than scattering evenly. `zig build gaps` asks, per upstream class, whether
its name is referenced anywhere in our shims:

| module | classes | touched | untouched |
|---|--:|--:|--:|
| Ast | 70 | 54 | 16 |
| Compiler | 2 | 2 | 0 |
| Config | 10 | 5 | 5 |
| Bytecode | 38 | 11 | 27 |
| CodeGen | 66 | 16 | 50 |
| Analysis | 377 | 111 | 266 |

(De-noised: STL/libc++ template leakage and hash/eq functor helpers are filtered
out — `zig build gaps` no longer counts them. Analysis "touched" rose
81→111 as the structured-diagnostics binding now references the error structs.)

Two caveats, both load-bearing:
- **"touched" is an upper bound.** It means the shim *names* the class, not that
  it exposes its data. (This is how the Ast type-node classes once read as
  "touched" with zero field accessors — now fixed: `luau.ast` exposes full
  type-annotation reflection, see the Ast row below.)
- **Many untouched classes are intentional.** A large share of the CodeGen (50)
  and Analysis (266) untouched classes are IR/solver internals that need live
  state and were never in scope (see "Intentionally NOT bound" below). The
  untouched count is *not* a to-do list — but it is an honest map of what a given
  use case might hit. Run `zig build gaps -- <Module>` to read the list.

Remaining functional gaps cluster in live-state-only territory: the constraint
solver / type-function runtime (Analysis), the JIT lowering pipeline (CodeGen),
and the in-place `.luaurc` mutation paths (Config) — all requiring live engine
state, and all reachable in spirit through the higher-level handles we expose.

### Per module

| module | bound (idiomatic Zig) |
|---|---|
| **Ast** | parse + diagnostics + hot comments; full node walk; **typed field accessors** for every node kind; **AST construction** (all node types) + `compileOrThrow`; pretty-print; lexer; AST→JSON; CST; attributes; confusables |
| **Compiler** | `compile()` (C); `compileOrThrow` from a built AST; structured `CompileError` |
| **Bytecode** | `BytecodeBuilder` — frames, constants (incl. table/class shapes), opcodes, debug & type info; bytecode graph |
| **CodeGen** | public `CodeGen.h` (compile + stats + native/IR disassembly + toggles); **full x64 + a64 assemblers** (~109/~108 instructions via operand handles); `IrBuilder`/IR inspection |
| **Config** | `.luaurc`: mode, globals, aliases (original case, case-folded key, value, `.luaurc` location, case-insensitive resolve), per-rule lint state |
| **Require** | configurable require-by-string (native C) |
| **Analysis** | type-check, lint (+ rule list + standalone), autocomplete (+ rich/fragment), type-at-position, **type graph** (`Type`/`TypePack` inspection), type relations (`isSubtype`), normalize/equality, type transforms (instantiate/quantify/anyify), multi-module + `require`, host type definitions, go-to-definition / symbols, module/scope inspection, ToDot/JSON viz, type utilities, RequireTracer; **structured diagnostics** (stable kind enum + location + message + cheap typed string fields); **richer documentation variants** (function/table/overloaded for signature help & hover); **require-path suggestion data carriers** (suggestion/alias + tags) |

### Intentionally NOT bound — and why

These were attempted and removed (build stays green) because they are not
callable from outside Luau's solver:

| function | reason |
|---|---|
| `occursCheck(TypeId, TypeId)` | `LUAU_ASSERT`-aborts unless called with live free-type/unifier state |
| `Normalizer::unionType` / `intersectionType` | **private** members of `Normalizer` |

Plus, by design, the long tail not worth a binding: the ~573 methods whose
signatures require live solver state (`NotNull<Scope>`, `TxnLog&`,
`UnifierSharedState`, in-flight free types — the constraint solver / unifier
internals), redundant operator/constructor overloads, and trivial private-member
getters. These serve neither embedding nor tooling; `zig build api` lists them.

## Summary

| layer | guarantee |
|---|---|
| C API | **100%, gated** — nothing skipped, won't break |
| C++ embedding + tooling | **broad, curated subset** — common paths covered (parse/build/compile/typecheck/lint/autocomplete/types/JIT/format/bytecode); thin in corners like Ast type-node reflection. `zig build gaps` maps it. |
| C++ deep internals | bound where callable; the rest provably needs solver state |
