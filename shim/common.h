// Shared conventions for every Luau C++ shim.
//
// All shim headers are valid C (for translate-c) and C++ (for the .cpp shims).
// Use LUAU_BEGIN_DECLS / LUAU_END_DECLS to wrap declarations, and LUAU_HANDLE to
// declare an opaque handle for a C++ class (the Zig side sees an opaque pointer;
// the .cpp reinterpret_casts it to the real type).
#pragma once

#include <stddef.h>

#ifdef __cplusplus
#define LUAU_BEGIN_DECLS extern "C" {
#define LUAU_END_DECLS }
#else
#define LUAU_BEGIN_DECLS
#define LUAU_END_DECLS
#endif

// Declare `name` as an opaque handle type: `LUAU_HANDLE(LuauFoo);`
#define LUAU_HANDLE(name) typedef struct name name

LUAU_BEGIN_DECLS

// A 0-based source position, shared by every module that reports locations.
typedef struct LuauPosition {
    unsigned int line;
    unsigned int column;
} LuauPosition;

LUAU_END_DECLS
