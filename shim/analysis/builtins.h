// Shim: Luau Analysis builtin/host type-definition checking.
//
// A host can register Luau type-definition source (declaring the types of its
// API) so that type-checking sees those declarations. This shim loads a
// DEFINITIONS source into a Frontend's globals and then type-checks a MODULE
// source against the augmented globals.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Type-check `src` against builtin globals augmented with the host type
// definitions in `defs`. Always returns a handle; free with free().
//
// If the definitions source itself fails to parse/load, that is reported as an
// error (index 0) and the module is not checked.
LuauDefCheck* luau_analysis_check_with_defs(const char* defs, size_t defs_len, const char* src, size_t src_len);

// Number of type errors produced (including any definition-load error).
int luau_analysis_defcheck_error_count(const LuauDefCheck* h);
// NUL-terminated message for error `i`, or "" if out of range.
const char* luau_analysis_defcheck_error_message(const LuauDefCheck* h, int i);
// 0-based start position for error `i`, or {0,0} if out of range.
LuauPosition luau_analysis_defcheck_error_position(const LuauDefCheck* h, int i);
// Whether loading the definitions source succeeded (1) or failed (0).
int luau_analysis_defcheck_defs_ok(const LuauDefCheck* h);

void luau_analysis_defcheck_free(LuauDefCheck* h);

LUAU_END_DECLS
