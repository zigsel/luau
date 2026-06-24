// Shim: Luau Analysis — serialize a parsed AST to JSON (external tooling / editors).
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Parse the self-contained module `src` (length `len`) and serialize its AST to
// JSON (via Luau::toJson).
//
// On success, returns a malloc'd, NUL-terminated JSON string the caller owns and
// must free(); `*out_err` is left NULL. On parse failure (or internal error),
// returns NULL and sets `*out_err` to a malloc'd, NUL-terminated error message
// the caller owns and must free().
char* luau_ast_to_json(const char* src, size_t len, char** out_err);

LUAU_END_DECLS
