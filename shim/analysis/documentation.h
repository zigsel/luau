// Shim: the Luau DOCUMENTATION database (Analysis/Documentation.h).
//
// LIMIT — what the public Luau API actually offers here is *only* the in-memory
// data model: `Documentation.h` declares the structs (BasicDocumentation,
// FunctionDocumentation, ...) and the type alias
//   DocumentationDatabase = DenseHashMap<DocumentationSymbol, Documentation>
// but ships NO loader/parser (no JSON reader, no file reader) and no public
// "lookup symbol -> doc" helper. A real editor builds the database itself
// (e.g. by parsing an .json doc file) and indexes it by the `documentationSymbol`
// string that autocomplete entries carry.
//
// This shim therefore exposes the only thing that exists portably: an in-memory
// DocumentationDatabase you can populate with BasicDocumentation entries and
// then query by symbol — i.e. the bridge from an autocomplete
// `documentationSymbol` to its documentation text / code sample / learn-more
// link. Only the BasicDocumentation variant is surfaced (the common case for a
// symbol's hover text); the function/table/overload variants are not, as they
// reference other symbols rather than carrying displayable text.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Create an empty documentation database. Free with `luau_docs_free`.
LuauDocs* luau_docs_new(void);

// Insert (or overwrite) a BasicDocumentation entry keyed by `symbol`.
// Any of `documentation` / `learnMoreLink` / `codeSample` may be NULL (treated
// as empty). Returns 1 on success, 0 on failure.
int luau_docs_add_basic(
    LuauDocs* d,
    const char* symbol,
    const char* documentation,
    const char* learnMoreLink,
    const char* codeSample
);

// Number of entries in the database.
int luau_docs_count(const LuauDocs* d);

// 1 if `symbol` is present (and is a BasicDocumentation entry), else 0.
int luau_docs_has(const LuauDocs* d, const char* symbol);

// The documentation text / learn-more link / code sample for `symbol`. Each
// returns a borrowed string valid until the next mutation or free, or NULL if
// the symbol is absent (or not a BasicDocumentation entry).
const char* luau_docs_text(const LuauDocs* d, const char* symbol);
const char* luau_docs_learn_more(const LuauDocs* d, const char* symbol);
const char* luau_docs_code_sample(const LuauDocs* d, const char* symbol);

void luau_docs_free(LuauDocs* d);

LUAU_END_DECLS
