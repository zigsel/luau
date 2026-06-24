// extern "C" shim: richer autocomplete with per-entry type/doc/insert metadata.

#include "complete.h"

#include "Luau/Autocomplete.h"
#include "Luau/AutocompleteTypes.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/ToString.h"
#include "Luau/Type.h"

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

struct LuauComplete {
    std::vector<std::string> names;
    std::vector<int> kinds;
    std::vector<std::string> typeStrings;
    std::vector<std::string> documentationSymbols;
    std::vector<std::string> insertTexts;
    std::vector<int> deprecated;
};

extern "C" LuauComplete* luau_complete(const char* src, size_t len, unsigned int line, unsigned int col) {
    LuauComplete* h = new LuauComplete();
    try {
        SingleFileResolver files;
        files.moduleName = "main";
        files.source.assign(src, len);
        NullConfigResolver config;

        FrontendOptions options;
        options.forAutocomplete = true;
        // Keep full type graphs so we can stringify entry types.
        options.retainFullTypeGraphs = true;
        Frontend frontend(&files, &config, options);
        registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete, /*forAutocomplete*/ true);
        frontend.check("main");

        Position pos{line, col};
        AutocompleteResult result = autocomplete(
            frontend, "main", pos,
            [](std::string, std::optional<const ExternType*>, std::optional<std::string>) {
                return std::nullopt;
            });

        // Sort entries by name for stable iteration order.
        std::vector<const std::pair<const std::string, AutocompleteEntry>*> entries;
        for (const auto& kv : result.entryMap)
            entries.push_back(&kv);
        std::sort(entries.begin(), entries.end(), [](const auto* a, const auto* b) {
            return a->first < b->first;
        });

        for (const auto* kv : entries) {
            const AutocompleteEntry& e = kv->second;
            h->names.push_back(kv->first);
            h->kinds.push_back(static_cast<int>(e.kind));

            std::string typeStr;
            if (e.type)
                typeStr = toString(*e.type);
            h->typeStrings.push_back(std::move(typeStr));

            h->documentationSymbols.push_back(e.documentationSymbol.value_or(std::string{}));
            h->insertTexts.push_back(e.insertText.value_or(std::string{}));
            h->deprecated.push_back(e.deprecated ? 1 : 0);
        }
    } catch (const std::exception&) {
        // leave empty on failure
    }
    return h;
}

extern "C" int luau_complete_count(const LuauComplete* h) {
    return static_cast<int>(h->names.size());
}
extern "C" const char* luau_complete_name(const LuauComplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->names.size()) return "";
    return h->names[i].c_str();
}
extern "C" int luau_complete_kind(const LuauComplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->kinds.size()) return 0;
    return h->kinds[i];
}
extern "C" const char* luau_complete_type_string(const LuauComplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->typeStrings.size()) return "";
    return h->typeStrings[i].c_str();
}
extern "C" const char* luau_complete_documentation_symbol(const LuauComplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->documentationSymbols.size()) return "";
    return h->documentationSymbols[i].c_str();
}
extern "C" const char* luau_complete_insert_text(const LuauComplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->insertTexts.size()) return "";
    return h->insertTexts[i].c_str();
}
extern "C" int luau_complete_deprecated(const LuauComplete* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->deprecated.size()) return 0;
    return h->deprecated[i];
}
extern "C" void luau_complete_free(LuauComplete* h) {
    delete h;
}
