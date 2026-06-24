// extern "C" shim: structured access to Luau type-check diagnostics.
//
// Type-checks a single self-contained module (same Frontend pattern as
// analysis/builtins.cpp), then for each TypeError records a stable kind enum
// (the TypeErrorData variant index), the location, the rendered message, and —
// for variants with a cheap string/name field that needs no live type printer —
// that field. Variants whose interesting payload is TypeId/TypePackId expose
// only kind + location + message.

#include "diagnostics.h"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/Error.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/Location.h"
#include "Luau/ToString.h"
#include "Luau/Variant.h"

#include <optional>
#include <string>
#include <vector>

using namespace Luau;

namespace {

struct SingleFileResolver : FileResolver {
    std::string moduleName;
    std::string source;

    std::optional<SourceCode> readSource(const ModuleName& name) override {
        if (name != moduleName)
            return std::nullopt;
        return SourceCode{source, SourceCode::Module};
    }
};

struct Diag {
    int kind = LUAU_DIAG_UNKNOWN;
    LuauPosition begin{0, 0};
    LuauPosition end{0, 0};
    std::string message;
    std::string field;
    bool hasField = false;
};

// Pull out the cheap, printer-free string field for variants that have one.
// Returns true and fills `out` when present.
bool cheapField(const TypeError& e, std::string& out) {
    if (const auto* d = get<UnknownSymbol>(e)) { out = d->name; return true; }
    if (const auto* d = get<UnknownProperty>(e)) { out = d->key; return true; }
    if (const auto* d = get<UnknownPropButFoundLikeProp>(e)) { out = d->key; return true; }
    if (const auto* d = get<MissingUnionProperty>(e)) { out = d->key; return true; }
    if (const auto* d = get<CannotExtendTable>(e)) { out = d->prop; return true; }
    if (const auto* d = get<PropertyAccessViolation>(e)) { out = d->key; return true; }
    if (const auto* d = get<DuplicateTypeDefinition>(e)) { out = d->name; return true; }
    if (const auto* d = get<IncorrectGenericParameterCount>(e)) { out = d->name; return true; }
    if (const auto* d = get<SwappedGenericTypeParameter>(e)) { out = d->name; return true; }
    if (const auto* d = get<GenericError>(e)) { out = d->message; return true; }
    if (const auto* d = get<InternalError>(e)) { out = d->message; return true; }
    if (const auto* d = get<SyntaxError>(e)) { out = d->message; return true; }
    if (const auto* d = get<ExtraInformation>(e)) { out = d->message; return true; }
    if (const auto* d = get<UserDefinedTypeFunctionError>(e)) { out = d->message; return true; }
    if (const auto* d = get<UnknownRequire>(e)) { out = d->modulePath; return true; }
    if (const auto* d = get<IllegalRequire>(e)) { out = d->moduleName; return true; }
    if (const auto* d = get<ReservedIdentifier>(e)) { out = d->name; return true; }
    if (const auto* d = get<DuplicateGenericParameter>(e)) { out = d->parameterName; return true; }
    if (const auto* d = get<DeprecatedApiUsed>(e)) { out = d->symbol; return true; }
    if (const auto* d = get<NonStrictFunctionDefinitionError>(e)) { out = d->functionName; return true; }
    if (const auto* d = get<CheckedFunctionIncorrectArgs>(e)) { out = d->functionName; return true; }
    if (const auto* d = get<CheckedFunctionCallError>(e)) { out = d->checkedFunctionName; return true; }
    if (const auto* d = get<CountMismatch>(e)) { out = d->function; return true; }
    return false;
}

} // namespace

struct LuauDiagnostics {
    SingleFileResolver files;
    NullConfigResolver config;
    std::vector<Diag> diags;
};

extern "C" LuauDiagnostics* luau_analysis_diagnostics_check(const char* src, size_t src_len) {
    LuauDiagnostics* h = new LuauDiagnostics();
    h->files.moduleName = "main";
    h->files.source.assign(src, src_len);

    try {
        FrontendOptions options;
        Frontend frontend(&h->files, &h->config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        CheckResult result = frontend.check("main");
        for (const TypeError& e : result.errors) {
            Diag d;
            // The variant index IS the stable kind (enum order mirrors the
            // TypeErrorData alternative order in Error.h).
            d.kind = static_cast<int>(e.data.index());
            d.begin.line = e.location.begin.line;
            d.begin.column = e.location.begin.column;
            d.end.line = e.location.end.line;
            d.end.column = e.location.end.column;
            d.message = toString(e);
            d.hasField = cheapField(e, d.field);
            h->diags.push_back(std::move(d));
        }
    } catch (const std::exception& e) {
        Diag d;
        d.kind = LUAU_DIAG_UNKNOWN;
        d.message = e.what();
        h->diags.push_back(std::move(d));
    }
    return h;
}

extern "C" int luau_analysis_diagnostics_count(const LuauDiagnostics* h) {
    return static_cast<int>(h->diags.size());
}

extern "C" int luau_analysis_diagnostics_kind(const LuauDiagnostics* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->diags.size())
        return LUAU_DIAG_UNKNOWN;
    return h->diags[i].kind;
}

extern "C" LuauPosition luau_analysis_diagnostics_position(const LuauDiagnostics* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->diags.size())
        return LuauPosition{0, 0};
    return h->diags[i].begin;
}

extern "C" LuauPosition luau_analysis_diagnostics_end_position(const LuauDiagnostics* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->diags.size())
        return LuauPosition{0, 0};
    return h->diags[i].end;
}

extern "C" const char* luau_analysis_diagnostics_message(const LuauDiagnostics* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->diags.size())
        return "";
    return h->diags[i].message.c_str();
}

extern "C" const char* luau_analysis_diagnostics_field(const LuauDiagnostics* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->diags.size())
        return "";
    return h->diags[i].field.c_str();
}

extern "C" int luau_analysis_diagnostics_has_field(const LuauDiagnostics* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->diags.size())
        return 0;
    return h->diags[i].hasField ? 1 : 0;
}

extern "C" void luau_analysis_diagnostics_free(LuauDiagnostics* h) {
    delete h;
}
