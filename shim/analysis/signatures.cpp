// extern "C" shim over Luau's richer documentation variants (Analysis module):
// FunctionDocumentation, FunctionParameterDocumentation, TableDocumentation and
// OverloadedFunctionDocumentation. See signatures.h for why this is an in-memory
// builder + reader rather than a parser (Luau ships the structs, no loader).
//
// NOTE: TableDocumentation and OverloadedFunctionDocumentation embed a
// DenseHashMap, which has no default constructor — it needs a sentinel "empty"
// key. We construct those variants explicitly with a reserved sentinel string
// that user keys must not equal; the sentinel is namespaced to make collision
// implausible. Reads borrow the database's storage (valid until next mutation).

#include "signatures.h"

#include "Luau/Documentation.h"

#include <string>

using namespace Luau;

namespace {

// Reserved DenseHashMap sentinel; real overload signatures / table keys must
// differ from this. Empty string would otherwise be the default sentinel and we
// keep it explicit so callers can store "" as a key if they ever needed to.
const std::string kEmptyKey = "@@luau_signatures_empty@@";

} // namespace

struct LuauDocs {
    DocumentationDatabase db{DocumentationSymbol{"@@nil"}};

    Documentation* raw(const char* symbol) {
        if (!symbol)
            return nullptr;
        return db.find(DocumentationSymbol(symbol));
    }
    FunctionDocumentation* fn(const char* symbol) {
        Documentation* doc = raw(symbol);
        return doc ? get_if<FunctionDocumentation>(doc) : nullptr;
    }
    TableDocumentation* tbl(const char* symbol) {
        Documentation* doc = raw(symbol);
        return doc ? get_if<TableDocumentation>(doc) : nullptr;
    }
    OverloadedFunctionDocumentation* ovl(const char* symbol) {
        Documentation* doc = raw(symbol);
        return doc ? get_if<OverloadedFunctionDocumentation>(doc) : nullptr;
    }
};

extern "C" LuauDocs* luau_signatures_new(void) {
    try {
        return new LuauDocs();
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_signatures_free(LuauDocs* d) {
    delete d;
}

extern "C" int luau_signatures_count(const LuauDocs* d) {
    return d ? static_cast<int>(d->db.size()) : 0;
}

// ----- FunctionDocumentation ---------------------------------------------

extern "C" int luau_signatures_add_function(
    LuauDocs* d,
    const char* symbol,
    const char* documentation,
    const char* learnMoreLink,
    const char* codeSample
) {
    if (!d || !symbol)
        return 0;
    try {
        FunctionDocumentation fn;
        fn.documentation = documentation ? documentation : "";
        fn.learnMoreLink = learnMoreLink ? learnMoreLink : "";
        fn.codeSample = codeSample ? codeSample : "";
        d->db[DocumentationSymbol(symbol)] = Documentation{fn};
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_signatures_function_add_parameter(
    LuauDocs* d,
    const char* symbol,
    const char* paramName,
    const char* paramDocSymbol
) {
    if (!d)
        return 0;
    try {
        FunctionDocumentation* fn = d->fn(symbol);
        if (!fn)
            return 0;
        FunctionParameterDocumentation p;
        p.name = paramName ? paramName : "";
        p.documentation = paramDocSymbol ? paramDocSymbol : "";
        fn->parameters.push_back(p);
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_signatures_function_add_return(LuauDocs* d, const char* symbol, const char* returnDocSymbol) {
    if (!d)
        return 0;
    try {
        FunctionDocumentation* fn = d->fn(symbol);
        if (!fn)
            return 0;
        fn->returns.push_back(returnDocSymbol ? returnDocSymbol : "");
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_signatures_is_function(const LuauDocs* d, const char* symbol) {
    return d && const_cast<LuauDocs*>(d)->fn(symbol) ? 1 : 0;
}

extern "C" const char* luau_signatures_function_documentation(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    return fn ? fn->documentation.c_str() : nullptr;
}

extern "C" const char* luau_signatures_function_learn_more(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    return fn ? fn->learnMoreLink.c_str() : nullptr;
}

extern "C" const char* luau_signatures_function_code_sample(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    return fn ? fn->codeSample.c_str() : nullptr;
}

extern "C" int luau_signatures_function_parameter_count(const LuauDocs* d, const char* symbol) {
    if (!d) return 0;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    return fn ? static_cast<int>(fn->parameters.size()) : 0;
}

extern "C" int luau_signatures_function_return_count(const LuauDocs* d, const char* symbol) {
    if (!d) return 0;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    return fn ? static_cast<int>(fn->returns.size()) : 0;
}

extern "C" const char* luau_signatures_function_parameter_name(const LuauDocs* d, const char* symbol, int i) {
    if (!d) return nullptr;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    if (!fn || i < 0 || static_cast<size_t>(i) >= fn->parameters.size())
        return nullptr;
    return fn->parameters[i].name.c_str();
}

extern "C" const char* luau_signatures_function_parameter_doc(const LuauDocs* d, const char* symbol, int i) {
    if (!d) return nullptr;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    if (!fn || i < 0 || static_cast<size_t>(i) >= fn->parameters.size())
        return nullptr;
    return fn->parameters[i].documentation.c_str();
}

extern "C" const char* luau_signatures_function_return(const LuauDocs* d, const char* symbol, int i) {
    if (!d) return nullptr;
    FunctionDocumentation* fn = const_cast<LuauDocs*>(d)->fn(symbol);
    if (!fn || i < 0 || static_cast<size_t>(i) >= fn->returns.size())
        return nullptr;
    return fn->returns[i].c_str();
}

// ----- TableDocumentation -------------------------------------------------

extern "C" int luau_signatures_add_table(
    LuauDocs* d,
    const char* symbol,
    const char* documentation,
    const char* learnMoreLink,
    const char* codeSample
) {
    if (!d || !symbol)
        return 0;
    try {
        TableDocumentation tbl{
            documentation ? documentation : "",
            DenseHashMap<std::string, DocumentationSymbol>(kEmptyKey),
            learnMoreLink ? learnMoreLink : "",
            codeSample ? codeSample : "",
        };
        d->db[DocumentationSymbol(symbol)] = Documentation{tbl};
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_signatures_table_add_key(LuauDocs* d, const char* symbol, const char* key, const char* keyDocSymbol) {
    if (!d || !key || key == kEmptyKey)
        return 0;
    try {
        TableDocumentation* tbl = d->tbl(symbol);
        if (!tbl)
            return 0;
        tbl->keys[std::string(key)] = keyDocSymbol ? keyDocSymbol : "";
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_signatures_is_table(const LuauDocs* d, const char* symbol) {
    return d && const_cast<LuauDocs*>(d)->tbl(symbol) ? 1 : 0;
}

extern "C" const char* luau_signatures_table_documentation(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    TableDocumentation* tbl = const_cast<LuauDocs*>(d)->tbl(symbol);
    return tbl ? tbl->documentation.c_str() : nullptr;
}

extern "C" const char* luau_signatures_table_learn_more(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    TableDocumentation* tbl = const_cast<LuauDocs*>(d)->tbl(symbol);
    return tbl ? tbl->learnMoreLink.c_str() : nullptr;
}

extern "C" const char* luau_signatures_table_code_sample(const LuauDocs* d, const char* symbol) {
    if (!d) return nullptr;
    TableDocumentation* tbl = const_cast<LuauDocs*>(d)->tbl(symbol);
    return tbl ? tbl->codeSample.c_str() : nullptr;
}

extern "C" const char* luau_signatures_table_key_doc(const LuauDocs* d, const char* symbol, const char* key) {
    if (!d || !key) return nullptr;
    TableDocumentation* tbl = const_cast<LuauDocs*>(d)->tbl(symbol);
    if (!tbl) return nullptr;
    const DocumentationSymbol* v = tbl->keys.find(std::string(key));
    return v ? v->c_str() : nullptr;
}

// ----- OverloadedFunctionDocumentation -----------------------------------

extern "C" int luau_signatures_add_overloaded(LuauDocs* d, const char* symbol) {
    if (!d || !symbol)
        return 0;
    try {
        OverloadedFunctionDocumentation ovl{
            DenseHashMap<std::string, DocumentationSymbol>(kEmptyKey),
        };
        d->db[DocumentationSymbol(symbol)] = Documentation{ovl};
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_signatures_overloaded_add(LuauDocs* d, const char* symbol, const char* signature, const char* overloadDocSymbol) {
    if (!d || !signature || signature == kEmptyKey)
        return 0;
    try {
        OverloadedFunctionDocumentation* ovl = d->ovl(symbol);
        if (!ovl)
            return 0;
        ovl->overloads[std::string(signature)] = overloadDocSymbol ? overloadDocSymbol : "";
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int luau_signatures_is_overloaded(const LuauDocs* d, const char* symbol) {
    return d && const_cast<LuauDocs*>(d)->ovl(symbol) ? 1 : 0;
}

extern "C" const char* luau_signatures_overloaded_doc(const LuauDocs* d, const char* symbol, const char* signature) {
    if (!d || !signature) return nullptr;
    OverloadedFunctionDocumentation* ovl = const_cast<LuauDocs*>(d)->ovl(symbol);
    if (!ovl) return nullptr;
    const DocumentationSymbol* v = ovl->overloads.find(std::string(signature));
    return v ? v->c_str() : nullptr;
}
