// Opaque handle typedefs for every shimmed Luau C++ class.
//
// Centralising these lets any module's shim reference any class as an opaque
// pointer without including that class's shim header — so the shim files stay
// decoupled and can be authored independently. (When the binding grows, this
// file is regenerated from the upstream class list.)
#pragma once

#include "common.h"

LUAU_BEGIN_DECLS

// the Lua VM state (also forward-declared in lua.h; identical typedef is fine)
typedef struct lua_State lua_State;

LUAU_HANDLE(LuauParseResult);
LUAU_HANDLE(LuauConfig);
LUAU_HANDLE(LuauCheck);
LUAU_HANDLE(LuauDefCheck);
LUAU_HANDLE(LuauAutocomplete);
LUAU_HANDLE(LuauComplete);
LUAU_HANDLE(LuauBytecodeBuilder);
LUAU_HANDLE(LuauBcGraph);
LUAU_HANDLE(LuauAstBuilder);
LUAU_HANDLE(LuauAstNode);
LUAU_HANDLE(LuauTokens);
LUAU_HANDLE(LuauProject);
LUAU_HANDLE(LuauTypes);
LUAU_HANDLE(LuauType);
LUAU_HANDLE(LuauTypePack);
LUAU_HANDLE(LuauRelations);
LUAU_HANDLE(LuauModule);
LUAU_HANDLE(LuauTypePath);
LUAU_HANDLE(LuauFragment);
LUAU_HANDLE(LuauAsmX64);
LUAU_HANDLE(LuauIrBuilder);
LUAU_HANDLE(LuauLint);
LUAU_HANDLE(LuauDfg);
LUAU_HANDLE(LuauCst);
LUAU_HANDLE(LuauSymbols);
LUAU_HANDLE(LuauCompileErrors);
LUAU_HANDLE(LuauDocs);
LUAU_HANDLE(LuauAttributes);
LUAU_HANDLE(LuauX64Asm);
LUAU_HANDLE(LuauX64Reg);
LUAU_HANDLE(LuauX64Operand);
LUAU_HANDLE(LuauX64Label);
LUAU_HANDLE(LuauAsmA64);
LUAU_HANDLE(LuauA64Reg);
LUAU_HANDLE(LuauA64Address);

// Analysis TAIL: a self-contained parse-and-trace handle (RequireTracer +
// AstUtils type-guard matching). Owns its own Allocator/AstNameTable/ParseResult.
LUAU_HANDLE(LuauScan);

LUAU_END_DECLS
