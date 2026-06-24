// Shim: the richer Luau DOCUMENTATION variants for signature help / hover
// (Analysis/Documentation.h).
//
// The companion `documentation.*` shim surfaces only the BasicDocumentation
// variant (flat hover text for a symbol). This shim surfaces the THREE richer
// variants that drive signature help and structured hover:
//
//   FunctionDocumentation     — a callable: doc text, an ordered list of
//                               FunctionParameterDocumentation (param name +
//                               that param's own documentation symbol), and a
//                               list of return documentation symbols.
//   OverloadedFunctionDocumentation
//                             — a map from a function-signature string to the
//                               documentation symbol of that overload.
//   TableDocumentation        — a table/class: doc text plus a map from key
//                               name to that key's documentation symbol.
//
// As with `documentation.*`, Luau ships these structs but NO loader/parser; an
// editor builds the DocumentationDatabase itself and indexes it by the
// `documentationSymbol` an autocomplete entry carries. This shim is therefore an
// in-memory builder + reader over one shared DocumentationDatabase: populate it
// with function/table/overload entries, then read the param names, per-parameter
// doc symbols, returns, overload signatures and table keys back out — exactly
// what signature help needs. Pure data; no solver, no Frontend.
//
// Symbols are keyed in the SAME database namespace as `documentation.*`'s basic
// entries (both are just `DocumentationSymbol`/`Documentation` variants), so a
// `FunctionParameterDocumentation.documentation` symbol can point at a Basic
// entry stored via the documentation shim. The two shims own separate handles,
// however; this one is self-contained for signature data.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// An in-memory signature/documentation database. Free with
// `luau_signatures_free`. Returns NULL on allocation failure.
LuauDocs* luau_signatures_new(void);
void luau_signatures_free(LuauDocs* d);

// Total number of entries (across all variants) in the database.
int luau_signatures_count(const LuauDocs* d);

// ----- FunctionDocumentation ---------------------------------------------

// Insert (or overwrite) a FunctionDocumentation entry keyed by `symbol`, with
// the given doc text / learn-more link / code sample (any may be NULL). The
// entry starts with no parameters and no returns; add them with the calls
// below. Returns 1 on success, 0 on failure.
int luau_signatures_add_function(
    LuauDocs* d,
    const char* symbol,
    const char* documentation,
    const char* learnMoreLink,
    const char* codeSample
);

// Append a parameter (name + that parameter's documentation symbol) to the
// FunctionDocumentation entry `symbol`. `paramDocSymbol` may be NULL/"".
// Returns 1 on success, 0 if `symbol` is absent or not a function entry.
int luau_signatures_function_add_parameter(
    LuauDocs* d,
    const char* symbol,
    const char* paramName,
    const char* paramDocSymbol
);

// Append a return documentation symbol to the FunctionDocumentation `symbol`.
// Returns 1 on success, 0 if absent / not a function entry.
int luau_signatures_function_add_return(LuauDocs* d, const char* symbol, const char* returnDocSymbol);

// 1 if `symbol` is a FunctionDocumentation entry, else 0.
int luau_signatures_is_function(const LuauDocs* d, const char* symbol);

// Borrowed doc text / learn-more / code sample of the function `symbol`, or NULL
// if absent / not a function entry. Valid until the next mutation or free.
const char* luau_signatures_function_documentation(const LuauDocs* d, const char* symbol);
const char* luau_signatures_function_learn_more(const LuauDocs* d, const char* symbol);
const char* luau_signatures_function_code_sample(const LuauDocs* d, const char* symbol);

// Number of parameters / returns recorded for the function `symbol` (0 if
// absent or not a function entry).
int luau_signatures_function_parameter_count(const LuauDocs* d, const char* symbol);
int luau_signatures_function_return_count(const LuauDocs* d, const char* symbol);

// Borrowed name / doc symbol of parameter `i` of the function `symbol`, or NULL
// if out of range.
const char* luau_signatures_function_parameter_name(const LuauDocs* d, const char* symbol, int i);
const char* luau_signatures_function_parameter_doc(const LuauDocs* d, const char* symbol, int i);

// Borrowed doc symbol of return `i` of the function `symbol`, or NULL.
const char* luau_signatures_function_return(const LuauDocs* d, const char* symbol, int i);

// ----- TableDocumentation -------------------------------------------------

// Insert (or overwrite) a TableDocumentation entry keyed by `symbol`. Starts
// with no keys. Returns 1 on success, 0 on failure.
int luau_signatures_add_table(
    LuauDocs* d,
    const char* symbol,
    const char* documentation,
    const char* learnMoreLink,
    const char* codeSample
);

// Map table key `key` to the documentation symbol `keyDocSymbol` on the
// TableDocumentation `symbol`. Returns 1 on success, 0 if absent / not a table.
int luau_signatures_table_add_key(LuauDocs* d, const char* symbol, const char* key, const char* keyDocSymbol);

// 1 if `symbol` is a TableDocumentation entry, else 0.
int luau_signatures_is_table(const LuauDocs* d, const char* symbol);

// Borrowed doc text / learn-more / code sample for the table `symbol`, or NULL.
const char* luau_signatures_table_documentation(const LuauDocs* d, const char* symbol);
const char* luau_signatures_table_learn_more(const LuauDocs* d, const char* symbol);
const char* luau_signatures_table_code_sample(const LuauDocs* d, const char* symbol);

// Borrowed documentation symbol for table key `key`, or NULL if absent.
const char* luau_signatures_table_key_doc(const LuauDocs* d, const char* symbol, const char* key);

// ----- OverloadedFunctionDocumentation -----------------------------------

// Insert (or overwrite) an empty OverloadedFunctionDocumentation entry keyed by
// `symbol`. Returns 1 on success, 0 on failure.
int luau_signatures_add_overloaded(LuauDocs* d, const char* symbol);

// Map an overload `signature` string to its documentation symbol on the
// OverloadedFunctionDocumentation `symbol`. Returns 1 on success, 0 if absent /
// not an overloaded entry.
int luau_signatures_overloaded_add(LuauDocs* d, const char* symbol, const char* signature, const char* overloadDocSymbol);

// 1 if `symbol` is an OverloadedFunctionDocumentation entry, else 0.
int luau_signatures_is_overloaded(const LuauDocs* d, const char* symbol);

// Borrowed documentation symbol for overload `signature`, or NULL if absent.
const char* luau_signatures_overloaded_doc(const LuauDocs* d, const char* symbol, const char* signature);

LUAU_END_DECLS
