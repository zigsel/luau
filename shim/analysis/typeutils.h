// extern "C" shim for Luau TYPE UTILITIES & MANIPULATION.
//
// Free-function utilities from Luau's Analysis library that operate over the
// inferred type graph: `TypeUtils.h` predicates/reducers, `Simplify.h`
// union/intersection simplification + relation queries, `TypePath.h` path
// construction + traversal, and `Clone.h` deep cloning.
//
// These reuse the LuauType*/LuauTypePack* handle model from "types.h": every
// handle is owned by the LuauTypes* checker that produced it, and any new type
// minted here (by simplify/clone/traverse) is wrapped back into that SAME
// checker (allocated in the checker's module arena) so it stays valid until the
// checker is freed. Utilities that need a checker context take a LuauTypes* as
// their first argument; they source the arena, builtinTypes and module scope
// from it the way relations.cpp does.
//
// Methods requiring deep solver state (TxnLog, Normalizer, UnifierSharedState,
// constraint-solving) are intentionally omitted; see typeutils.cpp for notes.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// ---- follow ----------------------------------------------------------------

// follow(TypeId): resolve bound/redirected types to their representative. The
// result is wrapped into the same owning checker. NULL on failure.
LuauType* luau_typeutils_follow(LuauType* t);

// ---- pointer-only predicates (no checker context required) -----------------
//
// Each returns 0/1. They follow the type internally. A NULL handle yields 0.

int luau_typeutils_is_nil(LuauType* t);
int luau_typeutils_is_boolean(LuauType* t);
int luau_typeutils_is_number(LuauType* t);
int luau_typeutils_is_integer(LuauType* t);
int luau_typeutils_is_string(LuauType* t);
int luau_typeutils_is_thread(LuauType* t);
int luau_typeutils_is_buffer(LuauType* t);
// isOptional: structurally `T?` (a union containing nil). Quick, not semantic.
int luau_typeutils_is_optional(LuauType* t);
int luau_typeutils_is_table_union(LuauType* t);
int luau_typeutils_is_table_intersection(LuauType* t);
int luau_typeutils_is_overloaded_function(LuauType* t);
// maybeSingleton: whether the type is, or unions, a singleton.
int luau_typeutils_maybe_singleton(LuauType* t);
// isGeneric: the type is a (possibly bound) generic.
int luau_typeutils_is_generic(LuauType* t);
// isPrim(ty, primKind) where primKind is a LuauPrimitiveKind from types.h.
int luau_typeutils_is_prim(LuauType* t, int prim_kind);

// TypeUtils.h fast/approximate predicates.
// fastIsSubtype: a fast (incomplete) approximation of subTy <: superTy.
int luau_typeutils_fast_is_subtype(LuauType* sub, LuauType* super);
// approximately `false | nil` / `~(false | nil)` (syntactic, not semantic).
int luau_typeutils_is_approx_falsy(LuauType* t);
int luau_typeutils_is_approx_truthy(LuauType* t);
// isBlocked: a blocked / pending-expansion / unsolved-type-function type.
int luau_typeutils_is_blocked(LuauType* t);
// NOTE: occursCheck is intentionally NOT exposed. Luau's occursCheck(TypeId,
// TypeId) LUAU_ASSERTs on a concrete (non-free) needle, so it is only safe
// inside the unifier with live solver state; see typeutils.cpp.

// ---- predicates needing builtinTypes (checker context) ---------------------

// isOptionalType: ty is a supertype of nil. Needs builtinTypes -> takes checker.
int luau_typeutils_is_optional_type(LuauTypes* h, LuauType* t);

// ---- reducers / manipulators (checker context) -----------------------------

// stripNil(ty): remove nil from a union if another option remains, allocating in
// the checker arena. Returns the (possibly new) type wrapped into the checker.
LuauType* luau_typeutils_strip_nil(LuauTypes* h, LuauType* t);

// getApproximateReturnTypeForFunctionCall(ty): an approximate return type pack
// for a function/union-of-functions. NULL if not applicable.
LuauTypePack* luau_typeutils_approximate_return(LuauType* t);

// ---- Simplify.h ------------------------------------------------------------

// simplifyUnion / simplifyIntersection: combine two types into a simplified
// result type, allocated in the checker arena and wrapped into the checker.
// NULL on failure.
LuauType* luau_typeutils_simplify_union(LuauTypes* h, LuauType* a, LuauType* b);
LuauType* luau_typeutils_simplify_intersection(LuauTypes* h, LuauType* a, LuauType* b);

// relate(a, b): the set relation between two types (a LuauRelation below).
enum LuauRelation {
    LUAU_RELATION_DISJOINT = 0,    // No A is a B or vice versa
    LUAU_RELATION_COINCIDENT = 1,  // Every A is in B and vice versa
    LUAU_RELATION_INTERSECTS = 2,  // Some As are Bs and some Bs are As
    LUAU_RELATION_SUBSET = 3,      // Every A is in B
    LUAU_RELATION_SUPERSET = 4,    // Every B is in A
    LUAU_RELATION_UNKNOWN = -1     // error / null handle
};
int luau_typeutils_relate(LuauType* a, LuauType* b);

// ---- Clone.h ---------------------------------------------------------------

// clone(ty): deep-clone a type into the checker's arena, returning a new handle.
// shallow=1 clones only the top-level constructor. NULL on failure.
LuauType* luau_typeutils_clone(LuauTypes* h, LuauType* t, int shallow);

// ---- TypePath.h ------------------------------------------------------------
//
// Build a relative path, then traverse it from a root type/pack. A path handle
// is independent of any checker; it must be freed with luau_typepath_free.

LuauTypePath* luau_typepath_new(void);
void luau_typepath_free(LuauTypePath* p);

// Append components (each mirrors a PathBuilder method). `name` is copied.
void luau_typepath_read_prop(LuauTypePath* p, const char* name);
void luau_typepath_write_prop(LuauTypePath* p, const char* name);
void luau_typepath_index(LuauTypePath* p, size_t i);    // Index (union/intersection/pack)
void luau_typepath_metatable(LuauTypePath* p);
void luau_typepath_lower_bound(LuauTypePath* p);
void luau_typepath_upper_bound(LuauTypePath* p);
void luau_typepath_index_key(LuauTypePath* p);          // IndexLookup
void luau_typepath_index_value(LuauTypePath* p);        // IndexResult
void luau_typepath_negated(LuauTypePath* p);
void luau_typepath_variadic(LuauTypePath* p);
void luau_typepath_args(LuauTypePath* p);               // function arguments pack
void luau_typepath_rets(LuauTypePath* p);               // function returns pack

// Stringify the path. Both return malloc'd, NUL-terminated strings to free().
// `human` produces the error-reporting form; otherwise the debug form.
char* luau_typepath_tostring(LuauTypePath* p, int human);

// Traverse `p` from `root` (a type) to a type / pack endpoint. The result is
// wrapped into the checker owning `root`. NULL if the traversal fails.
LuauType* luau_typepath_traverse_to_type(LuauType* root, LuauTypePath* p);
LuauTypePack* luau_typepath_traverse_to_pack(LuauType* root, LuauTypePath* p);

LUAU_END_DECLS
