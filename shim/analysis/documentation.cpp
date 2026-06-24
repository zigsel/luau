// extern "C" shim over Luau's documentation data model (Analysis module).
//
// See documentation.h for why this is an in-memory builder + lookup rather than
// a parser: the public Luau API ships the structs but no loader.

#include "documentation.h"

#include "Luau/Documentation.h"

#include <string>

using namespace Luau;

struct LuauDocs {
    DocumentationDatabase db{DocumentationSymbol{"@@nil"}};

    const BasicDocumentation* lookup(const char* symbol) const {
        if (!symbol)
            return nullptr;
        // DenseHashMap::find is non-const but does not mutate; safe to cast.
        const Documentation* doc = const_cast<DocumentationDatabase&>(db).find(DocumentationSymbol(symbol));
        if (!doc)
            return nullptr;
        return get_if<BasicDocumentation>(doc);
    }
};

extern "C" LuauDocs* luau_docs_new(void) {
    try {
        return new LuauDocs();
    } catch (...) {
        return nullptr;
    }
}

extern "C" int luau_docs_add_basic(
    LuauDocs* d,
    const char* symbol,
    const char* documentation,
    const char* learnMoreLink,
    const char* codeSample
) {
    if (!d || !symbol)
        return 0;
    try {
        BasicDocumentation basic;
        basic.documentation = documentation ? documentation : "";
        basic.learnMoreLink = learnMoreLink ? learnMoreLink : "";
        basic.codeSample = codeSample ? codeSample : "";
        d->db[DocumentationSymbol(symbol)] = Documentation{basic};
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_docs_count(const LuauDocs* d) {
    return d ? static_cast<int>(d->db.size()) : 0;
}

extern "C" int luau_docs_has(const LuauDocs* d, const char* symbol) {
    return d && d->lookup(symbol) ? 1 : 0;
}

extern "C" const char* luau_docs_text(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    const BasicDocumentation* b = d->lookup(symbol);
    return b ? b->documentation.c_str() : nullptr;
}

extern "C" const char* luau_docs_learn_more(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    const BasicDocumentation* b = d->lookup(symbol);
    return b ? b->learnMoreLink.c_str() : nullptr;
}

extern "C" const char* luau_docs_code_sample(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    const BasicDocumentation* b = d->lookup(symbol);
    return b ? b->codeSample.c_str() : nullptr;
}

extern "C" void luau_docs_free(LuauDocs* d) {
    delete d;
}
