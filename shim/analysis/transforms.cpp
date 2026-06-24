// extern "C" shim: deep type TRANSFORMS over an inferred Luau type graph.
//
// Reuses the LuauTypes/LuauType handle model from types.cpp via the internal
// bridge (types_internal.h). Every transform sources its context (arena +
// builtinTypes + a module Scope + a fresh UnifierSharedState/TxnLog) from the
// owning checker the same way relations.cpp builds its Subtyping engine, so any
// synthesized TypeIds are minted into the module arena and wrapped back into the
// SAME checker. See transforms.h for which transforms are deliberately skipped
// (those that need live solver state and would assert/crash or corrupt state).

#include "transforms.h"
#include "types_internal.h" // handle bridge into types.cpp

#include "Luau/Anyification.h"
#include "Luau/DenseHash.h"
#include "Luau/ApplyTypeFunction.h"
#include "Luau/Error.h" // InternalErrorReporter
#include "Luau/Generalization.h"
#include "Luau/Instantiation.h"
#include "Luau/Scope.h"
#include "Luau/Type.h"
#include "Luau/TypeArena.h"
#include "Luau/TypeCheckLimits.h"

#include <exception>
#include <optional>

using namespace Luau;

namespace {

TypeId tid(LuauType* h) {
    return luau_types_internal_typeid(h);
}

// The context every transform needs, sourced from a finished checker handle.
struct Ctx {
    LuauTypes* owner = nullptr;
    TypeArena* arena = nullptr;
    BuiltinTypes* builtins = nullptr;
    Scope* scope = nullptr;
    bool ok() const {
        return owner && arena && builtins && scope;
    }
};

Ctx ctxFromChecker(LuauTypes* h) {
    Ctx c;
    c.owner = h;
    c.arena = luau_types_internal_arena(h);
    c.builtins = luau_types_internal_builtins(h);
    c.scope = luau_types_internal_scope(h);
    return c;
}

} // namespace

// ---- instantiate -----------------------------------------------------------

extern "C" LuauType* luau_transforms_instantiate(LuauTypes* h, LuauType* t) {
    if (!h || !t)
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;

        InternalErrorReporter iceReporter;
        TypeCheckLimits limits;

        std::optional<TypeId> res = instantiate(
            NotNull{c.builtins},
            NotNull{c.arena},
            NotNull{&limits},
            NotNull{c.scope},
            follow(tid(t))
        );
        if (!res)
            return nullptr;
        return luau_types_internal_wrap_type(c.owner, *res);
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- anyify ----------------------------------------------------------------

extern "C" LuauType* luau_transforms_anyify(LuauTypes* h, LuauType* t) {
    if (!h || !t)
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;

        InternalErrorReporter iceReporter;

        // Anyification is a Substitution constructed with TxnLog::empty(); it is
        // safe to drive without live solver state.
        Anyification anyify{
            c.arena,
            NotNull<Scope>{c.scope},
            NotNull{c.builtins},
            &iceReporter,
            c.builtins->anyType,
            c.builtins->anyTypePack,
        };

        std::optional<TypeId> res = anyify.substitute(follow(tid(t)));
        if (!res || anyify.normalizationTooComplex)
            return nullptr;
        return luau_types_internal_wrap_type(c.owner, *res);
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- applyTypeFunction -----------------------------------------------------

extern "C" LuauType*
luau_transforms_apply_type_function(LuauTypes* h, LuauType* fn, LuauType* const* args, int arg_count) {
    if (!h || !fn || arg_count < 0 || (arg_count > 0 && !args))
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;

        const FunctionType* ftv = get<FunctionType>(follow(tid(fn)));
        if (!ftv)
            return nullptr;

        // Only ordinary type generics are substituted here; a mismatch in count
        // is treated as "not applicable" rather than a partial application.
        if (ftv->generics.size() != static_cast<size_t>(arg_count))
            return nullptr;

        ApplyTypeFunction atf{c.arena}; // uses TxnLog::empty() internally
        for (int i = 0; i < arg_count; ++i) {
            if (!args[i])
                return nullptr;
            atf.typeArguments[ftv->generics[i]] = follow(tid(args[i]));
        }

        std::optional<TypeId> res = atf.substitute(follow(tid(fn)));
        if (!res || atf.encounteredForwardedType)
            return nullptr;
        return luau_types_internal_wrap_type(c.owner, *res);
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- generalize ------------------------------------------------------------

extern "C" LuauType* luau_transforms_generalize(LuauTypes* h, LuauType* t) {
    if (!h || !t)
        return nullptr;
    try {
        Ctx c = ctxFromChecker(h);
        if (!c.ok())
            return nullptr;

        // generalize() reports resource-limit failure by returning nullopt rather
        // than asserting, and is a no-op on a graph with no remaining free types.
        DenseHashSet<TypeId> cachedTypes{nullptr};

        std::optional<TypeId> res = generalize(
            NotNull{c.arena},
            NotNull{c.builtins},
            NotNull<Scope>{c.scope},
            NotNull{&cachedTypes},
            follow(tid(t))
        );
        if (!res)
            return nullptr;
        return luau_types_internal_wrap_type(c.owner, *res);
    } catch (const std::exception&) {
        return nullptr;
    }
}
