// extern "C" shim: visualization / serialization of inferred Luau types.

#include "viz.h"

#include "Luau/ToDot.h"
#include "Luau/ToString.h"
#include "Luau/TypeFwd.h"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace Luau;

// The `types` shim defines `LuauType`/`LuauTypePack` as single-field wrappers
// around a TypeId/TypePackId in its own translation unit. We re-declare the
// identical layout here so we can read the wrapped id; the structs are POD and
// the layouts must stay in lockstep with shim/analysis/types.cpp.
struct LuauType {
    TypeId id;
};
struct LuauTypePack {
    TypePackId id;
};

namespace {

char* dupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}

ToDotOptions dotOptions(int showPointers, int duplicatePrimitives) {
    ToDotOptions o;
    o.showPointers = showPointers != 0;
    o.duplicatePrimitives = duplicatePrimitives != 0;
    return o;
}

ToStringOptions stringOptions(const LuauToStringOptions* in) {
    ToStringOptions o;
    if (!in)
        return o;
    o.exhaustive = in->exhaustive != 0;
    o.useLineBreaks = in->use_line_breaks != 0;
    o.functionTypeArguments = in->function_type_arguments != 0;
    o.hideTableKind = in->hide_table_kind != 0;
    o.hideNamedFunctionTypeParameters = in->hide_named_function_type_parameters != 0;
    o.hideFunctionSelfArgument = in->hide_function_self_argument != 0;
    o.hideTableAliasExpansions = in->hide_table_alias_expansions != 0;
    o.useQuestionMarks = in->use_question_marks != 0;
    o.ignoreSyntheticName = in->ignore_synthetic_name != 0;
    if (in->max_table_length != 0)
        o.maxTableLength = in->max_table_length;
    if (in->max_type_length != 0)
        o.maxTypeLength = in->max_type_length;
    if (in->composite_types_single_line_limit != 0)
        o.compositeTypesSingleLineLimit = in->composite_types_single_line_limit;
    return o;
}

} // namespace

// ---- Graphviz dot ----------------------------------------------------------

extern "C" char* luau_viz_type_to_dot(LuauType* t, int showPointers, int duplicatePrimitives) {
    if (!t)
        return nullptr;
    try {
        return dupString(toDot(t->id, dotOptions(showPointers, duplicatePrimitives)));
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" char* luau_viz_typepack_to_dot(LuauTypePack* tp, int showPointers, int duplicatePrimitives) {
    if (!tp)
        return nullptr;
    try {
        return dupString(toDot(tp->id, dotOptions(showPointers, duplicatePrimitives)));
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- toString with options -------------------------------------------------

extern "C" char* luau_viz_type_to_string(LuauType* t, const LuauToStringOptions* opts) {
    if (!t)
        return nullptr;
    try {
        ToStringOptions o = stringOptions(opts);
        return dupString(toString(t->id, o));
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" char* luau_viz_typepack_to_string(LuauTypePack* tp, const LuauToStringOptions* opts) {
    if (!tp)
        return nullptr;
    try {
        ToStringOptions o = stringOptions(opts);
        return dupString(toString(tp->id, o));
    } catch (const std::exception&) {
        return nullptr;
    }
}

// ---- toStringDetailed ------------------------------------------------------

extern "C" int luau_viz_type_to_string_detailed(
    LuauType* t,
    const LuauToStringOptions* opts,
    char** out,
    int* out_invalid,
    int* out_error,
    int* out_cycle,
    int* out_truncated) {
    if (out)
        *out = nullptr;
    if (!t || !out)
        return 0;
    try {
        ToStringOptions o = stringOptions(opts);
        ToStringResult r = toStringDetailed(t->id, o);
        char* s = dupString(r.name);
        if (!s)
            return 0;
        *out = s;
        if (out_invalid)
            *out_invalid = r.invalid ? 1 : 0;
        if (out_error)
            *out_error = r.error ? 1 : 0;
        if (out_cycle)
            *out_cycle = r.cycle ? 1 : 0;
        if (out_truncated)
            *out_truncated = r.truncated ? 1 : 0;
        return 1;
    } catch (const std::exception&) {
        return 0;
    }
}
