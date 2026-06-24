// Shim: Luau Analysis — go-to-definition and top-level symbol locations.
//
// Given a self-contained module source, resolve where the symbol at a given
// position is DECLARED (go-to-definition), and enumerate the declaration
// positions of every top-level binding (document outline / symbols view).
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Resolve the DECLARATION span of the symbol at (`line`, `column`) (0-based) in
// the self-contained module `src`. On success, returns 1 and fills `*out_begin`
// and `*out_end` with the begin/end of the declaration's location. Returns 0 if
// nothing resolves at that position or an internal error occurred.
int luau_analysis_definition(const char* src, size_t len, unsigned int line, unsigned int column,
    LuauPosition* out_begin, LuauPosition* out_end);

// Collect all top-level binding declarations (locals, functions, type aliases)
// in `src`. Returns an opaque handle (never NULL on success; NULL on error)
// that the caller must release with luau_analysis_symbols_free.
LuauSymbols* luau_analysis_symbols(const char* src, size_t len);

// Number of collected symbols.
size_t luau_analysis_symbols_count(const LuauSymbols* symbols);

// Name of the i-th symbol as a malloc'd, NUL-terminated string the caller owns
// and must free(). Returns NULL if `i` is out of range or on allocation failure.
char* luau_analysis_symbols_name(const LuauSymbols* symbols, size_t i);

// Begin/end declaration position of the i-th symbol. Returns 1 on success and
// fills `*out`; returns 0 if `i` is out of range.
int luau_analysis_symbols_begin(const LuauSymbols* symbols, size_t i, LuauPosition* out);
int luau_analysis_symbols_end(const LuauSymbols* symbols, size_t i, LuauPosition* out);

// Release a handle returned by luau_analysis_symbols.
void luau_analysis_symbols_free(LuauSymbols* symbols);

LUAU_END_DECLS
