// extern "C" shim: go-to-definition and top-level symbol locations.

#include "locate.h"

#include "Luau/Ast.h"
#include "Luau/AstQuery.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/Location.h"
#include "Luau/Module.h"
#include "Luau/Scope.h"

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

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

char* dupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}

LuauPosition toPos(const Position& p) {
    return LuauPosition{p.line, p.column};
}

} // namespace

struct LuauSymbols {
    struct Entry {
        std::string name;
        Location location;
    };
    std::vector<Entry> entries;
};

extern "C" int luau_analysis_definition(const char* src, size_t len, unsigned int line,
    unsigned int column, LuauPosition* out_begin, LuauPosition* out_end) {
    try {
        SingleFileResolver files;
        files.moduleName = "main";
        files.source.assign(src, len);
        NullConfigResolver config;

        FrontendOptions options;
        options.retainFullTypeGraphs = true;
        Frontend frontend(&files, &config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        frontend.check("main");

        const SourceModule* sourceModule = frontend.getSourceModule("main");
        ModulePtr module = frontend.moduleResolver.getModule("main");
        if (!sourceModule || !module)
            return 0;

        Position pos{line, column};

        std::optional<Location> defLoc;

        ExprOrLocal exprOrLocal = findExprOrLocalAtPosition(*sourceModule, pos);
        if (AstLocal* local = exprOrLocal.getLocal()) {
            // The declaration name of the local itself.
            defLoc = local->location;
        } else if (AstExpr* expr = exprOrLocal.getExpr()) {
            if (AstExprLocal* exprLocal = expr->as<AstExprLocal>()) {
                defLoc = exprLocal->local->location;
            } else if (AstExprGlobal* exprGlobal = expr->as<AstExprGlobal>()) {
                // Resolve the global through the scope at this position.
                ScopePtr scope = findScopeAtPosition(*module, pos);
                while (scope) {
                    auto it = scope->bindings.find(exprGlobal->name);
                    if (it != scope->bindings.end()) {
                        defLoc = it->second.location;
                        break;
                    }
                    scope = scope->parent;
                }
            }
        }

        // Fall back to a binding lookup if the symbol is on a declaration name.
        if (!defLoc) {
            std::optional<Binding> binding = findBindingAtPosition(*module, *sourceModule, pos);
            if (binding)
                defLoc = binding->location;
        }

        if (!defLoc)
            return 0;

        if (out_begin)
            *out_begin = toPos(defLoc->begin);
        if (out_end)
            *out_end = toPos(defLoc->end);
        return 1;
    } catch (const std::exception&) {
        return 0;
    }
}

extern "C" LuauSymbols* luau_analysis_symbols(const char* src, size_t len) {
    try {
        SingleFileResolver files;
        files.moduleName = "main";
        files.source.assign(src, len);
        NullConfigResolver config;

        FrontendOptions options;
        Frontend frontend(&files, &config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        frontend.check("main");

        const SourceModule* sourceModule = frontend.getSourceModule("main");
        if (!sourceModule || !sourceModule->root)
            return nullptr;

        auto symbols = new LuauSymbols();

        for (AstStat* stat : sourceModule->root->body) {
            if (AstStatLocal* sl = stat->as<AstStatLocal>()) {
                for (AstLocal* var : sl->vars)
                    symbols->entries.push_back({std::string(var->name.value), var->location});
            } else if (AstStatLocalFunction* slf = stat->as<AstStatLocalFunction>()) {
                symbols->entries.push_back(
                    {std::string(slf->name->name.value), slf->name->location});
            } else if (AstStatFunction* sf = stat->as<AstStatFunction>()) {
                if (AstName name = getIdentifier(sf->name); name.value)
                    symbols->entries.push_back({std::string(name.value), sf->name->location});
            } else if (AstStatTypeAlias* sta = stat->as<AstStatTypeAlias>()) {
                symbols->entries.push_back(
                    {std::string(sta->name.value), sta->nameLocation});
            }
        }

        return symbols;
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" size_t luau_analysis_symbols_count(const LuauSymbols* symbols) {
    if (!symbols)
        return 0;
    return symbols->entries.size();
}

extern "C" char* luau_analysis_symbols_name(const LuauSymbols* symbols, size_t i) {
    if (!symbols || i >= symbols->entries.size())
        return nullptr;
    return dupString(symbols->entries[i].name);
}

extern "C" int luau_analysis_symbols_begin(const LuauSymbols* symbols, size_t i, LuauPosition* out) {
    if (!symbols || i >= symbols->entries.size())
        return 0;
    if (out)
        *out = toPos(symbols->entries[i].location.begin);
    return 1;
}

extern "C" int luau_analysis_symbols_end(const LuauSymbols* symbols, size_t i, LuauPosition* out) {
    if (!symbols || i >= symbols->entries.size())
        return 0;
    if (out)
        *out = toPos(symbols->entries[i].location.end);
    return 1;
}

extern "C" void luau_analysis_symbols_free(LuauSymbols* symbols) {
    delete symbols;
}
