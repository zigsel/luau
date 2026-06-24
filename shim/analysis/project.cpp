// extern "C" shim over multi-module Luau type checking with require resolution.

#include "project.h"

#include "Luau/Ast.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/Error.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/ToString.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Luau;

namespace {

// Serves a project's in-memory modules by name and resolves require() calls.
struct ProjectFileResolver : FileResolver {
    std::unordered_map<std::string, std::string> modules;

    std::optional<SourceCode> readSource(const ModuleName& name) override {
        auto it = modules.find(name);
        if (it == modules.end())
            return std::nullopt;
        return SourceCode{it->second, SourceCode::Module};
    }

    // Resolve `require(expr)` to a module name. The require argument is expected
    // to be a constant string literal naming another project module.
    std::optional<ModuleInfo> resolveModule(const ModuleInfo* context, AstExpr* expr, const TypeCheckLimits& limits) override {
        if (AstExprConstantString* str = expr->as<AstExprConstantString>())
            return ModuleInfo{std::string(str->value.data, str->value.size)};
        return std::nullopt;
    }
};

} // namespace

struct LuauProject {
    ProjectFileResolver files;
    NullConfigResolver config;
    std::vector<std::string> moduleNames;
    std::vector<std::string> messages;
    std::vector<LuauPosition> positions;
};

extern "C" LuauProject* luau_project_new(void) {
    return new LuauProject();
}

extern "C" void luau_project_add_module(LuauProject* p, const char* name, size_t name_len, const char* src, size_t src_len) {
    if (!p)
        return;
    p->files.modules[std::string(name, name_len)] = std::string(src, src_len);
}

extern "C" void luau_project_check(LuauProject* p, const char* entry_name) {
    if (!p)
        return;
    p->moduleNames.clear();
    p->messages.clear();
    p->positions.clear();

    try {
        FrontendOptions options;
        Frontend frontend(&p->files, &p->config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        CheckResult result = frontend.check(ModuleName(entry_name));
        for (const TypeError& e : result.errors) {
            p->moduleNames.push_back(e.moduleName);
            p->messages.push_back(toString(e));
            LuauPosition pos;
            pos.line = e.location.begin.line;
            pos.column = e.location.begin.column;
            p->positions.push_back(pos);
        }
    } catch (const std::exception& e) {
        p->moduleNames.push_back(entry_name ? entry_name : "");
        p->messages.push_back(e.what());
        p->positions.push_back(LuauPosition{0, 0});
    }
}

extern "C" int luau_project_error_count(const LuauProject* p) {
    return p ? static_cast<int>(p->messages.size()) : 0;
}

extern "C" const char* luau_project_error_module_name(const LuauProject* p, int i) {
    if (!p || i < 0 || static_cast<size_t>(i) >= p->moduleNames.size())
        return "";
    return p->moduleNames[i].c_str();
}

extern "C" const char* luau_project_error_message(const LuauProject* p, int i) {
    if (!p || i < 0 || static_cast<size_t>(i) >= p->messages.size())
        return "";
    return p->messages[i].c_str();
}

extern "C" LuauPosition luau_project_error_position(const LuauProject* p, int i) {
    if (!p || i < 0 || static_cast<size_t>(i) >= p->positions.size())
        return LuauPosition{0, 0};
    return p->positions[i];
}

extern "C" void luau_project_free(LuauProject* p) {
    delete p;
}
