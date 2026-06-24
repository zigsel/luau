// extern "C" shim for self-contained Analysis scans over a parsed module —
// small public entry points that need no live solver/TypeArena state.
//
// What this binds (the genuinely-callable, no-live-solver-state pieces):
//
//   * RequireTracer  (Luau/RequireTracer.h)
//       `traceRequires` walks a parsed module and collects every `require(...)`
//       target. It only needs a FileResolver to map a require *argument* to a
//       module name; we supply a trivial resolver that reads the string literal
//       passed to `require`, so the result is the list of statically-known
//       require string targets in the source. No solver / TypeArena needed.
//
//   * AstUtils::matchTypeGuard  (Luau/AstUtils.h)
//       Pure pattern-match over a binary expression (`typeof(x) == "string"`,
//       `type(x) == "number"`, ...). No types, no solver — just AST shape.
//
//   * Polarity helpers  (Luau/Polarity.h)
//       Pure inline bit ops over the Polarity enum (positive/negative/known,
//       invert, union/intersect). Exposed as plain int functions.
//
// What is deliberately SKIPPED (deep solver internals; see tail.cpp for the
// per-header rationale): OverloadResolver, Refinement, Predicate,
// TableLiteralInference (pushTypeInto), TypeFunction / TypeFunctionRuntime,
// RecursionCounter. Each of those requires live solver state (ConstraintSolver,
// Unifier2, TxnLog, a runtime lua_State, free types, ...) that cannot be
// reconstructed from a finished checker, or is an RAII guard with no meaningful
// C entry point. Binding them would LUAU_ASSERT-abort or be inert.
//
// Lifetime: a `LuauScan*` owns its own parse (Allocator + AstNameTable +
// ParseResult). Borrowed strings it returns live until `luau_scan_free`.
// The require-target strings are malloc'd copies the caller frees.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// ---- parse-and-trace handle ------------------------------------------------

// Parse `src` (length `len`) into a self-contained handle. Never returns NULL;
// `luau_scan_has_root` reports whether parsing produced a usable AST root.
LuauScan* luau_scan_parse(const char* src, size_t len);

// 1 if the parse produced a non-null statement-block root, else 0.
int luau_scan_has_root(const LuauScan* h);

// Number of parse errors (requires may still be traceable on a partial root).
int luau_scan_error_count(const LuauScan* h);
const char* luau_scan_error_message(const LuauScan* h, int i); // borrowed; NULL OOB

void luau_scan_free(LuauScan* h);

// ---- RequireTracer ---------------------------------------------------------

// Run `traceRequires` over the parsed root. Returns the number of require
// targets discovered (string-literal arguments to `require`), or -1 if there is
// no root. Results are cached on the handle until the next call.
int luau_scan_trace_requires(LuauScan* h);

// Number of require targets from the most recent successful trace.
int luau_scan_require_count(const LuauScan* h);

// The i-th require target module name as a malloc'd, NUL-terminated string the
// caller frees. NULL if `i` is out of range.
char* luau_scan_require_name(const LuauScan* h, int i);

// Source location (begin) of the i-th require call expression.
LuauPosition luau_scan_require_position(const LuauScan* h, int i);

// ---- AstUtils: matchTypeGuard ----------------------------------------------
//
// Scan the parsed AST for binary expressions that `matchTypeGuard` recognises as
// type guards (e.g. `typeof(x) == "string"`). Each match records: whether it
// used `typeof` (vs `type`), and the guard's target type string.

// Run the scan; returns the number of type guards found, or -1 if no root.
// Cached until the next call.
int luau_scan_type_guards(LuauScan* h);

// Number of type guards from the most recent successful scan.
int luau_scan_type_guard_count(const LuauScan* h);

// 1 if the i-th guard used `typeof(...)`, 0 if `type(...)`; -1 if OOB.
int luau_scan_type_guard_is_typeof(const LuauScan* h, int i);

// The i-th guard's target type string (e.g. "string") as a malloc'd, NUL-
// terminated string the caller frees. NULL if OOB.
char* luau_scan_type_guard_type(const LuauScan* h, int i);

// Source location (begin) of the i-th guard's binary expression.
LuauPosition luau_scan_type_guard_position(const LuauScan* h, int i);


LUAU_END_DECLS
