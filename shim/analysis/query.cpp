// extern "C" shim: inferred type at a source position (hover / LSP core).

#include "query.h"

#include "Luau/AstQuery.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/Module.h"
#include "Luau/Scope.h"
#include "Luau/ToString.h"

#include <cstdlib>
#include <cstring>
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

// Duplicate `s` into a malloc'd, NUL-terminated buffer the caller frees.
char* dupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}

} // namespace

extern "C" int luau_analysis_type_at(
    const char* src, size_t len, unsigned int line, unsigned int column, char** out_type) {
    if (out_type)
        *out_type = nullptr;

    try {
        SingleFileResolver files;
        files.moduleName = "main";
        files.source.assign(src, len);
        NullConfigResolver config;

        FrontendOptions options;
        // Keep per-AST-node type bindings so findTypeAtPosition can resolve a
        // type at an expression; without this the graph is discarded post-check.
        options.retainFullTypeGraphs = true;
        Frontend frontend(&files, &config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        frontend.check("main");

        const SourceModule* sourceModule = frontend.getSourceModule("main");
        ModulePtr module = frontend.moduleResolver.getModule("main");
        if (!sourceModule || !module)
            return 0;

        Position pos{line, column};
        std::optional<TypeId> ty = findTypeAtPosition(*module, *sourceModule, pos);

        // `findTypeAtPosition` only resolves expressions. If the position is on
        // a binding's declaration name (e.g. the `x` in `local x = 1`), fall
        // back to the binding's declared/inferred type.
        if (!ty) {
            std::optional<Binding> binding = findBindingAtPosition(*module, *sourceModule, pos);
            if (binding)
                ty = binding->typeId;
        }

        if (!ty) {
            if (out_type) {
                char* empty = dupString("");
                if (!empty)
                    return 0;
                *out_type = empty;
            }
            return 1;
        }

        std::string result = toString(*ty);
        char* dup = dupString(result);
        if (!dup)
            return 0;
        if (out_type)
            *out_type = dup;
        else
            std::free(dup);
        return 1;
    } catch (const std::exception&) {
        if (out_type && *out_type) {
            std::free(*out_type);
            *out_type = nullptr;
        }
        return 0;
    }
}
