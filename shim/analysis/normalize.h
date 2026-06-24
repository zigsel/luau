// extern "C" shim for TYPE NORMALIZATION and STRUCTURAL EQUALITY of inferred
// Luau types.
//
// These operate over the `LuauType*` handles produced by a `LuauTypes*` checker
// (see types.h). Each `LuauType*` carries its owning checker, from which we
// source the analysis context (module TypeArena + Frontend builtinTypes + a
// solver mode read off the checked module) needed to build a `Luau::Normalizer`
// — exactly the same context `relations.cpp` assembles for subtyping.
//
// Normalization rewrites a type into the canonical disjunctive-normal form the
// solver uses internally (see Normalize.h). Newly minted normalized types are
// allocated in the owning checker's arena, so every returned `LuauType*` stays
// valid until that checker is freed with `luau_types_free` (the borrowed-handle
// contract from types.h). Do NOT free returned handles individually.
//
// NOT BOUND (and why):
//   * Substitution.h — `Luau::Substitution`/`Tarjan` are abstract bases whose
//     `isDirty`/`clean`/`foundDirty` are pure virtual and meant to be
//     subclassed INSIDE the solver (quantification/instantiation). They also
//     require a live `TxnLog*` (transaction-log of in-flight unification) and
//     operate over free/blocked types. None of that is reconstructible from a
//     finished checker, so the whole header is intentionally skipped.
//   * Normalizer methods that mutate solver-owned NormalizedType aggregates,
//     consume free/blocked/generic `tyvars`, or need a `NotNull<Scope>` +
//     SeenSet plumbing (unionNormalWithTy, intersectNormals, negateNormal,
//     isInhabited, ...) are skipped: they are solver-internal and several
//     LUAU_ASSERT-abort or read stale free-type state when called standalone.
//   * isSubtype free functions in Normalize.h require a `Simplifier`
//     (EqSat-backed) and Scope; subtyping is already exposed via relations.*.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Normalize `t` into canonical form and return the resulting type, freshly
// built into the owning checker's arena (so it lives as long as the checker).
// Returns NULL if `t` is NULL, the owning checker lacks usable context, the
// type is too complex to normalize, or an internal error is raised.
LuauType* luau_normalize_type(LuauType* t);

// Convenience: the stringified normal form of `t` (Luau::toString of the
// normalized type) as a malloc'd, NUL-terminated string the caller frees.
// NULL on any failure.
char* luau_normalize_tostring(LuauType* t);

// DROPPED: luau_normalize_union / luau_normalize_intersection — they would call
// Normalizer::unionType / intersectionType, which are PRIVATE members of
// Luau::Normalizer and thus not bindable from the shim.

// STRUCTURAL type equality (StructuralTypeEquality.h `areEqual`): a deep,
// syntactic, cycle-safe comparison of two types' shapes. This is NOT semantic
// equivalence (e.g. it does not normalize unions); two semantically equal but
// differently-spelled types may compare unequal. Returns 1 if equal, 0 if not,
// -1 on error (NULL handle or internal failure).
int luau_type_structurally_equal(LuauType* a, LuauType* b);

LUAU_END_DECLS
