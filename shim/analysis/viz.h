// extern "C" shim: VISUALIZATION / serialization of inferred Luau types.
//
// Operates on the `LuauType*` / `LuauTypePack*` handles produced by the
// `types` shim (each wraps a Luau `TypeId` / `TypePackId`). Provides:
//   - Graphviz `dot` rendering of a type's object graph (Luau::toDot).
//   - `toString` with the full `ToStringOptions` exposed as POD args.
//
// All returned `char*` strings are malloc'd, NUL-terminated, and owned by the
// caller (free with free()). NULL is returned on failure.
//
// NOTE on JSON: Luau's JsonEmitter (JsonEmitter.h) only provides `write`
// overloads for POD/string/container types, not for `TypeId`/`TypePackId` — the
// type-graph JSON encoder is internal (no public entry). A usable type-graph
// JSON serializer is therefore NOT exposed here; use `toString` / `toDot`
// instead. (AST-level JSON lives in the `json` shim.)

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// ---- Graphviz dot ----------------------------------------------------------

// Render `t`'s type object graph as a Graphviz `dot` string (Luau::toDot).
// `show_pointers` / `duplicate_primitives` map to ToDotOptions. NULL on failure.
char* luau_viz_type_to_dot(LuauType* t, int show_pointers, int duplicate_primitives);

// Same for a type pack.
char* luau_viz_typepack_to_dot(LuauTypePack* tp, int show_pointers, int duplicate_primitives);

// ---- toString with options -------------------------------------------------

// Options mirroring Luau::ToStringOptions, exposed as plain ints/bools. A value
// of 0 for `max_table_length` / `max_type_length` means "use Luau's default".
typedef struct LuauToStringOptions {
    int exhaustive;                       // produce complete (vs comprehensible) output
    int use_line_breaks;                  // insert newlines between table entries/metatable
    int function_type_arguments;          // output function arg names when available
    int hide_table_kind;                  // surround all tables with plain '{}'
    int hide_named_function_type_parameters;
    int hide_function_self_argument;      // omit `self: X` from signatures
    int hide_table_alias_expansions;
    int use_question_marks;               // postfix '?' for options instead of `| nil`
    int ignore_synthetic_name;
    size_t max_table_length;              // 0 => Luau default
    size_t max_type_length;               // 0 => Luau default
    size_t composite_types_single_line_limit; // 0 => Luau default (5)
} LuauToStringOptions;

// Luau::toString(TypeId, ToStringOptions) with the given options. NULL on failure.
char* luau_viz_type_to_string(LuauType* t, const LuauToStringOptions* opts);

// Luau::toString(TypePackId, ToStringOptions) with the given options.
char* luau_viz_typepack_to_string(LuauTypePack* tp, const LuauToStringOptions* opts);

// ---- toStringDetailed ------------------------------------------------------

// Detailed stringification: the result string plus the boolean status flags
// (invalid/error/cycle/truncated) ToStringResult reports. The string is written
// to `*out` (malloc'd; caller frees) and each `*out_<flag>` (if non-NULL) is set
// to 0/1. Returns 1 on success, 0 on failure (in which case `*out` is NULL).
int luau_viz_type_to_string_detailed(
    LuauType* t,
    const LuauToStringOptions* opts,
    char** out,
    int* out_invalid,
    int* out_error,
    int* out_cycle,
    int* out_truncated);

LUAU_END_DECLS
