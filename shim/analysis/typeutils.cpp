// extern "C" shim: Luau type utilities & manipulation.
//
// Reuses the LuauTypes/LuauType/LuauTypePack handle model from types.cpp via the
// internal bridge (types_internal.h). New types minted here are allocated in the
// owning checker's module arena and wrapped back into that same checker.

#include "typeutils.h"
#include "types.h"          // LuauPrimitiveKind for isPrim
#include "types_internal.h" // handle bridge into types.cpp

#include "Luau/Clone.h"
#include "Luau/Simplify.h"
#include "Luau/Type.h"
#include "Luau/TypeArena.h"
#include "Luau/TypeOrPack.h"
#include "Luau/TypePack.h"
#include "Luau/TypePath.h"
#include "Luau/TypeUtils.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <string>

using namespace Luau;

namespace {

char* dupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}

TypeId tid(LuauType* h) {
    return luau_types_internal_typeid(h);
}

// Source the checker context (arena + builtinTypes) needed by many utilities.
struct Ctx {
    LuauTypes* owner = nullptr;
    TypeArena* arena = nullptr;
    BuiltinTypes* builtins = nullptr;
    bool ok() const {
        return owner && arena && builtins;
    }
};

Ctx ctxFromChecker(LuauTypes* h) {
    Ctx c;
    c.owner = h;
    c.arena = luau_types_internal_arena(h);
    c.builtins = luau_types_internal_builtins(h);
    return c;
}

// The same, but derive the owning checker from a type handle.
Ctx ctxFromType(LuauType* t) {
    return ctxFromChecker(luau_types_internal_owner_of_type(t));
}

} // namespace

// ---- follow ----------------------------------------------------------------

extern "C" LuauType* luau_typeutils_follow(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        LuauTypes* owner = luau_types_internal_owner_of_type(t);
        return luau_types_internal_wrap_type(owner, follow(tid(t)));
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- pointer-only predicates -----------------------------------------------

#define PRED(fn, expr)                          \
    extern "C" int fn(LuauType* t) {            \
        if (!t)                                 \
            return 0;                           \
        try {                                   \
            TypeId ty = follow(tid(t));         \
            return (expr) ? 1 : 0;              \
        } catch (const std::exception&) {       \
            return 0;                           \
        }                                       \
    }

PRED(luau_typeutils_is_nil, isNil(ty))
PRED(luau_typeutils_is_boolean, isBoolean(ty))
PRED(luau_typeutils_is_number, isNumber(ty))
PRED(luau_typeutils_is_integer, isInteger(ty))
PRED(luau_typeutils_is_string, isString(ty))
PRED(luau_typeutils_is_thread, isThread(ty))
PRED(luau_typeutils_is_buffer, isBuffer(ty))
PRED(luau_typeutils_is_optional, isOptional(ty))
PRED(luau_typeutils_is_table_union, isTableUnion(ty))
PRED(luau_typeutils_is_table_intersection, isTableIntersection(ty))
PRED(luau_typeutils_is_overloaded_function, isOverloadedFunction(ty))
PRED(luau_typeutils_maybe_singleton, maybeSingleton(ty))
PRED(luau_typeutils_is_generic, isGeneric(ty))
PRED(luau_typeutils_is_approx_falsy, isApproximatelyFalsyType(ty))
PRED(luau_typeutils_is_approx_truthy, isApproximatelyTruthyType(ty))
PRED(luau_typeutils_is_blocked, isBlocked(ty))

#undef PRED

extern "C" int luau_typeutils_is_prim(LuauType* t, int prim_kind) {
    if (!t)
        return 0;
    try {
        PrimitiveType::Type pt;
        switch (prim_kind) {
            case LUAU_PRIM_NIL: pt = PrimitiveType::NilType; break;
            case LUAU_PRIM_BOOLEAN: pt = PrimitiveType::Boolean; break;
            case LUAU_PRIM_NUMBER: pt = PrimitiveType::Number; break;
            case LUAU_PRIM_INTEGER: pt = PrimitiveType::Integer; break;
            case LUAU_PRIM_STRING: pt = PrimitiveType::String; break;
            case LUAU_PRIM_THREAD: pt = PrimitiveType::Thread; break;
            case LUAU_PRIM_FUNCTION: pt = PrimitiveType::Function; break;
            case LUAU_PRIM_TABLE: pt = PrimitiveType::Table; break;
            case LUAU_PRIM_BUFFER: pt = PrimitiveType::Buffer; break;
            default: return 0;
        }
        return isPrim(follow(tid(t)), pt) ? 1 : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

extern "C" int luau_typeutils_fast_is_subtype(LuauType* sub, LuauType* super) {
    if (!sub || !super)
        return 0;
    try {
        return fastIsSubtype(follow(tid(sub)), follow(tid(super))) ? 1 : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

// DROPPED: luau_typeutils_occurs_check. Luau's occursCheck(TypeId, TypeId)
// LUAU_ASSERTs (aborts, not throwable) on a concrete/non-free needle, so it is
// only safe to call from within the unifier with live free-type solver state.
// It cannot be driven safely from a finished checker, so the entry point and
// its Zig wrapper are removed rather than left as a crash hazard.

// ---- predicates needing builtinTypes ---------------------------------------

extern "C" int luau_typeutils_is_optional_type(LuauTypes* h, LuauType* t) {
    if (!h || !t)
        return 0;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.builtins)
            return 0;
        return isOptionalType(follow(tid(t)), NotNull{c.builtins}) ? 1 : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

// ---- reducers / manipulators -----------------------------------------------

extern "C" LuauType* luau_typeutils_strip_nil(LuauTypes* h, LuauType* t) {
    if (!h || !t)
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;
        TypeId res = stripNil(NotNull{c.builtins}, *c.arena, follow(tid(t)));
        return luau_types_internal_wrap_type(c.owner, res);
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" LuauTypePack* luau_typeutils_approximate_return(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        std::optional<TypePackId> ret = getApproximateReturnTypeForFunctionCall(follow(tid(t)));
        if (!ret)
            return nullptr;
        return luau_types_internal_wrap_pack(luau_types_internal_owner_of_type(t), *ret);
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- Simplify.h ------------------------------------------------------------

extern "C" LuauType* luau_typeutils_simplify_union(LuauTypes* h, LuauType* a, LuauType* b) {
    if (!h || !a || !b)
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;
        SimplifyResult r = simplifyUnion(NotNull{c.builtins}, NotNull{c.arena}, follow(tid(a)), follow(tid(b)));
        return luau_types_internal_wrap_type(c.owner, r.result);
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" LuauType* luau_typeutils_simplify_intersection(LuauTypes* h, LuauType* a, LuauType* b) {
    if (!h || !a || !b)
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;
        SimplifyResult r = simplifyIntersection(NotNull{c.builtins}, NotNull{c.arena}, follow(tid(a)), follow(tid(b)));
        return luau_types_internal_wrap_type(c.owner, r.result);
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" int luau_typeutils_relate(LuauType* a, LuauType* b) {
    if (!a || !b)
        return LUAU_RELATION_UNKNOWN;
    try {
        switch (relate(follow(tid(a)), follow(tid(b)))) {
            case Relation::Disjoint: return LUAU_RELATION_DISJOINT;
            case Relation::Coincident: return LUAU_RELATION_COINCIDENT;
            case Relation::Intersects: return LUAU_RELATION_INTERSECTS;
            case Relation::Subset: return LUAU_RELATION_SUBSET;
            case Relation::Superset: return LUAU_RELATION_SUPERSET;
        }
        return LUAU_RELATION_UNKNOWN;
    } catch (const std::exception&) {
        return LUAU_RELATION_UNKNOWN;
    }
}

// ---- Clone.h ---------------------------------------------------------------

extern "C" LuauType* luau_typeutils_clone(LuauTypes* h, LuauType* t, int shallow) {
    if (!h || !t)
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;
        CloneState cs{NotNull{c.builtins}};
        TypeId res = shallow ? shallowClone(follow(tid(t)), *c.arena, cs, /*clonePersistentTypes*/ false)
                             : clone(follow(tid(t)), *c.arena, cs);
        return luau_types_internal_wrap_type(c.owner, res);
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- TypePath.h ------------------------------------------------------------
//
// Bound: path construction over named props / index / type & pack fields, plus
// stringification and traversal from a type root.
//
// Skipped path components that require live solver/type-graph state to be
// meaningful: PackSlice / Reduction / GenericPackMapping (these are produced by
// the unifier/normalizer and carry TypePackIds/TypeIds tied to internal state).

struct LuauTypePath {
    TypePath::PathBuilder builder;
};

extern "C" LuauTypePath* luau_typepath_new(void) {
    try {
        return new LuauTypePath();
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" void luau_typepath_free(LuauTypePath* p) {
    delete p;
}

extern "C" void luau_typepath_read_prop(LuauTypePath* p, const char* name) {
    if (p && name)
        try { p->builder.readProp(name); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_write_prop(LuauTypePath* p, const char* name) {
    if (p && name)
        try { p->builder.writeProp(name); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_index(LuauTypePath* p, size_t i) {
    if (p)
        try { p->builder.index(i); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_metatable(LuauTypePath* p) {
    if (p)
        try { p->builder.mt(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_lower_bound(LuauTypePath* p) {
    if (p)
        try { p->builder.lb(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_upper_bound(LuauTypePath* p) {
    if (p)
        try { p->builder.ub(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_index_key(LuauTypePath* p) {
    if (p)
        try { p->builder.indexKey(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_index_value(LuauTypePath* p) {
    if (p)
        try { p->builder.indexValue(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_negated(LuauTypePath* p) {
    if (p)
        try { p->builder.negated(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_variadic(LuauTypePath* p) {
    if (p)
        try { p->builder.variadic(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_args(LuauTypePath* p) {
    if (p)
        try { p->builder.args(); } catch (const std::exception&) {}
}
extern "C" void luau_typepath_rets(LuauTypePath* p) {
    if (p)
        try { p->builder.rets(); } catch (const std::exception&) {}
}

extern "C" char* luau_typepath_tostring(LuauTypePath* p, int human) {
    if (!p)
        return nullptr;
    try {
        // build() consumes the components, so snapshot into a Path first.
        TypePath::Path path{p->builder.components};
        return dupString(human ? toStringHuman(path) : toString(path, /*prefixDot*/ false));
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" LuauType* luau_typepath_traverse_to_type(LuauType* root, LuauTypePath* p) {
    if (!root || !p)
        return nullptr;
    try {
        Ctx c = ctxFromType(root);
        if (!c.ok())
            return nullptr;
        TypePath::Path path{p->builder.components};
        std::optional<TypeId> res = traverseForType(follow(tid(root)), path, NotNull{c.builtins}, NotNull{c.arena});
        if (!res)
            return nullptr;
        return luau_types_internal_wrap_type(c.owner, *res);
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" LuauTypePack* luau_typepath_traverse_to_pack(LuauType* root, LuauTypePath* p) {
    if (!root || !p)
        return nullptr;
    try {
        Ctx c = ctxFromType(root);
        if (!c.ok())
            return nullptr;
        TypePath::Path path{p->builder.components};
        std::optional<TypePackId> res = traverseForPack(follow(tid(root)), path, NotNull{c.builtins}, NotNull{c.arena});
        if (!res)
            return nullptr;
        return luau_types_internal_wrap_pack(c.owner, *res);
    } catch (const std::exception&) {
        return nullptr;
    }
}
