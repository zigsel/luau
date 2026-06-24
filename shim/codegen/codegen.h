// Shim: Luau CodeGen — the richer CodeGen.h C++ surface (compilation stats,
// disassembly, native-execution toggles) beyond the small luacodegen.h C API.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

typedef struct LuauCodegenStats {
    size_t bytecode_size;
    size_t native_code_size;
    size_t native_data_size;
    size_t native_metadata_size;
    unsigned int functions_total;
    unsigned int functions_compiled;
    unsigned int functions_bound;
} LuauCodegenStats;

// Compile the function at `idx` with `flags`; fills `stats` if non-null. Returns
// the CodeGenCompilationResult code (0 == Success).
int luau_codegen_compile2(lua_State* L, int idx, unsigned int flags, LuauCodegenStats* stats);
// Enable/disable native execution globally for the VM.
void luau_codegen_set_native_execution_enabled(lua_State* L, int enabled);
// Disable native execution for the function at stack `level`.
void luau_codegen_disable_native_for_function(lua_State* L, int level);
// Disassemble the function at `idx` to text (caller frees with free()).
char* luau_codegen_get_assembly(lua_State* L, int idx, int include_assembly, int include_ir);

LUAU_END_DECLS
