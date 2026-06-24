// Shim: structured COMPILE ERRORS over Luau::compileOrThrow.
//
// The public C `luau_compile` only ever produces a bytecode blob — on failure
// it encodes the error *into* the bytecode for luau_load to decode. This shim
// instead compiles via Luau::compileOrThrow and, on failure, captures the
// structured error (a source location + a message) so callers can report it
// without round-tripping through the VM loader.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Compile `src`. Always returns a non-null handle; inspect the outcome with the
// accessors below. Free with `luau_compile_errors_free`.
//
// optimizationLevel / debugLevel mirror lua_CompileOptions (defaults 1/1).
LuauCompileErrors* luau_compile_errors_check(const char* src, size_t len, int optimizationLevel, int debugLevel);

// 1 if compilation succeeded (no errors), else 0.
int luau_compile_errors_ok(const LuauCompileErrors* e);

// Number of captured errors. compileOrThrow stops at the first CompileError or
// the ParseErrors batch, so this is 1 for a CompileError and >= 1 for parse
// failures.
int luau_compile_errors_count(const LuauCompileErrors* e);

// The i-th error message (borrowed, valid until free). NULL if out of range.
const char* luau_compile_errors_message(const LuauCompileErrors* e, int i);

// The begin position of the i-th error. {0,0} if out of range.
LuauPosition luau_compile_errors_position(const LuauCompileErrors* e, int i);

void luau_compile_errors_free(LuauCompileErrors* e);

LUAU_END_DECLS
