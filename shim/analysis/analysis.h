// Shim: Luau Analysis — type checking, linting, autocomplete.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// ---- type checking & linting -------------------------------------------------

// Type-check a single self-contained Luau module. Always returns a handle.
LuauCheck* luau_analysis_check(const char* src, size_t len);

int luau_analysis_error_count(const LuauCheck* h);
const char* luau_analysis_error_message(const LuauCheck* h, int i);
LuauPosition luau_analysis_error_position(const LuauCheck* h, int i);

// Lint warnings (lint is always run by the shim).
int luau_analysis_lint_count(const LuauCheck* h);
const char* luau_analysis_lint_message(const LuauCheck* h, int i);
int luau_analysis_lint_code(const LuauCheck* h, int i);
const char* luau_analysis_lint_name(const LuauCheck* h, int i);
LuauPosition luau_analysis_lint_position(const LuauCheck* h, int i);

void luau_analysis_check_free(LuauCheck* h);

// ---- autocomplete ------------------------------------------------------------

// Autocomplete at (line, column) (0-based) in `src`. Always returns a handle.
LuauAutocomplete* luau_autocomplete(const char* src, size_t len, unsigned int line, unsigned int column);
int luau_autocomplete_count(const LuauAutocomplete* h);
const char* luau_autocomplete_name(const LuauAutocomplete* h, int i);
int luau_autocomplete_kind(const LuauAutocomplete* h, int i);
void luau_autocomplete_free(LuauAutocomplete* h);

LUAU_END_DECLS
