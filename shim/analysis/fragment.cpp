// extern "C" shim: fragment (incremental) autocomplete with documentation symbols.

#include "fragment.h"

#include "Luau/AutocompleteTypes.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/FileResolver.h"
#include "Luau/FragmentAutocomplete.h"
#include "Luau/Frontend.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace Luau;

namespace {

// Serves a single in-memory module by name. The fragment API re-reads the source
// from the resolver while parsing the fragment, so we hand it the *new* source.
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

struct LuauFragment {
    int ok = 0;
    std::vector<std::string> names;
    std::vector<int> kinds;
    std::vector<std::string> documentationSymbols;
    std::vector<int> deprecated;
};

extern "C" LuauFragment* luau_fragment_autocomplete(
    const char* staleSrc, size_t staleLen,
    const char* newSrc, size_t newLen,
    unsigned int line, unsigned int col) {
    LuauFragment* h = new LuauFragment();
    try {
        SingleFileResolver files;
        files.moduleName = "main";
        // Check the *stale* source first so the Frontend holds a (possibly
        // outdated) type-checked module to reuse.
        files.source.assign(staleSrc, staleLen);
        NullConfigResolver config;

        FrontendOptions options;
        options.forAutocomplete = true;
        // The fragment path squashes/clones scopes from the stale module, so the
        // full type graph must be retained.
        options.retainFullTypeGraphs = true;
        Frontend frontend(&files, &config, options);
        registerBuiltinGlobals(frontend, frontend.globalsForAutocomplete, /*forAutocomplete*/ true);
        frontend.check("main");

        // Swap in the new (edited) source; fragmentAutocomplete diffs it against
        // the stale parse to isolate and re-analyse only the changed fragment.
        files.source.assign(newSrc, newLen);

        Position pos{line, col};
        FragmentAutocompleteResult result = fragmentAutocomplete(
            frontend,
            std::string_view(files.source),
            "main",
            pos,
            std::nullopt,
            [](std::string, std::optional<const ExternType*>, std::optional<std::string>) {
                return std::nullopt;
            });
        h->ok = 1;

        // Sort entries by name for stable iteration order.
        std::vector<const std::pair<const std::string, AutocompleteEntry>*> entries;
        for (const auto& kv : result.acResults.entryMap)
            entries.push_back(&kv);
        std::sort(entries.begin(), entries.end(), [](const auto* a, const auto* b) {
            return a->first < b->first;
        });

        for (const auto* kv : entries) {
            const AutocompleteEntry& e = kv->second;
            h->names.push_back(kv->first);
            h->kinds.push_back(static_cast<int>(e.kind));
            h->documentationSymbols.push_back(e.documentationSymbol.value_or(std::string{}));
            h->deprecated.push_back(e.deprecated ? 1 : 0);
        }
    } catch (const std::exception&) {
        // leave empty / ok=0 on failure
    }
    return h;
}

extern "C" int luau_fragment_ok(const LuauFragment* h) {
    return h->ok;
}
extern "C" int luau_fragment_count(const LuauFragment* h) {
    return static_cast<int>(h->names.size());
}
extern "C" const char* luau_fragment_name(const LuauFragment* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->names.size()) return "";
    return h->names[i].c_str();
}
extern "C" int luau_fragment_kind(const LuauFragment* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->kinds.size()) return 0;
    return h->kinds[i];
}
extern "C" const char* luau_fragment_documentation_symbol(const LuauFragment* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->documentationSymbols.size()) return "";
    return h->documentationSymbols[i].c_str();
}
extern "C" int luau_fragment_deprecated(const LuauFragment* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->deprecated.size()) return 0;
    return h->deprecated[i];
}
extern "C" void luau_fragment_free(LuauFragment* h) {
    delete h;
}
