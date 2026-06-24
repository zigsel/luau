// extern "C" shim over loading host type definitions into a Frontend and
// type-checking a module against the augmented globals (Analysis module).

#include "builtins.h"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/Error.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/GlobalTypes.h"
#include "Luau/ToString.h"

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

} // namespace

struct LuauDefCheck {
    SingleFileResolver files;
    NullConfigResolver config;
    std::vector<std::string> messages;
    std::vector<LuauPosition> positions;
    bool defsOk = false;
};

extern "C" LuauDefCheck* luau_analysis_check_with_defs(
    const char* defs, size_t defs_len, const char* src, size_t src_len) {
    LuauDefCheck* h = new LuauDefCheck();
    h->files.moduleName = "main";
    h->files.source.assign(src, src_len);
    std::string defsSource(defs, defs_len);

    try {
        FrontendOptions options;
        Frontend frontend(&h->files, &h->config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        // Load the host definitions into the shared global scope so that
        // type-checking sees them as declared globals/types.
        LoadDefinitionFileResult loaded = frontend.loadDefinitionFile(
            frontend.globals,
            frontend.globals.globalScope,
            defsSource,
            /*packageName*/ "@host",
            /*captureComments*/ false);

        if (!loaded.success) {
            h->defsOk = false;
            for (const ParseError& e : loaded.parseResult.errors) {
                h->messages.push_back(e.getMessage());
                LuauPosition p;
                p.line = e.getLocation().begin.line;
                p.column = e.getLocation().begin.column;
                h->positions.push_back(p);
            }
            if (loaded.module) {
                for (const TypeError& e : loaded.module->errors) {
                    h->messages.push_back(toString(e));
                    LuauPosition p;
                    p.line = e.location.begin.line;
                    p.column = e.location.begin.column;
                    h->positions.push_back(p);
                }
            }
            if (h->messages.empty()) {
                h->messages.push_back("failed to load definitions");
                h->positions.push_back(LuauPosition{0, 0});
            }
            return h;
        }

        h->defsOk = true;

        CheckResult result = frontend.check("main");
        for (const TypeError& e : result.errors) {
            h->messages.push_back(toString(e));
            LuauPosition p;
            p.line = e.location.begin.line;
            p.column = e.location.begin.column;
            h->positions.push_back(p);
        }
    } catch (const std::exception& e) {
        h->messages.push_back(e.what());
        h->positions.push_back(LuauPosition{0, 0});
    }
    return h;
}

extern "C" int luau_analysis_defcheck_error_count(const LuauDefCheck* h) {
    return static_cast<int>(h->messages.size());
}

extern "C" const char* luau_analysis_defcheck_error_message(const LuauDefCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->messages.size())
        return "";
    return h->messages[i].c_str();
}

extern "C" LuauPosition luau_analysis_defcheck_error_position(const LuauDefCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->positions.size())
        return LuauPosition{0, 0};
    return h->positions[i];
}

extern "C" int luau_analysis_defcheck_defs_ok(const LuauDefCheck* h) {
    return h->defsOk ? 1 : 0;
}

extern "C" void luau_analysis_defcheck_free(LuauDefCheck* h) {
    delete h;
}
