// Internal (C++ only) bridge so sibling Analysis shims can reuse the
// LuauTypes/LuauType/LuauTypePack handle model defined in types.cpp.
//
// This header is NOT part of the public C ABI: it exposes the real Luau
// TypeId/TypePackId plumbing and the per-checker handle cache so that other
// shim translation units (e.g. typeutils.cpp) can wrap freshly-minted TypeIds
// into the SAME owning checker, keeping the borrowed-handle contract intact.
//
// Only include this from .cpp shims (after the relevant Luau headers).
#pragma once

#include "Luau/Type.h"
#include "Luau/TypePack.h"

struct LuauTypes; // opaque to the public header; defined in types.cpp

// Wrap a TypeId/TypePackId into a handle owned by `owner` (interned + cached).
// Returns nullptr for a null id. The handle stays valid until the owner is freed.
Luau::TypeId luau_types_internal_typeid(LuauType* h);
Luau::TypePackId luau_types_internal_packid(LuauTypePack* h);

LuauType* luau_types_internal_wrap_type(LuauTypes* owner, Luau::TypeId id);
LuauTypePack* luau_types_internal_wrap_pack(LuauTypes* owner, Luau::TypePackId id);

// The checker that owns a handle (for sourcing arena/builtinTypes/scope).
LuauTypes* luau_types_internal_owner_of_type(const LuauType* h);
LuauTypes* luau_types_internal_owner_of_pack(const LuauTypePack* h);

// Accessors onto the owning checker's analysis context. Any may be null if the
// module failed to check.
namespace Luau {
struct TypeArena;
struct BuiltinTypes;
struct Scope;
struct Module;
} // namespace Luau

Luau::TypeArena* luau_types_internal_arena(LuauTypes* owner);
Luau::BuiltinTypes* luau_types_internal_builtins(LuauTypes* owner);
Luau::Scope* luau_types_internal_scope(LuauTypes* owner);

// The owning checker's checked Module (null if it failed to check).
Luau::Module* luau_types_internal_module(LuauTypes* owner);
