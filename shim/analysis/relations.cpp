// extern "C" shim: subtype queries between inferred Luau types.

#include "relations.h"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/Error.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/Module.h"
#include "Luau/Normalize.h"
#include "Luau/Scope.h"
#include "Luau/Subtyping.h"
#include "Luau/TypeArena.h"
#include "Luau/TypeCheckLimits.h"
#include "Luau/TypeFunctionRuntime.h"
#include "Luau/UnifierSharedState.h"

#include <memory>
#include <optional>
#include <string>

using namespace Luau;

namespace {

// Serves a single in-memory module by name.
struct SingleFileResolver : FileResolver {
    std::string moduleName;
    std::string source;

    std::optional<SourceCode> readSource(const ModuleName& name) override {
        if (name != moduleName)
            return std::nullopt;
        return SourceCode{source, SourceCode::Module};
    }
};

} // namespace

struct LuauRelations {
    SingleFileResolver files;
    NullConfigResolver config;
    std::unique_ptr<Frontend> frontend;
    ModulePtr module; // keep the checked module (and its arenas) alive
    bool ok = false;
};

// Resolve the inferred TypeId of a top-level binding by source name.
static std::optional<TypeId> bindingType(LuauRelations* h, const char* name) {
    if (!h || !name || !h->module || !h->module->hasModuleScope())
        return std::nullopt;
    ScopePtr scope = h->module->getModuleScope();
    if (!scope)
        return std::nullopt;
    std::optional<Binding> binding = scope->linearSearchForBinding(name, /*traverseScopeChain*/ true);
    if (binding && binding->typeId)
        return binding->typeId;
    return std::nullopt;
}

extern "C" LuauRelations* luau_relations_check(const char* src, size_t len) {
    LuauRelations* h = new LuauRelations();
    h->files.moduleName = "main";
    h->files.source.assign(src, len);

    try {
        FrontendOptions options;
        options.retainFullTypeGraphs = true;
        h->frontend = std::make_unique<Frontend>(&h->files, &h->config, options);
        registerBuiltinGlobals(*h->frontend, h->frontend->globals);

        h->frontend->check("main");
        h->module = h->frontend->moduleResolver.getModule("main");
        h->ok = (h->module != nullptr);
    } catch (const std::exception&) {
        h->ok = false;
    }
    return h;
}

extern "C" int luau_relations_is_subtype(LuauRelations* h, const char* nameA, const char* nameB) {
    if (!h || !h->ok || !h->frontend || !h->module)
        return -1;

    try {
        std::optional<TypeId> subTy = bindingType(h, nameA);
        std::optional<TypeId> superTy = bindingType(h, nameB);
        if (!subTy || !superTy)
            return -1;

        // The module owns its own arena; build the subtyping engine against it
        // so any synthesized types share the same arena and stay alive with the
        // module. The module's scope serves as the lexical scope for the query.
        NotNull<TypeArena> arena{&h->module->internalTypes};
        NotNull<BuiltinTypes> builtinTypes = h->frontend->builtinTypes;

        ScopePtr scope = h->module->getModuleScope();
        if (!scope)
            return -1;
        NotNull<Scope> scopeRef{scope.get()};

        InternalErrorReporter iceReporter;
        UnifierSharedState sharedState{&iceReporter};
        TypeCheckLimits limits;

        SolverMode solverMode = h->frontend->globals.mode;

        Normalizer normalizer{arena.get(), builtinTypes, NotNull{&sharedState}, solverMode};
        TypeFunctionRuntime typeFunctionRuntime{NotNull{&iceReporter}, NotNull{&limits}};

        Subtyping subtyping{
            builtinTypes,
            arena,
            NotNull{&normalizer},
            NotNull{&typeFunctionRuntime},
            NotNull{&iceReporter},
        };

        SubtypingResult result = subtyping.isSubtype(follow(*subTy), follow(*superTy), scopeRef);
        return result.isSubtype ? 1 : 0;
    } catch (const std::exception&) {
        return -1;
    }
}

extern "C" void luau_relations_free(LuauRelations* h) {
    delete h;
}
