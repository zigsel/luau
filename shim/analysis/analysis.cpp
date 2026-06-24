// extern "C" shim over Luau::Frontend type checking (Analysis module).

#include "analysis.h"

#include "Luau/Autocomplete.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/Error.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/ToString.h"

#include <algorithm>
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

struct LuauCheck {
    SingleFileResolver files;
    NullConfigResolver config;
    std::vector<std::string> messages;
    std::vector<LuauPosition> positions;
    std::vector<std::string> lintMessages;
    std::vector<std::string> lintNames;
    std::vector<int> lintCodes;
    std::vector<LuauPosition> lintPositions;
};

extern "C" LuauCheck* luau_analysis_check(const char* src, size_t len) {
    LuauCheck* h = new LuauCheck();
    h->files.moduleName = "main";
    h->files.source.assign(src, len);

    try {
        FrontendOptions options;
        options.runLintChecks = true;
        Frontend frontend(&h->files, &h->config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        CheckResult result = frontend.check("main");
        for (const TypeError& e : result.errors) {
            h->messages.push_back(toString(e));
            LuauPosition p;
            p.line = e.location.begin.line;
            p.column = e.location.begin.column;
            h->positions.push_back(p);
        }
        auto collectLint = [&](const std::vector<LintWarning>& ws) {
            for (const LintWarning& w : ws) {
                h->lintMessages.push_back(w.text);
                h->lintNames.push_back(LintWarning::getName(w.code));
                h->lintCodes.push_back(static_cast<int>(w.code));
                LuauPosition p;
                p.line = w.location.begin.line;
                p.column = w.location.begin.column;
                h->lintPositions.push_back(p);
            }
        };
        collectLint(result.lintResult.errors);
        collectLint(result.lintResult.warnings);
    } catch (const std::exception& e) {
        h->messages.push_back(e.what());
        h->positions.push_back(LuauPosition{0, 0});
    }
    return h;
}

extern "C" int luau_analysis_lint_count(const LuauCheck* h) {
    return static_cast<int>(h->lintMessages.size());
}
extern "C" const char* luau_analysis_lint_message(const LuauCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->lintMessages.size()) return "";
    return h->lintMessages[i].c_str();
}
extern "C" int luau_analysis_lint_code(const LuauCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->lintCodes.size()) return 0;
    return h->lintCodes[i];
}
extern "C" const char* luau_analysis_lint_name(const LuauCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->lintNames.size()) return "";
    return h->lintNames[i].c_str();
}
extern "C" LuauPosition luau_analysis_lint_position(const LuauCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->lintPositions.size()) return LuauPosition{0, 0};
    return h->lintPositions[i];
}

extern "C" int luau_analysis_error_count(const LuauCheck* h) {
    return static_cast<int>(h->messages.size());
}

extern "C" const char* luau_analysis_error_message(const LuauCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->messages.size())
        return "";
    return h->messages[i].c_str();
}

extern "C" LuauPosition luau_analysis_error_position(const LuauCheck* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->positions.size())
        return LuauPosition{0, 0};
    return h->positions[i];
}

extern "C" void luau_analysis_check_free(LuauCheck* h) {
    delete h;
}

// ---- autocomplete ----------------------------------------------------------

struct LuauAutocomplete {
    std::vector<std::string> names;
    std::vector<int> kinds;
};

extern "C" LuauAutocomplete* luau_autocomplete(const char* src, size_t len, unsigned int line, unsigned int column) {
    LuauAutocomplete* h = new LuauAutocomplete();
    try {
        SingleFileResolver files;
        files.moduleName = "main";
        files.source.assign(src, len);
        NullConfigResolver config;

        FrontendOptions options;
        options.forAutocomplete = true;
        Frontend frontend(&files, &config, options);
        registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete, /*forAutocomplete*/ true);
        frontend.check("main");

        Position pos{line, column};
        AutocompleteResult result = autocomplete(
            frontend, "main", pos,
            [](std::string, std::optional<const ExternType*>, std::optional<std::string>) {
                return std::nullopt;
            });

        std::vector<std::pair<std::string, int>> entries;
        for (const auto& kv : result.entryMap)
            entries.emplace_back(kv.first, static_cast<int>(kv.second.kind));
        std::sort(entries.begin(), entries.end());
        for (auto& e : entries) {
            h->names.push_back(e.first);
            h->kinds.push_back(e.second);
        }
    } catch (const std::exception&) {
        // leave empty on failure
    }
    return h;
}

extern "C" int luau_autocomplete_count(const LuauAutocomplete* h) {
    return static_cast<int>(h->names.size());
}
extern "C" const char* luau_autocomplete_name(const LuauAutocomplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->names.size()) return "";
    return h->names[i].c_str();
}
extern "C" int luau_autocomplete_kind(const LuauAutocomplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->kinds.size()) return 0;
    return h->kinds[i];
}
extern "C" void luau_autocomplete_free(LuauAutocomplete* h) {
    delete h;
}
