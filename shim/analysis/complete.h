// extern "C" shim: richer autocomplete with per-entry type/doc/insert metadata.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Compute autocomplete suggestions at (0-based) `line`/`col` in `src`.
// Entries are sorted by name. Returns a handle (never NULL); free with
// luau_complete_free.
LuauComplete* luau_complete(const char* src, size_t len, unsigned int line, unsigned int col);

int luau_complete_count(const LuauComplete* h);
// Borrowed, NUL-terminated strings owned by the handle. "" when absent/OOB.
const char* luau_complete_name(const LuauComplete* h, int i);
int luau_complete_kind(const LuauComplete* h, int i);
const char* luau_complete_type_string(const LuauComplete* h, int i);
const char* luau_complete_documentation_symbol(const LuauComplete* h, int i);
const char* luau_complete_insert_text(const LuauComplete* h, int i);
int luau_complete_deprecated(const LuauComplete* h, int i);

void luau_complete_free(LuauComplete* h);

LUAU_END_DECLS
