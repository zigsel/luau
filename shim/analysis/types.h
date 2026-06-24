// extern "C" shim for STRUCTURAL INSPECTION of inferred Luau types.
//
// `luau_types_check` typechecks a single in-memory module (with the full type
// graph retained) and keeps the Frontend + Module alive in an opaque handle so
// the `TypeId` handles it hands back stay valid. From there you can walk the
// inferred type object graph: kinds, stringification, and per-kind structural
// accessors (functions, tables, unions, classes, ...).
//
// Lifetime: every `LuauType*` / `LuauTypePack*` derived from a `LuauTypes*`
// remains valid until that `LuauTypes*` is freed with `luau_types_free`. The
// derived handles are owned by the checker; do NOT free them individually.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// ---- stable kind enum ------------------------------------------------------
//
// A stable, documented classification of a (followed) inferred type. The
// numeric values are part of this shim's ABI and must not be reordered.
enum LuauTypeKind {
    LUAU_TYPE_PRIMITIVE = 0,    // PrimitiveType (nil/boolean/number/string/...)
    LUAU_TYPE_SINGLETON = 1,    // SingletonType (literal bool/string)
    LUAU_TYPE_FUNCTION = 2,     // FunctionType
    LUAU_TYPE_TABLE = 3,        // TableType
    LUAU_TYPE_METATABLE = 4,    // MetatableType
    LUAU_TYPE_CLASS = 5,        // ExternType (a.k.a. class)
    LUAU_TYPE_UNION = 6,        // UnionType
    LUAU_TYPE_INTERSECTION = 7, // IntersectionType
    LUAU_TYPE_GENERIC = 8,      // GenericType
    LUAU_TYPE_FREE = 9,         // FreeType
    LUAU_TYPE_BOUND = 10,       // BoundType (normally followed away; defensive)
    LUAU_TYPE_ANY = 11,         // AnyType
    LUAU_TYPE_UNKNOWN = 12,     // UnknownType
    LUAU_TYPE_NEVER = 13,       // NeverType
    LUAU_TYPE_ERROR = 14,       // ErrorType
    LUAU_TYPE_NEGATION = 15,    // NegationType
    LUAU_TYPE_UNKNOWN_KIND = 16 // anything else (blocked / pending / lazy / tyfun)
};

// A primitive's concrete kind (mirrors PrimitiveType::Type).
enum LuauPrimitiveKind {
    LUAU_PRIM_NIL = 0,
    LUAU_PRIM_BOOLEAN = 1,
    LUAU_PRIM_NUMBER = 2,
    LUAU_PRIM_INTEGER = 3,
    LUAU_PRIM_STRING = 4,
    LUAU_PRIM_THREAD = 5,
    LUAU_PRIM_FUNCTION = 6,
    LUAU_PRIM_TABLE = 7,
    LUAU_PRIM_BUFFER = 8,
    LUAU_PRIM_UNKNOWN = 9
};

// The kind of a singleton literal.
enum LuauSingletonKind {
    LUAU_SINGLETON_NONE = 0,
    LUAU_SINGLETON_BOOL = 1,
    LUAU_SINGLETON_STRING = 2
};

// ---- checker ---------------------------------------------------------------

// Typecheck the module "main" with `retainFullTypeGraphs = true` and builtin
// globals registered. Never returns NULL (errors are reported via the error
// accessors below). Keeps the Frontend + checked Module alive.
LuauTypes* luau_types_check(const char* src, size_t len);

// Error accessors (mirror LuauCheck).
int luau_types_error_count(const LuauTypes* h);
const char* luau_types_error_message(const LuauTypes* h, int i); // borrowed
LuauPosition luau_types_error_position(const LuauTypes* h, int i);

// Inferred TypeId of a top-level binding/global named `name`, or NULL if not
// found. The returned handle is owned by `h`.
LuauType* luau_types_require_global(LuauTypes* h, const char* name);

void luau_types_free(LuauTypes* h);

// ---- type inspection -------------------------------------------------------

// Stable kind of `t` (one of LuauTypeKind). `t` is followed before inspection.
int luau_type_kind(LuauType* t);

// Luau::toString(TypeId) as a malloc'd, NUL-terminated string the caller frees.
// NULL on failure.
char* luau_type_tostring(LuauType* t);

// PRIMITIVE: the primitive's kind (LuauPrimitiveKind) and a borrowed name
// ("nil", "boolean", ...). name returns NULL when not a primitive.
int luau_type_primitive_kind(LuauType* t);
const char* luau_type_primitive_name(LuauType* t);

// SINGLETON: which singleton, and its value. *_kind returns LUAU_SINGLETON_NONE
// when not a singleton.
int luau_type_singleton_kind(LuauType* t);
int luau_type_singleton_bool(LuauType* t);           // 0/1; 0 if N/A
const char* luau_type_singleton_string(LuauType* t); // borrowed; NULL if N/A

// FUNCTION: argument / return type packs (NULL if not a function).
LuauTypePack* luau_type_function_args(LuauType* t);
LuauTypePack* luau_type_function_rets(LuauType* t);

// TABLE: properties (sorted by name, std::map order) + indexer presence.
int luau_type_table_prop_count(LuauType* t);
const char* luau_type_table_prop_name(LuauType* t, int i); // borrowed; NULL if N/A
LuauType* luau_type_table_prop_type(LuauType* t, int i);   // NULL if N/A
int luau_type_table_has_indexer(LuauType* t);              // 0/1

// METATABLE: the underlying table and the metatable (NULL if not a metatable).
LuauType* luau_type_metatable_table(LuauType* t);
LuauType* luau_type_metatable_metatable(LuauType* t);

// UNION / INTERSECTION: option/part count and the i-th option/part.
int luau_type_union_count(LuauType* t);
LuauType* luau_type_union_at(LuauType* t, int i);
int luau_type_intersection_count(LuauType* t);
LuauType* luau_type_intersection_at(LuauType* t, int i);

// CLASS (ExternType): name + parent type (parent NULL if root or N/A).
const char* luau_type_class_name(LuauType* t); // borrowed; NULL if N/A
LuauType* luau_type_class_parent(LuauType* t);

// NEGATION: the negated type (NULL if not a negation).
LuauType* luau_type_negation_inner(LuauType* t);

// GENERIC: borrowed name (may be NULL/empty for synthetic generics).
const char* luau_type_generic_name(LuauType* t);

// ---- type pack inspection --------------------------------------------------

// Number of head types in the (flattened-by-one-follow) pack.
int luau_typepack_count(LuauTypePack* tp);
// The i-th head type (NULL if out of range).
LuauType* luau_typepack_at(LuauTypePack* tp, int i);
// The pack's tail (variadic/generic/free continuation), or NULL if fixed-length.
LuauTypePack* luau_typepack_tail(LuauTypePack* tp);

LUAU_END_DECLS
