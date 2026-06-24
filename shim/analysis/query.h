// Shim: Luau Analysis — inferred type at a source position (hover / LSP core).
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Infer the type at (line, column) (0-based) in the self-contained module `src`.
//
// On success, sets `*out_type` to a malloc'd, NUL-terminated string of the
// inferred type (or an empty string "" if no type was found at that position)
// and returns 1. Returns 0 (and leaves *out_type set to NULL) if nothing could
// be resolved or an internal error occurred. The caller owns `*out_type` and
// must free() it.
int luau_analysis_type_at(const char* src, size_t len, unsigned int line, unsigned int column, char** out_type);

LUAU_END_DECLS
