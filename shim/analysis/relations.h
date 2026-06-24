// extern "C" shim for TYPE RELATIONS between inferred Luau types.
//
// `luau_relations_check` typechecks a single in-memory module (retaining the
// full type graph) and keeps the Frontend + Module alive in an opaque handle.
// You can then ask a subtype query between the inferred types of two top-level
// bindings: `type(nameA) <: type(nameB)`.
//
// The relation is decided with Luau's `Subtyping` engine, wired up from the
// Frontend's builtin types, the module's own `TypeArena`, a fresh `Scope`, a
// `UnifierSharedState`, a `Normalizer` and a `TypeFunctionRuntime`. Any failure
// (parse/type error, missing binding, or an exception thrown while building the
// engine) is reported as -1 ("unknown/error").

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Type-check `src` (length `len`) as a single module and retain its type graph.
// Always returns a handle; query results may still be -1 if checking failed.
LuauRelations* luau_relations_check(const char* src, size_t len);

// Is the inferred type of top-level binding `nameA` a subtype of `nameB`?
//   1  -> yes
//   0  -> no
//  -1  -> unknown/error (missing binding, check failed, or engine error)
int luau_relations_is_subtype(LuauRelations* h, const char* nameA, const char* nameB);

void luau_relations_free(LuauRelations* h);

LUAU_END_DECLS
