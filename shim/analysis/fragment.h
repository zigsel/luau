// extern "C" shim: fragment (incremental) autocomplete with documentation symbols.
//
// Wraps Luau::fragmentAutocomplete — the incremental completion entry point that
// reuses a stale (already type-checked) module and patches in a fragment of new
// source around the cursor. Each returned entry also carries its documentation
// symbol string (for hover), satisfying the Documentation binding requirement:
// an AutocompleteEntry's `documentationSymbol` is the lookup key into a
// DocumentationDatabase. Parsing/owning a full DocumentationDatabase is out of
// scope here; exposing the per-entry symbol is the bounded, self-contained slice.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Compute fragment autocomplete suggestions.
//
// `staleSrc` is the source that was previously type-checked (the "stale" module);
// `newSrc` is the current buffer contents the user is editing. The cursor is at
// (0-based) `line`/`col` within `newSrc`. Internally this builds a Frontend,
// checks `staleSrc`, then calls fragmentAutocomplete with `newSrc` so only the
// changed fragment is re-analysed. Entries are sorted by name.
//
// Returns a handle (never NULL); free with luau_fragment_free. On any failure the
// handle is returned empty (count == 0).
LuauFragment* luau_fragment_autocomplete(
    const char* staleSrc, size_t staleLen,
    const char* newSrc, size_t newLen,
    unsigned int line, unsigned int col);

// 1 if the fragment autocomplete ran successfully (FragmentAutocompleteStatus
// internal success), 0 otherwise.
int luau_fragment_ok(const LuauFragment* h);

int luau_fragment_count(const LuauFragment* h);
// Borrowed, NUL-terminated strings owned by the handle. "" when absent/OOB.
const char* luau_fragment_name(const LuauFragment* h, int i);
int luau_fragment_kind(const LuauFragment* h, int i);
// Documentation symbol for the entry (hover lookup key), or "" when absent.
const char* luau_fragment_documentation_symbol(const LuauFragment* h, int i);
int luau_fragment_deprecated(const LuauFragment* h, int i);

void luau_fragment_free(LuauFragment* h);

LUAU_END_DECLS
