// extern "C" shim: type normalization + structural equality of inferred Luau
// types.
//
// Reuses the LuauTypes/LuauType handle model from types.cpp via the internal
// bridge (types_internal.h). The normalization context (TypeArena +
// builtinTypes + UnifierSharedState + SolverMode) is assembled from a finished
// checker exactly as relations.cpp does for subtyping; the solver mode is read
// off the checked Module (`checkedInNewSolver`). All throwing calls are wrapped
// in try/catch.
//
// See normalize.h for the rationale on what is intentionally NOT bound
// (Substitution.h and the solver-internal Normalizer mutators).

#include "normalize.h"
#include "types_internal.h" // handle bridge into types.cpp

#include "Luau/Module.h"
#include "Luau/Normalize.h"
#include "Luau/StructuralTypeEquality.h"
#include "Luau/ToString.h"
#include "Luau/Type.h"
#include "Luau/TypeArena.h"
#include "Luau/UnifierSharedState.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <set>
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

// All the pieces a Normalizer needs, sourced from a finished checker. The
// reporter/sharedState are heap-owned so a caller can keep the Normalizer alive
// across helper calls without dangling references.
struct NormCtx {
    LuauTypes* owner = nullptr;
    TypeArena* arena = nullptr;
    BuiltinTypes* builtins = nullptr;
    SolverMode solverMode = SolverMode::New;

    std::unique_ptr<InternalErrorReporter> ice;
    std::unique_ptr<UnifierSharedState> shared;
    std::unique_ptr<Normalizer> normalizer;

    bool ok() const {
        return owner && arena && builtins && normalizer;
    }
};

NormCtx makeCtx(LuauTypes* owner) {
    NormCtx c;
    c.owner = owner;
    c.arena = luau_types_internal_arena(owner);
    c.builtins = luau_types_internal_builtins(owner);
    if (!c.owner || !c.arena || !c.builtins)
        return c;

    // Solver mode is recorded on the checked module; default to New otherwise.
    if (Module* m = luau_types_internal_module(owner))
        c.solverMode = m->checkedInNewSolver ? SolverMode::New : SolverMode::Old;

    c.ice = std::make_unique<InternalErrorReporter>();
    c.shared = std::make_unique<UnifierSharedState>(c.ice.get());
    c.normalizer = std::make_unique<Normalizer>(c.arena, NotNull{c.builtins}, NotNull{c.shared.get()}, c.solverMode);
    return c;
}

TypeId tid(LuauType* h) {
    return luau_types_internal_typeid(h);
}

} // namespace

// ---- normalization ---------------------------------------------------------

extern "C" LuauType* luau_normalize_type(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        LuauTypes* owner = luau_types_internal_owner_of_type(t);
        NormCtx ctx = makeCtx(owner);
        if (!ctx.ok())
            return nullptr;

        std::shared_ptr<const NormalizedType> norm = ctx.normalizer->normalize(follow(tid(t)));
        if (!norm)
            return nullptr; // too complex / resource limited

        // Convert the normal form back into a concrete TypeId in the checker's
        // arena so it outlives this call along with its owner.
        TypeId result = ctx.normalizer->typeFromNormal(*norm);
        return luau_types_internal_wrap_type(owner, result);
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" char* luau_normalize_tostring(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        LuauTypes* owner = luau_types_internal_owner_of_type(t);
        NormCtx ctx = makeCtx(owner);
        if (!ctx.ok())
            return nullptr;

        std::shared_ptr<const NormalizedType> norm = ctx.normalizer->normalize(follow(tid(t)));
        if (!norm)
            return nullptr;

        TypeId result = ctx.normalizer->typeFromNormal(*norm);
        return dupString(toString(result));
    } catch (const std::exception&) {
        return nullptr;
    }
}

// DROPPED: luau_normalize_union / luau_normalize_intersection.
// Normalizer::unionType / intersectionType are PRIVATE members of
// Luau::Normalizer (Analysis/include/Luau/Normalize.h) and are not callable
// from the shim, so these entry points cannot be bound.

// ---- structural equality ----------------------------------------------------

extern "C" int luau_type_structurally_equal(LuauType* a, LuauType* b) {
    if (!a || !b)
        return -1;
    try {
        SeenSet seen;
        // Use the `const Type&` overload: the `TypeId` overload is declared in
        // StructuralTypeEquality.h but not defined in this Luau version.
        return areEqual(seen, *follow(tid(a)), *follow(tid(b))) ? 1 : 0;
    } catch (const std::exception&) {
        return -1;
    }
}
