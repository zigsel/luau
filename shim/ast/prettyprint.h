// Shim: Luau Ast pretty-printing (transpile source -> normalized source).
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Parse `src` (length `len`) and return the pretty-printed source as a
// malloc'd NUL-terminated string the caller must free().
//
// On parse failure (or any thrown exception), returns NULL and, if `out_err`
// is non-NULL, sets *out_err to a malloc'd NUL-terminated error message the
// caller must free(). On success *out_err is left untouched.
char* luau_ast_format(const char* src, size_t len, char** out_err);

LUAU_END_DECLS
