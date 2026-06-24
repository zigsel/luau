// extern "C" shim for Luau type TRANSFORMS over an inferred type graph.
//
// These wrap the deep solver-internal type transformations (Instantiation,
// Anyification, ApplyTypeFunction, Generalization) so they can be driven from a
// FINISHED `LuauTypes*` checker (see types.h / types_internal.h). Each transform
// sources its required context (TypeArena, builtinTypes, a module Scope, a fresh
// TxnLog/UnifierSharedState) from the owning checker the same way relations.cpp
// builds a Subtyping engine — so any synthesized types live in the module arena
// and are wrapped back into the SAME checker's handle cache.
//
// Lifetime: every returned `LuauType*` is owned by the input checker `h` and
// stays valid until `luau_types_free(h)`. Do NOT free returned handles.
//
// SCOPE / SAFETY: only transforms that are safely reachable from a finished
// checker are bound. Transforms requiring LIVE solver state (in-flight TxnLog,
// free types with tracked polarity/use-count, a live Subtyping for generic
// substitution maps) are intentionally NOT exposed because they would
// LUAU_ASSERT-abort or corrupt the shared module type graph. Specifically:
//   - quantify(): SKIPPED. It mutates shared module types IN PLACE
//     (`*asMutable(ty) = GenericType{...}`), corrupting the checker's cached
//     handle graph, and is only meaningful on fresh free types produced by the
//     old solver. No safe return-by-value form exists.
//   - generalizeType()/generalizeTypePack(): SKIPPED. They require a
//     GeneralizationParams (polarity / useCount / foundOutsideFunctions) that is
//     only known to the live solver while generalizing a function signature.
//   - instantiate2(): SKIPPED. It requires a live `Subtyping` plus generic->free
//     substitution maps that only the constraint solver constructs in-flight.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// INSTANTIATE: given a generic function type, return a copy with its outermost
// generics replaced by fresh free types (higher-order generics are left as-is).
// Non-generic inputs are returned essentially unchanged. Returns NULL on failure
// (recursion limit exceeded / not a checked module / internal error).
//
// `h` is the owning checker; `t` must be one of its type handles.
LuauType* luau_transforms_instantiate(LuauTypes* h, LuauType* t);

// ANYIFY: substitute every free type/pack reachable from `t` by `any`. On a
// finished module (which generally has no free types) this is effectively a
// structural copy. Returns NULL on failure / normalization-too-complex.
LuauType* luau_transforms_anyify(LuauTypes* h, LuauType* t);

// APPLY TYPE FUNCTION: if `fn` is a generic FunctionType with N generics and
// `args` supplies exactly N type arguments, substitute each generic by its
// corresponding argument and return the resulting (monomorphic) type. Returns
// NULL if `fn` is not a generic function, if the argument count does not match
// the generic count, or on internal error. `genericPacks` are not handled by
// this entry point (only ordinary type generics are substituted).
LuauType* luau_transforms_apply_type_function(LuauTypes* h, LuauType* fn, LuauType* const* args, int arg_count);

// GENERALIZE: attempt to generalize `t`, replacing free types by their bounds
// (turning a not-fully-generalized function into a polymorphic one). Returns
// NULL on failure (resource limits exceeded / internal error). On a finished
// module with no remaining free types this typically returns `t` unchanged.
LuauType* luau_transforms_generalize(LuauTypes* h, LuauType* t);

LUAU_END_DECLS
